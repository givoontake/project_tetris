import pygame

from tetris.net.packet_structs import MAX_ROOM_PASSWORD
from tetris.ui.popupbox import PopupBox
from tetris.ui.inputbox import InputBox
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.fonts import *
from tetris.ui.rectangle import Rectangle

class InputWindow:
    def __init__(self, screen: pygame.Surface, rm: ResourceManager, buttons_text: list[str]):
        self.screen = screen
        self.rm = rm
        self.window = PopupBox(screen, rm, "", buttons_text)

        self.set_layout()

    def set_layout(self):
        input_w = self.window.popup_window.rect.w // 2
        input_h = 50
        sw, sh = self.screen.get_size()
        input_x = sw // 2 - input_w // 2
        input_y = sh // 2 - input_h // 2
        input_rect = pygame.Rect(input_x, input_y, input_w, input_h)
        self.input = InputBox(self.screen, input_rect, self.rm, "비밀번호 입력", MAX_ROOM_PASSWORD, False, False)

    def handle_event(self, ev: pygame.event.Event) -> Optional[str]:
        self.input.handle_event(ev)
        return self.window.handle_event(ev)

    def update(self, dt_ms: int):
        self.input.update(dt_ms)
    
    def draw(self):
        self.window.draw()
        self.input.draw()