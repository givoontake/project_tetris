import pygame
from typing import Optional

from tetris.resources.resource_manager import ResourceManager
from tetris.resources.define_colors import *
from tetris.ui.rectangle import Rectangle
from tetris.ui.button import Button
from tetris.ui.setting_base import SettingBase
from tetris.ui.setting_sound import SettingSound
class SettingWindow:
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager):
        self.screen = screen
        self.rect = rect
        self.rm = rm

        self.set_layout()

    def set_layout(self):
        self.setting_types: dict[str, SettingBase] = {}
        self.top_menu_texts = ["소리"]
        # 바텀은 모두 동일하므로, 세팅 클래스로 텍스트를 전달하여 내부에서 구현중
        # 여기서 구현하고 어떤 세팅이 활성화되어 있는지를 알고 여기서 적용을 때려도 상관없긴 한데..
        # 어차피 어떤 세팅 화면을 그리고 있는지 구현하려면 상태 값이 필요하려나? 그렇다면 여기로 빼는게 자연스러워 보인다
        # 어차피 세팅 화면이 여러개라면 모두 보유한 상태에서 창만 이동할거니까.. 상태로 관리하자
        # 빼야겠다 그럼
        self.buttom_menu_texts = ["적용", "취소"] 
        self.activated_setting = self.top_menu_texts[0]
        self.top_menus: list[Rectangle] = []
        self.buttom_menus: list[Button] = []
        
        top_menu_w = self.rect.w // len(self.top_menu_texts)
        top_menu_h = int(self.rect.h*0.2)
        top_menu_x = self.rect.x
        top_menu_y = self.rect.y

        for top_menu_text in self.top_menu_texts:
            top_menu_rect = pygame.Rect(top_menu_x, top_menu_y, top_menu_w, top_menu_h)
            top_menu = Rectangle(self.screen, top_menu_rect, self.rm, None, top_menu_text, 1)
            self.top_menus.append(top_menu)
            top_menu_x += top_menu_w

        buttom_menu_w = self.rect.w // len(self.buttom_menu_texts)
        buttom_menu_h = top_menu_h
        buttom_menu_x = self.rect.x
        buttom_menu_y = self.rect.y + self.rect.h - top_menu_rect.h # 탑과 바텀의 메뉴 높이는 같다.

        for buttom_menu_text in self.buttom_menu_texts:
            buttom_menu_rect = pygame.Rect(buttom_menu_x, buttom_menu_y, buttom_menu_w, buttom_menu_h)
            buttom_menu = Button(self.screen, buttom_menu_rect, self.rm, None, buttom_menu_text, 1)
            self.buttom_menus.append(buttom_menu)
            buttom_menu_x += buttom_menu_w

        setting_x = self.rect.x
        setting_y = self.rect.y + top_menu_h
        setting_w = self.rect.w
        setting_h = int(self.rect.h*0.6)
        setting_rect = pygame.Rect(setting_x, setting_y, setting_w, setting_h)
        
        for top_menu_text in self.top_menu_texts:
            if top_menu_text == "소리":
                self.setting_types[top_menu_text] = SettingSound(self.screen, setting_rect, self.rm)

            # 나중에 설정 창 추가되면 추가로 구현

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
        for top_menu in self.top_menus:
            if top_menu.text == self.activated_setting:
                top_menu.set_background_color(GREEN)
            else:
                top_menu.set_background_color(BLACK)
            top_menu.draw()

        self.setting_types[self.activated_setting].draw()

        for buttom_menu in self.buttom_menus:
            buttom_menu.draw()
                

            