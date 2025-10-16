import pygame
import queue
from menu import Button
from tetris_game import TetrisGame
from network import NetworkClient
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
    - 서버 연결 성공 후에도 로그인 패킷 수신 전까지 검은 화면 유지
    - 로그인 패킷 도착 시: 세션 id만 세팅(메인 스레드에서 직접) + "Login Success!" 1초 표시 후 SelectPlay
    - handle_packet은 그대로 호출(타 패킷 처리 용도)
    """
    def __init__(self, screen, players=1, dev_mode=False):
        super().__init__(screen)
        self.players = players
        self.dev_mode = dev_mode
        self.net = None
        self.network_ok = False
        self.session = None

        self._login_done = False
        self._login_msg_start_ms = None  # pygame.time.get_ticks()

    def on_resize(self, w, h, screen):
        self.screen = screen

    def init(self):
        if not self.dev_mode:
            self.session = Session.shared()
            self.session.reset()
            self.net = NetworkClient(self.session)
            self.network_ok = self.net.connect()
        else:
            self.network_ok = True

    def _drain_packets(self, max_per_frame=64):
        """
        수신 스레드가 채워둔 '완성 패킷 큐'를 메인 스레드에서 드레인.
        - S2C_LOGIN은 여기서 직접 파싱해 세션 id 세팅 + 디버그 출력
        - 그 외 패킷은 기존대로 pm.handle_packet(pkt)을 호출(호환성 유지)
        """
        if not self.network_ok or not self.net:
            return

        pm = self.net.get_packet_manager()
        q = self.net.get_packet_queue()

        processed = 0
        while processed < max_per_frame:
            try:
                pkt = q.get_nowait()
            except queue.Empty:
                break

            processed += 1

            # ---- 로그인 직접 처리 ----
            if len(pkt) >= 7 and pkt[2] == S2C_LOGIN:
                size_val = int.from_bytes(pkt[0:2], "little", signed=True)
                type_val = int.from_bytes(pkt[2:3], "little", signed=True)
                id_val   = int.from_bytes(pkt[3:7], "little", signed=True)
                print(f"[S2C_LOGIN][MAIN] size={size_val} type={type_val} id={id_val}")
                print(f"[S2C_LOGIN][HEX ] {pkt.hex(' ')}")
                # 세션 id만 세팅
                if self.session:
                    self.session.set_id(id_val)
                # 성공 표시용 타임스탬프
                if not self._login_done:
                    self._login_done = True
                    self._login_msg_start_ms = pygame.time.get_ticks()

            # ---- 나머지는 기존 handle_packet에 위임 ----
            try:
                pm.handle_packet(pkt)
            except Exception as e:
                print(f"[ConnectState] handle_packet error: {e}")

    def update(self, dt, events):
        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit(); exit()

        if not self.network_ok:
            return ConnectErrorState(self.screen)

        # 매 프레임 큐 드레인
        self._drain_packets()

        # 로그인 전: 검은 화면 유지
        if not self._login_done:
            return self

        # 로그인 성공 후 1초간 메시지 표시
        now = pygame.time.get_ticks()
        if self._login_msg_start_ms is not None and (now - self._login_msg_start_ms) < 1000:
            return self

        # 1초 경과 → 인원 선택 화면
        return SelectPlayState(self.screen, dev_mode=self.dev_mode)

    def draw(self):
        self.screen.fill((0,0,0))
        if self._login_done and self._login_msg_start_ms is not None:
            elapsed = pygame.time.get_ticks() - self._login_msg_start_ms
            if elapsed < 1000:
                font = pygame.font.SysFont(None, 48)
                msg  = font.render('Login Success!', True, (0, 200, 0))
                sw, sh = self.screen.get_size()
                rect   = msg.get_rect(center=(sw//2, sh//2))
                self.screen.blit(msg, rect)


class ConnectErrorState(BaseState):
    def init(self): pass
    def on_resize(self, w, h, screen): self.screen = screen

    def update(self, dt, events):
        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit(); exit()
        return self

    def draw(self):
        self.screen.fill((0,0,0))
        font = pygame.font.SysFont(None,36)
        msg  = font.render('Connection Failed', True, (255,0,0))
        sw, sh = self.screen.get_size()
        rect   = msg.get_rect(center=(sw//2, sh//2 - 50))
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
