import pygame
import struct
from typing import Optional

from define import *
from define_format import *
from packet_type import *

from session import Session
from network import NetworkWorker
from resource_manager import ResourceManager
from font_manager import FontManager

from button import Button
from inputbox import InputBox
from popupbox import PopupBox
from lobby_state import LobbyState
from base_state import BaseState
from label_frame import LabelFrame

class LoginState(BaseState):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager, fm: FontManager, net_worker: NetworkWorker, session: Session):
        super().__init__(screen, rm, fm, net_worker, session)
        self.fm = fm
        self.background_image = self.rm.shutter_image
        self.id_label: Optional[LabelFrame] = None
        self.pw_label: Optional[LabelFrame] = None
        self.btn_login: Optional[Button] = None
        self.popup: PopupBox = PopupBox(screen, rm, fm, "서버와의 연결이 원활하지 않습니다.", ["재시도", "종료"])
        self.popup2: PopupBox = PopupBox(screen, rm, fm, "아이디 또는 비밀번호를 확인하세요.", ["재시도", "종료"])
        self.set_layout()

    def connect(self):
        if not self.net_worker.connect_to_server(): self.popup.visible = True

    def set_layout(self):
        sw, sh = self.screen.get_size()

        ADJUST_SCALE_X = 0.85
        ADJUST_SCALE_Y = 0.7
        
        label_w, label_h = 400, 100
        label_image = self.rm.scale_image(self.rm.login_label_frame, label_w, label_h)
        button_w, button_h = 200, 100
        button_image = self.rm.scale_image(self.rm.login_button, button_w, button_h)
        adjust_x = (1-ADJUST_SCALE_X)*label_w
        adjust_y = (1-ADJUST_SCALE_Y)*label_h
        input_box_w, input_box_h = label_w - adjust_x*2, label_h - adjust_y*2

        total_h = label_h * 2 + (button_h // 2) + button_h

        group_top = (sh - total_h) // 2
        label_left = (sw - label_w) // 2
        draw_x, draw_y = label_left, group_top

        id_label_rect = pygame.Rect(draw_x, draw_y, label_w, label_h)
        id_input_box_rect = pygame.Rect(draw_x + adjust_x, draw_y + adjust_y, input_box_w, input_box_h)
        self.id_label = LabelFrame(self.screen, label_image, id_input_box_rect, id_label_rect, self.fm, "아이디", MAX_INPUT, False, False)

        draw_y += label_h
        pw_label_rect = pygame.Rect(draw_x, draw_y, label_w, label_h)
        pw_input_box_rect = pygame.Rect(draw_x + adjust_x, draw_y + adjust_y, input_box_w, input_box_h)
        self.pw_label = LabelFrame(self.screen, label_image, pw_input_box_rect, pw_label_rect, self.fm, "비밀번호", MAX_INPUT, True, False)

        draw_x += (label_w - button_w) // 2
        draw_y += label_h + (button_h // 2)
        btn_rect = pygame.Rect(draw_x, draw_y, button_w, button_h)
        self.btn_login = Button(self.screen, btn_rect, self.rm, self.fm, button_image, "로그인", True)

    def send_login(self):
        user_id = self.id_label.input_box.text
        user_pw = self.pw_label.input_box.text
        data = {
            "size": 2 + 1 + MAX_USER_ID + MAX_USER_PASSWORD,
            "type": C2S_LOGIN,
            "user_id": user_id,
            "user_password": user_pw
        }
        try:
            self.net_worker.send_packet(self.net_worker._pm.dic_to_bytes(data))
            self.active_loading = True
        except Exception as e:
            print("[LoginState] send_login error:", e)

    def handle_packet(self, data: Optional[dict]):
        if data:
            # print(", ".join(f"{k}: {v}" for k, v in data.items()))
            if data.get("type") == S2C_LOGIN:
                if data.get("id") == -1:
                    self.popup2.visible = True
                
                else:
                    id = data.get("id")
                    self.session.id = id
                    nickname = data.get("user_name")
                    self.session.nickname = nickname
                    win = data.get("win_count")
                    self.session.win = win
                    lose = data.get("lose_count")
                    self.session.lose = lose
                    max_score = data.get("max_score")
                    self.session.max_score = max_score

                    self.session.load_texture(self.rm)
                    print(f"id={id}, nickname={nickname}, win={win}, lose={lose}")
                    return LobbyState(self.screen, self.rm, self.net_worker, self.session, is_animation=True)
            
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
                
            self.id_label.handle_event(ev)
            self.pw_label.handle_event(ev)

            if ev.type == pygame.KEYDOWN and ev.key == pygame.K_RETURN:
                self.send_login()

            if self.btn_login.handle_event(ev):
                self.send_login()

        self.id_label.update(dt_ms)
        self.pw_label.update(dt_ms)
        return self

    def draw(self):
        background_rect = pygame.Rect(0,0,BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT)
        self.screen.blit(self.background_image, background_rect)
        sw, _ = self.screen.get_size()
        #title = self.title_font.render("로그인", True, (255, 255, 255))
        #self.screen.blit(title, title.get_rect(center=(sw // 2, 90)))
        self.id_label.draw()
        self.pw_label.draw()
        self.btn_login.draw()
        if self.popup.visible:
            self.popup.draw()
        if self.popup2.visible:
            self.popup2.draw()

