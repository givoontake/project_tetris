import pygame
from resource_manager import ResourceManager
from font_manager import *
from define import *

class Rectangle:
    def __init__(self, 
        screen: pygame.Surface,
        rect: pygame.Rect, 
        fm: FontManager,
        image: pygame.Surface = None, 
        text: str = "", 
        border_width = 0
        ):
        self.screen = screen
        self.rect = rect
        self.fm = fm
        self.font = fm.get_font(RECTANGLE_FONT_SIZE)
        self.image = None
        if image is not None: 
            self.set_image(image)
        self.text = text
        self.text_color: tuple[int, int, int] = WHITE
        self.background_color: tuple[int, int, int] = BLACK
        self.border_color: tuple[int, int, int] = WHITE
        self.font_surface = self.font.render(self.text, False, self.text_color)

        self.border_width = border_width

    # def get_rect(self) -> pygame.Rect: 
    #     return self.rect
    
    def set_background_color(self, color = tuple[int, int, int]): # r, g, b
        self.background_color = color

    def set_text_color(self, color = tuple[int, int, int]): # r, g, b
        self.text_color = color
        self.font_surface = self.font.render(self.text, False, self.text_color)

    def set_image(self, new_image: pygame.Surface):
        scaled_image = pygame.transform.smoothscale(new_image, (self.rect.w, self.rect.h))
        self.image = scaled_image
        
    def set_text(self, new_text: pygame.Surface):
        self.text = new_text
        self.font_surface = self.font.render(self.text, False, self.text_color)

    def set_border(self, new_border_width: int): # 0이면 테두리 없음. 0보다 크면 그 두께만큼 테두리 생성
        self.border_width = new_border_width

    def _get_center_pos(self) -> tuple[int, int]: # x, y
        center_x = self.rect.x + self.rect.w // 2
        center_y = self.rect.y + self.rect.h // 2
        return center_x, center_y
    
    def update_rect(self, new_rect: pygame.Rect):
        self.rect = new_rect
    
    def draw(self):
        if self.image == None:
            pygame.draw.rect(self.screen, self.background_color, self.rect)
            if self.border_width > 0:
                pygame.draw.rect(self.screen, self.border_color, self.rect, self.border_width)
        else:
            self.screen.blit(self.image, self.rect)

        font_rect = self.font_surface.get_rect()
        center_x, center_y = self._get_center_pos()
        draw_font_x = center_x - font_rect.w / 2
        draw_font_y = center_y - font_rect.h / 2
        font_rect.x = draw_font_x
        font_rect.y = draw_font_y
        self.screen.blit(self.font_surface, font_rect)        
    