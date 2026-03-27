from enum import IntEnum
BOARD_WIDTH = 15
BOARD_HEIGHT = 25

BOARD_COLS   = 10
HIDDEN_ROWS  = 5
VALID_ROWS = 20
BOARD_ROWS   = VALID_ROWS + HIDDEN_ROWS
PREVIEW_COLS = 5
PREVIEW_ROWS = 5

class MoveType(IntEnum):
    RIGHT = 0
    LEFT = 1
    ROTATE = 2
    DOWN = 3
    DROP = 4
    UP = 5

class TSessionState(IntEnum):
    WAIT = 0
    PLAY = 1
    GAMEOVER = 2
    GAMEOVER_ANIMATING = 4
    EMPTY = 3
