# change_game_state.py
import time
import pygame
from menu import *
from tetris_game import TetrisGame
from define import *
from tetris_screen import TetrisScreen
from session import Session
from define_format import *
from packet_type import *
from typing import Optional

class BaseState:
    def __init__(self, screen):
        self.screen = screen

    def init(self): 
        pass

    def update(self, events, data=None):
        """dt 없이 이벤트/데이터만 받아 상태 갱신"""
        return self

    def draw(self):
        pass

    def on_resize(self, w, h, screen):
        self.screen = screen

class ConnectState(BaseState):
    """
    - is_connect == False: "Connection Failed" 1초 표시 후 종료
    - is_connect == True : "Login Success!" 1초 표시 후 SelectPlayState로 전환
    """
    def __init__(self, screen, is_connect, dev_mode=False, net_worker=None):
        super().__init__(screen)
        self.is_connect = bool(is_connect)
        self.dev_mode = dev_mode
        self._start_ms = None  # pygame.time.get_ticks()
        self.net_worker = net_worker  # ← 보관
        self.screen = screen

    def on_resize(self, w, h, screen):
        self.screen = screen

    def init(self):
        self._start_ms = pygame.time.get_ticks()

    def update(self, events, data=None):
        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit(); raise SystemExit

        if self._start_ms is None:
            return self

        elapsed = pygame.time.get_ticks() - self._start_ms

        if not self.is_connect:
            if elapsed >= 1000:
                pygame.quit(); raise SystemExit
            return self

        if elapsed >= 1000:
            return LoginState(self.screen, net_worker=self.net_worker)

        return self

    def draw(self):
        self.screen.fill((0, 0, 0))
        sw, sh = self.screen.get_size()
        font = pygame.font.Font("resource/dodamdodam.ttf", 48)
        if self.is_connect:
            msg = font.render('Connected!', True, (0, 200, 0))
        else:
            msg = font.render('Connection Failed', True, (255, 0, 0))

        rect = msg.get_rect(center=(sw // 2, sh // 2))
        self.screen.blit(msg, rect)

class LoginState(BaseState):
    def __init__(self, screen, net_worker=None):
        super().__init__(screen)
        self.net_worker = net_worker
        self.title_font = pygame.font.Font("resource/dodamdodam.ttf", 36)
        #self.label_font = pygame.font.Font("resource/dodamdodam.ttf", 28)

        # 런타임 배치 요소
        self.id_box: Optional[InputBox] = None
        self.pw_box: Optional[InputBox] = None
        self.btn_login: Optional[Button] = None
        self._layout = None  # (id_rect, pw_rect, btn_rect)

    def init(self):
        self.set_layout()

    def set_layout(self):
        sw, sh = self.screen.get_size()
        # 중앙 배치
        input_box_w, input_box_h = 360, 42
        gap_y = 64
        center_x = (sw - input_box_w) // 2
        center_y = (sh - (input_box_h * 2 + gap_y + 46)) // 2  # 46은 버튼 높이

        id_rect = pygame.Rect(center_x, center_y, input_box_w, input_box_h)
        pw_rect = pygame.Rect(center_x, center_y + input_box_h + 20, input_box_w, input_box_h)
        btn_w, btn_h = 160, 46
        btn_rect = pygame.Rect((sw - btn_w) // 2, pw_rect.bottom + 28, btn_w, btn_h)

        self.id_box = InputBox(id_rect.x, id_rect.y, id_rect.w, id_rect.h, "아이디")
        self.pw_box = InputBox(pw_rect.x, pw_rect.y, pw_rect.w, pw_rect.h, "비밀번호", is_password=True)
        self.btn_login = Button(self.screen, (btn_rect.x, btn_rect.y, btn_rect.w, btn_rect.h), text="로그인")

        self._layout = (id_rect, pw_rect, btn_rect)

    def on_resize(self, w, h, screen):
        self.screen = screen
        self.set_layout()

    def send_login(self):
        user_id = self.id_box.text
        user_pw = self.pw_box.text
        data = {"size": 2+1+MAX_USER_ID+MAX_USER_PASSWORD,"type": C2S_LOGIN, "user_id": user_id, "user_password": user_pw}
        self.net_worker.send_packet(data)

    def update(self, events, data=None):

        if data != None:
            d = data
            # 나중에 다음 상태로 넘기는 코드 작성

        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit(); raise SystemExit

            self.id_box.handle_event(ev)
            self.pw_box.handle_event(ev)

            if ev.type == pygame.KEYDOWN and ev.key == pygame.K_RETURN: #enter 누르기
                self.send_login()

            if self.btn_login.clicked(ev): # 마우스 클릭
                self.send_login()

        
        dt_ms = 16  # 간단한 커서 점멸용(정확한 dt 필요 없으므로 고정)
        self.id_box.update(dt_ms)
        self.pw_box.update(dt_ms)

        return self

    def draw(self):
        self.screen.fill((18, 18, 18))
        sw, _ = self.screen.get_size()

        # 제목
        title = self.title_font.render("로그인", True, (255, 255, 255))
        self.screen.blit(title, title.get_rect(center=(sw // 2, 90)))

        # 라벨
        # id_label = self.label_font.render("아이디", True, (255, 255, 255))
        # pw_label = self.label_font.render("비밀번호", True, (255, 255, 255))
        # id_rect, pw_rect, _ = self._layout
        # self.screen.blit(id_label, (id_rect.x, id_rect.y - 24))
        # self.screen.blit(pw_label, (pw_rect.x, pw_rect.y - 24))

        # 입력창 + 버튼
        self.id_box.draw(self.screen)
        self.pw_box.draw(self.screen)
        #self.btn_login.draw(self.screen)


class SelectModeState(BaseState):
    def init(self):
        self.buttons = [
            Button('Game',      (0.4, 0.3,  0.2, 0.1), self.screen),
            Button('Developer', (0.4, 0.45, 0.2, 0.1), self.screen),
            Button('Quit',      (0.4, 0.6,  0.2, 0.1), self.screen),
        ]

    def on_resize(self, w, h, screen):
        self.screen = screen
        self.init()

    def update(self, events, data=None):
        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit(); raise SystemExit
            if self.buttons[0].clicked(ev):
                return SelectPlayState(self.screen, dev_mode=False)
            if self.buttons[1].clicked(ev):
                return SelectPlayState(self.screen, dev_mode=True)
            if self.buttons[2].clicked(ev):
                pygame.quit(); raise SystemExit
        return self

    def draw(self):
        self.screen.fill((0, 0, 0))
        for b in self.buttons:
            b.draw(self.screen)


class SelectPlayState(BaseState):
    def __init__(self, screen, dev_mode=False):
        super().__init__(screen)
        self.dev_mode = dev_mode

    def init(self):
        self.buttons = [
            Button('Single',   (0.4, 0.3,  0.2, 0.1), self.screen),
            Button('2 Player', (0.4, 0.45, 0.2, 0.1), self.screen),
            Button('5 Player', (0.4, 0.6,  0.2, 0.1), self.screen),
            Button('Back',     (0.9, 0.02, 0.08, 0.05), self.screen),
        ]

    def on_resize(self, w, h, screen):
        self.screen = screen
        self.init()

    def update(self, events, data=None):
        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit(); raise SystemExit
            if self.buttons[0].clicked(ev):
                # Connect 성공/실패는 GameLoop에서 판단해 is_connect로 넘겨줄 수도 있음
                return ConnectState(self.screen, is_connect=True,  dev_mode=self.dev_mode)
            if self.buttons[1].clicked(ev):
                return ConnectState(self.screen, is_connect=True,  dev_mode=self.dev_mode)
            if self.buttons[2].clicked(ev):
                return ConnectState(self.screen, is_connect=True,  dev_mode=self.dev_mode)
            if self.buttons[3].clicked(ev):
                return SelectModeState(self.screen)
        return self

    def draw(self):
        self.screen.fill((0, 0, 0))
        for b in self.buttons:
            b.draw(self.screen)

class SinglePlayState(BaseState):
    def __init__(self, screen, dev_mode=False):
        super().__init__(screen)
        self.dev_mode = dev_mode
        self.tetris: TetrisGame | None = None
        self._prev_time = None  # perf_counter 값 저장(상태 내부에서만 dt 계산용)

    def init(self):
        ox, oy, scale = self._compute_layout()
        self.tetris = TetrisGame(self.screen, ox, oy, scale)
        self.tetris.init()
        self._prev_time = time.perf_counter()

    def on_resize(self, w, h, screen):
        self.screen = screen
        ox, oy, scale = self._compute_layout()
        if self.tetris:
            self.tetris.set_layout(ox, oy, scale)

    def update(self, events, data=None):
        # 상태 인터페이스는 dt 없이 유지하면서, 내부에서만 dt(ms) 계산
        if self._prev_time is None:
            self._prev_time = time.perf_counter()
        now = time.perf_counter()
        dt_ms = (now - self._prev_time) * 1000.0
        self._prev_time = now

        # TetrisGame은 기존 시그니처 유지 (dt 필요)
        self.tetris.update(dt_ms, events)

        # 필요 시 data 처리
        # if data: ...

        if self.tetris.game_over:
            return SelectModeState(self.screen)
        return self

    def draw(self):
        self.tetris.draw()

    def _compute_layout(self):
        sw, sh = self.screen.get_size()
        design_w = GRID_COLUMNS * CELL_SIZE + PREVIEW_CELL_SIZE + BLANK_WIDTH
        design_h = GRID_ROWS * CELL_SIZE + BLANK_HEIGHT
        scale = min(sw / design_w, sh / design_h)
        offset_x = (sw - design_w * scale) / 2
        offset_y = (sh - design_h * scale) / 2
        return (offset_x, offset_y, scale)


class MultiPlayState(BaseState):
    def __init__(self, screen, players):
        super().__init__(screen)
        self.players = players
        self.screens: list[TetrisScreen] = []

    def init(self):
        self.screens.clear()
        layout = self._compute_layout()
        regions = self._regions_2(layout) if self.players == 2 else self._regions_5(layout)
        for ox, oy, scale in regions:
            self.screens.append(
                TetrisScreen(self.screen, ox, oy, scale, TEST_TETROMINOS.copy())
            )

    def on_resize(self, w, h, screen):
        self.screen = screen
        self.init()

    def update(self, events, data=None):
        # 멀티 보드 렌더만 담당. 로직이 필요하면 여기서 처리
        return self

    def draw(self):
        self.screen.fill((0, 0, 0))
        for ts in self.screens:
            ts.draw()

    def _compute_layout(self):
        sw, sh = self.screen.get_size()
        region_w = GRID_COLUMNS * CELL_SIZE + PREVIEW_CELL_SIZE + BLANK_WIDTH
        region_h = GRID_ROWS * CELL_SIZE + BLANK_HEIGHT
        design_w = (2 if self.players > 2 else 1) * region_w
        design_h = region_h
        scale = min(sw / design_w, sh / design_h)
        offset_x = (sw - design_w * scale) / 2
        offset_y = (sh - design_h * scale) / 2
        return (offset_x, offset_y, scale)

    def _regions_2(self, layout):
        ox, oy, scale = layout
        region_w = (GRID_COLUMNS * CELL_SIZE + PREVIEW_CELL_SIZE + BLANK_WIDTH) * scale
        return [(ox + i * region_w, oy, scale) for i in range(2)]

    def _regions_5(self, layout):
        ox, oy, scale = layout
        region_w = (GRID_COLUMNS * CELL_SIZE + PREVIEW_CELL_SIZE + BLANK_WIDTH) * scale
        region_h = GRID_ROWS * CELL_SIZE + BLANK_HEIGHT
        main = (ox, oy, scale)
        small = scale / 2
        regions = [main]
        base_x = ox + region_w
        for idx in range(4):
            col, row = idx % 2, idx // 2
            regions.append(
                (base_x + col * (region_w / 2), oy + row * (region_h * small), small)
            )
        return regions
