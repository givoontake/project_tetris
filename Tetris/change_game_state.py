import pygame
import queue
from menu import Button
from tetris_game import TetrisGame
from network import NetworkWorker
from define import *
from tetris_screen import TetrisScreen
from session import Session
from packet_type import S2C_LOGIN  # 로그인 타입 상수 사용


class BaseState:
    def __init__(self, screen):
        self.screen = screen

    def init(self): pass
    def update(self, dt, events): return self
    def draw(self): pass

    def on_resize(self, w, h, screen):
        self.screen = screen


class SelectModeState(BaseState):
    def init(self):
        self.buttons = [
            Button('Game', (0.4, 0.3, 0.2, 0.1), self.screen),
            Button('Developer', (0.4, 0.45, 0.2, 0.1), self.screen),
            Button('Quit', (0.4, 0.6, 0.2, 0.1), self.screen)
        ]

    def on_resize(self, w, h, screen):
        self.screen = screen
        self.init()

    def update(self, dt, events):
        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit(); exit()
            if self.buttons[0].clicked(ev):
                return SelectPlayState(self.screen, dev_mode=False)
            if self.buttons[1].clicked(ev):
                return SelectPlayState(self.screen, dev_mode=True)
            if self.buttons[2].clicked(ev):
                pygame.quit(); exit()
        return self

    def draw(self):
        self.screen.fill((0,0,0))
        for btn in self.buttons:
            btn.draw(self.screen)


class SelectPlayState(BaseState):
    def __init__(self, screen, dev_mode=False):
        super().__init__(screen)
        self.dev_mode = dev_mode

    def init(self):
        self.buttons = [
            Button('Single', (0.4,0.3,0.2,0.1), self.screen),
            Button('2 Player', (0.4,0.45,0.2,0.1), self.screen),
            Button('5 Player', (0.4,0.6,0.2,0.1), self.screen),
            Button('Back', (0.9,0.02,0.08,0.05), self.screen)
        ]

    def on_resize(self, w, h, screen):
        self.screen = screen
        self.init()

    def update(self, dt, events):
        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit(); exit()
            if self.buttons[0].clicked(ev):
                return ConnectState(self.screen, players=1, dev_mode=self.dev_mode)
            if self.buttons[1].clicked(ev):
                return ConnectState(self.screen, players=2, dev_mode=self.dev_mode)
            if self.buttons[2].clicked(ev):
                return ConnectState(self.screen, players=5, dev_mode=self.dev_mode)
            if self.buttons[3].clicked(ev):
                return SelectModeState(self.screen)
        return self

    def draw(self):
        self.screen.fill((0,0,0))
        for btn in self.buttons:
            btn.draw(self.screen)


class ConnectState(BaseState):
    """
    ConnectState + ConnectErrorState 통합
    - is_connect == False: "Connection Failed" 1초 표시 후 종료
    - is_connect == True : "Login Success!" 1초 표시 후 SelectPlayState로 전환
    """
    def __init__(self, screen, is_connect, dev_mode=False):
        super().__init__(screen)
        self.is_connect = bool(is_connect)
        self.dev_mode = dev_mode
        self._start_ms = None  # pygame.time.get_ticks()

    def on_resize(self, w, h, screen):
        self.screen = screen

    def init(self):
        self._start_ms = pygame.time.get_ticks()

    def update(self, dt, events):
        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit(); exit()

        if self._start_ms is None:
            return self

        elapsed = pygame.time.get_ticks() - self._start_ms

        # 실패: 1초 뒤 종료
        if not self.is_connect:
            if elapsed >= 1000:
                pygame.quit(); exit()
            return self

        # 성공: 1초 뒤 다음 스테이트로
        if elapsed >= 1000:
            return SelectPlayState(self.screen, dev_mode=self.dev_mode)

        return self

    def draw(self):
        self.screen.fill((0, 0, 0))
        sw, sh = self.screen.get_size()
        font = pygame.font.SysFont(None, 48)

        if self.is_connect:
            msg = font.render('Login Success!', True, (0, 200, 0))
        else:
            msg = font.render('Connection Failed', True, (255, 0, 0))

        rect = msg.get_rect(center=(sw // 2, sh // 2))
        self.screen.blit(msg, rect)


class SinglePlayState(BaseState):
    def __init__(self, screen, dev_mode=False):
        super().__init__(screen)
        self.dev_mode = dev_mode
        self.tetris = None

    def init(self):
        ox, oy, scale = self._compute_layout()
        self.tetris = TetrisGame(self.screen, ox, oy, scale)
        self.tetris.init()

    def on_resize(self, w, h, screen):
        self.screen = screen
        ox, oy, scale = self._compute_layout()
        if self.tetris:
            self.tetris.set_layout(ox, oy, scale)

    def update(self, dt, events):
        self.tetris.update(dt, events)
        if self.tetris.game_over:
            return SelectModeState(self.screen)
        return self

    def draw(self):
        self.tetris.draw()

    def _compute_layout(self):
        sw, sh = self.screen.get_size()
        design_w = GRID_COLUMNS * CELL_SIZE + PREVIEW_CELL_SIZE + BLANK_WIDTH
        design_h = GRID_ROWS * CELL_SIZE + BLANK_HEIGHT
        scale = min(sw/design_w, sh/design_h)
        offset_x = (sw - design_w*scale)/2
        offset_y = (sh - design_h*scale)/2
        return (offset_x, offset_y, scale)


class MultiPlayState(BaseState):
    def __init__(self, screen, players):
        super().__init__(screen)
        self.players = players
        self.screens = []

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

    def update(self, dt, events):
        return self

    def draw(self):
        self.screen.fill((0,0,0))
        for ts in self.screens:
            ts.draw()

    def _compute_layout(self):
        sw, sh = self.screen.get_size()
        region_w = GRID_COLUMNS * CELL_SIZE + PREVIEW_CELL_SIZE + BLANK_WIDTH
        region_h = GRID_ROWS * CELL_SIZE + BLANK_HEIGHT
        design_w = (2 if self.players>2 else 1) * region_w
        design_h = region_h
        scale = min(sw/design_w, sh/design_h)
        offset_x = (sw - design_w*scale)/2
        offset_y = (sh - design_h*scale)/2
        return (offset_x, offset_y, scale)

    def _regions_2(self, layout):
        ox, oy, scale = layout
        region_w = (GRID_COLUMNS * CELL_SIZE + PREVIEW_CELL_SIZE + BLANK_WIDTH) * scale
        return [(ox + i*region_w, oy, scale) for i in range(2)]

    def _regions_5(self, layout):
        ox, oy, scale = layout
        region_w = (GRID_COLUMNS * CELL_SIZE + PREVIEW_CELL_SIZE + BLANK_WIDTH) * scale
        region_h = GRID_ROWS * CELL_SIZE + BLANK_HEIGHT
        main = (ox, oy, scale)
        small = scale/2
        regions = [main]
        base_x = ox + region_w
        for idx in range(4):
            col, row = idx%2, idx//2
            regions.append((base_x + col*(region_w/2), oy + row*(region_h*small), small))
        return regions
