from tetris.config.define import *

FALLBACK_COLORS = {
    'I': (0, 240, 240),
    'J': (0, 0, 240),
    'L': (240, 160, 0),
    'O': (240, 240, 0),
    'S': (0, 240, 0),
    'T': (160, 0, 240),
    'Z': (240, 0, 0),
    'G': GRAY
}


class Tetromino:
    def __init__(self, shape_key: str, x: int, y: int, rotation: int = 0):
        self.shape_key = shape_key
        self.x = x
        self.y = y
        self.rotation = rotation

    @property
    def blocks(self):
        """현재 회전/위치 기준 블록 4개의 절대 좌표를 반환."""
        shape = SHAPES[self.shape_key][self.rotation]
        return [(self.x + cx, self.y + cy) for (cx, cy) in shape]

    def rotated(self, delta: int = 1) -> "Tetromino":
        """회전이 적용된 새로운 Tetromino 인스턴스를 반환."""
        max_rot = len(SHAPES[self.shape_key])
        return Tetromino(
            self.shape_key,
            self.x,
            self.y,
            (self.rotation + delta) % max_rot,
        )