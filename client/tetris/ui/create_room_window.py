import pygame
from typing import Optional
from tetris.ui.button import Button
from tetris.ui.inputbox import InputBox
from tetris.config.define import *
from tetris.net.define_format import *
from tetris.resources.resource_manager import *
from tetris.resources.font_manager import FontManager
from tetris.ui.rectangle import Rectangle
from tetris.net.network import NetworkWorker
from tetris.net.session import Session
from tetris.ui.toggle_button import ToggleButton
from tetris.net.packet_manager import *

class RoomCreateWindow:
    """
    방 만들기 창.

    - 제목: InputBox
    - 인원: 1인 / 2인 / 5인 (ToggleButton 그룹)
    - 공개 / 비공개: ToggleButton 그룹
    - 암호: InputBox (비공개일 때만 보이고 사용됨)
    - 하단: Button("방 만들기"), Button("취소")
    """
    TOP_PADDING = 50
    BOTTOM_PADDING = 50
    ROW_HEIGHT = 50
    GAP = 50
    WIDTH = 600

    # 🔹 좌우 패딩 추가
    LEFT_PADDING = 20
    RIGHT_PADDING = 20

    def __init__(self, screen: pygame.Surface, rm: ResourceManager, fm: FontManager, net_worker: NetworkWorker, my_session: Session):
        self.screen = screen
        self.rm = rm
        self.fm = fm
        self.net_worker = net_worker
        self.my_session = my_session
        self.rect = pygame.Rect(0, 0, 0, 0)
        self.window: Rectangle
        self.background: Rectangle

        self.title: InputBox = None
        self.option_player: list[ToggleButton] = []
        self.option_open: list[ToggleButton] = []
        self.password: InputBox = None
        self.option_make: list[ToggleButton] = []

        self.col_num = 5 # 총 5줄

        self.title_val: Optional[str] = None
        self.player_val: Optional[int] = None
        self.is_open: Optional[bool] = None
        self.password_val: Optional[str] = None
        
        self.visible = False

        self.set_layout()

    # -------- 외부에서 제어 -------- #
    def open(self):
        self.visible = True

    def close(self):
        self.visible = False

    def _reset(self):
        self.title_val = None
        self.player_val = None
        self.is_open = None
        self.password_val = None

    # def on_resize(self, new_screen: pygame.Surface):
    #     self.screen = new_screen
    #     self.set_layout()

    def set_layout(self):
        sw, sh = self.screen.get_size()
        # background_rect = pygame.Rect(0, 0, sw, sh)
        # self.background = Rectangle(self.screen, background_rect, self.fm, None, "")
        
        self.rect.w = sw // 2
        self.rect.h = sh // 2
        self.rect.x = (sw // 2) - (self.rect.w // 2)
        self.rect.y = (sh // 2) - (self.rect.h // 2)
        outline_padding_w = self.rect.w // 10 # 10%
        outline_padding_h = self.rect.h // 10
        inner_padding_w = self.rect.w // 20 # 5%
        inner_padding_h = self.rect.h // 20
        self.window = Rectangle(self.screen, self.rect, self.fm, None, "")
        # 각 h은 전체 줄수와 관련이 있다.

        # copy_rect = pygame.Rect(0, 0, 0, 0) # copy로 여러 복사본을 만들기 위해 값 복사용 rect 생성
        draw_x, draw_y = self.rect.x + outline_padding_w, self.rect.y + outline_padding_h # 우선 시작 좌표, 계속 갱신할 예정
        draw_h = (self.rect.h - (outline_padding_h*2 + inner_padding_h*(self.col_num - 1))) // self.col_num # 고정값
        # draw_w는 그때그때 계산
        # draw_ 는 레이아웃 값 조작용, 이 값을 기준으로 복사본을 생성해 계속 넣어주면 된다.
        
        draw_w = self.rect.w - outline_padding_w*2
        title_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        self.title = InputBox(self.screen, title_rect, self.fm, "방 제목", MAX_INPUT, False, True)
        draw_y += draw_h + inner_padding_h

        op_player_texts = ["1인", "2인", "5인"]
        draw_w = (self.rect.w - (outline_padding_w*2 + inner_padding_w*(len(op_player_texts) - 1))) // len(op_player_texts)
        for op_text in op_player_texts:
            op_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
            option = ToggleButton(self.screen, op_rect, self.rm, self.fm, None, op_text)
            self.option_player.append(option)
            draw_x += (draw_w + inner_padding_w)
        draw_x = self.rect.x + outline_padding_w
        draw_y += draw_h + inner_padding_h

        op_open_texts = ["공개", "비공개"]
        draw_w = (self.rect.w - (outline_padding_w*2 + inner_padding_w*(len(op_open_texts) - 1))) // len(op_open_texts)
        
        for op_text in op_open_texts:
            op_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
            option = ToggleButton(self.screen, op_rect, self.rm, self.fm, None, op_text)
            self.option_open.append(option)
            draw_x += (draw_w + inner_padding_w)
        draw_x = self.rect.x + outline_padding_w
        draw_y += draw_h + inner_padding_h

        draw_w = self.rect.w - outline_padding_w*2
        pw_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        self.password = InputBox(self.screen, pw_rect, self.fm, "비밀번호", MAX_INPUT, False, True)
        draw_y += draw_h + inner_padding_h

        op_make_texts = ["만들기", "취소"]
        draw_w = (self.rect.w - (outline_padding_w*2 + inner_padding_w*(len(op_make_texts) - 1))) // len(op_make_texts)
        
        for op_text in op_make_texts:
            op_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
            option = ToggleButton(self.screen, op_rect, self.rm, self.fm, None, op_text)
            self.option_make.append(option)
            draw_x += (draw_w + inner_padding_w)

    def _send_create_room(self):
        if self.title_val == None: return
        if self.player_val == None: return
        if self.is_open == None: return
        if self.is_open == False: 
            if self.password_val == None: return
        id = self.my_session.id
        encoded_title = self.title_val.encode("utf-8")
        
        if self.is_open:
            size = 2+1+4+1+MAX_ROOM_NAME
            type = C2S_ADD_OPEN_ROOM
            packet_bytes = struct.pack(
                f"<hbib{MAX_ROOM_NAME}s",
                size,
                type,
                id,
                int(self.player_val),
                encoded_title
            )

        else:
            size = 2+1+4+1+MAX_ROOM_NAME + MAX_ROOM_PASSWORD
            type = C2S_ADD_LOCK_ROOM
            encoded_password = self.password_val.encode("utf-8")           

            packet_bytes = struct.pack(
                f"<hbib{MAX_ROOM_NAME}s{MAX_ROOM_PASSWORD}s",
                size,
                type,
                id,
                int(self.player_val),
                encoded_title,
                encoded_password
            )
            
        try:
            self.net_worker.send_packet(packet_bytes)
        except Exception as e:
            print("[LoginState] send_create_room() error:", e)


    # -------- 내부 상태 보조 -------- #
    def _set_player_value(self, val: str):
        for option in self.option_player:
            if option.text == val: option.pressed = True
            else: option.pressed = False

        int_val = int(val.replace("인", "")) # "인"을 모두 찾아 ""()빈칸으로 대체
        self.player_val = int_val

    def _set_open_value(self, val: str):
        for option in self.option_open:
            if option.text == val: option.pressed = True
            else: option.pressed = False
        if val == "공개": 
            self.is_open = True
            self.password.active = False
        elif val == "비공개":
            self.is_open = False
            self.password.active = True

    # -------- 프레임 업데이트 -------- #
    def update(self, dt_ms: int):
        if not self.visible:
            return
        
        self.title.update(dt_ms)
        self.password.update(dt_ms)

    # -------- 이벤트 처리 (버튼 이벤트 반환) -------- #
    def handle_event(self, ev: pygame.event.Event):
        if not self.visible:
            return None
        
        if ev.type == pygame.KEYDOWN and ev.key == pygame.K_RETURN:
            # input의 enter 키는 입력된 문자열을 반환하고 초기화
            return
        
        self.title.handle_event(ev)

        for option in self.option_player:
            if option.handle_event(ev):
                self._set_player_value(option.text)
                break

        for option in self.option_open:
            if option.handle_event(ev):
                self._set_open_value(option.text)
                break

        if self.is_open == False:
            self.password.handle_event(ev)

        for option in self.option_make:
            if option.handle_event(ev):
                if option.text == "만들기":
                    self.title_val = self.title.extract_text()
                    self.password_val = self.password.extract_text()
                    self._send_create_room()
                elif option.text == "취소":
                    pass
                self.visible = False
                self._reset()

    # -------- 그리기 -------- #
    def draw(self):
        if not self.visible:
            return
        
        self.window.draw()

        self.title.draw()
        for option in self.option_player:
            option.draw()

        for option in self.option_open:
            option.draw()

        self.password.draw()

        for option in self.option_make:
            option.draw()
