import pygame
import struct
from typing import Optional

from define import *
from packet_type import *
from session import Session
from my_info import MyInfo
from menu import Button
from network import NetworkWorker
from resource_manager import ResourceManager

BOARD_COLS   = 10
HIDDEN_ROWS  = 5
BOARD_ROWS   = 20+HIDDEN_ROWS
PREVIEW_COLS = 5
PREVIEW_ROWS = 5

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


RIGHT = 0
LEFT = 1
ROTATE = 2
DOWN = 3
DROP = 4
UP = 5


# -------------------- 싱글 플레이 보드 --------------------
class TetrisBoard:
    """
    싱글플레이 테트리스 보드.

    - grid / current / next_shape 를 모두 로컬로 가진다.
    - 블록 렌더링은 Session.block_texture 를 우선 사용한다.
    """

    def __init__(
        self,
        screen: pygame.Surface,
        board_rect: pygame.Rect,
        valid_rect: pygame.Rect,
        preview_rect: pygame.Rect,
        score_rect: pygame.Rect,
        session: Session,
    ):
        self.screen = screen
        self.board_rect = board_rect
        self.valid_rect = valid_rect
        self.preview_rect = preview_rect
        self.score_rect = score_rect
        self.score = 0
        self.session = session

        self.cols = BOARD_COLS
        self.rows = BOARD_ROWS

        # 보드 생성 및 None으로 초기화 (각 칸에는 shape_key('I','J',...) 또는 None)
        self.grid: list[list[Optional[str]]] = [
            [None for _ in range(self.cols)] for _ in range(self.rows)
        ]

        # 상태 플래그
        self.game_started = False
        self.game_over = False

        # 현재/다음 블록
        self.current_tetromino: Optional[Tetromino] = None
        # next_tetromino는 미리보기용 shape_key(str)
        self.next_tetromino_shape: Optional[str] = None

        # 보드 내부 개인정보 패널(MyInfo)
        info_h = self.board_rect.h // 3
        profile_rect = pygame.Rect(
            self.board_rect.x + 20,
            self.board_rect.y + self.board_rect.h // 2 - info_h,
            self.board_rect.w - 40,
            info_h,
        )
        self.my_info = MyInfo(profile_rect, self.session)

    # # ------------ 좌표/충돌 판정 ------------ #
    # def in_bounds(self, x: int, y: int) -> bool:
    #     return 0 <= x < self.cols and 0 <= y < self.rows

    # def can_place(self, tet: Tetromino) -> bool:
    #     """tetromino의 각 블록이 보드 안이고, 이미 고정된 블록과 겹치지 않는지 확인."""
    #     for x, y in tet.blocks:
    #         if not self.in_bounds(x, y):
    #             return False
    #         if self.grid[y][x] is not None:
    #             return False
    #     return True

    # ------------ 현재 블록을 고정 + 라인 삭제 ------------ #
    def fix(self, fix_x, fix_y):
        if not self.current_tetromino:
            return

        self.current_tetromino.x = fix_x
        self.current_tetromino.y = fix_y
        # 현재 블록을 grid에 고정
        for x, y in self.current_tetromino.blocks:
            self.grid[y][x] = self.current_tetromino.shape_key

        self.current_tetromino = None

    def clear_lines(self, row_index: int):
        del self.grid[row_index]
        self.grid.insert(0, [None for _ in range(self.cols)])

    def add_line(self, hole_x: int):
        new_line = ['G' for _ in range(BOARD_COLS)]
        new_line[hole_x] = None
        self.grid.append(new_line)
        del self.grid[0]

    # ------------ 로컬 이동/회전/하드드랍 (델타 기반) ------------ #
    def move(self, dx: int, dy: int):
        """현재 테트로미노를 (dx, dy)만큼 이동시키는 내부 함수."""
        if not self.current_tetromino or not self.game_started or self.game_over:
            return
        
        self.current_tetromino.x += dx
        self.current_tetromino.y += dy

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

    def clear_board(self):
        for r in range(self.rows):
            for c in range(self.cols):
                self.grid[r][c] = None

    # ------------ 서버 move_type에 대응하는 진입점 ------------ #
    def handle_move(self, move_type: int):
        """
        서버에서 승인된 move_type을 SinglePlayState가 넘겨주는 함수.

        move_type 값:
            RIGHT, LEFT, DOWN, ROTATE, DROP (TIMEOUT은 필요시 확장)
        """
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

        # 보드 리셋
        self.clear_board()

        # 서버 주도 게임이라면 current_tetromino는
        # S2C_START 이후/또는 별도 패킷에서 세팅된다고 가정할 수 있음.
        # 여기서는 일단 None 유지.

    # ------------ 렌더링 ------------ #
    def draw_board_frame(self):
        """보드 전체 테두리."""
        # pygame.draw.rect(self.screen, (0, 0, 0), self.board_rect)
        valid_x = self.valid_rect.x
        valid_y = self.valid_rect.y
        valid_w = self.valid_rect.w
        valid_h = self.valid_rect.h
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

        bx = self.board_rect.x
        by = self.board_rect.y

        # 1) grid에 고정된 블록 렌더 (25칸 전체)
        for r in range(self.rows):
            for c in range(self.cols):
                if (c, r) in falling_cells:
                    continue

                shape_key = self.grid[r][c]
                if not shape_key:
                    continue

                tex = tex_map[shape_key]

                px = bx + c * CELL_SIZE
                py = by + r * CELL_SIZE
                self.screen.blit(tex, (px, py))

        # 2) 현재 테트로미노 렌더 (여기서만 1번)
        if self.current_tetromino and self.game_started and not self.game_over:
            tex = tex_map[self.current_tetromino.shape_key]

            for x, y in self.current_tetromino.blocks:
                px = bx + x * CELL_SIZE
                py = by + y * CELL_SIZE
                self.screen.blit(tex, (px, py))



    def draw_preview(self):
        pygame.draw.rect(self.screen, (0, 0, 0), self.preview_rect)
        pygame.draw.rect(self.screen, (255, 255, 255), self.preview_rect, 1)

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

        shape_w = (max_x - min_x + 1) * CELL_SIZE
        shape_h = (max_y - min_y + 1) * CELL_SIZE

        offx = (
            self.preview_rect.x
            + (self.preview_rect.w - shape_w) / 2
            - min_x * CELL_SIZE
        )
        offy = (
            self.preview_rect.y
            + (self.preview_rect.h - shape_h) / 2
            - min_y * CELL_SIZE
        )

        for cx, cy in shape:
            px = int(offx + cx * CELL_SIZE)
            py = int(offy + cy * CELL_SIZE)
            self.screen.blit(tex, (px, py))


    def draw_landing_blocks(self):
        LANDING_ALPHA = 50
        landing = self.make_landing_tetromino()
        if not landing or not self.game_started or self.game_over:
            return
        if not self.current_tetromino:
            return
        if landing.y == self.current_tetromino.y:
            return

        tex = self.session.block_texture[landing.shape_key]

        bx = self.board_rect.x
        by = self.board_rect.y

        for x, y in landing.blocks:
            px = bx + x * CELL_SIZE
            py = by + y * CELL_SIZE

            ghost = tex.copy()
            ghost.set_alpha(LANDING_ALPHA)
            self.screen.blit(ghost, (px, py))



    def draw_score(self):
        pygame.draw.rect(self.screen, BLACK, self.score_rect)
        pygame.draw.rect(self.screen, WHITE, self.score_rect, 1)

        # 중앙 정렬을 위해 텍스트 렌더링
        font = pygame.font.Font("resource/dodamdodam.ttf", 32)
        text = font.render(f"Score: {self.score}", True, WHITE)

        # 텍스트를 score_rect 중앙에 배치
        text_rect = text.get_rect(center=self.score_rect.center)

        self.screen.blit(text, text_rect)

    def draw(self):
        # 보드 프레임 & 미리보기는 항상 그림
        self.draw_board_frame()
        self.draw_preview()
        self.draw_score()

        if not self.game_started:
            # 시작 전: 보드 내부에 내 정보
            self.my_info.draw(self.screen)
        else:
            # 게임 중: 로컬 보드/블록 렌더
            self.draw_cells()
            self.draw_landing_blocks()


# -------------------- 싱글 플레이 상태 --------------------
class SinglePlayState:
    def __init__(self, screen: pygame.Surface, rm: ResourceManager, net_worker: NetworkWorker, session: Session, room_title: str, room_password: str = None):
        self.screen = screen
        self.room_title = room_title
        self.room_password = room_password

        self.rm = rm
        self.net_worker = net_worker
        self.my_session = session

        # UI 요소들
        self.board: Optional[TetrisBoard] = None
        self.btn_start: Optional[Button] = None

        self.btn_room_title: Optional[Button] = None
        self.btn_room_password: Optional[Button] = None
        self.btn_room_exit: Optional[Button] = None

        # 상단 텍스트용 폰트
        self.header_font = pygame.font.Font("resource/dodamdodam.ttf", 28)

        # 연속 전송용 변수
        self.left_pressed = False
        self.right_pressed = False
        self.down_pressed = False
        self.rotate_pressed = False
        self.drop_pressed = False

        self.first_delay_ms = 300
        self.delay_ms = 50

        self.left_elapsed_time = 0
        self.left_first_over = False
        self.left_first_move = False
        self.right_elapsed_time = 0
        self.right_first_over = False
        self.right_first_move = False
        self.down_elapsed_time = 0
        self.down_first_over = False
        self.down_first_move = False

        self.init()
        self.clear()

    def clear(self):
        self.left_pressed = False
        self.right_pressed = False
        self.down_pressed = False
        self.rotate_pressed = False
        self.drop_pressed = False

        self.left_elapsed_time = 0
        self.left_first_over = False
        self.left_first_move = False
        self.right_elapsed_time = 0
        self.right_first_over = False
        self.right_first_move = False
        self.down_elapsed_time = 0
        self.down_first_over = False
        self.down_first_move = False

    def handle_event(self, ev):
        if ev.key == pygame.K_LEFT:
            # move_type = LEFT
            if ev.type == pygame.KEYDOWN: 
                self.left_pressed = True
            elif ev.type == pygame.KEYUP: 
                self.left_pressed = False
                self.left_first_over = False
                self.left_first_move = False
                self.left_elapsed_time = 0

        elif ev.key == pygame.K_RIGHT:
            # move_type = RIGHT
            if ev.type == pygame.KEYDOWN: 
                self.right_pressed = True
            elif ev.type == pygame.KEYUP: 
                self.right_pressed = False
                self.right_first_over = False
                self.right_first_move = False
                self.right_elapsed_time = 0

        # 소프트 드랍
        elif ev.key == pygame.K_DOWN:
            # move_type = DOWN
            if ev.type == pygame.KEYDOWN:
                self.down_pressed = True
            elif ev.type == pygame.KEYUP:
                self.down_pressed = False
                self.down_first_over = False
                self.down_first_move = False
                self.down_elapsed_time = 0

        # 하드 드랍(스페이스)
        elif ev.key == pygame.K_SPACE:
            if ev.type == pygame.KEYDOWN: self.drop_pressed = True
            elif ev.type == pygame.KEYUP: self.drop_pressed = False
            # move_type = DROP

        # 회전(위)
        elif ev.key == pygame.K_UP:
            if ev.type == pygame.KEYDOWN: self.rotate_pressed = True
            elif ev.type == pygame.KEYUP: self.rotate_pressed = False

    # ------------ 네트워크 연동용 함수 (키 입력 → C2S_MOVE) ------------ # 

    def send_move_handler(self):
        if self.left_pressed:
            if self.left_first_over == False:
                if self.left_first_move == False:
                    self.send_move(LEFT)
                    self.left_first_move = True
                    
                if self.left_elapsed_time >= self.first_delay_ms:
                    self.send_move(LEFT)
                    self.left_first_over = True
                    self.left_elapsed_time = 0

            else:
                if self.left_elapsed_time >= self.delay_ms:
                    self.send_move(LEFT)
                    self.left_elapsed_time = 0
            

        if self.right_pressed:
            if self.right_first_over == False:
                if self.right_first_move == False:
                    self.send_move(RIGHT)
                    self.right_first_move = True

                if self.right_elapsed_time >= self.first_delay_ms:
                    self.send_move(RIGHT)
                    self.right_first_over = True
                    self.right_elapsed_time = 0

            else:
                if self.right_elapsed_time >= self.delay_ms:
                    self.send_move(RIGHT)
                    self.right_elapsed_time = 0

        # 소프트 드랍
        if self.down_pressed:
            if self.down_first_over == False:
                if self.down_first_move == False:
                    self.send_move(DOWN)
                    self.down_first_move = True
                if self.down_elapsed_time >= self.first_delay_ms:
                    self.send_move(DOWN)
                    self.down_first_over = True
                    self.down_elapsed_time = 0

            else:
                if self.down_elapsed_time >= self.delay_ms:
                    self.send_move(DOWN)
                    self.down_elapsed_time = 0
        # 하드 드랍(스페이스)
        if self.rotate_pressed:
            self.send_move(ROTATE)
            self.rotate_pressed = False
        # 회전(위)
        if self.drop_pressed:
            self.send_move(DROP)
            self.left_pressed = False
            self.right_pressed = False
            self.down_pressed = False
            self.rotate_pressed = False
            self.drop_pressed = False

    def send_move(self, move_type):
        size = 2 + 1 + 1
        type = C2S_MOVE
        self.move_type = move_type

        packet_bytes = struct.pack(
            "<hbb",
            size,
            type,
            self.move_type
        )

        MOVE_NAME = {LEFT: "LEFT", RIGHT: "RIGHT", DOWN: "DOWN", DROP: "DROP", ROTATE: "ROTATE"}
        # print("[C2S_MOVE] Send move_type =", MOVE_NAME.get(self.move_type, self.move_type))

        try:
            self.net_worker.send_packet(packet_bytes)
        except Exception as e:
            print("[SinglePlayState] send_move() error:", e)

    def send_start(self):
        size = 2 + 1
        type = C2S_START

        packet_bytes = struct.pack(
            "<hb",
            size,
            type,
        )

        try:
            self.net_worker.send_packet(packet_bytes)
        except Exception as e:
            print("[SinglePlayState] send_start() error:", e)

    def send_delete_user(self):
        size = 2 + 1
        type = C2S_DELETE_USER

        packet_bytes = struct.pack(
            "<hb",
            size,
            type,
        )

        try:
            self.net_worker.send_packet(packet_bytes)
        except Exception as e:
            print("[SinglePlayState] send_delete_user() error:", e)

    # ------------ 서버 → 클라 패킷 처리 ------------ #
    def handle_packet(self, data: Optional[dict]):
        from change_game_state import LobbyState

        packet_type = data.get("type")

        if packet_type == S2C_START:
            # 게임이 시작되었다고 서버가 알려줌
            if data.get("is_start") and self.board:
                self.board.start_game()
                pygame.mixer.music.play(-1)

        elif packet_type == S2C_MOVE:
            # move_type에 따라 보드에 반영
            move_type = data.get("move_type")
            if move_type is not None and self.board:
                self.board.handle_move(move_type)
                if move_type != DOWN:
                    self.rm.move_sound.play()

        elif packet_type == S2C_DELETE_USER:
            delete_id = data.get("id")
            if delete_id == self.my_session.id:
                pygame.mixer.music.stop()
                return LobbyState(self.screen, self.rm, self.net_worker, self.my_session)
            
        elif packet_type == S2C_SPAWN:
            print("[SPAWN DEBUG]", ", ".join(f"{k}={v}" for k, v in data.items()))
            if self.my_session.id == data.get("id"):
                self.board.current_tetromino = Tetromino(SHAPES_INDEX[data.get("tetromino_type")],data.get("spawn_x"), data.get("spawn_y"))
                self.board.next_tetromino_shape = SHAPES_INDEX[data.get("next_tetromino_type")]

        elif packet_type == S2C_FIX:
            if self.my_session.id == data.get("id"):
                if self.board.current_tetromino == None:
                    pass
                else:
                    self.board.fix(data.get("fixed_x"), data.get("fixed_y"))
                    self.rm.fix_sound.play()

        elif packet_type == S2C_CLEARLINE:
            if self.my_session.id == data.get("id"):
                self.board.clear_lines(data.get("line_index"))
                self.rm.clearline_sound.play()
                self.board.score = data.get("score")

        elif packet_type == S2C_ADDLINE:
            if self.my_session.id == data.get("id"):
                self.rm.addline_sound.play()
                self.board.add_line(data.get("hole_x"))

        elif packet_type == S2C_GAMEOVER:
            self.board.game_over = True
            self.board.current_tetromino = None
            self.board.game_started = False
            self.init()
            self.clear()
            pygame.mixer.music.stop()

        elif packet_type == S2C_UPDATE_SCORE:
            self.my_session.single_score = data.get("max_score")

        # 그 외 패킷은 현재 싱글플레이에서는 사용하지 않음
        return self

    # ------------ 레이아웃 초기화 ------------ #
    def init(self):
        self._build_layout()

    def _build_layout(self):
        sw, sh = self.screen.get_size()

        # --- 보드 / 미리보기 배치 ---
        board_w = BOARD_COLS * CELL_SIZE
        board_h = BOARD_ROWS * CELL_SIZE
        valid_w = board_w
        valid_h = (BOARD_ROWS - HIDDEN_ROWS) * CELL_SIZE
        preview_w = PREVIEW_COLS * CELL_SIZE
        preview_h = PREVIEW_ROWS * CELL_SIZE
        score_w = PREVIEW_COLS*CELL_SIZE
        score_h = PREVIEW_ROWS*CELL_SIZE

        total_w = board_w + preview_w
        total_h = board_h

        offset_x = (sw - total_w) // 2
        offset_y = (sh - total_h) // 2

        board_rect = pygame.Rect(
            offset_x,
            offset_y,
            board_w,
            board_h,
        )

        valid_rect = pygame.Rect(
            offset_x,
            offset_y + HIDDEN_ROWS*CELL_SIZE,
            valid_w,
            valid_h,
        )

        preview_rect = pygame.Rect(
            offset_x + board_w,
            offset_y + HIDDEN_ROWS*CELL_SIZE,
            preview_w,
            preview_h,
        )

        score_rect = pygame.Rect(
            offset_x + board_w,
            offset_y + HIDDEN_ROWS*CELL_SIZE + preview_h,
            score_w,
            score_h
        )

        self.board = TetrisBoard(
            screen=self.screen,
            board_rect=board_rect,
            valid_rect=valid_rect,
            preview_rect=preview_rect,
            score_rect=score_rect,
            session=self.my_session
        )

        # 게임 시작 버튼 (보드 중앙 아래쪽 정도에 배치)
        btn_w, btn_h = 200, 100
        btn_x = board_rect.centerx - btn_w // 2
        btn_y = board_rect.bottom - 200

        self.btn_start = Button(
            x=btn_x,
            y=btn_y,
            w=btn_w,
            h=btn_h,
            text="게임 시작",
            rm=self.rm,
            react=True,
        )

        # 방 제목 / 비밀번호용 상단 버튼 (단순한 박스 역할)
        header_w, header_h = 300, 50
        header_y = 0
        title_x = 0
        pw_x = header_w  # 바로 오른쪽에 붙이기

        # 상호작용 X: react=False, 텍스트는 직접 그릴 것이므로 text=None
        self.btn_room_title = Button(
            x=title_x,
            y=header_y,
            w=header_w,
            h=header_h,
            text=None,
            rm=self.rm,
            react=False,
        )
        self.btn_room_password = Button(
            x=pw_x,
            y=header_y,
            w=header_w,
            h=header_h,
            text=None,
            rm=self.rm,
            react=False,
        )
        exit_w, exit_h = 200, 100
        exit_x, exit_y = sw - exit_w, 0
        self.btn_room_exit = Button(
            x=exit_x,
            y=exit_y,
            w=exit_w,
            h=exit_h,
            text="나가기",
            rm=self.rm,
            react=True,
        )
    # ------------ 상단 방 정보 그리기 ------------ #
    def draw_room_header(self):
        """상단의 방 제목 / 비밀번호 영역을 그린다."""
        if not self.btn_room_title or not self.btn_room_password:
            return

        # 버튼 사각형만 그리기 (테두리)
        self.btn_room_title.draw(self.screen)
        self.btn_room_password.draw(self.screen)

        # 텍스트는 직접 렌더링
        title_text = f"방 제목: {self.room_title}"
        pw_text = f"비밀번호: {self.room_password or '없음'}"

        title_surf = self.header_font.render(title_text, True, (255, 255, 255))
        pw_surf = self.header_font.render(pw_text, True, (255, 255, 255))

        title_rect = title_surf.get_rect(center=self.btn_room_title.rect.center)
        pw_rect = pw_surf.get_rect(center=self.btn_room_password.rect.center)

        self.screen.blit(title_surf, title_rect)
        self.screen.blit(pw_surf, pw_rect)

    # ------------ 이벤트 처리 ------------ #
    def update(self, dt_ms, events):
        for ev in events:
            if ev.type == pygame.QUIT:
                # 상위 루프에서 처리
                continue

            if (ev.type == pygame.KEYDOWN or ev.type == pygame.KEYUP) and self.board and self.board.game_started:
                self.handle_event(ev)

            # # 게임 시작 버튼 클릭
            # elif ev.type == pygame.MOUSEBUTTONDOWN == 1:
            if self.btn_start and self.btn_start.handle_event(ev):
                self.send_start()

            if self.btn_room_exit.handle_event(ev):
                self.send_delete_user()
                
        if self.left_pressed: self.left_elapsed_time += dt_ms
        if self.right_pressed: self.right_elapsed_time += dt_ms
        if self.down_pressed: self.down_elapsed_time += dt_ms

        if self.board and self.board.game_started:
            self.send_move_handler()

        return self

    def draw(self):
        if self.board is None:
            return

        self.screen.fill((0, 0, 0))

        # 상단 방 제목/비밀번호
        self.draw_room_header()
        self.btn_room_exit.draw(self.screen)

        # 보드 및 미리보기/프로필/블록
        self.board.draw()

        # 게임 시작 버튼 (게임 시작 전)
        if not self.board.game_started and self.btn_start:
            self.btn_start.draw(self.screen)
