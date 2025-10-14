# tetris_screen.py
import pygame
from game_object import Grid, PreviewTetromino
from define import GRID_COLUMNS, CELL_SIZE, PREVIEW_CELL_SIZE

class TetrisScreen:
    def __init__(self, screen, offset_x, offset_y, scale, preview_queue):
        self.screen = screen
        self.scale = scale
        self.preview_queue = preview_queue
        self.grid = Grid(screen, offset_x, offset_y, scale)
        px = offset_x + GRID_COLUMNS * CELL_SIZE * scale
        py = offset_y
        self.previews = [
            PreviewTetromino(screen, px, py, scale, None),
            PreviewTetromino(screen, px, py + PREVIEW_CELL_SIZE * scale, scale, None)
        ]

    def init(self): pass
    def update(self, dt): pass

    def draw(self):
        self.grid.draw()
        for idx, prev in enumerate(self.previews):
            if idx < len(self.preview_queue):
                prev.shape_key = self.preview_queue[idx]
                prev.draw(idx)
