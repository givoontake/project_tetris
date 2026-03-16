import pygame
import struct
from typing import Optional, cast

from tetris.config.define import *
from tetris.net.packet_types import *

from tetris.net.session import Session
from tetris.net.network import NetworkWorker
from tetris.net.packet_structs import *
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.fonts import Fonts

from tetris.ui.button import Button
from tetris.game.tetris_board import *
from tetris.game.tetris_session import TetrisSession
from tetris.states.base_state import BaseState
from tetris.states.define import *

class MultiPlayState(BaseState):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager,
                  net_worker: NetworkWorker, session: Session, room_title: str, max_player: int, room_password: str = None):
        super().__init__(screen, rm, net_worker, session)
        self.title = room_title
        self.password = room_password
        self.max_player = max_player

        self.players: list[TetrisSession] = []

        self.title_box: Optional[Rectangle] = None
        self.password_box: Optional[Rectangle] = None
        self.btn_exit: Optional[Button] = None

        self.room_state: RoomState = RoomState.WAIT

        self.set_layout()

    def set_layout(self):
        if self.max_player == 2:
            self.set_layout_2player()
        elif self.max_player == 5:
            self.set_layout_5player()

        else:
            pass # 예외 발생시키고 종료

    def set_layout_2player(self): 
        sw, sh = self.screen.get_size()
        header_w, header_h = sw*INFO_HEADER_WIDTH, sh*INFO_HEADER_HEIGHT
        padding_w = sw*PADDING_WIDTH_RATE

        tetris_w = int(sw*BOARD_WIDTH_RATE)
        tetris_h = int(sh*BOARD_HEIGHT_RATE)
        tetris_x = (sw // 2) - (padding_w // 2) - tetris_w
        tetris_y = header_h
        tetris_rect1 = pygame.Rect(tetris_x, tetris_y, tetris_w, tetris_h)
        tetris_player1 = TetrisSession(self.screen, tetris_rect1, self.rm, self.net_worker, False)
        tetris_player1.init_session(self.session)
        self.players.append(tetris_player1)

        tetris_rect2 = tetris_rect1.copy()
        tetris_rect2.x = (sw // 2) + (padding_w // 2)
        tetris_player2 = TetrisSession(self.screen, tetris_rect2, self.rm, self.net_worker, False)
        self.players.append(tetris_player2)
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

    def add_user(self, session: Session):
        for player in self.players:
            if player.state == TSessionState.EMPTY:
                player.init_session(session)

    # def clear(self): # 방 나가면 그냥 없는거임
    #     for player in self.players:
    #         player.clear()

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
            print("[MultiPlayState] send_delete_user() error:", e)

    def handle_event(self, ev):
        for player in self.players:
            if player.state == TSessionState.EMPTY: continue
            player.handle_event(ev)

    # ------------ 서버 → 클라 패킷 처리 ------------ #
    def handle_packet(self, data: RecvPacketStruct):
        if data.type == S2C_MULTI_START:
            start_data = cast(S2C_MULTI_START_PACKET, data)
            if start_data.is_start:
                self.room_state = RoomState.PLAY
                for player in self.players:
                    player.set_state(TSessionState.PLAY)
                pygame.mixer.music.play(-1)

        elif data.type == S2C_UPDATE_HOST:
            host_data = cast(S2C_UPDATE_HOST_PACKET, data)
            for player in self.players:
                if player.session:
                    if player.session.id == host_data.new_host_id:
                        player.set_is_host()

        elif data.type == S2C_DELETE_USER: 
            from tetris.states.lobby_state import LobbyState
            delete_user = cast(S2C_DELETE_USER_PACKET, data)
            for player in self.players:
                if player.state == TSessionState.EMPTY: continue
                if delete_user.id == player.session.id:
                    if player.session.is_self:     
                        pygame.mixer.music.stop() # 게임 도중에 그냥 나가면 로비에서는 음악나오면 안되니까
                        return LobbyState(self.screen, self.rm, self.net_worker, self.session)
                    else:
                        player.clear()
                        player.nickname.set_text("")

        elif data.type == S2C_GAMEOVER:
            gameover_data = cast(S2C_GAMEOVER_PACKET, data)
            for player in self.players:
                if player.state == TSessionState.EMPTY: continue
                if gameover_data.id == player.session.id:
                    player.set_state(TSessionState.GAMEOVER)
                    # 판정은 서버에서 해서 보내주니 뭐.. 게임오버 애니메이션 같은거 만들어서 보여주면 될 듯
            # pygame.mixer.music.stop() -> 이건 이제 엔드게임 패킷 받아야 함

        else:
            ingame_data = cast(IngamePacket, data)
            id = ingame_data.id
            for player in self.players:
                if player.state == TSessionState.EMPTY: continue
                if id == player.session.id:
                    player.handle_packet(data)

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
            
            self.handle_event(ev)

            if self.btn_exit.handle_event(ev):
                self.send_delete_user()
        
        for player in self.players:
            player.update(dt_ms)

        return self

    def draw(self):

        self.screen.fill((0, 0, 0))

        # 상단 방 제목/비밀번호
        self.draw_room_header()
        self.btn_exit.draw()

        # 보드 및 미리보기/프로필/블록
        for player in self.players:
            player.draw()
