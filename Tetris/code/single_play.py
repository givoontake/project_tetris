import pygame
import struct
from typing import Optional

from define import *
from packet_type import *

from session import Session
from network import NetworkWorker
from resource_manager import ResourceManager

from button import Button
from tetris_board import TetrisBoard
from tetromino import Tetromino
from tetris_board import *

RIGHT = 0
LEFT = 1
ROTATE = 2 
DOWN = 3
DROP = 4
UP = 5

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
            from lobby_state import LobbyState
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
            self.my_session.max_score = data.get("max_score")

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
