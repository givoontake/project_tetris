import pygame
import struct
from typing import Optional, cast

from tetris.config.define import *
from tetris.net.packet_types import *

from tetris.net.session import Session
from tetris.net.network import NetworkWorker
from tetris.resources.resource_manager import *
from tetris.resources.font_manager import *

from tetris.ui.button import Button
from tetris.ui.inputbox import InputBox
from tetris.ui.room_list import RoomList
from tetris.ui.chat_window import ChatWindow
from tetris.ui.my_info import Profile
from tetris.ui.create_room_window import RoomCreateWindow
from tetris.states.single_play import SinglePlayState
from tetris.states.multi_play import MultiPlayState
from tetris.states.base_state import BaseState
from tetris.net.packet_manager import *
from tetris.animation.shutter_animaion import ShutterAnimation

MENU_WIDTH = 200
MENU_HEIGHT = 100

class LobbyState(BaseState):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager, fm: FontManager,
                 net_worker: NetworkWorker, session: Session, is_animation: bool = False):
        super().__init__(screen, rm, fm, net_worker, session)
        self.top_menus: list[Button] = []
        self.room_list = RoomList(screen, pygame.Rect(50, 150, 1000, 400), rm, fm)
        input_box_rect = pygame.Rect(50, 810, 1000, 30)
        self.chat_input_box = InputBox(screen, input_box_rect, self.fm,
                                       "채팅을 입력하세요", MAX_CHAT_INPUT, is_password=False, allow_korean=True)
        self.chat_window = ChatWindow(screen, pygame.Rect(50, 600, 1000, 200), self.chat_input_box.font)
        self.my_info_rect = Profile(screen, pygame.Rect(1050, 600, 300, 300), fm, session)

        self.room_create_window = None
        # self.reactable_screen = LOBBY
        self.open_shutter = ShutterAnimation(screen, rm)
        self.is_animation = is_animation
    
        self.set_layout()

        if is_animation:
            self.open_shutter.start_animation()

    def set_layout(self):
        draw_x, draw_y = 50, 0
        logo_w, logo_h = self.rm.logo.get_size()
        logo_rect = pygame.Rect(draw_x, draw_y, logo_w, logo_h)
        logo = Button(self.screen, logo_rect, self.rm, self.fm, self.rm.logo, "", False)
        self.top_menus.append(logo)
        draw_x += logo_rect.w
        menu_texts: list[str] = ["빠른시작", "방만들기", "상점", "설정", "", "게임종료"]
        for menu_text in menu_texts:
            menu_rect = pygame.Rect(draw_x, draw_y, MENU_WIDTH, MENU_HEIGHT) 
            menu = Button(self.screen, menu_rect, self.rm, self.fm, None, menu_text, True)
            self.top_menus.append(menu)
            draw_x += MENU_WIDTH

    # def on_resize(self, w, h, screen):
    #     self.screen = screen
    #     self.set_layout()

    def send_message(self, message: str): # 메세지는 가변이라 문자열 포맷을 크기만큼 만들어 직접 전송
        if len(message) == 0: return
        data = C2S_MESSAGE_PACKET()
        data.type = C2S_MESSAGE
        data.id = self.session.id
        data.message = message.encode("utf-8")
        message_bytes = len(message)
        data.size = 2 + 1 + 4 + message_bytes
        values = self.net_worker._pm.struct_to_values(data)

        packet_bytes = struct.pack(data.FMT, *values)

        try:
            self.net_worker.send_packet(packet_bytes)
        except Exception as e:
            print("[LoginState] send_login() error:", e)

    def handle_packet(self, data: RecvPacketStruct):
        if data:
            if data.type == S2C_MESSAGE:
                message_data = cast(S2C_MESSAGE_PACKET, data)
                self.chat_window.add_new_message(message_data.user_name, message_data.message)

            elif data.type == S2C_ADD_OPEN_ROOM:
                open_data = cast(S2C_ADD_OPEN_ROOM_PACKET, data)
                if open_data.max_user == 1:
                    return SinglePlayState(self.screen, self.rm, self.fm, self.net_worker, 
                                           self.session, open_data.room_name)
                elif open_data.max_user == 2 or open_data.max_user == 5:
                    return MultiPlayState(self.screen, self.rm, self.fm, self.net_worker, 
                                          self.session, open_data.room_name, open_data.max_user)

            elif data.type == S2C_ADD_LOCK_ROOM:
                lock_data = cast(S2C_ADD_LOCK_ROOM_PACKET, data)
                if lock_data.max_user == 1:
                    return SinglePlayState(self.screen, self.rm, self.fm, self.net_worker, 
                                           self.session, lock_data.room_name, lock_data.room_password)
                elif lock_data.max_user == 2 or lock_data.max_user == 5:
                    return MultiPlayState(self.screen, self.rm, self.fm, self.net_worker, 
                                          self.session, lock_data.room_name, lock_data.max_user, lock_data.room_password)
        return self

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.QUIT:
            pygame.quit()
            raise SystemExit
        
        event = None
        if self.room_create_window == None:
            for menu in self.top_menus:
                if menu.handle_event(ev): # 이벤트 함수의 반환값 형태 통일이 필요할 것 같긴 한데..
                    event = menu.idle.text
                    break

            if event is not None:
                if event == "방만들기":
                    self.room_create_window = RoomCreateWindow(self.screen, self.rm, self.fm, self.net_worker, self.session)
                    
                # 나중에 메뉴별 상태 만들고 동작 추가
                return

            event = self.room_list.handle_event(ev)
            if event is not None:
                # 나중에 방 참가 패킷 전송 추가
                pass
            # return
            self.chat_window.handle_event(ev) # 보여주기만 하므로 반환값은 없음
            message = self.chat_input_box.handle_event(ev)
            if message is not None:
                self.send_message(message)

        else:
            str = self.room_create_window.handle_event(ev)
            if str == "만들기" or str == "취소":
                self.room_create_window = None

    def update(self, dt_ms, events):
        if self.is_animation and self.open_shutter.is_active: 
            self.open_shutter.update(dt_ms)
            return
        
        self.chat_input_box.update(dt_ms)
                    
        for ev in events:
            self.handle_event(ev)

        if self.room_create_window:
            self.room_create_window.update(dt_ms)
        
        return self

    def draw(self):
        self.screen.fill(BLACK)

        for menu in self.top_menus:
            menu.draw()

        self.room_list.draw()
        self.chat_window.draw()
        self.chat_input_box.draw()
        self.my_info_rect.draw()

        if self.room_create_window: self.room_create_window.draw()
        if self.is_animation and self.open_shutter.is_active: self.open_shutter.draw()