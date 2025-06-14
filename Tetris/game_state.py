# game_state.py
import pygame
from collections import deque
from menu import Button
from tetris_screen import TetrisScreen
from define import*

class GameState:
    def __init__(self, screen):
        self.screen        = screen
        self.state         = 'select_mode'
        self.players       = 1
        self.dev_mode      = False
        self.network_ok    = False
        self.back_btn      = None

        # 플레이어별 TetrisScreen 캐시
        self.tetris_screens = {}

        # 서버에서 갱신할 프리뷰 큐 (max 10개)
        self.preview_queue = TEST_TETROMINOS

    def select_mode(self):
        btn_game = Button('Game',      (0.4,0.3,  0.2,0.1), self.screen)
        btn_dev  = Button('Developer', (0.4,0.45, 0.2,0.1), self.screen)
        btn_quit = Button('Quit',      (0.4,0.6,  0.2,0.1), self.screen)

        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                return 'quit'
            if ev.type == pygame.VIDEORESIZE:
                self.screen = pygame.display.set_mode(
                    (ev.w, ev.h), pygame.RESIZABLE)
                return 'select_mode'
            if btn_game.clicked(ev):
                self.dev_mode = False
                return 'select_play'
            if btn_dev.clicked(ev):
                self.dev_mode = True
                return 'select_play'
            if btn_quit.clicked(ev):
                return 'quit'

        self.screen.fill((0,0,0))
        for b in (btn_game, btn_dev, btn_quit):
            b.draw(self.screen)
        pygame.display.flip()
        return 'select_mode'

    def select_play(self):
        btn_single = Button('Single',   (0.4,0.3,  0.2,0.1), self.screen)
        btn_two    = Button('2 Player', (0.4,0.45, 0.2,0.1), self.screen)
        btn_five   = Button('5 Player', (0.4,0.6,  0.2,0.1), self.screen)
        btn_back   = Button('Back',     (0.9,0.02, 0.08,0.05), self.screen)

        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                return 'quit'
            if ev.type == pygame.VIDEORESIZE:
                self.screen = pygame.display.set_mode(
                    (ev.w, ev.h), pygame.RESIZABLE)
                return 'select_play'
            if btn_single.clicked(ev):
                self.players = 1
                return 'check_connect'
            if btn_two.clicked(ev):
                self.players = 2
                return 'check_connect'
            if btn_five.clicked(ev):
                self.players = 5
                return 'check_connect'
            if btn_back.clicked(ev):
                return 'select_mode'

        self.screen.fill((0,0,0))
        for b in (btn_single, btn_two, btn_five, btn_back):
            b.draw(self.screen)
        pygame.display.flip()
        return 'select_play'

    def connect_error(self):
        if not self.back_btn:
            self.back_btn = Button('Back', (0.4, 0.6, 0.2, 0.1), self.screen)

        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                return 'quit'
            if ev.type == pygame.VIDEORESIZE:
                self.screen = pygame.display.set_mode(
                    (ev.w, ev.h), pygame.RESIZABLE)
                self.back_btn = None
                return 'connect_error'
            if self.back_btn.clicked(ev):
                self.back_btn = None
                return 'select_play'

        self.screen.fill((0,0,0))
        font = pygame.font.SysFont(None, 36)
        msg  = font.render('Connection Failed', True, (255,0,0))
        sw, sh = self.screen.get_size()
        rect   = msg.get_rect(center=(sw//2, sh//2 - 50))
        self.screen.blit(msg, rect)
        self.back_btn.draw(self.screen)
        pygame.display.flip()
        return 'connect_error'

    def single_play(self):
        # 이벤트 처리
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                return 'quit'
            if ev.type == pygame.VIDEORESIZE:
                self.screen = pygame.display.set_mode(
                    (ev.w, ev.h), pygame.RESIZABLE)
                return 'single_play'

        # 스케일·오프셋 계산
        sw, sh    = self.screen.get_size()
        design_w  = GRID_COLUMNS * CELL_SIZE + PREVIEW_CELL_SIZE + BLANK_WIDTH
        design_h  = GRID_ROWS    * CELL_SIZE + BLANK_HEIGHT
        scale     = min(sw / design_w, sh / design_h)
        offset_x  = (sw - design_w * scale) / 2
        offset_y  = (sh - design_h * scale) / 2

        # TetrisScreen 생성 (최초 한 번)
        if 'single' not in self.tetris_screens:
            self.tetris_screens['single'] = TetrisScreen(
                self.screen,
                offset_x, offset_y,
                scale,
                self.preview_queue
            )

        # 그리기
        self.screen.fill((0,0,0))
        self.tetris_screens['single'].draw()
        pygame.display.flip()
        return 'single_play'

    def multi_play2(self):
        # 이벤트 처리
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                return 'quit'
            if ev.type == pygame.VIDEORESIZE:
                self.screen = pygame.display.set_mode(
                    (ev.w, ev.h), pygame.RESIZABLE)
                return 'multi_play2'

        # 스케일·오프셋 계산
        sw, sh     = self.screen.get_size()
        region_w   = GRID_COLUMNS * CELL_SIZE + PREVIEW_CELL_SIZE + BLANK_WIDTH
        region_h   = GRID_ROWS    * CELL_SIZE + BLANK_HEIGHT
        design_w   = 2 * region_w
        design_h   =     region_h
        scale_main = min(sw / design_w, sh / design_h)
        offset_x   = (sw - design_w * scale_main) / 2
        offset_y   = (sh - design_h * scale_main) / 2

        # TetrisScreen 생성 (최초 한 번)
        if 'multi2' not in self.tetris_screens:
            screens = []
            for i in range(2):
                ox = offset_x + i * region_w * scale_main
                screens.append(
                    TetrisScreen(
                        self.screen,
                        ox, offset_y,
                        scale_main,
                        self.preview_queue
                    )
                )
            self.tetris_screens['multi2'] = screens

        # 그리기
        self.screen.fill((0,0,0))
        for ts in self.tetris_screens['multi2']:
            ts.draw()
        pygame.display.flip()
        return 'multi_play2'

    def multi_play5(self):
        # 이벤트 처리
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                return 'quit'
            if ev.type == pygame.VIDEORESIZE:
                self.screen = pygame.display.set_mode(
                    (ev.w, ev.h), pygame.RESIZABLE)
                return 'multi_play5'

        # 스케일·오프셋 계산
        sw, sh     = self.screen.get_size()
        region_w   = GRID_COLUMNS * CELL_SIZE + PREVIEW_CELL_SIZE + BLANK_WIDTH
        region_h   = GRID_ROWS    * CELL_SIZE + BLANK_HEIGHT
        design_w   = 2 * region_w
        design_h   =     region_h
        scale_main = min(sw / design_w, sh / design_h)
        scale_opp  = scale_main / 2
        offset_x   = (sw - 2 * region_w * scale_main) / 2
        offset_y   = (sh -     design_h * scale_main) / 2

        # TetrisScreen 생성 (최초 한 번)
        if 'multi5' not in self.tetris_screens:
            screens = []
            # 왼쪽 메인
            screens.append(
                TetrisScreen(
                    self.screen,
                    offset_x, offset_y,
                    scale_main,
                    self.preview_queue
                )
            )
            # 오른쪽 2×2
            base_x = offset_x + region_w * scale_main
            for idx in range(4):
                col = idx % 2
                row = idx // 2
                ox = base_x + col * region_w * scale_opp
                oy = offset_y + row * region_h * scale_opp
                screens.append(
                    TetrisScreen(
                        self.screen,
                        ox, oy,
                        scale_opp,
                        self.preview_queue
                    )
                )
            self.tetris_screens['multi5'] = screens

        # 그리기
        self.screen.fill((0,0,0))
        for ts in self.tetris_screens['multi5']:
            ts.draw()
        pygame.display.flip()
        return 'multi_play5'
