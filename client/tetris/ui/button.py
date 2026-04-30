import pygame

from tetris.config.define import *
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.fonts import *
from tetris.resources.define import *
from tetris.resources.define_colors import *
from tetris.ui.rectangle import Rectangle
from tetris.game.define import *

class ButtonState(IntEnum):
    IDLE = 0
    HOVER = 1
    PRESS = 2

class Button:
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager, image: pygame.Surface = None, text: str = "", border_width: int = 0):
        self.state = ButtonState.IDLE
        self.rm = rm
        self.button = Rectangle(screen, rect, rm, image, text, border_width)
        self.idle_color: tuple = DARK_GRAY
        self.hover_color: tuple = GRAY
        self.press_color: tuple = ORANGE

        if image != None:
            self.idle = Rectangle(screen, rect, rm, image, text, border_width)
            self.hover = Rectangle(screen, rect.scale_by(1.05), self.rm, image, text, border_width)
            self.press = Rectangle(screen, rect.scale_by(0.95), self.rm, image, text, border_width)
    
    def set_btn_color(self, new_color: tuple, state: str): # state: idle, hover, press
        if state == "idle":
            self.idle_color = new_color
        elif state == "hover":
            self.hover_color = new_color
        elif state == "press":
            self.press_color = new_color
        
    def handle_event(self, ev: pygame.event.Event) -> bool:
        if ev.type in (pygame.MOUSEMOTION, pygame.MOUSEBUTTONDOWN, pygame.MOUSEBUTTONUP): # ev.pos는 마우스 이벤트 전용
            if self.button.rect.collidepoint(ev.pos):
                if ev.type == pygame.MOUSEMOTION:
                    if self.state == ButtonState.IDLE:
                        self.state = ButtonState.HOVER
                        self.rm.sounds.sound_effects[EFFECT_BUTTON_HOVER].play()
                        if self.button.image != None:
                            self.button = self.hover

                elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
                    if self.state == ButtonState.HOVER:
                        self.state = ButtonState.PRESS
                        self.rm.sounds.sound_effects[EFFECT_BUTTON_PRESS].play()
                        if self.button.image != None:
                            self.button = self.press

                elif ev.type == pygame.MOUSEBUTTONUP and ev.button == 1:
                    if self.state == ButtonState.PRESS:
                        self.state = ButtonState.IDLE
                        if self.button.image != None:
                            self.button = self.idle
                    return True # 버튼이 눌렀다 떼져야 클릭 이벤트 처리
                
            else:
                self.state = ButtonState.IDLE
                if self.button.image != None:
                    self.button = self.idle
            
        return False

    def draw(self):
        if self.button.image == None:
            if self.state == ButtonState.IDLE:
                self.button.set_background_color(self.idle_color)
            elif self.state == ButtonState.HOVER:
                self.button.set_background_color(self.hover_color)
            elif self.state == ButtonState.PRESS:
                self.button.set_background_color(self.press_color)
        
        self.button.draw()