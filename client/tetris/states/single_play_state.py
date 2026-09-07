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
from tetris.resources.define import *

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

        self.title_box: Optional[Rectangle] = None
        self.password_box: Optional[Rectangle] = None
        self.btn_exit: Optional[Button] = None

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


        # 방 제목 / 비밀번호용 상단 버튼 (단순한 박스 역할)
        draw_x, draw_y = 0, 0
        title_rect = pygame.Rect(draw_x, draw_y, header_w, header_h)
        title = f"방 제목: {self.title}"
        self.title_box = Rectangle(self.screen, title_rect, self.rm, False, None, title)
        self.title_box.set_text_size(20)

        draw_x += header_w 
        password_rect = pygame.Rect(draw_x, draw_y, header_w, header_h)
        if self.password == None:
            pw_val = "비밀번호: 없음"
        else:
            pw_val = f"비밀번호: {self.password}"
        self.password_box = Rectangle(self.screen, password_rect, self.rm, False, None, pw_val)
        self.password_box.set_text_size(20)

        from tetris.states.lobby_state import MENU_WIDTH, MENU_HEIGHT
        draw_x = sw - MENU_WIDTH
        draw_y = 0
        draw_w = MENU_WIDTH
        draw_h = MENU_HEIGHT
        exit_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        self.btn_exit = Button(self.screen, exit_rect, self.rm, "나가기", 0)
        self.btn_exit.set_images(
            self.rm.images.ui_images[UI_BUTTON_RED],
            self.rm.images.ui_images[UI_BUTTON_BLUE],
            self.rm.images.ui_images[UI_BUTTON_ORANGE],
        )
        giveup_rect = exit_rect.copy()
        giveup_rect.x -= MENU_WIDTH
        self.btn_giveup = Button(self.screen, giveup_rect, self.rm, "포기", 0)

    def clear(self):
        self.tetris_session.clear()

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
                self.queue_state(LobbyState(self.screen, self.rm, self.net_worker, self.session))

        elif data.type == S2C_GAMEOVER:
            self.tetris_session.process_gameover()
            pygame.mixer.music.stop()

        elif data.type == S2C_UPDATE_SCORE:
            update_score = cast(S2C_UPDATE_SCORE_PACKET, data)
            self.tetris_session.session.max_score = update_score.max_score

        elif isinstance(data, IngamePacket):
            self.tetris_session.handle_packet(data)

        # 그 외 패킷은 현재 싱글플레이에서는 사용하지 않음
        return self

    # ------------ 상단 방 정보 그리기 ------------ #
    def draw_room_header(self):
        self.title_box.draw()
        self.password_box.draw()

    def handle_event(self, ev: pygame.event.Event):
        if self.tetris_session.handle_event(ev) == "start":
            packet = self.net_worker.builder.build_start_pkt()
            self.net_worker.send_packet(packet)

        if self.btn_exit.handle_event(ev):
            packet = self.net_worker.builder.build_delete_user_pkt()
            self.net_worker.send_packet(packet)

        if self.tetris_session.state == TSessionState.PLAY:
            if self.btn_giveup.handle_event(ev):
                packet = self.net_worker.builder.build_giveup_pkt()
                self.net_worker.send_packet(packet)

    # ------------ 이벤트 처리 ------------ #
    def update(self, dt_ms, events):
        for ev in events:
            self.handle_event(ev)
     
        self.tetris_session.update(dt_ms)
        self.update_fade(dt_ms)

        return self.consume_state()

    def draw(self):
        if self.tetris_session.board is None:
            return

        self.screen.blit(self.rm.images.ui_images[UI_INGAME_BACKGROUND], (0, 0))

        # 상단 방 제목/비밀번호
        self.draw_room_header()
        self.btn_exit.draw()
        if self.tetris_session.state == TSessionState.PLAY:
            self.btn_giveup.draw()

        # 보드 및 미리보기/프로필/블록
        self.tetris_session.draw()
