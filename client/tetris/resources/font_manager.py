import pygame
from typing import Optional

MIN_SIZE = 10
MAX_SIZE = 50

RECTANGLE_FONT_SIZE = 30
POPUPBOX_FONT_SIZE = 40
class FontManager:
    def __init__(self):
        self.fonts: dict[int, Optional[pygame.font.Font]] = {}

        for i in range(MIN_SIZE, MAX_SIZE + 1, 2):
            if i == RECTANGLE_FONT_SIZE:
                font = self.load_font(i)
                self.fonts[i] = font
            elif i == POPUPBOX_FONT_SIZE:
                font = self.load_font(i)
                self.fonts[i] = font
            else:
                self.fonts[i] = None

    def get_font(self, font_size: int) -> pygame.font.Font:
        if font_size < 10: _font_size = 10
        elif font_size > 50: _font_size = 50
        else:
            _font_size = (font_size // 2)*2
        if self.fonts[_font_size] == None:
            font = self.load_font(_font_size)
            self.fonts[_font_size] = font
            return font
        else:    
            return self.fonts[_font_size]
            
    def load_font(self, font_size: int) -> pygame.font.Font:
        font = pygame.font.Font("tetris/resources/fonts/dodamdodam.ttf", font_size)
        return font
