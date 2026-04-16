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
from tetris.resources.define import *

from tetris.ui.button import Button
from tetris.ui.popupbox import PopupBox
from tetris.states.lobby_state import LobbyState
from tetris.states.base_state import BaseState
from tetris.ui.label_frame import LabelFrame

class LoginState(BaseState):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager, net_worker: NetworkWorker, session: Session):
        super().__init__(screen, rm, net_worker, session)
        self.background_image = self.rm.images.ui_images[UI_SHUTTER]
        self.id_label: Optional[LabelFrame] = None
        self.pw_label: Optional[LabelFrame] = None
        self.btn_login: Optional[Button] = None
        self.reactable = True
        self.fail_connect_popup = None
        self.fail_login_popup = None
        self.set_layout()

    def connect(self):
        if not self.net_worker.connect_to_server():
            self.fail_connect_popup = PopupBox(self.screen, self.rm, "서버와의 연결이 원활하지 않습니다.", ["재시도", "종료"])
            self.reactable = False

    def set_layout(self):
        sw, sh = self.screen.get_size()

        ADJUST_SCALE_X = 0.85
        ADJUST_SCALE_Y = 0.7
        
        label_w, label_h = 400, 100
        label_image = self.rm.images.scale_image(self.rm.images.ui_images[UI_LOGIN_LABEL_FRAME], label_w, label_h)
        button_w, button_h = 200, 100
        button_image = self.rm.images.scale_image(self.rm.images.ui_images[UI_LOGIN_BUTTON], button_w, button_h)
        adjust_x = (1-ADJUST_SCALE_X)*label_w
        adjust_y = (1-ADJUST_SCALE_Y)*label_h
        input_box_w, input_box_h = label_w - adjust_x*2, label_h - adjust_y*2

        total_h = label_h * 2 + (button_h // 2) + button_h

        group_top = (sh - total_h) // 2
        label_left = (sw - label_w) // 2
        draw_x, draw_y = label_left, group_top

        id_label_rect = pygame.Rect(draw_x, draw_y, label_w, label_h)
        id_input_box_rect = pygame.Rect(draw_x + adjust_x, draw_y + adjust_y, input_box_w, input_box_h)
        self.id_label = LabelFrame(self.screen, label_image, id_input_box_rect, id_label_rect, self.rm, "아이디", MAX_INPUT, False, False)

        draw_y += label_h
        pw_label_rect = pygame.Rect(draw_x, draw_y, label_w, label_h)
        pw_input_box_rect = pygame.Rect(draw_x + adjust_x, draw_y + adjust_y, input_box_w, input_box_h)
        self.pw_label = LabelFrame(self.screen, label_image, pw_input_box_rect, pw_label_rect, self.rm, "비밀번호", MAX_INPUT, True, False)

        draw_x += (label_w - button_w) // 2
        draw_y += label_h + (button_h // 2)
        btn_rect = pygame.Rect(draw_x, draw_y, button_w, button_h)
        self.btn_login = Button(self.screen, btn_rect, self.rm, button_image, "로그인")

    def handle_packet(self, data: Optional[RecvPacketStruct]):
        if data:
            # print(", ".join(f"{k}: {v}" for k, v in data.items()))
            if data.type == S2C_ERROR:
                error_data = cast(S2C_ERROR_PACKET, data)
                error_message = ERROR_MESSAGES[error_data.error_code]
                self.fail_login_popup = PopupBox(self.screen, self.rm, error_message, ["확인"])
                self.reactable = False

            elif data.type == S2C_LOGIN:
                login_data = cast(S2C_LOGIN_PACKET, data) # 코드 작성시 불편함을 줄이기 위한 힌트용, 논리적으로는 맞으므로 굳이 할 필요는 없음
                id = login_data.id
                self.session.id = id
                self.session.nickname = login_data.user_name
                self.session.win = login_data.win_count
                self.session.lose = login_data.lose_count
                self.session.max_score = login_data.max_score

                self.queue_state(LobbyState(self.screen, self.rm, self.net_worker, self.session))
            
            return self
        

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.QUIT:
            pygame.quit(); raise SystemExit
        
        if self.reactable:
            if ev.type == pygame.KEYDOWN and ev.key == pygame.K_RETURN:
                id = self.id_label.input_box.handle_event(ev)
                pw = self.pw_label.input_box.handle_event(ev)
                packet = self.net_worker.builder.build_login_pkt(id, pw)
                self.net_worker.send_packet(packet)
                return
            
            else:
                self.id_label.input_box.handle_event(ev)
                self.pw_label.input_box.handle_event(ev)
                
            if self.btn_login.handle_event(ev):
                id = self.id_label.input_box.extract_text()
                pw = self.pw_label.input_box.extract_text()
                packet = self.net_worker.builder.build_login_pkt(id, pw)
                self.net_worker.send_packet(packet)

        else:
            if self.fail_connect_popup:
                str = self.fail_connect_popup.handle_event(ev)

                if str == "재시도": 
                    self.fail_connect_popup = None
                    self.reactable = True
                    self.connect()

                elif str == "종료": pygame.quit(); raise SystemExit

            elif self.fail_login_popup:
                if self.fail_login_popup.handle_event(ev) == "확인":
                    self.fail_login_popup = None
                    self.reactable = True


    def update(self, dt_ms, events):
        for ev in events:
            self.handle_event(ev)

        self.id_label.update(dt_ms)
        self.pw_label.update(dt_ms)
        self.update_fade(dt_ms)
        return self.consume_state()

    def draw(self):
        background_rect = pygame.Rect(0,0,BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT)
        self.screen.blit(self.background_image, background_rect)
        self.id_label.draw()
        self.pw_label.draw()
        self.btn_login.draw()
        if self.fail_connect_popup:
            self.fail_connect_popup.draw()
        if self.fail_login_popup:
            self.fail_login_popup.draw()

