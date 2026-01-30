import pygame
import struct
from typing import Optional

from tetris.config.define import *
from tetris.net.packet_type import *

from tetris.net.session import Session
from tetris.net.network import NetworkWorker
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.font_manager import FontManager

from tetris.ui.button import Button
from tetris.game.tetris_board import *
from tetris.game.room_session import RoomSession
from tetris.states.base_state import BaseState

RIGHT = 0
LEFT = 1
ROTATE = 2 
DOWN = 3
DROP = 4
UP = 5

class SinglePlayState(BaseState):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager, fm: FontManager,
                  net_worker: NetworkWorker, session: Session, room_title: str, room_password: str = None):
        super().__init__(screen, rm, fm, net_worker, session)
        self.title = room_title
        self.password = room_password

        # UI 요소들
        # self.room_session.board: Optional[TetrisBoard] = None
        self.btn_start: Optional[Button] = None

        self.title_box: Optional[Rectangle] = None
        self.password_box: Optional[Rectangle] = None
        self.btn_exit: Optional[Button] = None

        self.set_layout()
        self.clear()

    def set_layout(self):
        header_w, header_h = 300, 50

        sw, sh = self.screen.get_size()
        board_w = int(sw*0.4)
        board_h = int(sh*0.8)
        board_x = ((sw - board_w) // 2) + int(board_w*0.15)
        board_y = header_h
        board_rect = pygame.Rect(board_x, board_y, board_w, board_h)
        self.room_session = RoomSession(self.screen, board_rect, self.rm, self.fm, self.net_worker, self.session, True)
        # self.board = TetrisBoard(self.screen, board_rect, self.fm, self.room_session)
        self.room_session.board.set_score(0)

        btn_w = self.room_session.board.rect.w - self.room_session.board.score_box.rect.w
        btn_h = sh*0.1
        btn_x = board_x
        btn_y = board_y + board_h
        btn_rect = pygame.Rect(btn_x, btn_y, btn_w, btn_h)

        self.btn_start = Button(self.screen, btn_rect, self.rm, self.fm, None, "게임 시작", True)

        # 방 제목 / 비밀번호용 상단 버튼 (단순한 박스 역할)
        draw_x, draw_y = 0, 0
        title_rect = pygame.Rect(draw_x, draw_y, header_w, header_h)
        title = f"방 제목: {self.title}"
        self.title_box = Rectangle(self.screen, title_rect, self.fm, None, title)

        draw_x += header_w 
        password_rect = pygame.Rect(draw_x, draw_y, header_w, header_h)
        if self.password == None:
            pw_val = "비밀번호: 없음"
        else:
            pw_val = f"비밀번호: {self.password}"
        self.password_box = Rectangle(self.screen, password_rect, self.fm, None, pw_val)

        from tetris.states.lobby_state import MENU_WIDTH, MENU_HEIGHT
        draw_x = sw - MENU_WIDTH
        draw_y = 0
        draw_w = MENU_WIDTH
        draw_h = MENU_HEIGHT
        exit_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        self.btn_exit = Button(self.screen, exit_rect, self.rm, self.fm, None, "나가기")

    def clear(self):
        self.room_session.clear()

    def handle_event(self, ev):
        self.room_session.handle_event(ev)

    def send_start(self):
        size = 2 + 1
        type = C2S_START

        packet_bytes = struct.pack(
            "<hb",
            size,
            type,
        )

        try:
            self.net_worker.send_packet(packet_bytes)
        except Exception as e:
            print("[SinglePlayState] send_start() error:", e)

    def send_delete_user(self):
        size = 2 + 1
        type = C2S_DELETE_USER

        packet_bytes = struct.pack(
            "<hb",
            size,
            type,
        )

        try:
            self.net_worker.send_packet(packet_bytes)
        except Exception as e:
            print("[SinglePlayState] send_delete_user() error:", e)

    # ------------ 서버 → 클라 패킷 처리 ------------ #
    def handle_packet(self, data: Optional[dict]):
        packet_type = data.get("type")

        if packet_type == S2C_START:
            # 게임이 시작되었다고 서버가 알려줌
            if data.get("is_start"):
                self.room_session.board.start_game()
                pygame.mixer.music.play(-1)

        elif packet_type == S2C_DELETE_USER: # 사실 싱글에는 의미 없음
            from tetris.states.lobby_state import LobbyState
            delete_id = data.get("id")
            if delete_id == self.room_session.session.id:
                pygame.mixer.music.stop()
                return LobbyState(self.screen, self.rm, self.fm, self.net_worker, self.session)

        elif packet_type == S2C_GAMEOVER:
            self.room_session.clear()
            pygame.mixer.music.stop()

        elif packet_type == S2C_UPDATE_SCORE:
            self.room_session.session.max_score = data.get("max_score")

        else:
            if self.room_session.session.id == data.get("id"):
                self.room_session.handle_packet(data)

        # 그 외 패킷은 현재 싱글플레이에서는 사용하지 않음
        return self

    # ------------ 상단 방 정보 그리기 ------------ #
    def draw_room_header(self):
        self.title_box.draw()
        self.password_box.draw()

    # ------------ 이벤트 처리 ------------ #
    def update(self, dt_ms, events):
        for ev in events:
            if ev.type == pygame.QUIT:
                # 상위 루프에서 처리
                continue

            if (ev.type == pygame.KEYDOWN or ev.type == pygame.KEYUP) and self.room_session.board.game_started:
                self.handle_event(ev)

            # # 게임 시작 버튼 클릭
            # elif ev.type == pygame.MOUSEBUTTONDOWN == 1:
            if self.btn_start and self.btn_start.handle_event(ev):
                self.send_start()

            if self.btn_exit.handle_event(ev):
                self.send_delete_user()
                
        self.room_session.update(dt_ms)

        return self

    def draw(self):
        if self.room_session.board is None:
            return

        self.screen.fill((0, 0, 0))

        # 상단 방 제목/비밀번호
        self.draw_room_header()
        self.btn_exit.draw()

        # 보드 및 미리보기/프로필/블록
        self.room_session.board.draw()

        # 게임 시작 버튼 (게임 시작 전)
        if not self.room_session.board.game_started and self.btn_start:
            self.btn_start.draw()
