# menu.py
import pygame
from define import FPS

WHITE  = (255, 255, 255)
ORANGE = (255, 165,   0)
GRAY   = (100, 100, 100)

class Button:
    def __init__(self, text, rel_rect, surface):
        sw, sh = surface.get_size()
        x, y, w, h = rel_rect
        self.rect = pygame.Rect(int(x*sw), int(y*sh), int(w*sw), int(h*sh))
        font_size = max(12, int(self.rect.height * 0.6))
        self.font = pygame.font.SysFont(None, font_size)
        self.text_surf = self.font.render(text, True, (255,255,255))
        self.text_rect = self.text_surf.get_rect(center=self.rect.center)
        self.text = text

    def draw(self, surf):
        mouse = pygame.mouse.get_pos()
        # 호버 시 주황색, 기본 회색
        bg = ORANGE if self.rect.collidepoint(mouse) else GRAY
        pygame.draw.rect(surf, bg, self.rect)
        surf.blit(self.text_surf, self.text_rect)

    def clicked(self, event):
        return (event.type == pygame.MOUSEBUTTONDOWN and event.button == 1
                and self.rect.collidepoint(event.pos))