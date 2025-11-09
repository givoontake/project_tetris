# menu.py
import pygame
from define import *
from define_format import MAX_INPUT_SIZE
from asset_manager import *

ORANGE = (255, 165, 0)
GRAY   = (128, 128, 128)
PLACEHOLDER = (100, 100, 100)
WHITE  = (255, 255, 255)
BLUE = (0, 0, 150)
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
        text: str = None,
        idle_btn_type=None,
        hover_btn_type=None,
        press_btn_type=None,
        react = True
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

        self.react = react

    def handle_event(self, ev: pygame.event.Event) -> bool:
        if self.react == False:
            return False
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

            pygame.draw.rect(surface, color, self.rect)

        # 텍스트 중앙에 그리기
        if self.text is not None:
            self.text_rect.center = self.rect.center
            surface.blit(self.text_surface, self.text_rect)



class InputBox:
    def __init__(
        self,
        x: int,
        y: int,
        w: int,
        h: int,
        placeholder: str = "",
        max_input_len: int | None = None,  # optional과 같음
        is_password: bool = False,
        allow_korean: bool = True,   # ✅ 한글 허용 여부
    ):
        self.rect = pygame.Rect(x, y, w, h)
        self.placeholder = placeholder
        self.text_h = int(h*0.8)
        self.text = ""
        self.editing_text = ""
        self.active = False
        self.is_password = is_password
        self.allow_korean = allow_korean
        self.max_input_len = max_input_len

        self.font = pygame.font.Font("resource/dodamdodam.ttf", self.text_h)
        self.padding = int(self.text_h / 2)
        self.color = GRAY

        # 커서 점멸
        self.cursor_visible = True
        self.cursor_timer_ms = 0
        self.cursor_blink_ms = 500

        # 키 입력은 윈도우 입력기를 통해 처리하지만 완성된 글자만 반환하므로 지우기는 따로 처리해야함
        self.backspace_pressed = False
        #self.backspace_repeat_active = False
        self.backspace_repeat_timer = 0
        self.backspace_repeat_time1 = 500
        self.backspace_repeat_time1_active = True
        self.backspace_repeat_time2 = 50
        self.backspace_repeat_time2_active = False

    def add_char(self, ch: str):
        if not self.active:
            return
        # 빈 문자, 제어문자 필터링
        if not ch or not ch.isprintable():
            return
        # 한글 비허용이면 ASCII만 받음
        if not self.allow_korean and not ch.isascii():
            return
        # 길이 초과 방지
        if self.max_input_len is not None and len(self.text) >= self.max_input_len:
            return
        
        if not ch.isascii():
            self.backspace_repeat_time1_active = False
            self.backspace_repeat_time2_active = True
        # 통과했으면 추가
        self.text += ch

    def delete_char(self):
        if not self.active:
            return
        if self.text:
            self.text = self.text[:-1]

    def get_text_width(self, text: str) -> int:
        width, _ = self.font.size(text)
        return width

    def get_render_text(self) -> str:
        text_px_len = None
        render_text = ""
        if len(self.text) < self.max_input_len:
            render_text = self.text + self.editing_text
            text_px_len = self.get_text_width(self.text) + self.get_text_width(self.editing_text)
        else:
            render_text = self.text
            text_px_len = self.get_text_width(self.text)

        box_px_len = self.rect.w - self.padding * 2# 좌우 기본 패딩
        offset = 0
        
        if self.is_password:
            render_text = "●" * (len(self.text) + len(self.editing_text))
                                      
        while True:            
            if text_px_len <= box_px_len:
                return render_text
            
            else:
                if self.get_text_width(render_text[offset:]) <= box_px_len:
                    return render_text[offset:]
                else:
                    offset += 1
                    if offset >= len(self.text):
                        return ""


    def handle_event(self, ev: pygame.event.Event):
        # 마우스 클릭으로 포커스 on/off
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            self.active = self.rect.collidepoint(ev.pos)
            if self.active:
                self.color = BLUE
            else:
                self.color = GRAY

        # ===== 한글/영문 조합 중 문자열 (프리뷰) =====
        elif ev.type == pygame.TEXTEDITING:
            # 한글 허용인 필드만 조합 상태 표시
            if self.active:
                self.editing_text = ev.text

        # ===== 최종 확정된 문자 입력 =====
        elif ev.type == pygame.TEXTINPUT:
            self.add_char(ev.text)

        # ===== 제어키 처리 (백스페이스 + 엔터) =====
        elif ev.type == pygame.KEYDOWN:
            if ev.key == pygame.K_BACKSPACE:  # TEXTINPUT으로는 문자 아니면 처리 안됨, 따로 처리 필요
                self.delete_char()
                self.backspace_pressed = True

            elif ev.key == pygame.K_RETURN:
                # 엔터는 상위(State)에서 처리
                pass
        elif ev.type == pygame.KEYUP:
            if ev.key == pygame.K_BACKSPACE:
                self.backspace_repeat_timer = 0
                self.backspace_repeat_time1_active = True
                self.backspace_repeat_time2_active = False
                self.backspace_pressed = False

    def update(self, dt_ms: int):
        # 커서 점멸만 유지
        if self.active:
            self.cursor_timer_ms += dt_ms
            if self.cursor_timer_ms >= self.cursor_blink_ms:
                self.cursor_timer_ms -= self.cursor_blink_ms
                self.cursor_visible = not self.cursor_visible

            if self.backspace_repeat_time1_active and self.backspace_pressed:
                self.backspace_repeat_timer += dt_ms
                if self.backspace_repeat_timer >= self.backspace_repeat_time1:
                    self.backspace_repeat_timer -= self.backspace_repeat_time1
                    self.backspace_repeat_time1_active = False
                    self.backspace_repeat_time2_active = True
                    self.delete_char()

            elif self.backspace_repeat_time2_active and self.backspace_pressed:
                self.backspace_repeat_timer += dt_ms
                if self.backspace_repeat_timer >= self.backspace_repeat_time2:
                    self.backspace_repeat_timer -= self.backspace_repeat_time2
                    self.delete_char()
                
        else:
            self.cursor_visible = False
            self.cursor_timer_ms = 0
            self.backspace_repeat_timer = 0
            self.backspace_repeat_time1_active = True
            self.backspace_repeat_time2_active = False


    def draw(self, surf: pygame.Surface):
        # 배경
        pygame.draw.rect(surf, self.color, self.rect)

        # 보여줄 텍스트 (비밀번호면 ●로 마스킹)
        show_text = self.get_render_text()
        if not show_text and not self.active:
            txt = self.font.render(self.placeholder, True, PLACEHOLDER)
        else:
            txt = self.font.render(show_text, True, WHITE)

        text_x = self.rect.x + self.padding
        text_y = self.rect.y + (self.rect.height - txt.get_height()) // 2
        text_pos = (text_x, text_y)
        surf.blit(txt, text_pos)

        # 커서
        if self.active and self.cursor_visible:
            cursor_x = text_x + txt.get_width()
            cursor_y = text_y
            cursor_h = self.text_h
            pygame.draw.rect(surf, WHITE,(cursor_x, cursor_y, 2, cursor_h))



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
