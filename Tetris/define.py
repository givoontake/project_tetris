# define.py

# Screen dimensions (FHD)
BASE_SCREEN_WIDTH = 1920
BASE_SCREEN_HEIGHT = 1080

# Scale factor for initial window (0.95)
INITIAL_SCALE = 0.95

# Grid
GRID_COLUMNS = 10
GRID_ROWS = 20
CELL_SIZE = 50

# Preview
# (5×5 셀 크기 기준. PREVIEW_COLUMNS/PREVIEW_ROWS는 더 이상 사용되지 않습니다.)
PREVIEW_CELL_SIZE = CELL_SIZE * 5

BLANK_WIDTH = 100
BLANK_HEIGHT = 50

# Frames per second
FPS = 30

# Network settings
SERVER_HOST = '127.0.0.1'
SERVER_PORT = 12345

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

# (무게중심 방식은 더 이상 사용되지 않습니다.)
# Bounding-box 방식으로 중앙 정렬하므로, 별도 SHAPE_CENTER 불필요

COLORS = {
    'I': (0,240,240), 'J': (0,0,240), 'L': (240,160,0),
    'O': (240,240,0), 'S': (0,240,0),  'T': (160,0,240),
    'Z': (240,0,0)
}

TEST_TETROMINOS = ['I', 'J', 'L', 'O', 'S', 'T', 'Z']
