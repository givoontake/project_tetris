import pygame
from define import *
from session import Session
from font_manager import FontManager

class Profile:
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, fm: FontManager, session: Session):
        self.screen = screen
        self.rect = rect
        self.my_session = session

        # 폰트 설정
        self.font_nickname = fm.load_font(40)
        self.font_stat = fm.load_font(30)
        self.font_max_score = fm.load_font(30)

        self.text_color = WHITE
        self.padding = 20  # 닉네임과 승/패 간 간격

    def draw(self):
        # 세션에서 정보 읽기

        # 1) 닉네임
        nick_surf = self.font_nickname.render(self.my_session.nickname, True, self.text_color)
        nick_rect = nick_surf.get_rect(
            center=(self.rect.centerx, self.rect.y + nick_surf.get_height() // 2)
        )

        # 2) 승/패
        stat_text = f"승: {self.my_session.win}   패: {self.my_session.lose}"
        stat_surf = self.font_stat.render(stat_text, True, self.text_color)
        stat_rect = stat_surf.get_rect(
            center=(self.rect.centerx,
                    nick_rect.bottom + self.padding + stat_surf.get_height() // 2)
        )

        score_text = f"최고점수: {self.my_session.max_score}"
        score_surf = self.font_stat.render(score_text, True, self.text_color)
        score_rect = stat_surf.get_rect(
            center=(self.rect.centerx,
                    stat_rect.bottom + self.padding + score_surf.get_height() // 2)
        )

        # 실제 그리기
        self.screen.blit(nick_surf, nick_rect)
        self.screen.blit(stat_surf, stat_rect)
        self.screen.blit(score_surf, score_rect)

