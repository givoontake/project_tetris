import pygame
import struct
from typing import Optional

from define import *
from define_format import *
from packet_type import *

from session import Session
from network import NetworkWorker
from resource_manager import ResourceManager

from button import Button
from inputbox import InputBox
from popupbox import PopupBox
from lobby_state import LobbyState

# class BaseState:
#     def __init__(self, screen, asset):
#         self.screen = screen

#     def init(self): 
#         pass

#     def update(self, dt_ms, events, data=None):
#         """메인 루프에서 계산된 dt(ms)를 전달받아 갱신."""
#         return self

#     def draw(self):
#         pass

#     def on_resize(self, w, h, screen):
#         self.screen = screen


class LoginState:
    def __init__(self, screen, rm: Optional[ResourceManager], net_worker: Optional[NetworkWorker]):
        self.screen = screen
        self.net_worker = net_worker
        self.title_font = pygame.font.Font("resource/dodamdodam.ttf", 36)

        self.rm = rm
        self.id_box: Optional[InputBox] = None
        self.pw_box: Optional[InputBox] = None
        self.btn_login: Optional[Button] = None
        self.popup = PopupBox(self.screen, self.rm, "서버와의 연결이 원활하지 않습니다.", "재시도", "종료")
        self.popup2 = PopupBox(self.screen, self.rm, "아이디 또는 비밀번호를 확인하세요.", "재시도", "종료")
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
            self.rm
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
                my_session = Session()
                id = data.get("id")
                my_session.id = id
                nickname = data.get("user_name")
                my_session.nickname = nickname
                win = data.get("win_count")
                my_session.win = win
                lose = data.get("lose_count")
                my_session.lose = lose
                max_score = data.get("max_score")
                my_session.max_score = max_score
                my_session.load_texture(self.rm)
                print(f"id={id}, nickname={nickname}, win={win}, lose={lose}")
                return LobbyState(self.screen, self.rm, self.net_worker, my_session)
            
            return self

    def update(self, dt_ms, events):            
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

