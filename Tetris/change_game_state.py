import pygame
from menu import Button
from tetris_game import TetrisGame
from network import NetworkClient
from define import *
from tetris_screen import TetrisScreen

class BaseState:
    def __init__(self, screen):
        self.screen = screen

    def init(self):
        pass

    def update(self, dt, events):
        return self

    def draw(self):
        pass

    # 리사이즈 훅(기본 구현: 화면만 교체)
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
        # 버튼은 상대 좌표 기반이므로 재생성으로 갱신
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
    def __init__(self, screen, players, dev_mode=False):
        super().__init__(screen)
        self.players = players
        self.dev_mode = dev_mode
        self.net = None
        self.network_ok = False

    def on_resize(self, w, h, screen):
        self.screen = screen
        # 특별한 레이아웃 없음

    def init(self):
        if not self.dev_mode:
            self.net = NetworkClient()
            self.network_ok = self.net.connect()
        else:
            self.network_ok = True

    def update(self, dt, events):
        if self.network_ok:
            if self.players == 1:
                return SinglePlayState(self.screen, dev_mode=self.dev_mode)
            return MultiPlayState(self.screen, self.players)
        return ConnectErrorState(self.screen)

    def draw(self):
        self.screen.fill((0,0,0))

class ConnectErrorState(BaseState):
    def init(self):
        self.back_btn = Button('Back', (0.4,0.6,0.2,0.1), self.screen)

    def on_resize(self, w, h, screen):
        self.screen = screen
        self.init()

    def update(self, dt, events):
        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit(); exit()
            if self.back_btn.clicked(ev):
                return SelectPlayState(self.screen)
        return self

    def draw(self):
        self.screen.fill((0,0,0))
        font = pygame.font.SysFont(None,36)
        msg = font.render('Connection Failed', True, (255,0,0))
        sw, sh = self.screen.get_size()
        rect = msg.get_rect(center=(sw//2,sh//2-50))
        self.screen.blit(msg,rect)
        self.back_btn.draw(self.screen)

class SinglePlayState(BaseState):
    def __init__(self, screen, dev_mode=False):
        super().__init__(screen)
        self.dev_mode = dev_mode
        self.tetris = None

    def init(self):
        ox, oy, scale = self._compute_layout()
        self.tetris = TetrisGame(self.screen, ox, oy, scale)
        self.tetris.init()  # TetrisGame.__init__에서 자동 init을 제거했으므로 여기서 1회 호출

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
        # 레이아웃/스크린 재구성(기존 기능 유지)
        self.init()

    def update(self, dt, events):
        # (멀티 플레이 자동 낙하나 입력처리 추가 원하면 여기서 호출)
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
