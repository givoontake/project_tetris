import pygame
from typing import Optional

from tetris.config.define import *
from tetris.net.network import NetworkWorker
from tetris.net.packet_structs import MAX_INPUT
from tetris.net.session import Session
from tetris.resources.define import *
from tetris.resources.resource_manager import ResourceManager
from tetris.states.base_state import BaseState
from tetris.states.login_state import LoginState
from tetris.ui.button import Button
from tetris.ui.label_frame import LabelFrame
from tetris.ui.popupbox import PopupBox


class ConnectState(BaseState):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager,
                 net_worker: NetworkWorker, session: Session, show_fail_popup: bool = False):
        super().__init__(screen, rm, net_worker, session)
        self.background_image = self.rm.images.ui_images[UI_MAIN_BACKGROUND]
        self.ip_label: Optional[LabelFrame] = None
        self.btn_connect: Optional[Button] = None
        self.fail_connect_popup: Optional[PopupBox] = None
        self.empty_ip_popup: Optional[PopupBox] = None
        self.reactable = True
        self.set_layout()
        if show_fail_popup:
            self.fail_connect_popup = PopupBox(self.screen, self.rm, "서버와의 연결이 원활하지 않습니다.", ["재시도", "종료"])
            self.reactable = False

    def set_layout(self):
        sw, sh = self.screen.get_size()

        adjust_scale_x = 1.0
        adjust_scale_y = 1.0

        label_w, label_h = 400, 50
        label_image = self.rm.images.scale_image(self.rm.images.ui_images[UI_TEXT_HOLDER], label_w, label_h)
        button_w, button_h = 200, 100
        button_gap = 50
        adjust_x = (1 - adjust_scale_x) * label_w
        adjust_y = (1 - adjust_scale_y) * label_h
        input_box_w, input_box_h = label_w - adjust_x * 2, label_h - adjust_y * 2

        total_h = label_h + button_gap + button_h
        group_top = (sh - total_h) // 2
        label_left = (sw - label_w) // 2
        draw_x, draw_y = label_left, group_top

        ip_label_rect = pygame.Rect(draw_x, draw_y, label_w, label_h)
        ip_input_box_rect = pygame.Rect(draw_x + adjust_x, draw_y + adjust_y, input_box_w, input_box_h)
        self.ip_label = LabelFrame(self.screen, label_image, ip_input_box_rect, ip_label_rect, self.rm, "IP", MAX_INPUT, False, False)
        self.ip_label.input_box.text = SERVER_HOST
        self.ip_label.input_box.text_h = 30
        self.ip_label.input_box.padding = 15
        self.ip_label.input_box.font = self.rm.fonts.get_font(30)

        draw_x += (label_w - button_w) // 2
        draw_y += label_h + button_gap
        btn_rect = pygame.Rect(draw_x, draw_y, button_w, button_h)
        self.btn_connect = Button(self.screen, btn_rect, self.rm, "연결", 0)
        self.btn_connect.set_text_size(30)

    def try_connect(self):
        host = self.ip_label.input_box.get_total_text().strip()
        if not host:
            self.empty_ip_popup = PopupBox(self.screen, self.rm, "IP를 입력하세요.", ["확인"])
            self.reactable = False
            return

        if self.net_worker.connect_to_server(host):
            self.queue_state(LoginState(self.screen, self.rm, self.net_worker, self.session))
        else:
            self.fail_connect_popup = PopupBox(self.screen, self.rm, "서버와의 연결이 원활하지 않습니다.", ["재시도", "종료"])
            self.reactable = False

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.QUIT:
            pygame.quit()
            raise SystemExit

        if self.reactable:
            if ev.type == pygame.KEYDOWN and ev.key == pygame.K_RETURN:
                self.try_connect()
                return

            self.ip_label.input_box.handle_event(ev)

            if self.btn_connect.handle_event(ev):
                self.try_connect()
        else:
            if self.fail_connect_popup:
                selected = self.fail_connect_popup.handle_event(ev)
                if selected == "재시도":
                    self.fail_connect_popup = None
                    self.reactable = True
                    self.try_connect()
                elif selected == "종료":
                    pygame.quit()
                    raise SystemExit

            elif self.empty_ip_popup:
                if self.empty_ip_popup.handle_event(ev) == "확인":
                    self.empty_ip_popup = None
                    self.reactable = True

    def update(self, dt_ms, events):
        for ev in events:
            self.handle_event(ev)

        self.ip_label.update(dt_ms)
        self.update_fade(dt_ms)
        return self.consume_state()

    def draw(self):
        background_rect = pygame.Rect(0, 0, BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT)
        self.screen.blit(self.background_image, background_rect)
        self.ip_label.draw()
        self.btn_connect.draw()
        if self.fail_connect_popup:
            self.fail_connect_popup.draw()
        if self.empty_ip_popup:
            self.empty_ip_popup.draw()
