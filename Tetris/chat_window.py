import pygame

CHAT_WINDOW_WIDTH = 1000
CHAT_WINDOW_HEIGHT = 250
SCROLL_WIDTH = 20

MAX_MESSAGE_LINES = 100   # 저장 가능한 최대 줄 수
FONT_SIZE = 20
LINE_PADDING = 5         # 줄 간 간격 (수직 패딩)

OPTIMIZED_OFFSET = 10
GRAY = (128, 128, 128)  # 회색 박스
WHITE = (255, 255, 255)  # 전부 흰색


class ChatWindow:
    def __init__(self, x: int, y: int, w: int, h: int):
        """
        x, y: 채팅창의 좌상단 좌표
        w, h: 채팅창 크기
        """
        self.window_x = x
        self.window_y = y
        self.window_w = w
        self.window_h = h

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
        # 각 요소는 {"user_name": str, "text": str} 형태
        # "text"는 실제로 화면에 바로 그릴 문자열 (닉네임 포함 여부까지 반영된 상태)
        self.texts: list[dict[str]] = []
        self.show_start = 0
        self.show_end = 0

        # 폰트 준비
        self.font = pygame.font.Font("resource/dodamdodam.ttf", FONT_SIZE)

    def set_show_lines(self) -> int:
        lines = self.window_h // self.line_height
        if lines < 1: lines = 1
        return lines

    def get_text_px(self, text: str) -> int:
        """현재 폰트로 렌더링했을 때 text의 가로 픽셀 길이를 반환한다."""
        width, _ = self.font.size(text)
        return width

    def add_new_message(self, user_name: str, new_message: str):
    # 1) 닉네임 + 메시지를 하나로 합침 (핵심!)
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



    def set_scroll_len(self):
        self.scroll_h = self.bg_scroll_h - int(self.scroll_px * self.scrollable_line) # 스크롤 길이 세팅

    def set_show_start_index(self):
        if self.scrollable_line > 0:
            sum_px = 0
            start_index = 0
            for i in range(self.show_lines + self.scrollable_line):
                if self.bg_scroll_y + sum_px < self.scroll_y:
                    sum_px += self.scroll_px    
                    start_index += 1
                else:
                    self.show_start = start_index
        else:
            return
        

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            window_collode = self.window_rect.collidepoint(ev.pos)
            bg_scroll_collide = self.bg_scroll_rect.collidepoint(ev.pos)
            scroll_collide = self.scroll_rect.collidepoint(ev.pos)
            _, pos_y = ev.pos

            if bg_scroll_collide :
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
                    
            self.scroll_rect = pygame.Rect(self.scroll_x, self.scroll_y, self.scroll_w, self.scroll_h)
            self.set_show_start_index()

        elif ev.type == pygame.MOUSEMOTION:
            _, pos_y = ev.pos
            if self.can_drag == True:
                if pos_y <= self.scroll_y:
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

            self.scroll_rect = pygame.Rect(self.scroll_x, self.scroll_y, self.scroll_w, self.scroll_h)
            self.set_show_start_index()

        elif ev.type == pygame.MOUSEBUTTONUP:
            self.can_drag = False

        elif ev.type == pygame.MOUSEWHEEL and self.can_scroll:
            if ev.y > 0: # 위 스크롤
                if self.scroll_y - self.scroll_px > self.bg_scroll_y:
                    self.scroll_y -= self.scroll_px
                else:
                    self.scroll_y = self.bg_scroll_y
            else:
                if self.scroll_y + self.scroll_h + self.scroll_px < self.bg_scroll_y + self.bg_scroll_h:
                    self.scroll_y += self.scroll_px
                else:
                    self.scroll_y = self.bg_scroll_y + self.bg_scroll_h
            self.set_show_start_index()
            self.scroll_rect = pygame.Rect(self.scroll_x, int(self.scroll_y), self.scroll_w, self.scroll_h)

    def clear(self):
        """저장된 모든 메시지를 삭제한다."""
        self.texts.clear()

    def draw(self, surface: pygame.Surface):
        """
        - 회색 사각형 배경을 먼저 그림
        - 최근 메시지 최대 10줄만 화면에 표시
        - 위에서부터 아래로 순서대로, 한 줄 높이(line_height) 간격으로 그린다.
        """
        # 배경 박스
        pygame.draw.rect(surface, GRAY, self.bg_scroll_rect)
        pygame.draw.rect(surface, WHITE, self.scroll_rect)

        if self.scrollable_line > 0:
            self.show_end = self.show_start + self.show_lines
        else:
            self.show_end = len(self.texts)

        draw_text_y = self.window_y
        for i in range(self.show_start, self.show_end):
            text_surf = self.font.render(self.texts[i], True, WHITE)
            surface.blit(text_surf, (self.window_x, draw_text_y))
            draw_text_y += self.line_height
