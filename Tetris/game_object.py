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

    def init(self): pass
    def update(self, dt): pass

    def draw(self):
        w = self.cols * self.cell
        h = self.rows * self.cell
        pygame.draw.rect(self.screen, (0, 0, 0), (self.x, self.y, w, h))
        for c in range(self.cols + 1):
            x = int(self.x + c * self.cell)
            pygame.draw.line(self.screen, (50, 50, 50), (x, self.y), (x, self.y + h))
        for r in range(self.rows + 1):
            y = int(self.y + r * self.cell)
            pygame.draw.line(self.screen, (50, 50, 50), (self.x, y), (self.x + w, y))

class Tetromino:
    def __init__(self, screen, grid_x, grid_y, scale, shape_key, offset_x=0, offset_y=0):
        self.screen   = screen
        self.grid_x   = grid_x        # 그리드 셀 단위 X
        self.grid_y   = grid_y        # 그리드 셀 단위 Y
        self.scale    = scale
        self.shape_key= shape_key
        self.rotation = 0
        self.cell     = CELL_SIZE * scale
        # 화면 내부 그리드 오프셋
        self.offset_x = offset_x
        self.offset_y = offset_y

    def init(self): pass
    def update(self, dt): pass

    def rotate(self):
        self.rotation = (self.rotation + 1) % len(SHAPES[self.shape_key])

    def move(self, dx, dy):
        self.grid_x += dx
        self.grid_y += dy

    def draw(self):
        for cx, cy in SHAPES[self.shape_key][self.rotation]:
            px = int(self.offset_x + cx * self.cell + self.grid_x * self.cell)
            py = int(self.offset_y + cy * self.cell + self.grid_y * self.cell)
            rect = pygame.Rect(px, py, int(self.cell), int(self.cell))
            pygame.draw.rect(self.screen, COLORS[self.shape_key], rect)
            pygame.draw.rect(self.screen, (0, 0, 0), rect, 1)

class PreviewTetromino:
    def __init__(self, screen, x, y, scale, shape_key):
        self.screen = screen
        self.x      = x
        self.y      = y
        self.scale  = scale
        self.cell   = CELL_SIZE * scale
        self.preview_cell = PREVIEW_CELL_SIZE * scale
        self.shape_key    = shape_key

    def init(self): pass
    def update(self, dt): pass

    def draw(self, offset_index):
        px = self.x
        py = self.y
        size = int(self.preview_cell)
        bg = pygame.Rect(px, py, size, size)
        pygame.draw.rect(self.screen, (50, 50, 50), bg)
        pygame.draw.rect(self.screen, (80, 80, 80), bg, 1)

        shape = SHAPES[self.shape_key][0]
        xs = [c for c, _ in shape]
        ys = [r for _, r in shape]
        minx, maxx = min(xs), max(xs)
        miny, maxy = min(ys), max(ys)
        w = (maxx - minx + 1) * self.cell
        h = (maxy - miny + 1) * self.cell
        offx = px + (self.preview_cell - w) / 2 - minx * self.cell
        offy = py + (self.preview_cell - h) / 2 - miny * self.cell

        for cx, cy in shape:
            rect = pygame.Rect(
                int(offx + cx * self.cell),
                int(offy + cy * self.cell),
                int(self.cell), int(self.cell)
            )
            pygame.draw.rect(self.screen, COLORS[self.shape_key], rect)
            pygame.draw.rect(self.screen, (0, 0, 0), rect, 1)
