import pygame
from resource_manager import ResourceManager
from define import *

class Rectangle:
    def __init__(self, 
        screen: pygame.Surface,
        rect: pygame.Rect, 
        font: pygame.font.Font,
        image: pygame.Surface = None, 
        text: str = "", 
        ):
        self.screen = screen
        self.rect = rect
        self.font = font
        self.image = None
        if image is not None: 
            self.image = pygame.transform.smoothscale(image, (self.rect.w, self.rect.h))
        self.text = text
        self.text_color: tuple[int, int, int] = WHITE
        self.background_color: tuple[int, int, int] = BLACK
        self.font_surface = self.font.render(self.text, False, self.text_color)

    # def get_rect(self) -> pygame.Rect: 
    #     return self.rect
    
    def set_background_color(self, color = tuple[int, int, int]): # r, g, b
        self.background_color = color

    def set_text_color(self, color = tuple[int, int, int]): # r, g, b
        self.text_color = color
        self.font_surface = self.font.render(self.text, False, self.text_color)
        
    def _get_center_pos(self) -> tuple[int, int]: # x, y
        center_x = self.rect.x + self.rect.w // 2
        center_y = self.rect.y + self.rect.h // 2
        return center_x, center_y
    
    def draw(self):
        if self.image == None:
            pygame.draw.rect(self.screen, self.background_color, self.rect)
        else:
            self.screen.blit(self.image, self.rect)

        font_rect = self.font_surface.get_rect()
        center_x, center_y = self._get_center_pos()
        draw_font_x = center_x - font_rect.w / 2
        draw_font_y = center_y - font_rect.h / 2
        font_rect.x = draw_font_x
        font_rect.y = draw_font_y
        self.screen.blit(self.font_surface, font_rect)        
    