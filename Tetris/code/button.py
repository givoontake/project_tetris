import pygame

from define import *
from resource_manager import ResourceManager

class Button:
    """
    통합 버튼 클래스.

    - idle_btn_type / hover_btn_type / press_btn_type 은 AssetManager에 등록된 이미지 키
      -> Button은 이미지를 새로 로드하거나 스케일하지 않는다.
      -> 그냥 AssetManager에서 꺼낸 Surface를 그대로 쓴다.
    - 세 타입 키가 모두 None이면 fallback(단색 사각형)으로 렌더한다.
    - handle_event(ev)에서 클릭이 성립하면 True를 반환한다.

    
    사용 전:
        Button.shared_font = pygame.font.Font(... ) # 외부에서 넣어주거나, 없으면 __init__에서 기본 생성
    """
    shared_font: pygame.font.Font | None = None

    def __init__(
        self,
        x: int,
        y: int,
        w: int,
        h: int,
        text: str = None,
        rm: ResourceManager = None,
        idle_btn_type=None,
        hover_btn_type=None,
        press_btn_type=None,
        react = True
    ):
        # 버튼 위치/크기 정보
        self.rect = pygame.Rect(x, y, w, h)

        # 상태 플래그
        self.rm = rm
        self.text = text
        self.hovered = False
        self.pressed = False
        self._pressed_inside = False  # 마우스 다운이 버튼 내부에서 시작했는지
        self.hover_sound_printed = False

        # 폰트 준비 (공용 폰트 없으면 기본 생성)
        if Button.shared_font is None:
            Button.shared_font = pygame.font.Font("resource/dodamdodam.ttf", 28)
        self.font = Button.shared_font

        # 텍스트 surface 미리 만들어두기
        self.text_surface = self.font.render(self.text, True, (255, 255, 255))
        self.text_rect = self.text_surface.get_rect(center=self.rect.center)

        # 상태별 이미지 Surface 직접 참조 (스케일 X, 로드 X)
        if rm == None:
            self.idle_img = None
            self.hover_img = None
            self.press_img = None
        
        else:
            self.idle_img  = self.rm.button_images[idle_btn_type]  if idle_btn_type  is not None else None
            self.hover_img = self.rm.button_images[hover_btn_type] if hover_btn_type is not None else None
            self.press_img = self.rm.button_images[press_btn_type] if press_btn_type is not None else None

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
            if self.hovered:
                if self.hover_sound_printed == False:
                    self.hover_sound_printed = True
                    self.rm.button_sound_hover.play()
            else: self.hover_sound_printed = False

        elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            if self.rect.collidepoint(ev.pos):
                self.pressed = True
                self._pressed_inside = True
                self.rm.button_sound_press.play()
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