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
from tetris.resources.images import *
from tetris.resources.fonts import Fonts, DEFAULT_FONT_SIZE
from tetris.resources.define_colors import *
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
        self.text_size = DEFAULT_FONT_SIZE
        self.prev_packet_type = None # clearline 1번 재생을 위한 변수

        self.set_layout()

    def init_session(self, session: Session):
        self.session = session
        if self.session.is_self == False and self.btn_ready: self.btn_ready.reactable = False
        if session.is_self: self.controller = TetrisController(self.net_worker)
        self.board.init()
        self.nickname.set_text(self.session.nickname)
        if self.is_single: self.set_score(0)
        self.board.set_textures(self.session.block_textures)
        self.state = TSessionState.WAIT

    def set_layout(self):
        board_rect = self.rect.copy()
        board_rect.h = self.rect.h*0.9
        self.board = TetrisBoard(self.screen, board_rect, self.rm)
        nickname_rect = self.board.valid_grid_rect.copy()
        nickname_rect.y += nickname_rect.h
        nickname_rect.h = nickname_rect.h*0.1
        self.nickname = Rectangle(self.screen, nickname_rect, self.rm, False, None, "", 1)
        self.nickname.set_background_color((0, 0, 0, 96))
        self.nickname.border_color = GOLD
        self.nickname.set_text_size(20)
        self.btn_start = None
        self.btn_ready = None
        self.btn_kick = None

        if self.is_single:
            start_rect = nickname_rect.copy()
            start_rect.x += nickname_rect.w
            start_rect.w = self.board.preview_rect.w
            self.btn_start = Button(self.screen, start_rect, self.rm, "게임시작", 1)

            self.btn_start.set_text_size(20)
            score_rect = self.board.preview_rect.copy()
            score_rect.y += score_rect.h
            score_rect.h = score_rect.h // 2
            score_text = f"score: {self.score}"
            self.score_box = Rectangle(self.screen, score_rect, self.rm, False, None, score_text, 1)
            self.score_box.set_background_color((0, 0, 0, 96))
            self.score_box.border_color = GOLD
            self.score_box.set_text_size(20)
        else:
            self.ready_rect = nickname_rect.copy()
            self.ready_rect.x += nickname_rect.w
            self.ready_rect.w = self.board.preview_rect.w
            self.btn_ready = ToggleButton(self.screen, self.ready_rect, self.rm, "준비")
            self.btn_ready.set_text_size(20)

            crown_rect = self.ready_rect.copy()
            crown_image = self.rm.images.ui_images[UI_HOST]
            self.crown = Rectangle(self.screen, crown_rect, self.rm, True, crown_image, "")

            self.is_host = False

    def set_score(self, new_score: Optional[int]):
        if self.is_single == False: return
        self.score = new_score
        score_text = f"score: {self.score}"
        self.score_box.set_text(score_text)

    def set_nickname(self, nickname: str):
        self.nickname.set_text(nickname)

    def set_ready(self, is_ready: bool):
        if self.is_single: return
        self.btn_ready.set_pressed(is_ready)

    def set_is_host(self):
        self.is_host = True # 방장 양도는 계획에 없다.
        if self.session.is_self == False: self.btn_ready.visible = False # 다른 플레이어의 준비 버튼은 표시하지 않는다.
        else: 
            self.btn_ready = None
            self.btn_start = Button(self.screen, self.ready_rect, self.rm, "게임시작", 1)

        if self.btn_start:
            self.btn_start.set_text_size(20)

    def set_state(self, new_state: TSessionState):
        self.state = new_state

    def set_multiplayer_text_size(self, new_size: int):
        if self.is_single: return
        self.text_size = new_size
        if self.btn_ready:
            self.btn_ready.set_text_size(new_size)
        if self.btn_start:
            self.btn_start.set_text_size(new_size)
        if self.btn_kick:
            self.btn_kick.set_text_size(new_size)

    def make_btn_kick(self):
        kick_rect_h = self.ready_rect.h // 2
        kick_rect_w = kick_rect_h
        kick_rect_x = self.ready_rect.x - kick_rect_w
        kick_rect_y = self.ready_rect.y
        kick_rect = pygame.Rect(kick_rect_x, kick_rect_y, kick_rect_w, kick_rect_h)
        self.btn_kick = Button(self.screen, kick_rect, self.rm, "X", 1)
        self.btn_kick.set_text_size(self.text_size)
    
    def process_gameover(self):
        self.state = TSessionState.GAMEOVER_ANIMATING
        self.board.find_anim_start_line()

    def reset(self):
        self.board.reset()
        if self.btn_ready: self.btn_ready.set_pressed(False)
        self.state = TSessionState.WAIT

    def clear(self):
        self.board.clear()
        self.state = TSessionState.EMPTY
        self.nickname.set_text("")
        self.session = None
        self.is_host = False
        if self.btn_ready:
            self.btn_ready.visible = True
        if self.is_single: self.set_score(0)
        if self.controller: self.controller.clear()

    def handle_packet(self, data: Optional[IngamePacket]):
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
            if self.prev_packet_type != S2C_CLEARLINE: # 연속된 줄 제거 패킷에는 애니메이션을 한 번만 재생한다.
                self.prev_packet_type = data.type
                self.board.animate_combo(clearline_data.line_index, clearline_data.combo)


        elif data.type == S2C_ADDLINE:
            addline_data = cast(S2C_ADDLINE_PACKET, data)
            self.board.add_line(addline_data.hole_x)

    def handle_event(self, ev: pygame.event.Event) -> Optional[str]:
        if self.session == None: return
        
        if self.state == TSessionState.PLAY:
            if self.controller: self.controller.handle_event(ev)

        else:
            if self.btn_start: 
                if self.btn_start.handle_event(ev): return "start"

            if self.btn_ready:
                if self.btn_ready.handle_event(ev): return "ready"
            
            if self.btn_kick:
                if self.btn_kick.handle_event(ev): return "kick"

        return None

    def update(self, dt_ms):
        if self.state == TSessionState.PLAY: 
            if self.controller: self.controller.update(dt_ms)
            self.board.update(dt_ms)
        
        if self.state == TSessionState.GAMEOVER_ANIMATING:
            if self.board.animate_gameover(dt_ms):
                if self.is_single: self.reset()
                else: self.state = TSessionState.GAMEOVER

    def draw(self):
        self.board.draw_frame()
        if self.session == None: return
        if self.state == TSessionState.PLAY or TSessionState.GAMEOVER_ANIMATING:
            self.board.draw_game()
            if self.is_single: self.board.draw_combo()

        self.nickname.draw()
        if self.is_single:
            self.score_box.draw()
        if self.session.is_self == False and self.is_host: self.crown.draw() #본인이 아닌 방장의 경우 위에 덧그려지는 형태

        # 호스트는 왕관으로 표시하고 다른 플레이어의 준비 버튼 입력은 비활성화한다.
        if self.state == TSessionState.WAIT:
            if self.btn_start: 
                self.btn_start.draw()
            if self.btn_ready:
                self.btn_ready.draw()
            if self.btn_kick:
                self.btn_kick.draw()
