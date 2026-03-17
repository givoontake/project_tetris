import pygame
import struct
from typing import Optional, cast

from tetris.config.define import *
from tetris.net.packet_types import *

from tetris.net.session import Session
from tetris.net.network import NetworkWorker
from tetris.net.packet_types import *
from tetris.net.packet_structs import *
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.fonts import Fonts

from tetris.ui.button import Button
from tetris.game.tetris_board import *
from tetris.game.tetris_session import TetrisSession
from tetris.states.base_state import BaseState
from tetris.states.define import *

class SinglePlayState(BaseState):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager,
                  net_worker: NetworkWorker, session: Session, room_title: str, room_password: str = None):
        super().__init__(screen, rm, net_worker, session)
        self.title = room_title
        self.password = room_password

        # UI 요소들
        # self.room_session.board: Optional[TetrisBoard] = None
        # self.btn_start: Optional[Button] = None

        self.title_box: Optional[Rectangle] = None
        self.password_box: Optional[Rectangle] = None
        self.btn_exit: Optional[Button] = None

        self.anim_elapsed_ms = 0
        self.anim_target_line = BOARD_ROWS
        self.is_animate = False

        self.set_layout()

    def set_layout(self):
        sw, sh = self.screen.get_size()
        header_w, header_h = sw*INFO_HEADER_WIDTH, sh*INFO_HEADER_HEIGHT
        tetris_w = int(sw*BOARD_WIDTH_RATE)
        tetris_h = int(sh*BOARD_HEIGHT_RATE)
        tetris_x = ((sw - int(tetris_w*0.66)) // 2)
        tetris_y = header_h
        tetris_rect = pygame.Rect(tetris_x, tetris_y, tetris_w, tetris_h)
        self.tetris_session = TetrisSession(self.screen, tetris_rect, self.rm, self.net_worker, True)
        self.tetris_session.init_session(self.session)
        # self.board = TetrisBoard(self.screen, board_rect, self.fm, self.room_session)

        # self.btn_start = Button(self.screen, btn_rect, self.rm, self.fm, None, "게임 시작", True)

        # 방 제목 / 비밀번호용 상단 버튼 (단순한 박스 역할)
        draw_x, draw_y = 0, 0
        title_rect = pygame.Rect(draw_x, draw_y, header_w, header_h)
        title = f"방 제목: {self.title}"
        self.title_box = Rectangle(self.screen, title_rect, self.rm, None, title)

        draw_x += header_w 
        password_rect = pygame.Rect(draw_x, draw_y, header_w, header_h)
        if self.password == None:
            pw_val = "비밀번호: 없음"
        else:
            pw_val = f"비밀번호: {self.password}"
        self.password_box = Rectangle(self.screen, password_rect, self.rm, None, pw_val)

        from tetris.states.lobby_state import MENU_WIDTH, MENU_HEIGHT
        draw_x = sw - MENU_WIDTH
        draw_y = 0
        draw_w = MENU_WIDTH
        draw_h = MENU_HEIGHT
        exit_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        self.btn_exit = Button(self.screen, exit_rect, self.rm, None, "나가기")
        giveup_rect = exit_rect.copy()
        giveup_rect.x -= MENU_WIDTH
        self.btn_giveup = Button(self.screen, giveup_rect, self.rm, None, "포기")

    def clear(self):
        self.tetris_session.clear()

    def reset(self):
        self.tetris_session.reset()
        self.anim_target_line = BOARD_ROWS
        self.anim_elapsed_ms = 0

    def play_gameover_anim(self, dt_ms) -> bool:
        if self.is_animate == False: return False
    
        self.anim_elapsed_ms += dt_ms
        grid = self.tetris_session.board.grid
        if self.anim_elapsed_ms > 100:
            self.anim_elapsed_ms -= 100
            self.anim_target_line -= 1
            done_flag = True
            for y in range(BOARD_COLS): # 1줄(가로)이 다 None이면 끝.
                if grid[self.anim_target_line][y] != None:
                    grid[self.anim_target_line][y] = 'G'
                    done_flag = False

            return done_flag
        return False
    def send_delete_user(self):
        data = C2S_DELETE_USER_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_DELETE_USER
        values = self.net_worker._pm.struct_to_values(data)
        packet_bytes = struct.pack(data.FMT, *values)

        try:
            self.net_worker.send_packet(packet_bytes)
        except Exception as e:
            print("[SinglePlayState] send_delete_user() error:", e)

    def send_giveup(self):
        data = C2S_GIVEUP_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_GIVEUP
        values = self.net_worker._pm.struct_to_values(data)
        packet_bytes = struct.pack(data.FMT, *values)

        try:
            self.net_worker.send_packet(packet_bytes)
        except Exception as e:
            print("[SinglePlayState] giveup_user() error:", e)

    # ------------ 서버 → 클라 패킷 처리 ------------ #
    def handle_packet(self, data: RecvPacketStruct):
        if data.type == S2C_SINGLE_START:
            start = cast(S2C_SINGLE_START_PACKET, data)
            self.tetris_session.set_score(start.score)
            self.tetris_session.set_state(TSessionState.PLAY)
            pygame.mixer.music.play(-1)

        elif data.type == S2C_DELETE_USER:
            from tetris.states.lobby_state import LobbyState
            delete_user = cast(S2C_DELETE_USER_PACKET, data)
            if delete_user.id == self.tetris_session.session.id:
                pygame.mixer.music.stop()
                return LobbyState(self.screen, self.rm, self.net_worker, self.session)

        elif data.type == S2C_GAMEOVER:
            self.is_animate = True
            pygame.mixer.music.stop()

        elif data.type == S2C_UPDATE_SCORE:
            update_score = cast(S2C_UPDATE_SCORE_PACKET, data)
            self.tetris_session.session.max_score = update_score.max_score

        else:
            self.tetris_session.handle_packet(data)

        # 그 외 패킷은 현재 싱글플레이에서는 사용하지 않음
        return self

    # ------------ 상단 방 정보 그리기 ------------ #
    def draw_room_header(self):
        self.title_box.draw()
        self.password_box.draw()

    def handle_event(self, ev: pygame.event.Event):
        self.tetris_session.handle_event(ev)

        if self.btn_exit.handle_event(ev):
            self.send_delete_user()

        if self.btn_giveup.handle_event(ev):
            if self.tetris_session.state == TSessionState.PLAY:
                self.send_giveup()

    # ------------ 이벤트 처리 ------------ #
    def update(self, dt_ms, events):
        if self.is_animate:
            if self.play_gameover_anim(dt_ms):
                self.is_animate = False
                self.reset()
            return

        for ev in events:
            self.handle_event(ev)
     
        self.tetris_session.update(dt_ms)

        return self

    def draw(self):
        if self.tetris_session.board is None:
            return

        self.screen.fill((0, 0, 0))

        # 상단 방 제목/비밀번호
        self.draw_room_header()
        self.btn_exit.draw()
        self.btn_giveup.draw()

        # 보드 및 미리보기/프로필/블록
        self.tetris_session.draw()
