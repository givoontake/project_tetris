import pygame
import struct
from typing import Optional, cast

from tetris.config.define import *
from tetris.net.session import Session
from tetris.net.network import NetworkWorker
from tetris.net.packet_types import *
from tetris.net.packet_structs import *
from tetris.game.tetromino import Tetromino
from tetris.game.tetris_board import TetrisBoard
from tetris.game.tetris_controller import TetrisController
from tetris.game.define import *
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.fonts import Fonts
from tetris.ui.rectangle import Rectangle
from tetris.ui.button import Button
from tetris.ui.toggle_button import ToggleButton

class TetrisSession:
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager,
                  net_worker: NetworkWorker, is_single: bool):
        self.screen = screen
        self.rm = rm
        self.net_worker = net_worker
        self.session: Optional[Session] = None
        self.is_single = is_single
        self.rect = rect
        self.controller = None
        self.state: TSessionState = TSessionState.EMPTY
        self.score = 0
        self.prev_packet_type = None # clearline 1번 재생을 위한 변수

        self.set_layout()

    def init_session(self, session: Session):
        self.session = session
        if session.is_my: self.controller = TetrisController(self.net_worker)
        self.board.init(session.block_texture)
        if self.is_single: self.set_score(0)
        else: self.nickname.set_text(self.session.nickname)
        self.state = TSessionState.WAIT

    def set_layout(self):
        board_rect = self.rect.copy()
        board_rect.h = self.rect.h*0.9
        self.board = TetrisBoard(self.screen, board_rect, self.rm)

        if self.is_single:
            ready_rect = self.board.valid_grid_rect.copy()
            ready_rect.y += ready_rect.h
            ready_rect.h = ready_rect.h*0.1
            self.ready = Button(self.screen, ready_rect, self.rm, None, "게임시작")

            score_rect = self.board.preview_rect.copy()
            score_rect.y += score_rect.h
            score_rect.h = score_rect.h // 2
            score_text = f"score: {self.score}"
            self.score_box = Rectangle(self.screen, score_rect, self.rm, None, score_text, 1)
            self.score_box.set_text_size(24)
        else:
            nickname_rect = self.board.valid_grid_rect.copy()
            nickname_rect.y += nickname_rect.h
            nickname_rect.h = nickname_rect.h*0.1
            self.nickname = Rectangle(self.screen, nickname_rect, self.rm, None, "", 1)

            ready_rect = nickname_rect.copy()
            ready_rect.x += nickname_rect.w
            ready_rect.w = self.board.preview_rect.w
            self.ready = ToggleButton(self.screen, ready_rect, self.rm, None, "준비")

    def set_score(self, new_score: Optional[int]):
        if self.is_single == False: return
        self.score = new_score
        score_text = f"score: {self.score}"
        self.score_box.set_text(score_text)

    def set_nickname(self, nickname: str):
        self.nickname.set_text(nickname)

    def set_ready(self, ready: bool):
        self.ready.active = ready

    def set_state(self, new_state: TSessionState):
        self.state = new_state

    def reset(self):
        self.board.reset()
        self.state = TSessionState.WAIT

    def clear(self):
        self.board.clear()
        self.state = TSessionState.EMPTY
        self.nickname.set_text("")
        self.session = None
        if self.is_single: self.set_score(0)
        if self.controller: self.controller.clear()

    def handle_packet(self, data: Optional[RecvPacketStruct]):
        if data.type == S2C_MOVE:
            move_data = cast(S2C_MOVE_PACKET, data)
            self.board.handle_move(move_data.move_type)
            
        elif data.type == S2C_SPAWN:
            spawn_data = cast(S2C_SPAWN_PACKET, data)
            self.board.current_tetromino = Tetromino(SHAPES_INDEX[spawn_data.tetromino_type], spawn_data.spawn_x, spawn_data.spawn_y)
            self.board.next_tetromino_shape = SHAPES_INDEX[spawn_data.next_tetromino_type]

        elif data.type == S2C_FIX:
            self.prev_packet_type = data.type # 서버는 clearline 전에 반드시 fix를 보냄. 그 점을 이용해 clearline 비교에 사용
            fix_data = cast(S2C_FIX_PACKET, data)
            if self.board.current_tetromino == None:
                pass
            else:
                self.board.fix(fix_data.fixed_x, fix_data.fixed_y)

        elif data.type == S2C_CLEARLINE: 
            clearline_data = cast(S2C_CLEARLINE_PACKET, data)
            self.board.clear_lines(clearline_data.line_index)
            self.set_score(clearline_data.score)
            if self.prev_packet_type != S2C_CLEARLINE: # 애니메이션 1번 재생 -> 서버는 clearline을 여러번 연속해서 보내는 점을 이용
                self.prev_packet_type = data.type
                self.board.animate_combo(clearline_data.line_index, clearline_data.combo)

        # 멀티용 클리어라인 패킷 추가 필요 (스코어 제거 버전)

        elif data.type == S2C_ADDLINE:
            addline_data = cast(S2C_ADDLINE_PACKET, data)
            self.board.add_line(addline_data.hole_x)

    def handle_event(self, ev: pygame.event.Event):
        if self.session == None: return
        if self.session.is_my == False: # 내꺼 아니면 할 필요가 없음
            return
        
        if self.state == TSessionState.PLAY:
            if self.controller: self.controller.handle_event(ev)

        else:
            if self.ready.handle_event(ev):
                self.send_start()

    def send_start(self):
        data = C2S_START_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_START
        values = self.net_worker._pm.struct_to_values(data)
        packet_bytes = struct.pack(data.FMT, *values)

        try:
            self.net_worker.send_packet(packet_bytes)
        except Exception as e:
            print("[TetrisSession] send_start() error:", e)

    def update(self, dt_ms):
        if self.state == TSessionState.PLAY:
            if self.controller: self.controller.update(dt_ms)
            self.board.update(dt_ms)

    def draw(self):
        self.board.draw_frame()
        if self.state == TSessionState.PLAY:
            self.board.draw_game()
            self.board.draw_combo()

        if self.state == TSessionState.WAIT:
            self.ready.draw()

        if self.is_single:
            self.score_box.draw()
        else:
            self.nickname.draw()

