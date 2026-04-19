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

class ButtonStyle(IntEnum):
    DEFAULT = 0
    SMALL = 1

class Button:
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager, *args):
        self.state = ButtonState.IDLE
        self.screen = screen
        self.rm = rm
        text = ""
        border_width = 0
        use_image = True
        button_style = ButtonStyle.DEFAULT
        legacy_image = None

        if len(args) > 0:
            first_arg = args[0]
            if isinstance(first_arg, bool):
                use_image = first_arg
                if len(args) > 1:
                    legacy_image = args[1]
                if len(args) > 2:
                    text = args[2]
                if len(args) > 3:
                    border_width = args[3]
                button_style = self._get_legacy_button_style(legacy_image)
            elif isinstance(first_arg, pygame.Surface) or first_arg is None:
                legacy_image = first_arg
                use_image = legacy_image is not None
                if len(args) > 1:
                    text = args[1]
                if len(args) > 2:
                    border_width = args[2]
                button_style = self._get_legacy_button_style(legacy_image)
            else:
                text = first_arg
                if len(args) > 1:
                    border_width = args[1]
                if len(args) > 2:
                    use_image = args[2]
                if len(args) > 3:
                    button_style = args[3]

        self.use_image = use_image
        self.button_style = button_style
        image = None
        hover_image = None
        press_image = None
        if use_image:
            image, hover_image, press_image = self._get_images()
        self.button = Rectangle(screen, rect, rm, use_image, image, text, border_width)
        self.idle_color: tuple = DARK_GRAY
        self.hover_color: tuple = GRAY
        self.press_color: tuple = ORANGE
        self.idle = None
        self.hover = None
        self.press = None

        if use_image:
            self.set_images(image, hover_image, press_image)

    def _get_legacy_button_style(self, legacy_image: pygame.Surface) -> ButtonStyle:
        if legacy_image is None:
            return ButtonStyle.DEFAULT
        if legacy_image is self.rm.images.ui_images[UI_BUTTON2_SKY]:
            return ButtonStyle.SMALL
        return ButtonStyle.DEFAULT

    def _get_images(self) -> tuple[pygame.Surface, pygame.Surface, pygame.Surface]:
        if self.button_style == ButtonStyle.SMALL:
            return (
                self.rm.images.ui_images[UI_BUTTON2_SKY],
                self.rm.images.ui_images[UI_BUTTON2_BLUE],
                self.rm.images.ui_images[UI_BUTTON2_ORANGE],
            )

        return (
            self.rm.images.ui_images[UI_BUTTON_SKY],
            self.rm.images.ui_images[UI_BUTTON_BLUE],
            self.rm.images.ui_images[UI_BUTTON_ORANGE],
        )

    def set_images(self, image: pygame.Surface, hover_image: pygame.Surface = None, press_image: pygame.Surface = None):
        rect = self.button.rect
        text = self.button.text
        border_width = self.button.border_width
        hover_image = hover_image or image
        press_image = press_image or hover_image
        self.idle = Rectangle(self.screen, rect, self.rm, True, image, text, border_width)
        self.hover = Rectangle(self.screen, rect, self.rm, True, hover_image, text, border_width)
        self.press = Rectangle(self.screen, rect, self.rm, True, press_image, text, border_width)
        self.button = self.idle

    def set_text(self, text: str):
        self.button.set_text(text)
        if self.idle:
            self.idle.set_text(text)
            self.hover.set_text(text)
            self.press.set_text(text)

    def set_text_size(self, new_size: int):
        self.button.set_text_size(new_size)
        if self.idle:
            self.idle.set_text_size(new_size)
            self.hover.set_text_size(new_size)
            self.press.set_text_size(new_size)
    
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
                        if self.idle:
                            self.button = self.hover

                elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
                    if self.state == ButtonState.HOVER:
                        self.state = ButtonState.PRESS
                        self.rm.sounds.sound_effects[EFFECT_BUTTON_PRESS].play()
                        if self.idle:
                            self.button = self.press

                elif ev.type == pygame.MOUSEBUTTONUP and ev.button == 1:
                    if self.state == ButtonState.PRESS:
                        self.state = ButtonState.IDLE
                        if self.idle:
                            self.button = self.idle
                        return True # 버튼이 눌렀다 떼져야 클릭 이벤트 처리
                
            else:
                self.state = ButtonState.IDLE
                if self.idle:
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
