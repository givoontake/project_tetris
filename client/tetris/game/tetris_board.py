import pygame
from typing import Optional

from tetris.animation.combo_animation import ComboAnimation
from tetris.config.define import *
from tetris.game.tetromino import Tetromino
from tetris.game.define import *
from tetris.resources.resource_manager import *
from tetris.resources.define import *
from tetris.resources.define_colors import *
from tetris.ui.rectangle import Rectangle

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
    ):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.block_textures = None
        # self.is_single = is_single
        self.score = None # 싱글용

        self.cols = BOARD_COLS
        self.rows = BOARD_ROWS
        # self.cell_rect = pygame.Rect(0, 0, 0, 0)

        # 보드 생성 및 None으로 초기화 (각 칸에는 shape_key('I','J',...) 또는 None)
        self.grid: list[list[Optional[str]]] = [
            [None for _ in range(self.cols)] for _ in range(self.rows)
        ]

        # 현재/다음 블록
        self.current_tetromino: Optional[Tetromino] = None
        self.next_tetromino_shape: Optional[str] = None

        # self.combo = 0
        self.combo_effects: Optional[list[ComboAnimation]] = []

        self.anim_elapsed_ms = 0
        self.anim_target_line = BOARD_ROWS - 1

        self.set_layout()

    def init(self):
        self._clear_board()

        self.current_tetromino = None
        self.next_tetromino_shape = None

    def set_textures(self, block_textures: dict):
        self.block_textures = block_textures
        self._set_texture_size()

    def set_layout(self):
        cell_w1 = self.rect.w // BOARD_WIDTH
        cell_w2 = self.rect.h // BOARD_HEIGHT
        self.cell_length = min(cell_w1, cell_w2)
        
        draw_x = self.rect.x
        draw_y = self.rect.y + HIDDEN_ROWS*self.cell_length
        draw_w = self.cell_length*self.cols
        draw_h = self.cell_length*VALID_ROWS
        self.valid_grid_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        self.full_grid_rect = pygame.Rect(self.rect.x, self.rect.y, draw_w, self.cell_length*BOARD_ROWS)
        frame_w = int(self.valid_grid_rect.w * 1.0)
        frame_h = int(self.valid_grid_rect.h * 1.0)
        frame_x = self.valid_grid_rect.centerx - (frame_w // 2)
        frame_y = self.valid_grid_rect.centery - (frame_h // 2)
        self.board_frame_rect = pygame.Rect(frame_x, frame_y, frame_w, frame_h)
        self.board_frame = Rectangle(self.screen, self.board_frame_rect, self.rm, True, self.rm.images.ui_images[UI_FRAME12], "")

        # profile_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        # self.profile = Profile(self.screen, profile_rect, self.fm, self.session)
        
        draw_x = self.rect.x + self.cell_length*self.cols
        draw_w = self.cell_length*PREVIEW_COLS
        draw_h = draw_w
        self.preview_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        self.preview_box = Rectangle(self.screen, self.preview_rect, self.rm, True, self.rm.images.ui_images[UI_FRAME11], "")

    def _set_texture_size(self):
        for key, texture in self.block_textures.items():
            self.block_textures[key] = self.rm.images.scale_image(texture, self.cell_length, self.cell_length)

    def find_anim_start_line(self):
        grid = self.grid
        self.anim_target_line = BOARD_ROWS - 1

        while True: # 애니메이션 적용할 첫 라인 찾기(첫 컬러 블록이 포함된 줄 찾기)
            escape = False 
            for y in range(BOARD_COLS):
                if grid[self.anim_target_line][y] == None or grid[self.anim_target_line][y] == 'G':
                    pass
                else:
                    escape = True
            if escape:
                break
            else:
                if self.anim_target_line > 0:
                    self.anim_target_line -= 1
                else:
                    break

    def animate_gameover(self, dt_ms) -> bool:
        self.anim_elapsed_ms += dt_ms
        grid = self.grid
        if self.anim_elapsed_ms > 100:
            self.anim_elapsed_ms -= 100
            done_flag = True
                
            for y in range(BOARD_COLS): # 1줄(가로)이 다 None이면 끝.
                if grid[self.anim_target_line][y] != None:
                    grid[self.anim_target_line][y] = 'G'
                    done_flag = False
            self.anim_target_line -= 1
            return done_flag
        return False

    # ------------ 현재 블록을 고정 + 라인 삭제 ------------ #
    def fix(self, fix_x, fix_y):
        if not self.current_tetromino:
            return

        self.current_tetromino.x = fix_x
        self.current_tetromino.y = fix_y
        # 현재 블록을 grid에 고정
        for x, y in self.current_tetromino.blocks:
            self.grid[y][x] = self.current_tetromino.shape_key

        self.rm.sounds.sound_effects[EFFECT_FIX].play()
        self.current_tetromino = None

    def clear_lines(self, row_index: int):
        del self.grid[row_index]
        self.grid.insert(0, [None for _ in range(self.cols)])
        self.rm.sounds.sound_effects[EFFECT_CLEARLINE].play()

    def animate_combo(self, row_index: int, combo: int):
        draw_x = self.full_grid_rect.x
        draw_y = self.full_grid_rect.y + self.cell_length*row_index
        make_combo = ComboAnimation(self.screen, self.rm, combo, draw_x, draw_y)
        self.combo_effects.append(make_combo)

    def add_line(self, hole_x: int):
        new_line = ['G' for _ in range(BOARD_COLS)]
        new_line[hole_x] = None
        self.grid.append(new_line)
        del self.grid[0]
        self.rm.sounds.sound_effects[EFFECT_ADDLINE].play()

    # ------------ 로컬 이동/회전/하드드랍 (델타 기반) ------------ #
    def move(self, dx: int, dy: int):
        """현재 테트로미노를 (dx, dy)만큼 이동시키는 내부 함수."""
        if not self.current_tetromino:
            return
        
        self.current_tetromino.x += dx
        self.current_tetromino.y += dy
        self.rm.sounds.sound_effects[EFFECT_MOVE].play()

    def rotate(self, delta: int = 1):
        """현재 테트로미노 회전."""
        if not self.current_tetromino:
            return
        self.current_tetromino = self.current_tetromino.rotated(delta)
        # if self.can_place(nxt):
            # self.current_tetromino

    def make_landing_tetromino(self) -> Optional[Tetromino]:
        if not self.current_tetromino:
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

    def _clear_board(self):
        for r in range(self.rows):
            for c in range(self.cols):
                self.grid[r][c] = None

    def reset(self):
        self._clear_board()
        self.current_tetromino = None
        self.next_tetromino_shape = None
        self.combo = 0
        self.combo_effects = []
        self.anim_elapsed_ms = 0
        self.anim_target_line = BOARD_ROWS - 1

    def clear(self):
        self._clear_board()
        self.current_tetromino = None
        self.next_tetromino_shape = None
        self.combo = 0
        self.combo_effects = []
        self.anim_elapsed_ms = 0
        self.anim_target_line = BOARD_ROWS - 1

    # ------------ 서버 move_type에 대응하는 진입점 ------------ #
    def handle_move(self, move_type: int):
        if not self.current_tetromino:
            return

        if move_type == MoveType.LEFT:
            self.move(-1, 0)
        elif move_type == MoveType.RIGHT:
            self.move(1, 0)
        elif move_type == MoveType.DOWN:
            self.move(0, 1)
        elif move_type == MoveType.ROTATE:
            self.rotate(1)
        elif move_type == MoveType.UP:
            self.move(0, -1)

    def update(self, dt_ms: int):
        for effect in self.combo_effects:
            effect.update(dt_ms)

        self.combo_effects = [effect for effect in self.combo_effects if effect.active] # 리스트 컴프리헨션으로 조건에 맞는 새 리스트 생성
    # ------------ 렌더링 ------------ #
    def draw_board_frame(self):
        self.board_frame.draw()
        board_background = pygame.Surface((self.valid_grid_rect.w, self.valid_grid_rect.h), pygame.SRCALPHA)
        board_background.fill((0, 0, 0, 96))
        self.screen.blit(board_background, self.valid_grid_rect.topleft)

    def draw_cells(self):
        tex_map = self.block_textures

        # 현재 테트로미노 좌표 (grid 렌더에서 제외)
        falling_cells = set()
        if self.current_tetromino:
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
        if self.current_tetromino:
            tex = tex_map[self.current_tetromino.shape_key]

            for x, y in self.current_tetromino.blocks:
                px = bx + x * self.cell_length
                py = by + y * self.cell_length
                self.screen.blit(tex, (px, py))

    def draw_preview(self):
        self.preview_box.draw()

        if self.next_tetromino_shape is None:
            return

        shape_key = self.next_tetromino_shape
        shape = SHAPES[shape_key][0]
        tex = self.block_textures[shape_key]

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
        if not landing:
            return
        if not self.current_tetromino:
            return
        if landing.y == self.current_tetromino.y:
            return

        tex = self.block_textures[landing.shape_key]

        bx = self.rect.x
        by = self.rect.y

        for x, y in landing.blocks:
            px = bx + x * self.cell_length
            py = by + y * self.cell_length

            ghost = tex.copy()
            ghost.set_alpha(LANDING_ALPHA)
            self.screen.blit(ghost, (px, py))

    def draw_frame(self):
        self.draw_board_frame()
        self.draw_preview()

    def draw_game(self):
        self.draw_cells()
        self.draw_landing_blocks()

    def draw_combo(self):
        for effect in self.combo_effects:
            effect.draw()


