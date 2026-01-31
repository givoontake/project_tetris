import pygame
import struct
from typing import Optional

from tetris.config.define import *
from tetris.net.session import Session
from tetris.net.network import NetworkWorker
from tetris.net.packet_type import *
from tetris.game.tetromino import Tetromino
from tetris.game.tetris_board import TetrisBoard
from tetris.game.tetris_controller import TetrisController
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.font_manager import FontManager
from tetris.ui.rectangle import Rectangle
from tetris.ui.button import Button
from tetris.ui.toggle_button import ToggleButton

class TetrisSession:
    def __init__(self, screen: pygame.Surface, board_rect: pygame.Rect, rm: ResourceManager, fm: FontManager,
                  net_worker: NetworkWorker, session: Session, is_single: bool):
        self.screen = screen
        self.rm = rm
        self.fm = fm
        self.net_worker = net_worker
        self.session = session
        self.is_single = is_single
        self.board = TetrisBoard(screen, board_rect, rm, fm, session)
        self.controller = None

        if session.is_my: self.controller = TetrisController(net_worker)
        self.score = 0
        self.set_layout()

    def set_layout(self):
        if self.is_single:
            ready_rect = self.board.grid_rect.copy()
            ready_rect.y += ready_rect.h
            ready_rect.h = ready_rect.h*0.1
            self.ready = Button(self.screen, ready_rect, self.rm, self.fm, None, "게임시작")

            score_rect = self.board.preview_rect.copy()
            score_rect.y += score_rect.h
            score_rect.h = score_rect.h // 2
            score_text = f"score: {self.score}"
            self.score_box = Rectangle(self.screen, score_rect, self.fm, None, score_text, 1)
            self.score_box.set_text_size(24)
        else:
            nickname_rect = self.board.grid_rect.copy()
            nickname_rect.y += nickname_rect.h
            nickname_rect.h = nickname_rect.h*0.1
            self.nickname = Rectangle(self.screen, nickname_rect, self.fm, None, "")

            ready_rect = nickname_rect.copy()
            ready_rect.x += nickname_rect.w
            ready_rect.w = self.board.preview_rect.w
            self.ready = ToggleButton(self.screen, ready_rect, self.rm, self.fm, None, "준비")

    def set_score(self, new_score: Optional[int]):
        if self.is_single == False: return
        self.score = new_score
        score_text = f"score: {self.score}"
        self.score_box.set_text(score_text)

    def set_nickname(self, nickname: str):
        self.nickname.set_text(nickname)

    def set_ready(self, ready: bool):
        self.ready.active = ready

    def clear(self):
        self.board.clear()
        if self.is_single: self.set_score(0)
        if self.controller: self.controller.clear()

    def handle_packet(self, data: Optional[dict]):
        packet_type = data.get("type")

        if packet_type == S2C_MOVE:
            # move_type에 따라 보드에 반영
            move_type = data.get("move_type")
            if move_type is not None:
                self.board.handle_move(move_type)
            
        elif packet_type == S2C_SPAWN:
            # print("[SPAWN DEBUG]", ", ".join(f"{k}={v}" for k, v in data.items()))
            self.board.current_tetromino = Tetromino(SHAPES_INDEX[data.get("tetromino_type")],data.get("spawn_x"), data.get("spawn_y"))
            self.board.next_tetromino_shape = SHAPES_INDEX[data.get("next_tetromino_type")]

        elif packet_type == S2C_FIX:
            if self.board.current_tetromino == None:
                pass
            else:
                self.board.fix(data.get("fixed_x"), data.get("fixed_y"))

        elif packet_type == S2C_CLEARLINE: 
            self.board.clear_lines(data.get("line_index"))
            self.set_score(data.get("score"))

        # 멀티용 클리어라인 패킷 추가 필요 (스코어 제거 버전)

        elif packet_type == S2C_ADDLINE:
            self.board.add_line(data.get("hole_x"))

    def handle_event(self, ev: pygame.event.Event):
        if self.board.game_started:
            if self.controller: self.controller.handle_event(ev)

        else:
            if self.ready.handle_event(ev):
                self.send_start()

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
            print("[TetrisSession] send_start() error:", e)

    def update(self, dt_ms):
        if self.board.game_started:
            if self.controller: self.controller.update(dt_ms)

    def draw(self):
        self.board.draw()
        if self.board.game_started == False:
            self.ready.draw()

        if self.is_single:
            self.score_box.draw()
        else:
            self.nickname.draw()

