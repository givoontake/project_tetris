import pygame
import struct
from typing import Optional, cast

from tetris.config.define import *
from tetris.net.define_format import *
from tetris.net.packet_types import *

from tetris.net.session import Session
from tetris.net.network import NetworkWorker
from tetris.net.packet_structs import *
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.font_manager import FontManager

from tetris.ui.button import Button
from tetris.ui.inputbox import InputBox
from tetris.ui.popupbox import PopupBox
from tetris.states.lobby_state import LobbyState
from tetris.states.base_state import BaseState
from tetris.ui.label_frame import LabelFrame

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

    def send_login(self, id: str, pw: str):
        data = C2S_LOGIN_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_LOGIN
        data.user_id = self.net_worker._pm.str_to_bytes(id, MAX_USER_ID)
        data.user_password = self.net_worker._pm.str_to_bytes(pw, MAX_USER_PASSWORD)
        values = self.net_worker._pm.struct_to_values(data)
        packet = struct.pack(data.FMT, *values)

        try:
            self.net_worker.send_packet(packet)
            self.active_loading = True
        except Exception as e:
            print("[LoginState] send_login error:", e)

    def handle_packet(self, data: Optional[RecvPacketStruct]):
        if data:
            # print(", ".join(f"{k}: {v}" for k, v in data.items()))
            if data.type == S2C_LOGIN:
                login_data = cast(S2C_LOGIN_PACKET, data) # 코드 작성시 불편함을 줄이기 위한 힌트용, 논리적으로는 맞으므로 굳이 할 필요는 없음
                if login_data.id == -1:
                    self.popup2.visible = True
                
                else:
                    id = login_data.id
                    self.session.id = id
                    self.session.nickname = login_data.user_name
                    self.session.win = login_data.win_count
                    self.session.lose = login_data.lose_count
                    self.session.max_score = login_data.max_score

                    self.session.load_texture(self.rm)
                    return LobbyState(self.screen, self.rm, self.fm, self.net_worker, self.session, is_animation=True)
            
            return self
        

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.QUIT:
            pygame.quit(); raise SystemExit
        
        if ev.type == pygame.KEYDOWN and ev.key == pygame.K_RETURN:
            id = self.id_label.input_box.handle_event(ev)
            pw = self.pw_label.input_box.handle_event(ev)
            self.send_login(id, pw)
            return
        
        else:
            self.id_label.input_box.handle_event(ev)
            self.pw_label.input_box.handle_event(ev)
            
        if self.btn_login.handle_event(ev):
            id = self.id_label.input_box.extract_text()
            pw = self.pw_label.input_box.extract_text()
            self.send_login(id, pw)

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
            self.handle_event(ev)

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

