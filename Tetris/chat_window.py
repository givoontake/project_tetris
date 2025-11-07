import pygame

CHAT_WINDOW_WIDTH = 1000
CHAT_WINDOW_HEIGHT = 250
SCROLL_WIDTH = 20

MAX_MESSAGE_LINES = 100   # 저장 가능한 최대 줄 수
SHOW_MESSAGE_LINES = 10   # 그릴 때 한 번에 보여줄 줄 수
FONT_SIZE = 20
LINE_PADDING = 5         # 줄 간 간격 (수직 패딩)

OPTIMIZED_OFFSET = 10
GRAY = (128, 128, 128)  # 회색 박스
WHITE = (255, 255, 255)  # 전부 흰색


class ChatWindow:
    def __init__(self, x: int, y: int, w: int = CHAT_WINDOW_WIDTH, h: int = CHAT_WINDOW_HEIGHT):
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

        self.min_h = FONT_SIZE + LINE_PADDING

        self.window_rect = pygame.Rect(self.window_x, self.window_y, self.window_w, self.window_h) 
        self.bg_scroll_rect = pygame.Rect(self.bg_scroll_x, self.bg_scroll_y, self.bg_scroll_w, self.bg_scroll_h) 
        self.scroll_rect = pygame.Rect(self.scroll_x, self.scroll_y, self.scroll_w, self.scroll_h) 

        self.max_scrollable_line = MAX_MESSAGE_LINES - SHOW_MESSAGE_LINES
        self.scrollable_line = 0
        self.scrollable_px = self.scroll_h - self.min_h
        self.scroll_px = self.scrollable_px / self.max_scrollable_line

        self.prev_y = 0
        self.can_drag = False
        self.can_scroll = False

        # 채팅 줄 단위로 누적 저장되는 리스트
        # 각 요소는 {"user_name": str, "text": str} 형태
        # "text"는 실제로 화면에 바로 그릴 문자열 (닉네임 포함 여부까지 반영된 상태)
        self.texts: list[dict[str, str]] = []
        self.show_start = 0
        self.show_end = 0

        # 폰트 준비
        self.font = pygame.font.Font("resource/dodamdodam.ttf", FONT_SIZE)

        # 한 줄의 렌더링 높이 = 글자 높이(20px) + 상하 간격(5px) = 25px 고정
        self.line_height = FONT_SIZE + LINE_PADDING  # 20 + 5 = 25

    def get_text_width(self, text: str) -> int:
        """현재 폰트로 렌더링했을 때 text의 가로 픽셀 길이를 반환한다."""
        width, _ = self.font.size(text)
        return width

    def add_new_message(self, user_name: str, new_message: str):
        """
        하나의 (user_name, message) 입력을 화면에 표시 가능한 여러 줄로 자른다.
        잘라진 결과는 [{'user_name': ..., 'text': ...}, ...] 형태로 반환한다.

        규칙:
        - 기본은 "user_name: message..." 형태로 첫 줄에 닉네임을 붙인다.
        - 만약 직전 저장된 라인의 user_name이 동일하면 닉네임을 생략한다.
        - 폭 초과 시 글자를 잘라 다음 줄로 넘긴다. (단어 단위 아님, 글자 단위 잘라도 허용)
        - 줄을 자를 때, 폭 검사 인덱스 탐색은 10글자 단위로 증가시키다가 초과 시
          직전 지점부터 1글자씩 전진하며 정확한 컷 위치를 찾는 방식.
        """
        fixed_text = f"[{user_name}]: "
        #fixed_text_len = len(fixed_text)
        fixed_text_px_len = self.get_text_width(fixed_text)

        new_message_len = len(new_message)
        new_message_px_len = self.get_text_width(new_message)

        remainning_text = new_message
        offset = 0 # 계산을 위해 계속 바뀌는 인덱스
        prev_offset = 0
        start_offset = 0 # 첫 문자열 인덱스
        optimize_flag = True # 트루이면 큰 간격으로 찾음
        first_process = True # 첫 줄이면 닉네임도 출력해야 함.
        total_px_len = 0

        while True:
            if first_process:
                total_px_len = fixed_text_px_len + self.get_text_width(new_message[start_offset:new_message_len])
            else:
                total_px_len = self.get_text_width(new_message[start_offset:new_message_len])
            
            if total_px_len <= self.window_w:
                offset = new_message_len
                self.texts.append({"user_name": user_name, "message": remainning_text[start_offset:offset]})
                break

            if optimize_flag == True:
                if offset + OPTIMIZED_OFFSET > new_message_len: # out of range 선체크
                    optimize_flag = False
                    continue
                prev_offset = offset
                offset += OPTIMIZED_OFFSET
                part_px_len = self.get_text_width(new_message[start_offset:offset])
                if first_process:
                    if fixed_text_px_len + part_px_len > self.window_w:
                        optimize_flag = False
                        offset = prev_offset # 넘지 않았던 인덱스로 돌려야함.
                else:
                    if part_px_len > self.window_w:
                        optimize_flag = False
                        offset = prev_offset
            else:
                prev_offset = offset
                offset += 1 # 하나씩 증가할 때는 범위 체크 문제없다.
                    
                part_px_len = self.get_text_width(new_message[start_offset:offset])
                total_px_len = 0
                if first_process:
                    total_px_len = fixed_text_px_len + part_px_len
                else:
                    total_px_len = part_px_len

                if total_px_len > self.window_w:            
                    self.texts.append({"user_name": user_name, "message": remainning_text[start_offset:prev_offset]})
                    offset = prev_offset
                    if offset == new_message_len: # 로직으로는 == 이 최대, 모든 처리가 끝났다면
                        break
                    

                elif total_px_len == self.window_w:
                    self.texts.append({"user_name": user_name, "message": remainning_text[start_offset:offset]})
                    if offset == new_message_len: # 로직으로는 == 이 최대, 모든 처리가 끝났다면
                        break

                first_process = False
                start_offset = offset
                optimize_flag = True

        if len(self.texts) - SHOW_MESSAGE_LINES > 0:
            self.scrollable_line = len(self.texts) - SHOW_MESSAGE_LINES
        else:
            self.scrollable_line = 0
            
        self.set_scroll_len()


    def set_scroll_len(self):
        self.scroll_h = self.bg_scroll_h - int(self.scroll_px * self.scrollable_line) # 스크롤 길이 세팅

    def set_show_start_index(self):
        if self.scrollable_line > 0:
            sum_px = 0
            start_index = 0
            for i in range(SHOW_MESSAGE_LINES + self.scrollable_line):
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
            self.show_end = self.show_start + SHOW_MESSAGE_LINES
        else:
            self.show_end = len(self.texts)

        prev_name = None
        draw_text_y = self.window_y
        for i in range(self.show_start, self.show_end):
            user_name = self.texts[i]["user_name"]
            message = self.texts[i]["message"]
            fixed_text = f"[{user_name}]: "
            if prev_name != user_name:
                text = fixed_text + message
                prev_name = user_name
            else:
                text = message
            text_surf = self.font.render(text, True, WHITE)
            surface.blit(text_surf, (self.window_x, draw_text_y))
            draw_text_y += self.line_height
