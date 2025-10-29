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
from asset_manager import *
from typing import Optional
from network import NetworkWorker

class BaseState:
    def __init__(self, screen):
        self.screen = screen

    def init(self): 
        pass

    def update(self, dt_ms, events, data=None):
        """메인 루프에서 계산된 dt(ms)를 전달받아 갱신."""
        return self

    def draw(self):
        pass

    def on_resize(self, w, h, screen):
        self.screen = screen


class ConnectState(BaseState):
    """
    - is_connect == False: "Connection Failed" 1초 표시 후 종료
    - is_connect == True : "Connected!" 1초 표시 후 LoginState로 전환
    """
    def __init__(self, screen, is_connect, dev_mode=False, net_worker: Optional[NetworkWorker] = None):
        super().__init__(screen)
        self.is_connect = bool(is_connect)
        self.dev_mode = dev_mode
        self._start_ms = None  # pygame.time.get_ticks()
        self.net_worker = net_worker
        self.screen = screen

    def on_resize(self, w, h, screen):
        self.screen = screen

    def init(self):
        self._start_ms = pygame.time.get_ticks()

    def update(self, dt_ms, events, data=None):
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


class LoginState:
    def __init__(self, screen, net_worker: Optional[NetworkWorker] = None):
        self.screen = screen
        self.net_worker = net_worker
        self.title_font = pygame.font.Font("resource/dodamdodam.ttf", 36)

        self.id_box: Optional[InputBox] = None
        self.pw_box: Optional[InputBox] = None
        self.btn_login: Optional[Button] = None

    def init(self):
        self.set_layout()

    def set_layout(self):
        sw, sh = self.screen.get_size()
        input_box_w, input_box_h = 360, 42
        center_x = (sw - input_box_w) // 2
        center_y = (sh - (input_box_h * 2 + 64 + 46)) // 2

        id_rect = pygame.Rect(center_x, center_y, input_box_w, input_box_h)
        pw_rect = pygame.Rect(center_x, center_y + input_box_h + 20, input_box_w, input_box_h)
        btn_rect = pygame.Rect((sw - 300) // 2, pw_rect.bottom + 28, 300, 100)

        self.id_box = InputBox(id_rect.x, id_rect.y, id_rect.w, id_rect.h, "아이디")
        self.pw_box = InputBox(pw_rect.x, pw_rect.y, pw_rect.w, pw_rect.h, "비밀번호", is_password=True)

        # 버튼은 폰트 전달 없이 생성됨
        self.btn_login = Button(
            btn_type=BUTTON_LOGIN,
            x=btn_rect.x,
            y=btn_rect.y,
            w=btn_rect.w,
            h=btn_rect.h,
            text="로그인"
        )

    def send_login(self):
        user_id = self.id_box.text
        user_pw = self.pw_box.text
        data = {
            "size": 2 + 1 + MAX_USER_ID + MAX_USER_PASSWORD,
            "type": C2S_LOGIN,
            "user_id": user_id,
            "user_password": user_pw
        }
        try:
            self.net_worker.send_packet(self.net_worker._pm.dic_to_bytes(data))
        except Exception as e:
            print("[LoginState] send_login error:", e)

    def update(self, dt_ms, events, data: Optional[dict] = None):
        if data:
            print(", ".join(f"{k}: {v}" for k, v in data.items()))
            if data.get("id") == -1:
                pass
                # 아이디 혹은 비밀번호를 다시 입력하라는 창 추가 필요
            
            else:
                # 내 세션의 아이디를 설정하는 코드 필요
                return SelectModeState(self.screen)
        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit(); raise SystemExit

            self.id_box.handle_event(ev)
            self.pw_box.handle_event(ev)

            if ev.type == pygame.KEYDOWN and ev.key == pygame.K_RETURN:
                self.send_login()

            if self.btn_login.handle_event(ev):
                self.send_login()

        self.id_box.update(dt_ms)
        self.pw_box.update(dt_ms)
        return self

    def draw(self):
        self.screen.fill((18, 18, 18))
        sw, _ = self.screen.get_size()
        title = self.title_font.render("로그인", True, (255, 255, 255))
        self.screen.blit(title, title.get_rect(center=(sw // 2, 90)))
        self.id_box.draw(self.screen)
        self.pw_box.draw(self.screen)
        self.btn_login.draw(self.screen)

class SelectModeState(BaseState):
    def init(self):
        self.buttons = [
            Button(self.screen, (0.4, 0.3,  0.2, 0.1), "Game"),
            Button(self.screen, (0.4, 0.45, 0.2, 0.1), "Developer"),
            Button(self.screen, (0.4, 0.6,  0.2, 0.1), "Quit"),
        ]

    def on_resize(self, w, h, screen):
        self.screen = screen
        self.init()

    def update(self, dt_ms, events, data=None):
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
            Button(self.screen, (0.4, 0.3,  0.2, 0.1), "Single"),
            Button(self.screen, (0.4, 0.45, 0.2, 0.1), "2 Player"),
            Button(self.screen, (0.4, 0.6,  0.2, 0.1), "5 Player"),
            Button(self.screen, (0.9, 0.02, 0.08, 0.05), "Back"),
        ]

    def on_resize(self, w, h, screen):
        self.screen = screen
        self.init()

    def update(self, dt_ms, events, data=None):
        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit(); raise SystemExit
            if self.buttons[0].clicked(ev):
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

    def init(self):
        ox, oy, scale = self._compute_layout()
        self.tetris = TetrisGame(self.screen, ox, oy, scale)
        self.tetris.init()

    def on_resize(self, w, h, screen):
        self.screen = screen
        ox, oy, scale = self._compute_layout()
        if self.tetris:
            self.tetris.set_layout(ox, oy, scale)

    def update(self, dt_ms, events, data=None):
        # ✅ 메인 루프에서 받은 dt_ms를 그대로 전달
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

    def update(self, dt_ms, events, data=None):
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
