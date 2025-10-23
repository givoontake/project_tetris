# define.py
# Network
BUF_SIZE = 10240
SERVER_HOST = '127.0.0.1'
SERVER_PORT = 12345

# Screen dimensions (FHD)
BASE_SCREEN_WIDTH = 1920
BASE_SCREEN_HEIGHT = 1080

# Scale factor for initial window
INITIAL_SCALE = 0.95

# Grid
GRID_COLUMNS = 10
GRID_ROWS = 20
CELL_SIZE = 50

# Preview (5x5 셀)
PREVIEW_CELL_SIZE = CELL_SIZE * 5

BLANK_WIDTH = 100
BLANK_HEIGHT = 50

# FPS
FPS = 60

# Tetromino shapes and colors
SHAPES = {
    'I': [[(0,1),(1,1),(2,1),(3,1)], [(2,0),(2,1),(2,2),(2,3)]],
    'J': [[(0,0),(0,1),(1,1),(2,1)], [(1,0),(2,0),(1,1),(1,2)],
          [(0,1),(1,1),(2,1),(2,2)], [(1,0),(1,1),(0,2),(1,2)]],
    'L': [[(2,0),(0,1),(1,1),(2,1)], [(1,0),(1,1),(1,2),(2,2)],
          [(0,1),(1,1),(2,1),(0,2)], [(0,0),(1,0),(1,1),(1,2)]],
    'O': [[(1,0),(2,0),(1,1),(2,1)]],
    'S': [[(1,1),(2,1),(0,2),(1,2)], [(1,0),(1,1),(2,1),(2,2)]],
    'T': [[(1,0),(0,1),(1,1),(2,1)], [(1,0),(1,1),(2,1),(1,2)],
          [(0,1),(1,1),(2,1),(1,2)], [(1,0),(0,1),(1,1),(1,2)]],
    'Z': [[(0,1),(1,1),(1,2),(2,2)], [(2,0),(1,1),(2,1),(1,2)]]
}

COLORS = {
    'I': (0,240,240), 'J': (0,0,240), 'L': (240,160,0),
    'O': (240,240,0), 'S': (0,240,0),  'T': (160,0,240),
    'Z': (240,0,0)
}

TEST_TETROMINOS = ['I', 'J', 'L', 'O', 'S', 'T', 'Z']

# ----------------------------------------------------------------------
# 블록 타입 정의 예시 (2차원 리스트)
# [정의 상수명, 파일명, 파일 경로]
# 실제 프로젝트에서는 여러 개를 나열하여 사용 가능
# ----------------------------------------------------------------------

DEFAULT_RED = 1
DEFAULT_ORANGE = 2
DEFAULT_YELLOW = 3
DEFAULT_GREEN = 4
DEFAULT_BLUE = 5
DEFAULT_INDIGO = 6
DEFAULT_PURPLE = 7

CANDY_RED = 8
CANDY_ORANGE = 9
CANDY_YELLOW = 10  
CANDY_GREEN = 11
CANDY_BLUE = 12
CANDY_INDIGO = 13
CANDY_PURPLE = 14

# 리스트에 알기쉽게 접근하기 위해 정의
TYPE = 0
PATH = 1

ASSET = [
    # --- Default Blocks ---
    [DEFAULT_RED,     "blocks/default/default_red.png"],
    [DEFAULT_ORANGE,  "blocks/default/default_orange.png"],
    [DEFAULT_YELLOW,  "blocks/default/default_yellow.png"],
    [DEFAULT_GREEN,   "blocks/default/default_green.png"],
    [DEFAULT_BLUE,    "blocks/default/default_blue.png"],
    [DEFAULT_INDIGO,  "blocks/default/default_indigo.png"],
    [DEFAULT_PURPLE,  "blocks/default/default_purple.png"],

    # --- Candy Blocks ---
    [CANDY_RED,       "blocks/candy/candy_red.png"],
    [CANDY_ORANGE,    "blocks/candy/candy_orange.png"],
    [CANDY_YELLOW,    "blocks/candy/candy_yellow.png"],
    [CANDY_GREEN,     "blocks/candy/candy_green.png"],
    [CANDY_BLUE,      "blocks/candy/candy_blue.png"],
    [CANDY_INDIGO,    "blocks/candy/candy_indigo.png"],
    [CANDY_PURPLE,    "blocks/candy/candy_purple.png"],
]