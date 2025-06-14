# game_object.py
import pygame
from define import *

class Grid:
    def __init__(self, screen, x, y, scale):
        self.screen = screen
        self.x = x
        self.y = y
        self.scale = scale
        self.cell = CELL_SIZE * scale
        self.cols = GRID_COLUMNS
        self.rows = GRID_ROWS

    def draw(self):
        # 배경
        total_w = int(round(self.cols * self.cell))
        total_h = int(round(self.rows * self.cell))
        bg = pygame.Rect(int(round(self.x)), int(round(self.y)), total_w, total_h)
        pygame.draw.rect(self.screen, (0, 0, 0), bg)

        # 수직 경계선
        for c in range(self.cols + 1):
            x = int(round(self.x + c * self.cell))
            pygame.draw.line(
                self.screen,
                (50, 50, 50),
                (x, int(round(self.y))),
                (x, int(round(self.y + total_h)))
            )
        # 수평 경계선
        for r in range(self.rows + 1):
            y = int(round(self.y + r * self.cell))
            pygame.draw.line(
                self.screen,
                (50, 50, 50),
                (int(round(self.x)), y),
                (int(round(self.x + total_w)), y)
            )

class Tetromino:
    def __init__(self, screen, grid_x, grid_y, scale, shape_key):
        self.screen = screen
        self.grid_x = grid_x
        self.grid_y = grid_y
        self.scale = scale
        self.shape_key = shape_key
        self.rotation = 0
        self.cell = CELL_SIZE * scale

    def rotate(self):
        self.rotation = (self.rotation + 1) % len(SHAPES[self.shape_key])

    def move(self, dx, dy):
        self.grid_x += dx
        self.grid_y += dy

    def draw(self):
        shape = SHAPES[self.shape_key][self.rotation]
        color = COLORS[self.shape_key]

        for cx, cy in shape:
            # → 정수 픽셀 좌표로 반올림
            px = int(round((self.grid_x + cx) * self.cell))
            py = int(round((self.grid_y + cy) * self.cell))
            size = int(round(self.cell))

            rect = pygame.Rect(px, py, size, size)
            pygame.draw.rect(self.screen, color, rect)
            pygame.draw.rect(self.screen, (0, 0, 0), rect, 1)

class PreviewTetromino:
    def __init__(self, screen, x, y, scale, shape_key):
        """
        screen: pygame Surface
        x, y: top-left pixel of the preview cell (5×5)
        scale: same scale as grid cells
        shape_key: one of 'I','O','T',...
        """
        self.screen = screen
        self.x = x
        self.y = y
        self.scale = scale
        self.cell = CELL_SIZE * scale
        self.preview_cell = PREVIEW_CELL_SIZE * scale
        self.shape_key = shape_key

    def draw(self, offset_index):
        # 1) preview cell의 좌표
        px_box = self.x
        py_box = self.y + offset_index * self.preview_cell

        # 2) preview 박스 배경+테두리
        pre_w = int(round(self.preview_cell))
        pre_h = pre_w
        bg = pygame.Rect(
            int(round(px_box)),
            int(round(py_box)),
            pre_w, pre_h
        )
        pygame.draw.rect(self.screen, (50,50,50), bg)
        pygame.draw.rect(self.screen, (80,80,80), bg, 1)

        # 3) 모양(회전 0)
        shape = SHAPES[self.shape_key][0]
        color = COLORS[self.shape_key]

        # 4) bounding-box 크기
        xs = [cx for cx,cy in shape]
        ys = [cy for cx,cy in shape]
        min_x, max_x = min(xs), max(xs)
        min_y, max_y = min(ys), max(ys)
        shape_w = (max_x - min_x + 1) * self.cell
        shape_h = (max_y - min_y + 1) * self.cell

        # 5) 중앙 정렬 오프셋 (float)
        offset_x = px_box + (self.preview_cell - shape_w) / 2 - min_x * self.cell
        offset_y = py_box + (self.preview_cell - shape_h) / 2 - min_y * self.cell

        # 6) 각 셀 그리기 (반올림 후 정수)
        for cx, cy in shape:
            px = int(round(offset_x + cx * self.cell))
            py = int(round(offset_y + cy * self.cell))
            size = int(round(self.cell))

            rect = pygame.Rect(px, py, size, size)
            pygame.draw.rect(self.screen, color, rect)
            pygame.draw.rect(self.screen, (0,0,0), rect, 1)
