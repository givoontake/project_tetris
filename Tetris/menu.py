# menu.py
import pygame
from define import *
from define_format import MAX_INPUT_SIZE
from asset_manager import *

ORANGE = (255, 165, 0)
GRAY   = (100, 100, 100)
WHITE  = (255, 255, 255)
BLACK = (0, 0, 0)

class Button:
    """
    통합 버튼 클래스.

    - idle_btn_type / hover_btn_type / press_btn_type 은 AssetManager에 등록된 이미지 키
      -> Button은 이미지를 새로 로드하거나 스케일하지 않는다.
      -> 그냥 AssetManager에서 꺼낸 Surface를 그대로 쓴다.
    - 세 타입 키가 모두 None이면 fallback(단색 사각형)으로 렌더한다.
    - handle_event(ev)에서 클릭이 성립하면 True를 반환한다.

    사용 전:
        Button.shared_am = AssetManager(...)        # 외부에서 준비된 에셋 매니저 (이미지 다 들어있음)
        Button.shared_font = pygame.font.Font(... ) # 외부에서 넣어주거나, 없으면 __init__에서 기본 생성
    """

    shared_am: AssetManager | None = None
    shared_font: pygame.font.Font | None = None

    def __init__(
        self,
        x: int,
        y: int,
        w: int,
        h: int,
        text: str,
        idle_btn_type=None,
        hover_btn_type=None,
        press_btn_type=None,
    ):
        # 버튼 위치/크기 정보
        self.rect = pygame.Rect(x, y, w, h)

        # 상태 플래그
        self.text = text
        self.hovered = False
        self.pressed = False
        self._pressed_inside = False  # 마우스 다운이 버튼 내부에서 시작했는지

        # AssetManager는 외부에서 반드시 세팅되어 있어야 함
        if Button.shared_am is None:
            # 네가 원래 하려던 스타일은 없으면 생성하는 거였는데,
            # 이제는 "AssetManager가 사이즈까지 맞춰서 들고 있다"가 전제니까
            # 여기서 새로 만들면 의미가 없음 → 없으면 그냥 에러 내서 바로 잡게 하자.
            Button.shared_am = AssetManager()
            Button.shared_am.init()
        self.am = Button.shared_am

        # 폰트 준비 (공용 폰트 없으면 기본 생성)
        if Button.shared_font is None:
            Button.shared_font = pygame.font.Font("resource/dodamdodam.ttf", 28)
        self.font = Button.shared_font

        # 텍스트 surface 미리 만들어두기
        self.text_surface = self.font.render(self.text, True, (255, 255, 255))
        self.text_rect = self.text_surface.get_rect(center=self.rect.center)

        # 상태별 이미지 Surface 직접 참조 (스케일 X, 로드 X)
        self.idle_img  = self.am.button_asset[idle_btn_type]  if idle_btn_type  is not None else None
        self.hover_img = self.am.button_asset[hover_btn_type] if hover_btn_type is not None else None
        self.press_img = self.am.button_asset[press_btn_type] if press_btn_type is not None else None

    def handle_event(self, ev: pygame.event.Event) -> bool:
        """
        마우스 이벤트 처리:
        - hover 상태 추적
        - 눌림/뗌 추적
        - '버튼 안에서 눌렀고 버튼 안에서 뗀 경우'만 True 반환
        """
        clicked = False

        if ev.type == pygame.MOUSEMOTION:
            self.hovered = self.rect.collidepoint(ev.pos)

        elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            if self.rect.collidepoint(ev.pos):
                self.pressed = True
                self._pressed_inside = True
            else:
                self.pressed = False
                self._pressed_inside = False

        elif ev.type == pygame.MOUSEBUTTONUP and ev.button == 1:
            if self.pressed and self._pressed_inside and self.rect.collidepoint(ev.pos):
                clicked = True
            self.pressed = False
            self._pressed_inside = False

        return clicked

    def draw(self, surface: pygame.Surface):
        """
        현재 상태에 맞는 이미지를 골라 그린다.
        우선순위: press_img > hover_img > idle_img
        아무 이미지도 없으면 fallback 사각형으로 그린다.
        텍스트는 항상 위에 얹는다.
        """

        used_img = None
        if self.pressed and self.press_img is not None:
            used_img = self.press_img
        elif self.hovered and self.hover_img is not None:
            used_img = self.hover_img
        elif self.idle_img is not None:
            used_img = self.idle_img

        if used_img is not None:
            # 준비된 이미지 그대로 blit
            surface.blit(used_img, self.rect.topleft)
        else:
            # fallback: 단색 버튼(색은 원하는 걸로 바꿔도 됨)
            if self.pressed:
                color = ORANGE    # ORANGE-ish
            elif self.hovered:
                color = GRAY # GRAY-ish
            else:
                color = BLACK    # DARK-ish

            pygame.draw.rect(surface, color, self.rect, border_radius=8)
            # pygame.draw.rect(surface, (180, 180, 180), self.rect, width=2, border_radius=8)

        # 텍스트 중앙에 그리기
        self.text_rect.center = self.rect.center
        surface.blit(self.text_surface, self.text_rect)



class InputBox:
    def __init__(self, x: int, y: int, w: int, h: int, placeholder: str = "", is_password: bool = False):
        self.rect = pygame.Rect(x, y, w, h)
        self.placeholder = placeholder
        self.text = ""
        self.active = False
        self.is_password = is_password

        self.color_idle = (40, 40, 40)
        self.color_active = (60, 140, 255)
        self.color = self.color_idle

        self.font = pygame.font.Font("resource/dodamdodam.ttf", 28)
        self.padding = 10

        # 커서 점멸
        self.cursor_visible = True
        self.cursor_timer_ms = 0
        self.cursor_blink_ms = 500

        # 백스페이스 키 반복(typematic) 상태
        self.input_active = False
        self.input_timer_ms = 0
        self.input_delay_ms = 500   # 첫 반복까지 대기(0.5s)
        self.input_repeat_ms = 50  # 이후 반복 간격(0.1s)
        self.input_delay_done = False
        self.repeat_key = None      # 현재 눌려 반복 중인 키 코드
        self.repeat_char = ""       # 문자키의 unicode(반복 시 재사용)

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            self.active = self.rect.collidepoint(ev.pos)
            self.color = self.color_active if self.active else self.color_idle

        if ev.type == pygame.KEYDOWN and self.active:
            if ev.key == pygame.K_BACKSPACE:
                # 누르는 순간 즉시 1글자 삭제
                if self.text:
                    self.text = self.text[:-1]
                # 반복 시작
                self.input_active = True
                self.input_timer_ms = 0
                self.input_delay_done = False
                self.repeat_key = pygame.K_BACKSPACE
                self.repeat_char = ""
            elif ev.key == pygame.K_RETURN:
                # 엔터는 상위에서 처리
                pass
            else:
                if ev.unicode and ev.unicode.isprintable():
                    if len(self.text) < MAX_INPUT_SIZE:
                        self.text += ev.unicode
                    # 반복 시작(문자키)
                    self.input_active = True
                    self.input_timer_ms = 0
                    self.input_delay_done = False
                    self.repeat_key = ev.key
                    self.repeat_char = ev.unicode
                else:
                    # 인쇄 불가 키는 반복 비활성
                    self.input_active = False
                    self.repeat_key = None
                    self.repeat_char = ""

        elif ev.type == pygame.KEYUP:
            # 손을 떼면(현재 반복 중인 키를 뗀 경우) 반복 종료
            if self.repeat_key is not None and ev.key == self.repeat_key:
                self.input_active = False
                self.input_timer_ms = 0
                self.input_delay_done = False
                self.repeat_key = None
                self.repeat_char = ""


    def update(self, dt_ms: int):
    # 커서 점멸
        if self.active:
            self.cursor_timer_ms += dt_ms
            if self.cursor_timer_ms >= self.cursor_blink_ms:
                self.cursor_timer_ms = 0
                self.cursor_visible = not self.cursor_visible
        else:
            self.cursor_visible = False
            self.cursor_timer_ms = 0

        # 키 반복
        if self.active and self.input_active and self.repeat_key is not None:
            self.input_timer_ms += dt_ms

            if not self.input_delay_done:
                # 첫 500ms 지연
                if self.input_timer_ms >= self.input_delay_ms:
                    self.input_timer_ms -= self.input_delay_ms
                    self.input_delay_done = True
            else:
                # 지연 이후 100ms 간격 반복
                while self.input_timer_ms >= self.input_repeat_ms:
                    self.input_timer_ms -= self.input_repeat_ms

                    if self.repeat_key == pygame.K_BACKSPACE:
                        if self.text:
                            self.text = self.text[:-1]
                        else:
                            # 더 지울 게 없으면 타이머만 정리하고 반복 유지
                            self.input_timer_ms = 0
                            break
                    else:
                        # 문자키 반복(인쇄 가능 + 길이 제한)
                        if self.repeat_char and self.repeat_char.isprintable() and len(self.text) < MAX_INPUT_SIZE:
                            self.text += self.repeat_char
                        else:
                            # 반복 불가(공간 부족/인쇄 불가) 시 타이머만 정리
                            self.input_timer_ms = 0
                            break


    def draw(self, surf: pygame.Surface):
        # 배경
        pygame.draw.rect(surf, self.color, self.rect, border_radius=8)

        # 표시 텍스트 (비밀번호면 마스킹)
        show = self.text if not self.is_password else ("●" * len(self.text))
        if not self.text and not self.active:
            txt = self.font.render(self.placeholder, True, (150, 150, 150))
        else:
            txt = self.font.render(show, True, (255, 255, 255))

        text_pos = (self.rect.x + self.padding, self.rect.y + (self.rect.height - txt.get_height()) // 2)
        surf.blit(txt, text_pos)

        # 커서
        if self.active and self.cursor_visible:
            cursor_x = text_pos[0] + txt.get_width()
            cursor_y = self.rect.y + 8
            pygame.draw.rect(surf, (255, 255, 255),
                             (cursor_x, cursor_y, 2, self.rect.height - 16))
            

class PopupBox:
    def __init__(self, screen: pygame.Surface, message: str,
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

        self.left_button = Button(left_x,  btn_y, btn_w, btn_h, self.left_button_text)
        self.right_button= Button(right_x, btn_y, btn_w, btn_h, self.right_button_text)

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
