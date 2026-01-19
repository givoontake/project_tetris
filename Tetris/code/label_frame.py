import pygame
from inputbox import InputBox
from resource_manager import ResourceManager
from define_format import *

class LabelFrame(InputBox):
    def __init__(
        self,
        screen: pygame.Surface,
        rm: ResourceManager,     
        x: int,
        y: int,
        w: int,
        h: int,
        placeholder: str = "",
        max_input_len: int | None = None,  # optional과 같음
        is_password: bool = False,
        allow_korean: bool = True,   # ✅ 한글 허용 여부
    ):
        super().__init__(screen, x, y, w, h, placeholder, max_input_len, is_password, allow_korean)
        self.screen = screen
        self.rm = rm
        self.frame = self.rm.login_label_frame
        self.adjust_x = 50
        self.adjust_y = 25
        self.frame_rect = pygame.Rect(self.rect.x - self.adjust_x, self.rect.y - self.adjust_y,
                                       self.frame.get_rect().w + self.adjust_x*2, self.frame.get_rect().h + self.adjust_y*2)
        

    def get_frame_rect(self):
        return self.frame_rect
        
    def draw(self):
        self.screen.blit(self.frame, self.frame_rect)
        super().draw()

        