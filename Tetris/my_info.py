import pygame

class MyInfo:
    def __init__(self, rect: pygame.Rect, session):
        """
        rect: MyInfo를 표시할 영역(Rect)
        session: Session 인스턴스 (nickname, win, lose 포함)
        """
        self.rect = rect
        self.my_session = session

        # 폰트 설정
        self.font_nick = pygame.font.Font("resource/dodamdodam.ttf", 40)   # 닉네임: 40px
        self.font_stat = pygame.font.Font("resource/dodamdodam.ttf", 28)   # 승/패: 28px

        self.text_color = (255, 255, 255)
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

        # 실제 그리기
        surface.blit(nick_surf, nick_rect)
        surface.blit(stat_surf, stat_rect)
