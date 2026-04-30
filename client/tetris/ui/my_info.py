import pygame
from tetris.config.define import *
from tetris.net.session import Session
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.define_colors import *


class Profile:
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager, session: Session):
        self.screen = screen
        self.rect = rect
        self.my_session = session

        self.font_nickname = rm.fonts.load_font(40)
        self.font_stat = rm.fonts.load_font(30)
        self.font_max_score = rm.fonts.load_font(30)

        self.text_color = WHITE
        self.padding = 20

    def draw(self):
        bg_surface = pygame.Surface((self.rect.w, self.rect.h), pygame.SRCALPHA)
        bg_surface.fill((0, 0, 0, 120))
        self.screen.blit(bg_surface, self.rect.topleft)
        pygame.draw.rect(self.screen, WHITE, self.rect, 1)

        nick_surf = self.font_nickname.render(self.my_session.nickname, True, self.text_color)
        stat_text = f"승 {self.my_session.win}   패 {self.my_session.lose}"
        stat_surf = self.font_stat.render(stat_text, True, self.text_color)
        score_text = f"최고점수: {self.my_session.max_score}"
        score_surf = self.font_stat.render(score_text, True, self.text_color)

        total_h = nick_surf.get_height() + stat_surf.get_height() + score_surf.get_height() + self.padding * 2
        start_y = self.rect.y + (self.rect.h - total_h) // 2

        nick_rect = nick_surf.get_rect(
            center=(self.rect.centerx, start_y + nick_surf.get_height() // 2)
        )
        stat_rect = stat_surf.get_rect(
            center=(self.rect.centerx, nick_rect.bottom + self.padding + stat_surf.get_height() // 2)
        )
        score_rect = score_surf.get_rect(
            center=(self.rect.centerx, stat_rect.bottom + self.padding + score_surf.get_height() // 2)
        )

        self.screen.blit(nick_surf, nick_rect)
        self.screen.blit(stat_surf, stat_rect)
        self.screen.blit(score_surf, score_rect)
