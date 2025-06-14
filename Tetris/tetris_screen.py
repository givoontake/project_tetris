# tetris_screen.py
import pygame
from game_object import Grid, PreviewTetromino
from define import GRID_COLUMNS, CELL_SIZE, PREVIEW_CELL_SIZE

class TetrisScreen:
    def __init__(self, screen, offset_x, offset_y, scale, preview_queue):
        """
        screen        : pygame Surface
        offset_x, y   : 메인 그리드의 top-left 픽셀 좌표
        scale         : 셀 스케일 (grid & preview 모두)
        preview_queue : list or deque 형태의 다음 블록 키 큐
        """
        self.screen = screen
        self.scale = scale
        self.preview_queue = preview_queue

        # 1) 메인 그리드
        self.grid = Grid(screen, offset_x, offset_y, scale)

        # 2) 두 개의 Preview 창 위치 계산
        #    첫 번째(맨 위) 프리뷰
        px1 = offset_x + GRID_COLUMNS * CELL_SIZE * scale
        py1 = offset_y
        #    두 번째 프리뷰는 y 에만 PREVIEW_CELL_SIZE*scale 만큼 더해 줌
        px2 = px1
        py2 = offset_y + PREVIEW_CELL_SIZE * scale

        # 3) PreviewTetromino 인스턴스 두 개 생성 (shape_key는 draw 시 덮어쓰기)
        self.previews = [
            PreviewTetromino(screen, px1, py1, scale, None),
            PreviewTetromino(screen, px2, py2, scale, None),
        ]

    def draw(self):
        # 1) 메인 그리드만 그린다
        self.grid.draw()

        # 2) preview_queue의 0,1번 인덱스를 꺼내서 각각의 PreviewTetromino에 넘겨 그린다
        for idx in (0, 1):
            if idx < len(self.preview_queue):
                shape_key = self.preview_queue[idx]
                prev = self.previews[idx]
                prev.shape_key = shape_key   # 동적으로 키 덮어쓰기
                prev.draw(0)  # offset_index는 PreviewTetromino 내부 y좌표 고정이므로 0
