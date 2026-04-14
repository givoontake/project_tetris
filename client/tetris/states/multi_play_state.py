import pygame
import struct
from typing import Optional, cast

from tetris.config.define import *
from tetris.net.packet_types import *

from tetris.net.session import Session
from tetris.net.network import NetworkWorker
from tetris.net.packet_structs import *
from tetris.net.error_types import *
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.fonts import Fonts

from tetris.ui.button import Button
from tetris.ui.popupbox import PopupBox
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
        self.error_popup = None
        self.winner_popup = None
        self.reactable = True

        self.host_index: int = -1
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

    def set_layout_5player(self):
        self.set_layout_2player()
        right_rect = self.players.pop().rect.copy()

        sub_w = right_rect.w // 2
        sub_h = right_rect.h // 2

        for row in range(2):
            for col in range(2):
                sub_rect = pygame.Rect(
                    right_rect.x + (sub_w * col),
                    right_rect.y + (sub_h * row),
                    sub_w,
                    sub_h,
                )
                tetris_player = TetrisSession(self.screen, sub_rect, self.rm, self.net_worker, False)
                self.players.append(tetris_player)

    def reset_room(self):
        pygame.mixer.music.stop()
        for player in self.players:
            if player.session == None: continue
            player.reset()

    def add_user(self, session: Session):
        for player in self.players:
            if player.session == None: continue
            if player.session.id == session.id:
                return

        for player in self.players:
            if player.session == None:
                player.init_session(session)
                if self.host_index != -1 and self.players[self.host_index].session and self.players[self.host_index].session.is_self:
                    player.make_btn_kick()
                break

    def find_host_index(self) -> int:
        for i in range(len(self.players)):
            if self.players[i].is_host: return i

        return -1

    def handle_event(self, ev):
        if self.reactable == False:
            if self.error_popup: 
                if self.error_popup.handle_event(ev) == "확인":
                    self.error_popup = None
                    self.reactable = True
            
            elif self.winner_popup:
                if self.winner_popup.handle_event(ev) == "확인":
                    self.winner_popup = None
                    self.reactable = True

        for player in self.players:
            if player.session == None: continue
            res = player.handle_event(ev)
            if res == "start":
                packet = self.net_worker.builder.build_start_pkt()
                self.net_worker.send_packet(packet)

            elif res == "ready":
                packet = self.net_worker.builder.build_ready_pkt()
                self.net_worker.send_packet(packet)

            elif res == "kick":
                packet = self.net_worker.builder.build_kick_pkt(player.session.id)
                self.net_worker.send_packet(packet)

    # ------------ 서버 → 클라 패킷 처리 ------------ #
    def handle_packet(self, data: RecvPacketStruct):
        if data.type == S2C_ERROR:
            error_data = cast(S2C_ERROR_PACKET, data)
            error_message = ERROR_MESSAGES[error_data.error_code]
            self.error_popup = PopupBox(self.screen, self.rm, error_message, ["확인"])
            self.reactable = False
            
        elif data.type == S2C_MULTI_START:
            # start_data = cast(S2C_MULTI_START_PACKET, data)
            # if start_data.is_start:
            self.room_state = RoomState.PLAY
            for player in self.players:
                if player.session == None: continue
                player.set_state(TSessionState.PLAY)
            pygame.mixer.music.play(-1)

        elif data.type == S2C_UPDATE_HOST:
            host_data = cast(S2C_UPDATE_HOST_PACKET, data)
            
            for i in range(len(self.players)): # 먼저 방장을 찾아 호스트로 세팅하고
                if self.players[i].session:
                    if self.players[i].session.id == host_data.new_host_id:
                        self.players[i].set_is_host()
                        self.host_index = i
                        break
            
            if self.players[self.host_index].session.is_self: # 방장이 본인이면 나머지 세션들에 강퇴버튼 추가
                for i in range(len(self.players)):
                    if self.players[i].session:
                        if i != self.host_index: self.players[i].make_btn_kick()

        elif data.type == S2C_ADD_USER:
            add_data = cast(S2C_ADD_USER_PACKET, data)
            new_session = Session(self.rm)
            new_session.set_is_not_my_session(add_data.id, add_data.name)
            self.add_user(new_session)


        elif data.type == S2C_READY:
            ready_data = cast(S2C_READY_PACKET, data)
            for player in self.players:
                if player.session == None: continue
                if player.session.id == ready_data.id:
                    player.set_ready(ready_data.is_ready)
                    break

        elif data.type == S2C_DELETE_USER: 
            from tetris.states.lobby_state import LobbyState
            delete_user = cast(S2C_DELETE_USER_PACKET, data)
            for player in self.players:
                if player.session == None: continue
                if delete_user.id == player.session.id:
                    if player.session.is_self:     
                        pygame.mixer.music.stop() # 게임 도중에 그냥 나가면 로비에서는 음악나오면 안되니까
                        self.queue_state(LobbyState(self.screen, self.rm, self.net_worker, self.session))
                    else:
                        player.clear()
                        player.nickname.set_text("")
                    break

        elif data.type == S2C_GAMEOVER:
            gameover_data = cast(S2C_GAMEOVER_PACKET, data)
            for player in self.players:
                if player.session == None: continue
                if gameover_data.id == player.session.id:
                    player.process_gameover()
                    break
                    
        elif data.type == S2C_GAMEEND:
            gameend_data = cast(S2C_GAMEEND_PACKET, data)
            winner_nickname = "?"
            for player in self.players:
                if player.session == None: continue # 세션이 None이면 상태도 EMPTY임. 따라서 상태가 EMPTY이 아니라 세션의 존재 여부를 따져야 안터짐
                if player.session.id == gameend_data.winner_id:  
                    winner_nickname = player.session.nickname
                    break

            self.reset_room() # 패킷 처리에서 리셋하지 않으면 값의 안전 보장이 불가능

            message = f"{winner_nickname} 승리!"
            self.winner_popup = PopupBox(self.screen, self.rm, message, ["확인"])
            self.reactable = False
                    
        elif data.type == S2C_MATCH_RECORD:
            match_data = cast(S2C_MATCH_RECORD_PACKET, data)
            for player in self.players:
                if player.session == None: continue
                if player.session.is_self: 
                    player.session.win = match_data.win_count
                    player.session.lose = match_data.lose_count
                    break

        elif isinstance(data, IngamePacket):
            ingame_data = cast(IngamePacket, data)
            id = ingame_data.id
            for player in self.players:
                if player.session == None: continue
                if id == player.session.id:
                    player.handle_packet(data)
                    break

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
                packet = self.net_worker.builder.build_delete_user_pkt()
                self.net_worker.send_packet(packet)
        
        for player in self.players:
            player.update(dt_ms)

        return self.consume_state()

    def draw(self):
        self.screen.fill((0, 0, 0))

        # 상단 방 제목/비밀번호
        self.draw_room_header()
        self.btn_exit.draw()

        # 보드 및 미리보기/프로필/블록
        for player in self.players:
            player.draw()

        if self.error_popup:
            self.error_popup.draw()

        elif self.winner_popup:
            self.winner_popup.draw()
