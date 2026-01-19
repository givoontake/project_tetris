import pygame
from define import *

class MyInfo:
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, session):
        """
        rect: MyInfo를 표시할 영역(Rect)
        session: Session 인스턴스 (nickname, win, lose 포함)
        """
        self.rect = rect
        self.my_session = session

        # 폰트 설정
        self.font_nick = pygame.font.Font("resource/dodamdodam.ttf", 40)   # 닉네임: 40px
        self.font_stat = pygame.font.Font("resource/dodamdodam.ttf", 28)   # 승/패: 28px
        self.font_max_score = pygame.font.Font("resource/dodamdodam.ttf", 28) # 최고점수

        self.text_color = WHITE
        self.padding = 20  # 닉네임과 승/패 간 간격

    def draw(self, surface: pygame.Surface):
        # 세션에서 정보 읽기

        # 1) 닉네임
        nick_surf = self.font_nick.render(self.my_session.nickname, True, self.text_color)
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
        surface.blit(nick_surf, nick_rect)
        surface.blit(stat_surf, stat_rect)
        surface.blit(score_surf, score_rect)

