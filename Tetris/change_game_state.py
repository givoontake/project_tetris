# change_game_state.py
import time
import pygame
import struct
from menu import *
from define import *
from session import Session
from define_format import *
from packet_type import *
from asset_manager import *
from typing import Optional
from network import NetworkWorker
from chat_window import *
from room_window import *
from my_info import *
from create_room_window import *
from state_single_play import SinglePlayState

class BaseState:
    def __init__(self, screen, asset):
        self.screen = screen

    def init(self): 
        pass

    def update(self, dt_ms, events, data=None):
        """메인 루프에서 계산된 dt(ms)를 전달받아 갱신."""
        return self

    def draw(self):
        pass

    def on_resize(self, w, h, screen):
        self.screen = screen


class LoginState:
    def __init__(self, screen, asset: Optional[AssetManager], net_worker: Optional[NetworkWorker]):
        self.screen = screen
        self.net_worker = net_worker
        self.title_font = pygame.font.Font("resource/dodamdodam.ttf", 36)

        self.am = asset
        self.id_box: Optional[InputBox] = None
        self.pw_box: Optional[InputBox] = None
        self.btn_login: Optional[Button] = None
        self.popup = PopupBox(self.screen, "서버와의 연결이 원활하지 않습니다.", "재시도", "종료")
        self.popup2 = PopupBox(self.screen, "아이디 또는 비밀번호를 확인하세요.", "재시도", "종료")
        self.set_layout()

    def connect(self):
        if not self.net_worker.connect_to_server(): self.popup.visible = True

    def set_layout(self):
        sw, sh = self.screen.get_size()
        input_box_w, input_box_h = 360, 42
        center_x = (sw - input_box_w) // 2
        center_y = (sh - (input_box_h * 2 + 64 + 46)) // 2

        id_rect = pygame.Rect(center_x, center_y, input_box_w, input_box_h)
        pw_rect = pygame.Rect(center_x, center_y + input_box_h + 20, input_box_w, input_box_h)
        btn_rect = pygame.Rect((sw - 300) // 2, pw_rect.bottom + 28, 300, 100)

        self.id_box = InputBox(id_rect.x, id_rect.y, id_rect.w, id_rect.h, "아이디", MAX_INPUT)
        self.pw_box = InputBox(pw_rect.x, pw_rect.y, pw_rect.w, pw_rect.h, "비밀번호", MAX_INPUT, is_password=True)

        # 버튼은 폰트 전달 없이 생성됨
        self.btn_login = Button(
            btn_rect.x,
            btn_rect.y,
            btn_rect.w,
            btn_rect.h,
            "로그인",
            self.am
        )

    def send_login(self):
        user_id = self.id_box.text
        user_pw = self.pw_box.text
        data = {
            "size": 2 + 1 + MAX_USER_ID + MAX_USER_PASSWORD,
            "type": C2S_LOGIN,
            "user_id": user_id,
            "user_password": user_pw
        }
        try:
            self.net_worker.send_packet(self.net_worker._pm.dic_to_bytes(data))
        except Exception as e:
            print("[LoginState] send_login error:", e)

    def handle_packet(self, data: Optional[dict]):
        if data:
            # print(", ".join(f"{k}: {v}" for k, v in data.items()))
            if data.get("id") == -1:
                self.popup2.visible = True
            
            else:
                # 내 세션의 아이디를 설정하는 코드 필요
                id = data.get("id")
                nickname = data.get("user_name")
                my_session = Session(id, nickname)
                return LobbyState(self.screen, self.am, self.net_worker, my_session)
            
            return None

    def update(self, dt_ms, events, data: Optional[dict] = None):
        next_state = self.handle_packet(data)
        if next_state is not None:
            return next_state
            
        if self.popup.visible:
            btn_name = self.popup.handle_event(events)
            if btn_name == "재시도":
                self.popup.visible = False
                self.connect()
            elif btn_name == "종료":
                pygame.quit(); raise SystemExit

            return self  # 로그인 UI는 건드리지도 않음
        
        if self.popup2.visible:
            btn_name = self.popup2.handle_event(events)
            if btn_name == "재시도":
                self.popup2.visible = False
                self.connect()
            elif btn_name == "종료":
                pygame.quit(); raise SystemExit

            return self  # 로그인 UI는 건드리지도 않음
        
        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit(); raise SystemExit
                
            self.id_box.handle_event(ev)
            self.pw_box.handle_event(ev)

            if ev.type == pygame.KEYDOWN and ev.key == pygame.K_RETURN:
                self.send_login()

            if self.btn_login.handle_event(ev):
                self.send_login()

        self.id_box.update(dt_ms)
        self.pw_box.update(dt_ms)
        return self

    def draw(self):
        self.screen.fill((18, 18, 18))
        sw, _ = self.screen.get_size()
        title = self.title_font.render("로그인", True, (255, 255, 255))
        self.screen.blit(title, title.get_rect(center=(sw // 2, 90)))
        self.id_box.draw(self.screen)
        self.pw_box.draw(self.screen)
        self.btn_login.draw(self.screen)
        if self.popup.visible:
            self.popup.draw()
        if self.popup2.visible:
            self.popup2.draw()

LOBBY = 1
CREATE_ROOM = 2

class LobbyState(BaseState):
    def __init__(self, screen, asset: AssetManager, net_worker: NetworkWorker, my_session: Session):
        self.screen = screen
        self.am = asset
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
    
        self.set_layout()

    def set_layout(self):
        sw, sh = self.screen.get_size()

        self.logo_surface = self.am.button_asset[BUTTON_LOGO_IDLE]
        logo_w, logo_h = self.logo_surface.get_size()

        btn_w, btn_h = 200, 100
        #btn_y = 0

        top_menus_text = ["방만들기", "상점", "설정"]

        logo = Button(self.btn_draw_x, self.btn_draw_y, logo_w, logo_h, None, self.am, BUTTON_LOGO_IDLE, BUTTON_LOGO_HOVER, BUTTON_LOGO_PRESS)
        self.buttons.append(logo)
        self.btn_draw_x += logo_w

        for btn_text in top_menus_text:
            self.buttons.append(Button(self.btn_draw_x, self.btn_draw_y, btn_w, btn_h, btn_text))
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
            print(f"LobbyState->handle_packet() recv_bytes: {data.get("size")} / recv type: {data.get("type")}")
            if data.get("type") == S2C_MESSAGE:
                self.chat_window.add_new_message(data.get("user_name"), data.get("message"))

            elif data.get("type") == S2C_ADD_OPEN_ROOM:
                return SinglePlayState(self.screen, self.am, self.net_worker, self.my_session, data.get("room_name"))

            elif data.get("type") == S2C_ADD_LOCK_ROOM:
                return SinglePlayState(self.screen, self.am, self.net_worker, self.my_session, data.get("room_name"), data.get("room_password"))
            
        return None


    def update(self, dt_ms, events, data=None):
        next_state = self.handle_packet(data)
        if next_state is not None:
            return next_state
        
        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit()
                raise SystemExit

            if self.reactable_screen == LOBBY:
                for btn in self.buttons:
                    if btn.handle_event(ev):
                        if btn.text == "방만들기":
                            self.room_create_window = RoomCreateWindow(self.screen)
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