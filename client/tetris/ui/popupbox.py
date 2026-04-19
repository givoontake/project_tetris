import pygame
from typing import Optional

from tetris.ui.button import Button
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.fonts import *
from tetris.ui.rectangle import Rectangle
from tetris.resources.define import *

class PopupBox:
    POPUP_WIDTH = 750
    POPUP_HEIGHT = 500
    MESSAGE_HEIGHT = 300
    BUTTON_AREA_HEIGHT = 250
    BUTTON_WIDTH = 150
    BUTTON_HEIGHT = 75

    def __init__(self, screen: pygame.Surface, rm: ResourceManager, message: str, buttons_text: list[str]):
        self.screen = screen
        self.message = message
        #self.visible = False
        self.buttons_text = buttons_text
        self.buttons: list[Button] = []
        self.message_window: Rectangle = None
        self.popup_window: Rectangle = None
        self.rm = rm

        # 색 / 스타일
        # self.bg_overlay_color = (0, 0, 0, 128)  # 전체 화면 어둡게 (반투명)
        # self.window_color = (0, 0, 0)           # 팝업 본체
        # self.border_color = (255, 255, 255)
        # self.border_thickness = 2

        # 폰트 (기존 폰트와 동일 계열)
        self.font_msg = self.rm.fonts.get_font(POPUPBOX_FONT_SIZE)

        # 레이아웃 계산
        self.set_layout()

    def set_layout(self):
        """현재 screen 사이즈를 기준으로 팝업 사각형을 다시 계산한다."""
        if len(self.buttons_text) < 1:
            raise ValueError("팝업은 최소 1개의 버튼을 포함해야 합니다.")
        elif len(self.buttons_text) > 3:
            raise ValueError("팝업은 최대 3개의 버튼을 포함할 수 있습니다.")

        sw, sh = self.screen.get_size()
        win_w = self.POPUP_WIDTH
        win_h = self.POPUP_HEIGHT
        win_x = (sw - win_w) // 2
        win_y = (sh - win_h) // 2
        win_rect = pygame.Rect(win_x, win_y, win_w, win_h)
        self.popup_window = Rectangle(
            self.screen,
            win_rect,
            self.rm,
            True,
            self.rm.images.ui_images[UI_POPUP_BACKGROUND],
            ""
        )
        
        msg_x = win_x
        msg_y = win_y
        msg_w = win_w
        msg_h = self.MESSAGE_HEIGHT
        msg_rect = pygame.Rect(msg_x, msg_y, msg_w, msg_h)
        self.message_window = Rectangle(self.screen, msg_rect, self.rm, False, None, self.message)
        self.message_window.set_font(self.rm.fonts.get_font(POPUPBOX_FONT_SIZE))

        btn_h = self.BUTTON_HEIGHT
        btn_w = self.BUTTON_WIDTH
        remain_w = win_w - btn_w * len(self.buttons_text)
        btn_padding = remain_w // (len(self.buttons_text) + 1)
        btn_y = win_y + (win_h - self.BUTTON_AREA_HEIGHT) + ((self.BUTTON_AREA_HEIGHT - btn_h) // 2)

        draw_x = win_x + btn_padding
        for i in range(len(self.buttons_text)):
            button_rect = pygame.Rect(draw_x + i * (btn_w + btn_padding), btn_y, btn_w, btn_h)
            self.buttons.append(Button(self.screen, button_rect, self.rm, self.buttons_text[i], 0))

    # def set_visible(self, value: bool):
    #     self.visible = value

    def handle_event(self, ev: pygame.event.Event)-> Optional[str]: # 어떤 버튼이 눌렸는가

        # 버튼 이벤트 처리
        for button in self.buttons:
            if button.handle_event(ev):
                return button.button.text
                
        return None

    def draw(self):
        # if not self.visible:
        #     return

        sw, sh = self.screen.get_size()

        # 1) 전체 화면 어둡게(반투명 오버레이)
        overlay = pygame.Surface((sw, sh), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 128))
        self.screen.blit(overlay, (0, 0))

        self.popup_window.draw()
        msg_font_rect = self.message_window.font_surface.get_rect()
        msg_center_x = self.message_window.rect.x + self.message_window.rect.w // 2
        msg_center_y = self.message_window.rect.y + self.message_window.rect.h // 2
        msg_font_rect.x = msg_center_x - msg_font_rect.w / 2
        msg_font_rect.y = msg_center_y - msg_font_rect.h / 2
        self.screen.blit(self.message_window.font_surface, msg_font_rect)
        for button in self.buttons:
            button.draw()
