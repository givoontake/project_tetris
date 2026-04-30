import pygame
from typing import Optional

from tetris.config.define import *
from tetris.net.error_types import ERROR_MESSAGES, ROOM_NAME_TOO_SHORT, ROOM_PASSWORD_TOO_SHORT
from tetris.net.network import NetworkWorker
from tetris.net.packet_structs import MAX_INPUT
from tetris.net.session import Session
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.define import *
from tetris.ui.button import Button
from tetris.ui.inputbox import InputBox
from tetris.ui.popupbox import PopupBox
from tetris.ui.rectangle import Rectangle
from tetris.ui.toggle_button import ToggleButton


class CreateRoomWindow:
    WINDOW_WIDTH = 500
    WINDOW_HEIGHT = 500
    INPUT_HEIGHT = 50
    BUTTON_WIDTH = 120
    BUTTON_HEIGHT = 60
    TOP_PADDING = 35
    GAP = 20

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
        self.option_make: list[Button] = []
        self.error_popup: Optional[PopupBox] = None

        self.col_num = 5

        self.title_val: Optional[str] = None
        self.player_val: Optional[int] = None
        self.is_open: Optional[bool] = None
        self.password_val: Optional[str] = None

        self.set_layout()

    def set_layout(self):
        sw, sh = self.screen.get_size()

        self.rect.w = self.WINDOW_WIDTH
        self.rect.h = self.WINDOW_HEIGHT
        self.rect.x = (sw // 2) - (self.rect.w // 2)
        self.rect.y = (sh // 2) - (self.rect.h // 2)
        self.window = Rectangle(self.screen, self.rect, self.rm, True, self.rm.images.ui_images[UI_WINDOW_BACKGROUND], "")

        total_h = self.INPUT_HEIGHT * 2 + self.BUTTON_HEIGHT * 3 + self.GAP * 4
        draw_y = self.rect.y + (self.rect.h - total_h) // 2
        draw_h = self.INPUT_HEIGHT

        draw_w = self.rect.w - 80
        draw_x = self.rect.x + (self.rect.w - draw_w) // 2
        title_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        self.title = InputBox(self.screen, title_rect, self.rm, "방 제목 (4~16자)", MAX_INPUT, False, True)
        draw_y += draw_h + self.GAP

        op_player_texts = ["2인", "5인"]
        padding_w = (self.rect.w - self.BUTTON_WIDTH * len(op_player_texts)) // (len(op_player_texts) + 1)
        draw_x = self.rect.x + padding_w
        for op_text in op_player_texts:
            op_rect = pygame.Rect(draw_x, draw_y, self.BUTTON_WIDTH, self.BUTTON_HEIGHT)
            option = ToggleButton(self.screen, op_rect, self.rm, op_text)
            self.option_player.append(option)
            draw_x += self.BUTTON_WIDTH + padding_w
        self._set_player_value("2인")
        draw_y += self.BUTTON_HEIGHT + self.GAP

        op_open_texts = ["공개", "비공개"]
        padding_w = (self.rect.w - self.BUTTON_WIDTH * len(op_open_texts)) // (len(op_open_texts) + 1)
        draw_x = self.rect.x + padding_w
        for op_text in op_open_texts:
            op_rect = pygame.Rect(draw_x, draw_y, self.BUTTON_WIDTH, self.BUTTON_HEIGHT)
            option = ToggleButton(self.screen, op_rect, self.rm, op_text)
            self.option_open.append(option)
            draw_x += self.BUTTON_WIDTH + padding_w
        draw_y += self.BUTTON_HEIGHT + self.GAP

        draw_w = self.rect.w - 80
        draw_x = self.rect.x + (self.rect.w - draw_w) // 2
        pw_rect = pygame.Rect(draw_x, draw_y, draw_w, draw_h)
        self.password = InputBox(self.screen, pw_rect, self.rm, "비밀번호 (4~16자)", MAX_INPUT, False, True)
        self._set_open_value("공개")
        draw_y += draw_h + self.GAP

        op_make_texts = ["만들기", "취소"]
        padding_w = (self.rect.w - self.BUTTON_WIDTH * len(op_make_texts)) // (len(op_make_texts) + 1)
        draw_x = self.rect.x + padding_w
        for op_text in op_make_texts:
            op_rect = pygame.Rect(draw_x, draw_y, self.BUTTON_WIDTH, self.BUTTON_HEIGHT)
            option = Button(self.screen, op_rect, self.rm, op_text, 0)
            self.option_make.append(option)
            draw_x += self.BUTTON_WIDTH + padding_w

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
                if option.button.text == "만들기":
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
                    player_val = self.player_val if self.player_val in (2, 5) else 2
                    packet = self.net_worker.builder.build_create_room(
                        self.title_val,
                        player_val,
                        self.is_open,
                        self.password_val,
                    )
                    self.net_worker.send_packet(packet)

                return option.button.text

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
