import pygame
from tetris.config.define import *
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.define_colors import *

CHAT_WINDOW_WIDTH = 1000
CHAT_WINDOW_HEIGHT = 250
SCROLL_WIDTH = 20

MAX_MESSAGE_LINES = 100   # 저장 가능한 최대 줄 수
FONT_SIZE = 20
LINE_PADDING = 5         # 줄 간 간격 (수직 패딩)

OPTIMIZED_OFFSET = 10

class ChatWindow:
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, font: pygame.font.Font):
        self.screen = screen
        self.rect = rect
        self.font = font
        
        self.window_x = rect.x
        self.window_y = rect.y
        self.window_w = rect.w
        self.window_h = rect.h

        self.bg_scroll_x = self.window_x + self.window_w
        self.bg_scroll_y = self.window_y
        self.bg_scroll_w = SCROLL_WIDTH
        self.bg_scroll_h = self.window_h

        self.scroll_x = self.bg_scroll_x
        self.scroll_y = self.bg_scroll_y
        self.scroll_w = self.bg_scroll_w
        self.scroll_h = self.bg_scroll_h

        self.line_height = FONT_SIZE + LINE_PADDING
        self.show_lines = self.set_show_lines()
        self.min_h = self.line_height

        self.window_rect = pygame.Rect(self.window_x, self.window_y, self.window_w, self.window_h)
        self.bg_scroll_rect = pygame.Rect(self.bg_scroll_x, self.bg_scroll_y, self.bg_scroll_w, self.bg_scroll_h)
        self.scroll_rect = pygame.Rect(self.scroll_x, self.scroll_y, self.scroll_w, self.scroll_h)

        self.max_scrollable_line = MAX_MESSAGE_LINES - self.show_lines
        self.scrollable_line = 0
        self.scrollable_px = self.scroll_h - self.min_h
        self.scroll_px = self.scrollable_px / self.max_scrollable_line

        self.prev_y = 0
        self.can_drag = False
        self.can_scroll = False

        # 채팅 줄 단위로 누적 저장되는 리스트
        self.texts: list[str] = []
        self.show_start = 0
        self.show_end = 0

    def get_text_px(self, text: str) -> int:
        """현재 폰트로 렌더링했을 때 text의 가로 픽셀 길이를 반환한다."""
        width, _ = self.font.size(text)
        return width

    def set_show_lines(self) -> int:
        lines = self.window_h // self.line_height
        if lines < 1:
            lines = 1
        return lines

    def add_new_message(self, user_name: str, new_message: str):
        # 1) 닉네임 + 메시지를 하나로 합침
        full_text = f"[{user_name}]: {new_message}"
        remain = full_text
        remain_len = len(remain)

        while remain_len > 0:
            # 전체가 한 줄에 들어가면 그냥 추가하고 끝
            if self.get_text_px(remain) <= self.window_w:
                self.texts.append(remain)
                break

            # 2) 한 줄에 들어갈 수 있는 최대 prefix 길이 cut_idx 찾기
            offset = 0
            prev_offset = 0

            # --- 2-1. 큰 폭 점프 탐색 ---
            while True:
                # 다음 jump가 범위를 넘으면 점프 종료 → 세밀 탐색으로 이동
                if offset + OPTIMIZED_OFFSET >= remain_len:
                    break

                prev_offset = offset
                offset += OPTIMIZED_OFFSET

                part_px = self.get_text_px(remain[:offset])
                if part_px > self.window_w:
                    # 넘었으면 이전 offset으로 되돌리고 세밀 탐색으로 이동
                    offset = prev_offset
                    break

            # --- 2-2. 세밀 탐색 (1씩 증가) ---
            while offset < remain_len:
                part_px = self.get_text_px(remain[:offset + 1])
                if part_px > self.window_w:
                    break
                offset += 1

            # 3) offset이 0일 수는 없도록 보호 (너무 좁아도 최소 한 글자)
            if offset == 0:
                offset = 1

            # 4) 한 줄 완성 → append
            self.texts.append(remain[:offset])

            # 5) 남은 문자열로 계속 처리
            remain = remain[offset:]
            remain_len = len(remain)

        # 스크롤 갱신
        if len(self.texts) - self.show_lines > 0:
            self.scrollable_line = len(self.texts) - self.show_lines
        else:
            self.scrollable_line = 0

        self.set_scroll_len()
        if self.can_drag: self.set_show_start_index()
        else: self.scroll_to_bottom()

    def scroll_to_bottom(self):
        self.show_start = self.scrollable_line
        
        bottom_scroll_y = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h
        self.scroll_y = bottom_scroll_y
        self.scroll_rect.y = bottom_scroll_y


    def set_scroll_len(self):
        # 기존 로직 유지
        self.scroll_h = self.bg_scroll_h - int(self.scroll_px * self.scrollable_line)
        if self.scroll_h < self.min_h:
            self.scroll_h = self.min_h

        # 스크롤 바가 바닥을 넘지 않도록 보정
        max_scroll_y = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h
        if self.scroll_y > max_scroll_y:
            self.scroll_y = max_scroll_y
        if self.scroll_y < self.bg_scroll_y:
            self.scroll_y = self.bg_scroll_y

        self.scroll_rect = pygame.Rect(self.scroll_x, int(self.scroll_y), self.scroll_w, self.scroll_h)

    def set_show_start_index(self):
        if self.scrollable_line <= 0:
            self.show_start = 0
            return

        # 트랙 상에서 thumb가 움직일 수 있는 범위
        track_top = self.bg_scroll_y
        track_bottom = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h
        if track_bottom <= track_top:
            self.show_start = 0
            return

        # 현재 thumb 위치를 0~1 비율로 정규화
        ratio = (self.scroll_y - track_top) / (track_bottom - track_top)
        if ratio < 0:
            ratio = 0
        elif ratio > 1:
            ratio = 1

        # 비율에 따라 시작 라인 인덱스 결정
        self.show_start = int(ratio * self.scrollable_line)
        if self.show_start < 0:
            self.show_start = 0
        if self.show_start > self.scrollable_line:
            self.show_start = self.scrollable_line

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            window_collode = self.window_rect.collidepoint(ev.pos)
            bg_scroll_collide = self.bg_scroll_rect.collidepoint(ev.pos)
            scroll_collide = self.scroll_rect.collidepoint(ev.pos)
            _, pos_y = ev.pos

            if bg_scroll_collide:
                if scroll_collide:
                    self.prev_y = pos_y
                    self.can_drag = True
                    self.can_scroll = True
                else:
                    self.scroll_y = pos_y - (self.scroll_h / 2)
                    if self.scroll_y <= self.bg_scroll_y:
                        self.scroll_y = self.bg_scroll_y
                    elif self.scroll_y > self.bg_scroll_y and self.scroll_y < self.bg_scroll_y + self.bg_scroll_h - self.scroll_h:
                        pass
                    else:
                        self.scroll_y = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h
                    self.prev_y = pos_y
                    self.can_drag = True
                    self.can_scroll = True

            elif window_collode:
                self.can_scroll = True
                self.can_drag = False

            else:
                self.can_drag = False
                self.can_scroll = False

            self.scroll_rect = pygame.Rect(self.scroll_x, int(self.scroll_y), self.scroll_w, self.scroll_h)
            self.set_show_start_index()

        elif ev.type == pygame.MOUSEMOTION:
            _, pos_y = ev.pos
            if self.can_drag:
                if pos_y <= self.bg_scroll_y:
                    self.scroll_y = self.bg_scroll_y
                    self.prev_y = self.bg_scroll_y
                elif pos_y > self.bg_scroll_y and pos_y < self.bg_scroll_y + self.bg_scroll_h:
                    px_diff = pos_y - self.prev_y
                    if self.scroll_y + px_diff + self.scroll_h > self.bg_scroll_y + self.bg_scroll_h:
                        self.scroll_y = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h
                    elif self.scroll_y + px_diff < self.bg_scroll_y:
                        self.scroll_y = self.bg_scroll_y
                    else:
                        self.scroll_y += px_diff
                    self.prev_y = pos_y
                else:
                    self.scroll_y = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h
                    self.prev_y = self.bg_scroll_y + self.bg_scroll_h

            self.scroll_rect = pygame.Rect(self.scroll_x, int(self.scroll_y), self.scroll_w, self.scroll_h)
            self.set_show_start_index()

        elif ev.type == pygame.MOUSEBUTTONUP:
            self.can_drag = False

        elif ev.type == pygame.MOUSEWHEEL and self.can_scroll:
            max_scroll_y = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h

            if ev.y > 0:  # 위 스크롤
                if self.scroll_y - self.scroll_px > self.bg_scroll_y:
                    self.scroll_y -= self.scroll_px
                else:
                    self.scroll_y = self.bg_scroll_y
            else:  # 아래 스크롤
                if self.scroll_y + self.scroll_px < max_scroll_y:
                    self.scroll_y += self.scroll_px
                else:
                    self.scroll_y = max_scroll_y

            self.set_show_start_index()
            self.scroll_rect = pygame.Rect(self.scroll_x, int(self.scroll_y), self.scroll_w, self.scroll_h)

    def clear(self):
        """저장된 모든 메시지를 삭제한다."""
        self.texts.clear()
        self.scrollable_line = 0
        self.show_start = 0
        self.show_end = 0
        self.scroll_y = self.bg_scroll_y
        self.scroll_h = self.bg_scroll_h
        self.scroll_rect = pygame.Rect(self.scroll_x, int(self.scroll_y), self.scroll_w, self.scroll_h)

    def draw(self):
        """
        - 회색 사각형 배경을 먼저 그림
        - 최근 메시지 최대 show_lines줄만 화면에 표시
        - 위에서부터 아래로 순서대로, 한 줄 높이(line_height) 간격으로 그린다.
        """
        # 배경 박스
        pygame.draw.rect(self.screen, GRAY, self.bg_scroll_rect)
        if self.can_drag: pygame.draw.rect(self.screen, ORANGE, self.scroll_rect)
        else: pygame.draw.rect(self.screen, WHITE, self.scroll_rect)

        if self.scrollable_line > 0:
            self.show_end = self.show_start + self.show_lines
            if self.show_end > len(self.texts):
                self.show_end = len(self.texts)
        else:
            self.show_start = 0
            self.show_end = len(self.texts)

        draw_text_y = self.window_y
        for i in range(self.show_start, self.show_end):
            text_surf = self.rm.render(self.texts[i], True, WHITE)
            self.screen.blit(text_surf, (self.window_x, draw_text_y))
            draw_text_y += self.line_height
