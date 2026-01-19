import pygame

from button import Button
from resource_manager import ResourceManager

class PopupBox:
    def __init__(self, screen: pygame.Surface, rm: ResourceManager, message: str,
                 left_text: str = "확인", right_text: str = "취소"):
        """
        screen : 현재 게임 화면 surface
        message: 알림 내용(팝업 중앙)
        left_text / right_text: 하단 좌/우 버튼에 표시할 텍스트
        """
        self.screen = screen
        self.message = message
        self.visible = False
        self.left_button_text = left_text
        self.right_button_text = right_text
        self.rm = rm

        # 색 / 스타일
        self.bg_overlay_color = (0, 0, 0, 128)  # 전체 화면 어둡게 (반투명)
        self.window_color = (0, 0, 0)           # 팝업 본체
        self.border_color = (255, 255, 255)
        self.border_thickness = 2

        # 폰트 (기존 폰트와 동일 계열)
        self.font_msg = pygame.font.Font("resource/dodamdodam.ttf", 36)

        # 레이아웃 계산
        self._recalc_layout()

        # 버튼 생성
        # 버튼 크기 규칙:
        #   높이 = 팝업 높이의 1/3
        #   너비 = 팝업 너비의 1/2
        #   팝업 하단에 위치 (좌우 배치)
        btn_h = self.win_h // 3
        btn_w = self.win_w // 2

        # y는 팝업 맨 아래에 버튼이 딱 붙게.
        # 팝업 내부 기준: 아래쪽 영역 전체가 버튼 높이
        btn_y = self.win_y + (self.win_h - btn_h)

        left_x  = self.win_x
        right_x = self.win_x + btn_w
        left_rect = pygame.Rect(left_x, btn_y, btn_w, btn_h)
        right_rect = pygame.Rect(right_x, btn_y, btn_w, btn_h)
        self.left_button = Button(self.screen, left_rect, self.rm, None, self.left_button_text, True)
        self.right_button= Button(self.screen, right_rect, self.rm, None, self.right_button_text, True)

    def _recalc_layout(self):
        """현재 screen 사이즈를 기준으로 팝업 사각형을 다시 계산한다."""
        sw, sh = self.screen.get_size()
        self.win_w = sw // 2
        self.win_h = sh // 2
        self.win_x = (sw - self.win_w) // 2
        self.win_y = (sh - self.win_h) // 2
        self.window_rect = pygame.Rect(self.win_x, self.win_y, self.win_w, self.win_h)

    def handle_event(self, events: list[pygame.event.Event])-> str: # 어떤 버튼이 눌렸는가
        """
        - ESC 키로 닫힘
        - 각 버튼 클릭 시 닫힘
        """
        if not self.visible:
            return

        for ev in events:
            # ESC로 닫기
            if ev.type == pygame.KEYDOWN and ev.key == pygame.K_ESCAPE:
                return "확인"

            # 버튼 이벤트 처리
            if self.left_button.handle_event(ev):
                return self.left_button_text
            
            if self.right_button.handle_event(ev):
                return self.right_button_text

    def draw(self):
        if not self.visible:
            return

        sw, sh = self.screen.get_size()

        # 1) 전체 화면 어둡게(반투명 오버레이)
        overlay = pygame.Surface((sw, sh), pygame.SRCALPHA)
        overlay.fill(self.bg_overlay_color)
        self.screen.blit(overlay, (0, 0))

        # 2) 팝업 본체(검은 박스)
        pygame.draw.rect(self.screen, self.window_color, self.window_rect, border_radius=8)

        # 3) 본문 메시지: 팝업 중앙
        msg_surf = self.font_msg.render(self.message, True, (255, 255, 255))
        msg_rect = msg_surf.get_rect(center=self.window_rect.center)
        self.screen.blit(msg_surf, msg_rect)

        # 4) 두 버튼 그리기
        self.left_button.draw(self.screen)
        self.right_button.draw(self.screen)

    def on_resize(self, new_screen: pygame.Surface):
        """
        화면 리사이즈 시 GameLoop에서 state.on_resize(...) 부르잖아?
        팝업도 똑같이 다시 가운데 정렬해야 하므로 이 함수 호출해주면 됨.
        """
        self.screen = new_screen
        self._recalc_layout()

        # 버튼 위치/크기도 다시 계산
        btn_h = self.win_h // 3
        btn_w = self.win_w // 2
        btn_y = self.win_y + (self.win_h - btn_h)
        left_x  = self.win_x
        right_x = self.win_x + btn_w

        # 버튼 rect들 갱신
        self.left_button.rect.update(left_x, btn_y, btn_w, btn_h)
        self.right_button.rect.update(right_x, btn_y, btn_w, btn_h)
