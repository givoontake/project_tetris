import pygame
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

class RoomSession:
    def __init__(self, screen: pygame.Surface, board_rect: pygame.Rect, rm: ResourceManager, fm: FontManager,
                  net_worker: NetworkWorker, session: Session, is_my_session: bool):
        self.board = TetrisBoard(screen, board_rect, rm, fm, session.block_texture)
        self.net_worker = net_worker
        self.session = session
        self.is_my_session = is_my_session
        if is_my_session: self.controller = TetrisController(net_worker, session)

    def clear(self):
        self.board.clear()
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
            self.board.set_score(data.get("score"))

        # 멀티용 클리어라인 패킷 추가 필요 (스코어 제거 버전)

        elif packet_type == S2C_ADDLINE:
            self.board.add_line(data.get("hole_x"))

    def handle_event(self, ev):
        if self.board.game_started:
            self.controller.handle_event(ev)

    def update(self, dt_ms):
        if self.board.game_started:
            self.controller.update(dt_ms)