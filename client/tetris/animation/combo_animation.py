import pygame

from tetris.resources.resource_manager import *
from tetris.resources.define_colors import *

class ComboAnimation:
    def __init__(self, screen: pygame.Surface, rm: ResourceManager, combo: int, draw_x: int, draw_y: int):
        self.screen = screen
        self.rm = rm
        self.draw_x = draw_x
        self.draw_y = draw_y
        self.elapsed_time = 0
        self.animation_time = 1000
        self.active = True
        self.font = rm.fonts.get_font(COMBO_FONT_SIZE)
        self.text = f"Combo {combo}"

        if combo % 5 == 0 or combo % 5 == 1:
            color = RED
        elif combo % 5 == 2:
            color = ORANGE
        elif combo % 5 == 3:
            color = YELLOW
        elif combo % 5 == 4:
            color = GREEN

        self.font_surface = self.font.render(self.text, False, color)


    def _get_alpha(self) -> int:
        alpha = int(255*((self.animation_time - self.elapsed_time) / self.animation_time))
        if alpha > 255: alpha = 255
        if alpha < 0: alpha = 0
        return alpha
        
    def update(self, dt_ms: int):
        if self.active:
            self.elapsed_time += dt_ms
            if self.elapsed_time >= self.animation_time:
                self.active = False

    def draw(self):
        if self.active:
            font_rect = self.font_surface.get_rect()
            font_rect.x = self.draw_x - 200
            font_rect.y = self.draw_y - self.elapsed_time / 10
            
            self.font_surface.set_alpha(self._get_alpha())
            self.screen.blit(self.font_surface, font_rect)    
