import pygame
from typing import Optional

from tetris.config.define import *
from tetris.net.error_types import ERROR_MESSAGES, ROOM_NAME_TOO_SHORT, ROOM_PASSWORD_TOO_SHORT
from tetris.net.network import NetworkWorker
from tetris.net.packet_structs import MAX_INPUT
from tetris.net.session import Session
from tetris.resources.resource_manager import ResourceManager
from tetris.ui.inputbox import InputBox
from tetris.ui.popupbox import PopupBox
from tetris.ui.rectangle import Rectangle
from tetris.ui.toggle_button import ToggleButton


class CreateRoomWindow:
    TOP_PADDING = 50
    BOTTOM_PADDING = 50
    ROW_HEIGHT = 50
    GAP = 50
    WIDTH = 600

    LEFT_PADDING = 20
    RIGHT_PADDING = 20

    def __init__(self, screen: pygame.Surface, rm: ResourceManager, net_worker: NetworkWorker, my_session: Session):
        self.screen = screen
        self.rm = rm
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
        self.error_popup: Optional[PopupBox] = None

        self.col_num = 5

        self.title_val: Optional[str] = None
        self.player_val: Optional[int] = None
        self.is_open: Optional[bool] = None
        self.password_val: Optional[str] = None

        self.set_layout()

    def set_layout(self):
        sw, sh = self.screen.get_size()

        self.rect.w = sw // 2
        self.rect.h = sh // 2
        self.rect.x = (sw // 2) - (self.rect.w // 2)
        self.rect.y = (sh // 2) - (self.rect.h // 2)
        outline_padding_w = self.rect.w // 10
        outline_padding_h = self.rect.h // 10
        inner_padding_w = self.rect.w // 20
        inner_padding_h = self.rect.h // 20
        self.window = Rectangle(self.screen, self.rect, self.rm, None, "")

        draw_x = self.rect.x + outline_padding_w
        draw_y = self.rect.y + outline_padding_h
        draw_h = (self.rect.h - (outline_padding_h * 2 + inner_padding_h * (self.col_num - 1))) // self.col_num

        draw_w = self.rect.w - outline_padding_w * 2
        title_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        self.title = InputBox(self.screen, title_rect, self.rm, "4~16자", MAX_INPUT, False, True)
        draw_y += draw_h + inner_padding_h

        op_player_texts = ["1인", "2인", "5인"]
        draw_w = (self.rect.w - (outline_padding_w * 2 + inner_padding_w * (len(op_player_texts) - 1))) // len(op_player_texts)
        for op_text in op_player_texts:
            op_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
            option = ToggleButton(self.screen, op_rect, self.rm, None, op_text)
            self.option_player.append(option)
            draw_x += draw_w + inner_padding_w
        draw_x = self.rect.x + outline_padding_w
        draw_y += draw_h + inner_padding_h

        op_open_texts = ["공개", "비공개"]
        draw_w = (self.rect.w - (outline_padding_w * 2 + inner_padding_w * (len(op_open_texts) - 1))) // len(op_open_texts)
        for op_text in op_open_texts:
            op_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
            option = ToggleButton(self.screen, op_rect, self.rm, None, op_text)
            self.option_open.append(option)
            draw_x += draw_w + inner_padding_w
        draw_x = self.rect.x + outline_padding_w
        draw_y += draw_h + inner_padding_h

        draw_w = self.rect.w - outline_padding_w * 2
        pw_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        self.password = InputBox(self.screen, pw_rect, self.rm, "4~16자", MAX_INPUT, False, True)
        draw_y += draw_h + inner_padding_h

        op_make_texts = ["만들기", "취소"]
        draw_w = (self.rect.w - (outline_padding_w * 2 + inner_padding_w * (len(op_make_texts) - 1))) // len(op_make_texts)
        for op_text in op_make_texts:
            op_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
            option = ToggleButton(self.screen, op_rect, self.rm, None, op_text)
            self.option_make.append(option)
            draw_x += draw_w + inner_padding_w

    def _set_player_value(self, val: str):
        for option in self.option_player:
            option.pressed = option.text == val
        self.player_val = int(val.replace("인", ""))

    def _set_open_value(self, val: str):
        for option in self.option_open:
            option.pressed = option.text == val

        if val == "공개":
            self.is_open = True
            self.password.active = False
        elif val == "비공개":
            self.is_open = False
            self.password.active = True

    def _is_min_length(self, value: str) -> bool:
        return len(value.encode("utf-8")) >= 4

    def update(self, dt_ms: int):
        self.title.update(dt_ms)
        self.password.update(dt_ms)

    def handle_event(self, ev: pygame.event.Event) -> Optional[str]:
        if self.error_popup:
            if self.error_popup.handle_event(ev):
                self.error_popup = None
            return None

        if ev.type == pygame.KEYDOWN and ev.key == pygame.K_RETURN:
            return None

        self.title.handle_event(ev)

        for option in self.option_player:
            if option.handle_event(ev):
                self._set_player_value(option.text)
                break

        for option in self.option_open:
            if option.handle_event(ev):
                self._set_open_value(option.text)
                break

        if self.is_open is False:
            self.password.handle_event(ev)

        for option in self.option_make:
            if option.handle_event(ev):
                if option.text == "만들기":
                    title_val = self.title.get_total_text()
                    password_val = self.password.get_total_text()

                    if not self._is_min_length(title_val):
                        self.error_popup = PopupBox(
                            self.screen,
                            self.rm,
                            ERROR_MESSAGES[ROOM_NAME_TOO_SHORT],
                            ["확인"],
                        )
                        return None

                    if self.is_open is False and not self._is_min_length(password_val):
                        self.error_popup = PopupBox(
                            self.screen,
                            self.rm,
                            ERROR_MESSAGES[ROOM_PASSWORD_TOO_SHORT],
                            ["확인"],
                        )
                        return None

                    self.title_val = self.title.extract_text()
                    self.password_val = self.password.extract_text()
                    packet = self.net_worker.builder.build_create_room(
                        self.title_val,
                        self.player_val,
                        self.is_open,
                        self.password_val,
                    )
                    self.net_worker.send_packet(packet)

                return option.text

        return None

    def draw(self):
        self.window.draw()

        self.title.draw()
        for option in self.option_player:
            option.draw()

        for option in self.option_open:
            option.draw()

        self.password.draw()

        for option in self.option_make:
            option.draw()

        if self.error_popup:
            self.error_popup.draw()
