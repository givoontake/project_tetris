import random
import pygame
from collections import deque

from define import *
from game_object import Tetromino
from tetris_screen import TetrisScreen

class TetrisGame:
    def __init__(self, screen, offset_x, offset_y, scale=1.0):
        self.screen   = screen
        self.offset_x = offset_x
        self.offset_y = offset_y
        self.scale    = scale
        # 이 시점에는 초기화하지 않음(상태에서 1회만 호출)
        self.grid = None
        self.next_shapes = None
        self.score = 0
        self.game_over = False
        self.drop_timer = 0
        self.drop_interval = 500
        self.tetris_screen = None
        self.current_tetromino = None

    def set_layout(self, offset_x, offset_y, scale):
        """리사이즈 시 레이아웃/스케일 갱신(코어 게임 상태 유지)."""
        self.offset_x = offset_x
        self.offset_y = offset_y
        self.scale = scale

        # 프리뷰 유지한 채 UI만 재생성
        preview_deque = deque()
        if self.tetris_screen and self.tetris_screen.preview_queue:
            preview_deque = deque(list(self.tetris_screen.preview_queue))

        self.tetris_screen = TetrisScreen(
            self.screen,
            self.offset_x, self.offset_y,
            self.scale,
            preview_deque
        )

        # 현재 조각의 화면 오프셋/스케일 갱신
        if self.current_tetromino:
            self.current_tetromino.offset_x = self.offset_x
            self.current_tetromino.offset_y = self.offset_y
            self.current_tetromino.scale = self.scale
            self.current_tetromino.cell = CELL_SIZE * self.scale

    def init(self):
        # 그리드 초기화
        self.grid = [[None] * GRID_COLUMNS for _ in range(GRID_ROWS)]
        # 7-bag 준비
        bag = list(SHAPES.keys()); random.shuffle(bag)
        self.next_shapes = deque(bag)

        self.score = 0
        self.game_over = False

        # **떨어짐 타이머** 설정 (밀리초 단위)
        self.drop_timer = 0
        self.drop_interval = 500  # 0.5초마다 한 칸

        # 프리뷰 초기화
        init_prev = list(self.next_shapes)[:10]
        self.tetris_screen = TetrisScreen(
            self.screen,
            self.offset_x, self.offset_y,
            self.scale,
            deque(init_prev)
        )

        self.spawn_piece()

    def spawn_piece(self) -> None:
        if not self.next_shapes:
            bag = list(SHAPES.keys()); random.shuffle(bag)
            self.next_shapes.extend(bag)

        shape_key = self.next_shapes.popleft()

        # 프리뷰 갱신
        self.tetris_screen.preview_queue.clear()
        self.tetris_screen.preview_queue.extend(
            list(self.next_shapes)[:10]
        )

        # 중앙 스폰
        start_x = GRID_COLUMNS // 2 - 2
        start_y = 0

        self.current_tetromino = Tetromino(
            self.screen,
            start_x, start_y,
            self.scale,
            shape_key,
            self.offset_x,
            self.offset_y
        )

        if not self.valid_position(0, 0, self.current_tetromino.rotation):
            self.game_over = True

    def valid_position(self, dx: int, dy: int, rotation: int) -> bool:
        shape = SHAPES[self.current_tetromino.shape_key][rotation]
        for cx, cy in shape:
            x = self.current_tetromino.grid_x + cx + dx
            y = self.current_tetromino.grid_y + cy + dy
            if x < 0 or x >= GRID_COLUMNS or y < 0 or y >= GRID_ROWS:
                return False
            if self.grid[y][x] is not None:
                return False
        return True

    def lock_piece(self) -> None:
        shape = SHAPES[self.current_tetromino.shape_key][
            self.current_tetromino.rotation
        ]
        for cx, cy in shape:
            x = self.current_tetromino.grid_x + cx
            y = self.current_tetromino.grid_y + cy
            self.grid[y][x] = self.current_tetromino.shape_key

        self.clear_lines()
        self.spawn_piece()

    def clear_lines(self) -> None:
        new_grid      = [row for row in self.grid if any(c is None for c in row)]
        lines_cleared = GRID_ROWS - len(new_grid)
        for _ in range(lines_cleared):
            new_grid.insert(0, [None] * GRID_COLUMNS)
        self.grid = new_grid
        self.score += lines_cleared * 100

    def move(self, dx: int, dy: int) -> bool:
        if self.valid_position(dx, dy, self.current_tetromino.rotation):
            self.current_tetromino.move(dx, dy)
            return True
        if dy == 1:
            self.lock_piece()
        return False

    def rotate(self) -> None:
        new_rot = (self.current_tetromino.rotation + 1) % len(
            SHAPES[self.current_tetromino.shape_key]
        )
        if self.valid_position(0, 0, new_rot):
            self.current_tetromino.rotate()

    def drop(self) -> None:
        while self.move(0, 1):
            pass

    def handle_input(self, event: pygame.event) -> None:
        if event.type == pygame.KEYDOWN:
            if   event.key == pygame.K_LEFT:  self.move(-1, 0)
            elif event.key == pygame.K_RIGHT: self.move(1, 0)
            elif event.key == pygame.K_DOWN:  self.move(0, 1)
            elif event.key == pygame.K_UP:    self.rotate()
            elif event.key == pygame.K_SPACE: self.drop()

    def update(self, dt, events) -> None:
        # 1) 키 입력 처리
        for ev in events:
            if ev.type == pygame.QUIT:
                self.game_over = True
            else:
                self.handle_input(ev)

        # 2) 자동 낙하 타이밍
        self.drop_timer += dt
        if self.drop_timer >= self.drop_interval:
            self.move(0, 1)
            self.drop_timer %= self.drop_interval

    def draw(self) -> None:
        # 1) 화면 클리어
        self.screen.fill((0, 0, 0))
        # 2) 그리드 + 미리보기
        self.tetris_screen.draw()
        # 3) 쌓인 블록
        for y, row in enumerate(self.grid):
            for x, cell in enumerate(row):
                if cell is not None:
                    color = COLORS[cell]
                    px = int(self.offset_x + x * CELL_SIZE * self.scale)
                    py = int(self.offset_y + y * CELL_SIZE * self.scale)
                    rect = pygame.Rect(px, py,
                                       int(CELL_SIZE * self.scale),
                                       int(CELL_SIZE * self.scale))
                    pygame.draw.rect(self.screen, color, rect)
                    pygame.draw.rect(self.screen, (0, 0, 0), rect, 1)
        # 4) 현재 조각
        if self.current_tetromino:
            self.current_tetromino.draw()
        # 5) 점수 표시
        font = pygame.font.SysFont(None, 24)
        score_surf = font.render(f"Score: {self.score}", True, (255, 255, 255))
        self.screen.blit(score_surf, (self.offset_x + 10, self.offset_y + 10))
