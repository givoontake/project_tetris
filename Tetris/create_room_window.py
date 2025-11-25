import pygame
from typing import Optional
from menu import Button, InputBox
from define import *
from define_format import *


class ToggleButton:
    """활성/비활성 상태를 가지는 작은 토글 버튼."""
    def __init__(self, rect: pygame.Rect, text: str):
        self.rect = rect
        self.text = text
        self.active = False

        self.hovered = False
        self.pressed = False
        self._pressed_inside = False

        if Button.shared_font is None:
            Button.shared_font = pygame.font.Font("resource/dodamdodam.ttf", 20)
        self.font = Button.shared_font
        self.text_surface = self.font.render(self.text, True, WHITE)
        self.text_rect = self.text_surface.get_rect(center=self.rect.center)

    def handle_event(self, ev: pygame.event.Event) -> bool:
        """클릭 성립 시 True 반환, 그 순간 active=True로 설정(그룹 관리는 외부에서)."""
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

        if clicked:
            self.active = True
        return clicked

    def draw(self, surface: pygame.Surface):
        if self.active:
            color = ORANGE
        else:
            if self.pressed:
                color = (200, 120, 0)
            elif self.hovered:
                color = GRAY
            else:
                color = BLACK

        pygame.draw.rect(surface, color, self.rect, border_radius=6)
        self.text_rect.center = self.rect.center
        surface.blit(self.text_surface, self.text_rect)


class RoomCreateWindow:
    """
    방 만들기 창.

    - 제목: InputBox
    - 인원: 1인 / 2인 / 5인 (ToggleButton 그룹)
    - 공개 / 비공개: ToggleButton 그룹
    - 암호: InputBox (비공개일 때만 보이고 사용됨)
    - 하단: Button("방 만들기"), Button("취소")
    """
    TOP_PADDING = 50
    BOTTOM_PADDING = 50
    ROW_HEIGHT = 50
    GAP = 50
    WIDTH = 600

    # 🔹 좌우 패딩 추가
    LEFT_PADDING = 20
    RIGHT_PADDING = 20

    def __init__(self, screen: pygame.Surface):
        self.screen = screen
        self.visible = False

        # 선택 상태
        self.selected_max_user = 2   # 1 / 2 / 5
        self.is_public = True        # True: 공개, False: 비공개

        # 레이아웃 관련
        self.window_rect: pygame.Rect | None = None

        # UI 요소
        self.title_box: Optional[InputBox] = None
        self.password_box: Optional[InputBox] = None
        self.count_buttons: list[ToggleButton] = []
        self.vis_buttons: list[ToggleButton] = []
        self.btn_create: Optional[Button] = None
        self.btn_cancel: Optional[Button] = None

        self.label_font = pygame.font.Font("resource/dodamdodam.ttf", 20)

        self._recalc_layout()

    # -------- 외부에서 제어 -------- #
    def open(self):
        self.visible = True

    def close(self):
        self.visible = False

    def on_resize(self, new_screen: pygame.Surface):
        self.screen = new_screen
        self._recalc_layout()

    # -------- 내부 레이아웃 계산 -------- #
    def _compute_height(self) -> int:
        base_rows = 4  # 제목 / 인원 / 공개여부 / 하단 버튼
        if not self.is_public:
            base_rows += 1  # 암호 줄

        rows = base_rows
        return (
            self.TOP_PADDING
            + rows * self.ROW_HEIGHT
            + (rows - 1) * self.GAP
            + self.BOTTOM_PADDING
        )

    def _recalc_layout(self):
        sw, sh = self.screen.get_size()
        win_w = self.WIDTH
        win_h = self._compute_height()

        win_x = (sw - win_w) // 2
        win_y = (sh - win_h) // 2
        self.window_rect = pygame.Rect(win_x, win_y, win_w, win_h)

        # 🔹 내부 usable 영역 (좌우 패딩 적용)
        inner_x = win_x + self.LEFT_PADDING
        inner_w = win_w - (self.LEFT_PADDING + self.RIGHT_PADDING)

        cur_y = win_y + self.TOP_PADDING
        row_x = inner_x
        row_w = inner_w

        # 1) 방 제목 InputBox
        title_rect = pygame.Rect(row_x, cur_y, row_w, self.ROW_HEIGHT)
        cur_y += self.ROW_HEIGHT + self.GAP

        if self.title_box is None:
            self.title_box = InputBox(
                title_rect.x, title_rect.y, title_rect.w, title_rect.h,
                "방 제목을 입력하세요",
                max_input_len=MAX_INPUT,
                is_password=False,
                allow_korean=True
            )
        else:
            self.title_box.rect = title_rect

        # 2) 인원 토글 버튼들 (1인 / 2인 / 5인)
        count_row_rect = pygame.Rect(row_x, cur_y, row_w, self.ROW_HEIGHT)
        cur_y += self.ROW_HEIGHT + self.GAP

        margin_x = 10
        spacing = 10
        btn_w = (row_w - margin_x * 2 - spacing * 2) // 3
        btn_h = self.ROW_HEIGHT
        y_btn = count_row_rect.y

        x1 = count_row_rect.x + margin_x
        x2 = x1 + btn_w + spacing
        x3 = x2 + btn_w + spacing

        if not self.count_buttons:
            self.count_buttons = [
                ToggleButton(pygame.Rect(x1, y_btn, btn_w, btn_h), "1인"),
                ToggleButton(pygame.Rect(x2, y_btn, btn_w, btn_h), "2인"),
                ToggleButton(pygame.Rect(x3, y_btn, btn_w, btn_h), "5인"),
            ]
        else:
            self.count_buttons[0].rect.update(x1, y_btn, btn_w, btn_h)
            self.count_buttons[1].rect.update(x2, y_btn, btn_w, btn_h)
            self.count_buttons[2].rect.update(x3, y_btn, btn_w, btn_h)

        # 3) 공개 / 비공개
        vis_row_rect = pygame.Rect(row_x, cur_y, row_w, self.ROW_HEIGHT)
        cur_y += self.ROW_HEIGHT + self.GAP

        btn_w2 = (row_w - margin_x * 2 - spacing) // 2
        yv = vis_row_rect.y
        x1v = vis_row_rect.x + margin_x
        x2v = x1v + btn_w2 + spacing

        if not self.vis_buttons:
            self.vis_buttons = [
                ToggleButton(pygame.Rect(x1v, yv, btn_w2, btn_h), "공개"),
                ToggleButton(pygame.Rect(x2v, yv, btn_w2, btn_h), "비공개"),
            ]
        else:
            self.vis_buttons[0].rect.update(x1v, yv, btn_w2, btn_h)
            self.vis_buttons[1].rect.update(x2v, yv, btn_w2, btn_h)
            self.vis_buttons[0].active = self.is_public
            self.vis_buttons[1].active = not self.is_public

        # 4) 암호 InputBox (비공개일 때만 레이아웃에 포함)
        pw_rect = None
        if not self.is_public:
            pw_rect = pygame.Rect(row_x, cur_y, row_w, self.ROW_HEIGHT)
            cur_y += self.ROW_HEIGHT + self.GAP

        if self.password_box is None:
            self.password_box = InputBox(
                row_x, cur_y, row_w, self.ROW_HEIGHT,
                "비밀번호",
                max_input_len=MAX_INPUT,
                is_password=True,
                allow_korean=False
            )
        if pw_rect is not None:
            self.password_box.rect = pw_rect

        # 5) 하단 버튼 (방 만들기 / 취소)
        btn_row_rect = pygame.Rect(row_x, cur_y, row_w, self.ROW_HEIGHT)
        btn_h_bottom = self.ROW_HEIGHT
        spacing_bottom = 10
        btn_w_bottom = (row_w - spacing_bottom) // 2
        x_left = btn_row_rect.x
        x_right = x_left + btn_w_bottom + spacing_bottom
        y_bottom = btn_row_rect.y

        if self.btn_create is None:
            self.btn_create = Button(x_left, y_bottom, btn_w_bottom, btn_h_bottom, "방 만들기")
            self.btn_cancel = Button(x_right, y_bottom, btn_w_bottom, btn_h_bottom, "취소")
        else:
            self.btn_create.rect.update(x_left, y_bottom, btn_w_bottom, btn_h_bottom)
            self.btn_cancel.rect.update(x_right, y_bottom, btn_w_bottom, btn_h_bottom)

    # -------- 내부 상태 보조 -------- #
    def _set_count_active(self, index: int):
        vals = [1, 2, 5]
        self.selected_max_user = vals[index]
        for i, btn in enumerate(self.count_buttons):
            btn.active = (i == index)

    def _set_visibility(self, is_public: bool):
        if self.is_public == is_public:
            return
        self.is_public = is_public
        self._recalc_layout()

    # -------- 프레임 업데이트 -------- #
    def update(self, dt_ms: int):
        """커서 깜빡임 등 시간 기반 업데이트만 처리."""
        if not self.visible:
            return
        self.title_box.update(dt_ms)
        if not self.is_public and self.password_box is not None:
            self.password_box.update(dt_ms)

    # -------- 이벤트 처리 (버튼 이벤트 반환) -------- #
    def handle_event(self, ev: pygame.event.Event):
        if not self.visible:
            return None

        # 제목 / 암호 입력
        self.title_box.handle_event(ev)
        if not self.is_public and self.password_box is not None:
            self.password_box.handle_event(ev)

        # 인원 토글
        for idx, btn in enumerate(self.count_buttons):
            if btn.handle_event(ev):
                self._set_count_active(idx)

        # 공개 / 비공개 토글
        if self.vis_buttons[0].handle_event(ev):   # 공개
            self._set_visibility(True)
        if self.vis_buttons[1].handle_event(ev):   # 비공개
            self._set_visibility(False)

        # 하단 버튼
        if self.btn_create.handle_event(ev):
            title = self.title_box.text.strip()
            max_user = self.selected_max_user
            is_public = self.is_public
            password = None
            if not is_public and self.password_box is not None:
                pw = self.password_box.text
                password = pw if pw != "" else None

            result = {
                "room_name": title,
                "max_user": max_user,
                "room_password": password,
            }
            self.close()
            return result

        if self.btn_cancel.handle_event(ev):
            self.close()
            return "취소"

        return None

    # -------- 그리기 -------- #
    def draw(self):
        if not self.visible:
            return

        sw, sh = self.screen.get_size()

        # 반투명 오버레이
        overlay = pygame.Surface((sw, sh), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 160))
        self.screen.blit(overlay, (0, 0))

        # 본체 박스
        pygame.draw.rect(self.screen, (20, 20, 20), self.window_rect, border_radius=10)
        pygame.draw.rect(self.screen, WHITE, self.window_rect, 2, border_radius=10)

        # 🔹 라벨 x도 패딩 적용
        x = self.window_rect.x + self.LEFT_PADDING
        y = self.window_rect.y + self.TOP_PADDING

        # 라벨들
        label_title = self.label_font.render("방 제목", True, WHITE)
        self.screen.blit(label_title, (x, y - self.label_font.get_height()))

        y_count_label = y + self.ROW_HEIGHT + self.GAP
        label_count = self.label_font.render("인원", True, WHITE)
        self.screen.blit(label_count, (x, y_count_label - self.label_font.get_height()))

        y_vis_label = y_count_label + self.ROW_HEIGHT + self.GAP
        label_vis = self.label_font.render("공개 여부", True, WHITE)
        self.screen.blit(label_vis, (x, y_vis_label - self.label_font.get_height()))

        if not self.is_public:
            y_pw_label = y_vis_label + self.ROW_HEIGHT + self.GAP
            label_pw = self.label_font.render("암호", True, WHITE)
            self.screen.blit(label_pw, (x, y_pw_label - self.label_font.get_height()))

        # 요소들 그리기
        self.title_box.draw(self.screen)
        for btn in self.count_buttons:
            btn.draw(self.screen)
        for btn in self.vis_buttons:
            btn.draw(self.screen)
        if not self.is_public and self.password_box is not None:
            self.password_box.draw(self.screen)

        self.btn_create.draw(self.screen)
        self.btn_cancel.draw(self.screen)
