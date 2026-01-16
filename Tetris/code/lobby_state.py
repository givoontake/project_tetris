import pygame
import struct
from typing import Optional

from define import *
from packet_type import *

from session import Session
from network import NetworkWorker
from resource_manager import *

from button import Button
from inputbox import InputBox

from room_window import RoomWindow
from chat_window import ChatWindow
from my_info import MyInfo
from create_room_window import RoomCreateWindow
from single_play import SinglePlayState
from packet_manager import *
from shutter_animaion import ShutterAnimation

LOBBY = 1
CREATE_ROOM = 2

class LobbyState:
    def __init__(self, screen, rm: ResourceManager, net_worker: NetworkWorker, my_session: Session, is_animation: bool = False):
        self.screen = screen
        self.rm = rm
        self.my_session = my_session
        self.net_worker = net_worker
        self.logo_surface = None
        self.buttons: list[Button] = []
        self.room_window = RoomWindow(pygame.Rect(50, 200, 1000, 400))
        self.chat_window = ChatWindow(50, 600, 1000, 200)
        input_box_rect = pygame.Rect(50, 810, 1000, 30)
        self.chat_input_box = InputBox(input_box_rect.x, input_box_rect.y, input_box_rect.w, input_box_rect.h,
                                        "채팅을 입력하세요", MAX_CHAT_INPUT, is_password=False, allow_korean=True)
        self.my_info_rect = MyInfo(pygame.Rect(1050, 600, 300, 300), self.my_session)
        self.btn_draw_x, self.btn_draw_y = 0, 0

        self.reactable_screen = LOBBY

        self.open_shutter = ShutterAnimation(self.screen, self.rm)
        self.is_animation = is_animation
    
        self.set_layout()

        if is_animation:
            self.open_shutter.start_animation()

    def set_layout(self):
        sw, sh = self.screen.get_size()

        self.logo_surface = self.rm.button_images[BUTTON_LOGO_IDLE]
        logo_w, logo_h = self.logo_surface.get_size()

        btn_w, btn_h = 200, 100
        #btn_y = 0

        top_menus_text = ["방만들기", "상점", "설정"]

        logo = Button(self.btn_draw_x, self.btn_draw_y, logo_w, logo_h, None, self.rm, BUTTON_LOGO_IDLE, BUTTON_LOGO_HOVER, BUTTON_LOGO_PRESS)
        self.buttons.append(logo)
        self.btn_draw_x += logo_w

        for btn_text in top_menus_text:
            self.buttons.append(Button(self.btn_draw_x, self.btn_draw_y, btn_w, btn_h, btn_text, self.rm))
            self.btn_draw_x += btn_w

    def on_resize(self, w, h, screen):
        self.screen = screen
        self.set_layout()

    def send_message(self, message: str): # 메세지는 가변이라 문자열 포맷을 크기만큼 만들어 직접 전송
        if len(message) == 0: return
        type = C2S_MESSAGE
        id = self.my_session.id
        message = message.encode("utf-8")
        message_bytes = len(message)
        size = 2 + 1 + 4 + message_bytes 

        packet_bytes = struct.pack(
            f"<hbi{message_bytes}s",
            size,
            type,      # or C2S_LOGIN이 아니라 실제 메시지 타입 상수
            id,
            message
        )

        try:
            self.net_worker.send_packet(packet_bytes)
        except Exception as e:
            print("[LoginState] send_login() error:", e)

    def send_create_room(self, data: dict[str]):
        id = self.my_session.id
        max_user = data["max_user"]
        room_name = data["room_name"]
        room_name = room_name.encode("utf-8")
        
        if data.get("room_password") == None:
            size = 2+1+4+1+MAX_ROOM_NAME
            type = C2S_ADD_OPEN_ROOM
            packet_bytes = struct.pack(
                f"<hbib{MAX_ROOM_NAME}s",
                size,
                type,
                id,
                max_user,
                room_name
            )
        else:
            size = 2+1+4+1+MAX_ROOM_NAME + MAX_ROOM_PASSWORD
            type = C2S_ADD_LOCK_ROOM
            room_password = data["room_password"]
            room_password = room_password.encode("utf-8")

            packet_bytes = struct.pack(
                f"<hbib{MAX_ROOM_NAME}s{MAX_ROOM_PASSWORD}s",
                size,
                type,
                id,
                max_user,
                room_name,
                room_password
            )
            
        
        try:
            self.net_worker.send_packet(packet_bytes)
        except Exception as e:
            print("[LoginState] send_create_room() error:", e)

    def handle_packet(self, data: dict):
        if data:
            #print(f"LobbyState->handle_packet() recv_bytes: {data.get("size")} / recv type: {data.get("type")}")
            if data.get("type") == S2C_MESSAGE:
                self.chat_window.add_new_message(data.get("user_name"), data.get("message"))

            elif data.get("type") == S2C_ADD_OPEN_ROOM:
                return SinglePlayState(self.screen, self.rm, self.net_worker, self.my_session, data.get("room_name"))

            elif data.get("type") == S2C_ADD_LOCK_ROOM:
                return SinglePlayState(self.screen, self.rm, self.net_worker, self.my_session, data.get("room_name"), data.get("room_password"))
            
        return self


    def update(self, dt_ms, events):
        if self.is_animation and self.open_shutter.is_active: 
            self.open_shutter.update(dt_ms)
            return

        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit()
                raise SystemExit

            if self.reactable_screen == LOBBY:
                for btn in self.buttons:
                    if btn.handle_event(ev):
                        if btn.text == "방만들기":
                            self.room_create_window = RoomCreateWindow(self.screen, self.rm)
                            self.room_create_window.open()
                            self.reactable_screen = CREATE_ROOM

                self.room_window.handle_event(ev)
                self.chat_window.handle_event(ev)
                send_input = self.chat_input_box.handle_event(ev)
                if send_input is not None:
                    self.send_message(send_input)

            elif self.reactable_screen == CREATE_ROOM:
                # 어떤 버튼이 눌렸느냐에 따른 동작 추가
                data = self.room_create_window.handle_event(ev)
                if data == "취소":
                    self.reactable_screen = LOBBY
                elif data == None:
                    pass
                else: 
                    self.send_create_room(data)
                    
        if self.reactable_screen == LOBBY: self.chat_input_box.update(dt_ms)
        elif self.reactable_screen == CREATE_ROOM: self.room_create_window.update(dt_ms)
        
        return self

    def draw(self):
        self.screen.fill(BLACK)

        for btn in self.buttons:
            btn.draw(self.screen)

        self.room_window.draw(self.screen)
        self.chat_window.draw(self.screen)
        self.chat_input_box.draw(self.screen)
        self.my_info_rect.draw(self.screen)

        if self.reactable_screen == CREATE_ROOM: self.room_create_window.draw()
        if self.is_animation and self.open_shutter.is_active: self.open_shutter.draw()