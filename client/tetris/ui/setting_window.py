import pygame
from typing import Optional

from tetris.resources.resource_manager import ResourceManager
from tetris.resources.define import *
from tetris.ui.rectangle import Rectangle
from tetris.ui.button import Button
from tetris.ui.toggle_button import ToggleButton
from tetris.ui.setting_base import SettingBase
from tetris.ui.setting_sound import SettingSound


class SettingWindow:
    WINDOW_WIDTH = 500
    WINDOW_HEIGHT = 500
    BUTTON_WIDTH = 120
    BUTTON_HEIGHT = 60
    TOP_PADDING = 35
    BOTTOM_PADDING = 35
    SETTING_GAP = 25

    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.window: Rectangle

        self.set_layout()

    def set_layout(self):
        sw, sh = self.screen.get_size()
        self.rect.w = self.WINDOW_WIDTH
        self.rect.h = self.WINDOW_HEIGHT
        self.rect.x = (sw // 2) - (self.rect.w // 2)
        self.rect.y = (sh // 2) - (self.rect.h // 2)
        self.window = Rectangle(self.screen, self.rect, self.rm, True, self.rm.images.ui_images[UI_WINDOW_BACKGROUND], "")

        self.setting_types: dict[str, SettingBase] = {}
        self.top_menu_texts = ["소리"]
        self.buttom_menu_texts = ["적용", "취소"]
        self.activated_setting = self.top_menu_texts[0]
        self.top_menus: list[ToggleButton] = []
        self.buttom_menus: list[Button] = []

        top_menu_w = self.BUTTON_WIDTH
        top_menu_h = self.BUTTON_HEIGHT
        top_padding_w = (self.rect.w - top_menu_w * len(self.top_menu_texts)) // (len(self.top_menu_texts) + 1)
        top_menu_x = self.rect.x + top_padding_w
        top_menu_y = self.rect.y + self.TOP_PADDING

        for top_menu_text in self.top_menu_texts:
            top_menu_rect = pygame.Rect(top_menu_x, top_menu_y, top_menu_w, top_menu_h)
            top_menu = ToggleButton(self.screen, top_menu_rect, self.rm, top_menu_text, 1)
            self.top_menus.append(top_menu)
            top_menu_x += top_menu_w + top_padding_w

        buttom_menu_w = self.BUTTON_WIDTH
        buttom_menu_h = self.BUTTON_HEIGHT
        buttom_padding_w = (self.rect.w - buttom_menu_w * len(self.buttom_menu_texts)) // (len(self.buttom_menu_texts) + 1)
        buttom_menu_x = self.rect.x + buttom_padding_w
        buttom_menu_y = self.rect.bottom - self.BOTTOM_PADDING - buttom_menu_h

        for buttom_menu_text in self.buttom_menu_texts:
            buttom_menu_rect = pygame.Rect(buttom_menu_x, buttom_menu_y, buttom_menu_w, buttom_menu_h)
            buttom_menu = Button(self.screen, buttom_menu_rect, self.rm, buttom_menu_text, 1)
            self.buttom_menus.append(buttom_menu)
            buttom_menu_x += buttom_menu_w + buttom_padding_w

        setting_x = self.rect.x
        setting_y = top_menu_y + top_menu_h + self.SETTING_GAP
        setting_w = self.rect.w
        setting_h = buttom_menu_y - self.SETTING_GAP - setting_y
        setting_w = int(setting_w * 0.8)
        setting_h = int(setting_h * 0.8)
        setting_x = self.rect.x + (self.rect.w - setting_w) // 2
        setting_y = setting_y + ((buttom_menu_y - self.SETTING_GAP - setting_y) - setting_h) // 2
        setting_rect = pygame.Rect(setting_x, setting_y, setting_w, setting_h)

        for top_menu_text in self.top_menu_texts:
            if top_menu_text == "소리":
                self.setting_types[top_menu_text] = SettingSound(self.screen, setting_rect, self.rm)

    def handle_event(self, ev: pygame.event.Event) -> Optional[str]:
        for top_menu in self.top_menus:
            if top_menu.handle_event(ev):
                self.activated_setting = top_menu.text

        self.setting_types[self.activated_setting].handle_event(ev)

        event = None
        for buttom_menu in self.buttom_menus:
            if buttom_menu.handle_event(ev):
                if buttom_menu.button.text == "적용":
                    if self.setting_types[self.activated_setting].apply_settings():
                        event = "성공"
                    else:
                        event = None
                elif buttom_menu.button.text == "취소":
                    event = "취소"

        return event

    def draw(self):
        self.window.draw()

        for top_menu in self.top_menus:
            if top_menu.text == self.activated_setting:
                top_menu.set_pressed(True)
            else:
                top_menu.set_pressed(False)
            top_menu.draw()

        self.setting_types[self.activated_setting].draw()

        for buttom_menu in self.buttom_menus:
            buttom_menu.draw()
