import pygame
from typing import Optional

from tetris.config.define import *
from tetris.net.session import Session
from tetris.ui.my_info import Profile
from tetris.game.tetromino import Tetromino
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.font_manager import FontManager
from tetris.ui.rectangle import Rectangle
from tetris.ui.button import Button
from tetris.ui.toggle_button import ToggleButton

BOARD_WIDTH = 15
BOARD_HEIGHT = 25

BOARD_COLS   = 10
HIDDEN_ROWS  = 5
VALID_ROWS = 20
BOARD_ROWS   = VALID_ROWS + HIDDEN_ROWS
PREVIEW_COLS = 5
PREVIEW_ROWS = 5

RIGHT = 0
LEFT = 1
ROTATE = 2
DOWN = 3
DROP = 4
UP = 5


# -------------------- 싱글 플레이 보드 --------------------
class TetrisBoard:
    # 각 ui들은 전체 rect에 상댓값으로 배치하는게 좋아 보인다.
    # 쓸데없이 rect를 전부 받을 필요가 없다. rect는 전체 하나만 받고, 나머지는 상댓값으로 배치한다.
    # 보드는 rect 클래스를 통해 2차원 격자로 생성한다. rect 클래스에 set_image 함수를 추가한다.
    def __init__(
        self,
        screen: pygame.Surface,
        rect: pygame.Rect,
        rm: ResourceManager,
        fm: FontManager,
        session : Session,
    ):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.fm = fm
        self.session = session
        # self.is_single = is_single
        self.score = None # 싱글용

        self.cols = BOARD_COLS
        self.rows = BOARD_ROWS
        # self.cell_rect = pygame.Rect(0, 0, 0, 0)

        # 보드 생성 및 None으로 초기화 (각 칸에는 shape_key('I','J',...) 또는 None)
        self.grid: list[list[Optional[str]]] = [
            [None for _ in range(self.cols)] for _ in range(self.rows)
        ]

        # 상태 플래그
        self.game_started = False
        self.game_over = False

        # 현재/다음 블록
        self.current_tetromino: Optional[Tetromino] = None
        self.next_tetromino_shape: Optional[str] = None

        self.set_layout()

    def set_layout(self):
        cell_w1 = self.rect.w // BOARD_WIDTH
        cell_w2 = self.rect.h // BOARD_HEIGHT
        self.cell_length = min(cell_w1, cell_w2)
        self._set_texture_size(self.cell_length)
        
        draw_x = self.rect.x
        draw_y = self.rect.y + HIDDEN_ROWS*self.cell_length
        draw_w = self.cell_length*self.cols
        draw_h = self.cell_length*VALID_ROWS
        self.grid_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)

        # profile_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        # self.profile = Profile(self.screen, profile_rect, self.fm, self.session)
        
        draw_x = self.rect.x + self.cell_length*self.cols
        draw_w = self.cell_length*PREVIEW_COLS
        draw_h = draw_w
        self.preview_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        self.preview_box = Rectangle(self.screen, self.preview_rect, self.fm, None, "", 1)

    def _set_texture_size(self, size: int):
        for key, texture in self.session.block_texture.items():
            self.session.block_texture[key] = self.rm.scale_image(texture, size, size)

    # ------------ 현재 블록을 고정 + 라인 삭제 ------------ #
    def fix(self, fix_x, fix_y):
        if not self.current_tetromino:
            return

        self.current_tetromino.x = fix_x
        self.current_tetromino.y = fix_y
        # 현재 블록을 grid에 고정
        for x, y in self.current_tetromino.blocks:
            self.grid[y][x] = self.current_tetromino.shape_key

        self.rm.fix_sound.play()
        self.current_tetromino = None

    def clear_lines(self, row_index: int):
        del self.grid[row_index]
        self.grid.insert(0, [None for _ in range(self.cols)])
        self.rm.clearline_sound.play()

    def add_line(self, hole_x: int):
        new_line = ['G' for _ in range(BOARD_COLS)]
        new_line[hole_x] = None
        self.grid.append(new_line)
        del self.grid[0]
        self.rm.addline_sound.play()

    # ------------ 로컬 이동/회전/하드드랍 (델타 기반) ------------ #
    def move(self, dx: int, dy: int):
        """현재 테트로미노를 (dx, dy)만큼 이동시키는 내부 함수."""
        if not self.current_tetromino or not self.game_started or self.game_over:
            return
        
        self.current_tetromino.x += dx
        self.current_tetromino.y += dy
        self.rm.move_sound.play()

    def rotate(self, delta: int = 1):
        """현재 테트로미노 회전."""
        if not self.current_tetromino or not self.game_started or self.game_over:
            return
        self.current_tetromino = self.current_tetromino.rotated(delta)
        # if self.can_place(nxt):
            # self.current_tetromino

    def make_landing_tetromino(self) -> Optional[Tetromino]:
        if not self.current_tetromino or not self.game_started or self.game_over:
            return None

        landing = Tetromino(
            self.current_tetromino.shape_key,
            self.current_tetromino.x,
            self.current_tetromino.y,
            self.current_tetromino.rotation,
        )

        while True:
            # 1칸 더 내려가 보자
            moved = Tetromino(landing.shape_key, landing.x, landing.y + 1, landing.rotation)

            for x, y in moved.blocks:
                if x < 0 or x >= self.cols or y < 0 or y >= self.rows:
                    return landing
                if self.grid[y][x] is not None:
                    return landing

            landing = moved

    def clear(self):
        for r in range(self.rows):
            for c in range(self.cols):
                self.grid[r][c] = None

        self.game_over = True
        self.current_tetromino = None
        self.game_started = False

    # ------------ 서버 move_type에 대응하는 진입점 ------------ #
    def handle_move(self, move_type: int):
        if not self.game_started or self.game_over or not self.current_tetromino:
            return

        if move_type == LEFT:
            self.move(-1, 0)
        elif move_type == RIGHT:
            self.move(1, 0)
        elif move_type == DOWN:
            self.move(0, 1)
        elif move_type == ROTATE:
            self.rotate(1)
        elif move_type == UP:
            self.move(0, -1)

        # elif move_type == DROP:
        #     self.hard_drop()
        # 알 수 없는 command는 무시

    # ------------ 게임 시작/리셋 ------------ #
    def start_game(self):
        """게임 시작 버튼이 눌렸을 때 호출."""
        if self.game_started:
            return

        self.game_started = True
        self.game_over = False

        # # 보드 리셋
        # self.clear()

        # 서버 주도 게임이라면 current_tetromino는
        # S2C_START 이후/또는 별도 패킷에서 세팅된다고 가정할 수 있음.
        # 여기서는 일단 None 유지.

    # ------------ 렌더링 ------------ #
    def draw_board_frame(self):
        """보드 전체 테두리."""
        # pygame.draw.rect(self.screen, (0, 0, 0), self.board_rect)
        valid_x = self.rect.x 
        valid_y = self.rect.y + self.cell_length*HIDDEN_ROWS
        valid_w = self.cell_length*self.cols
        valid_h = self.cell_length*VALID_ROWS
        pygame.draw.line(self.screen, WHITE, (valid_x,  valid_y), (valid_x, valid_y + valid_h))
        pygame.draw.line(self.screen, WHITE, (valid_x,  valid_y + valid_h), (valid_x + valid_w, valid_y + valid_h))
        pygame.draw.line(self.screen, WHITE, (valid_x + valid_w, valid_y + valid_h), (valid_x + valid_w, valid_y))

    def draw_cells(self):
        tex_map = self.session.block_texture

        # 현재 테트로미노 좌표 (grid 렌더에서 제외)
        falling_cells = set()
        if self.current_tetromino and self.game_started and not self.game_over:
            for x, y in self.current_tetromino.blocks:
                falling_cells.add((x, y))

        bx = self.rect.x
        by = self.rect.y

        # 1) grid에 고정된 블록 렌더 (25칸 전체)
        for r in range(self.rows):
            for c in range(self.cols):
                if (c, r) in falling_cells:
                    continue

                shape_key = self.grid[r][c]
                if not shape_key:
                    continue

                tex = tex_map[shape_key]

                px = bx + c * self.cell_length
                py = by + r * self.cell_length
                self.screen.blit(tex, (px, py))

        # 2) 현재 테트로미노 렌더 (여기서만 1번)
        if self.current_tetromino and self.game_started and not self.game_over:
            tex = tex_map[self.current_tetromino.shape_key]

            for x, y in self.current_tetromino.blocks:
                px = bx + x * self.cell_length
                py = by + y * self.cell_length
                self.screen.blit(tex, (px, py))

    def draw_preview(self):
        self.preview_box.draw()

        if self.next_tetromino_shape is None:
            return
        if not self.game_started:
            return

        shape_key = self.next_tetromino_shape
        shape = SHAPES[shape_key][0]
        tex = self.session.block_texture[shape_key]

        xs = [cx for (cx, _) in shape]
        ys = [cy for (_, cy) in shape]
        min_x, max_x = min(xs), max(xs)
        min_y, max_y = min(ys), max(ys)

        shape_w = (max_x - min_x + 1) * self.cell_length
        shape_h = (max_y - min_y + 1) * self.cell_length

        offx = (
            self.preview_rect.x
            + (self.preview_rect.w - shape_w) / 2
            - min_x * self.cell_length
        )
        offy = (
            self.preview_rect.y
            + (self.preview_rect.h - shape_h) / 2
            - min_y * self.cell_length
        )

        for cx, cy in shape:
            px = int(offx + cx * self.cell_length)
            py = int(offy + cy * self.cell_length)
            self.screen.blit(tex, (px, py))

    def draw_landing_blocks(self):
        LANDING_ALPHA = 80
        landing = self.make_landing_tetromino()
        if not landing or not self.game_started or self.game_over:
            return
        if not self.current_tetromino:
            return
        if landing.y == self.current_tetromino.y:
            return

        tex = self.session.block_texture[landing.shape_key]

        bx = self.rect.x
        by = self.rect.y

        for x, y in landing.blocks:
            px = bx + x * self.cell_length
            py = by + y * self.cell_length

            ghost = tex.copy()
            ghost.set_alpha(LANDING_ALPHA)
            self.screen.blit(ghost, (px, py))

    def draw(self):
        # 보드 프레임 & 미리보기는 항상 그림
        self.draw_board_frame()
        self.draw_preview()

        if not self.game_started:
            # 시작 전: 보드 내부에 내 정보
            # self.profile.draw()
            pass
        else:
            # 게임 중: 로컬 보드/블록 렌더
            self.draw_cells()
            self.draw_landing_blocks()


# -------------------- 싱글 플레이 상태 --------------------
