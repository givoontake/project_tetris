import pygame

from tetris.config.define import *
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.font_manager import *
from tetris.resources.define_colors import *
from tetris.ui.rectangle import Rectangle

class Button:
    def __init__(self, 
        screen: pygame.Surface,
        rect: pygame.Rect, 
        rm: ResourceManager,
        fm: FontManager,
        image: pygame.Surface = None, 
        text: str = "", 
        react: bool = True
    ):
        self.rm = rm
        self.fm = fm
        self.react = react
        self.hovered = False
        self.pressed = False
        self.pressed_inside = False  # 마우스 다운이 버튼 내부에서 시작했는지
        self.hover_sound_printed = False
        self.idle = Rectangle(screen, rect, fm, image, text)
        if image == None:
            self.hover = None
            self.press = None
        else: 

            hover_rect = self._set_rect_scale(self.idle.rect, self.rm.HOVER_SCALE)
            press_rect = self._set_rect_scale(self.idle.rect, self.rm.PRESS_SCALE)

            self.hover = Rectangle(screen, hover_rect, self.fm, image, text)
            self.press = Rectangle(screen, press_rect, self.fm, image, text)

    def _set_rect_scale(self, rect: pygame.Rect, scale: float) -> pygame.Rect:
        if scale < 0.1 or scale > 1.1:
            raise ValueError("scale must be between 0.1 and 1.1")
        
        temp_rect = rect.copy() 
        return temp_rect.scale_by(scale)
        
    def handle_event(self, ev: pygame.event.Event) -> bool:
        if self.react == False:
            return False
        """
        마우스 이벤트 처리:
        - hover 상태 추적
        - 눌림/뗌 추적
        - '버튼 안에서 눌렀고 버튼 안에서 뗀 경우'만 True 반환
        """
        clicked = False
        if ev.type == pygame.MOUSEMOTION:
            self.hovered = self.idle.rect.collidepoint(ev.pos)
            if self.hovered:
                if self.hover_sound_printed == False:
                    self.hover_sound_printed = True
                    self.rm.button_sound_hover.play()
            else: self.hover_sound_printed = False

        elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            if self.idle.rect.collidepoint(ev.pos):
                self.pressed = True
                self.pressed_inside = True
                self.rm.button_sound_press.play()
            else:
                self.pressed = False
                self.pressed_inside = False

        elif ev.type == pygame.MOUSEBUTTONUP and ev.button == 1:
            if self.pressed and self.pressed_inside and self.idle.rect.collidepoint(ev.pos):
                clicked = True
            self.pressed = False
            self.pressed_inside = False

        return clicked

    def draw(self):
        button = None
        if self.pressed and self.press is not None:
            button = self.press
        elif self.hovered and self.hover is not None:
            button = self.hover
        elif button is not None:
            button = self.idle

        if button is not None:
            button.draw()
            
        else:
            if self.pressed:
                self.idle.set_background_color(ORANGE)   # ORANGE-ish
            elif self.hovered:
                self.idle.set_background_color(GRAY)
            else:
                self.idle.set_background_color(BLACK)

            self.idle.draw()