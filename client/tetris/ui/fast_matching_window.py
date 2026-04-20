import pygame
from typing import Optional
from tetris.ui.popupbox import PopupBox
from tetris.config.define import *
from tetris.resources.define import *
from tetris.resources.resource_manager import *
from tetris.resources.fonts import Fonts
from tetris.ui.rectangle import Rectangle
from tetris.net.network import NetworkWorker
from tetris.net.session import Session
from tetris.ui.toggle_button import ToggleButton

class FastMatchingWindow:
    WINDOW_WIDTH = 500
    WINDOW_HEIGHT = 500
    BUTTON_WIDTH = 120
    BUTTON_HEIGHT = 60

    def __init__(self, screen: pygame.Surface, rm: ResourceManager, net_worker: NetworkWorker):
        self.screen = screen
        self.rm = rm
        self.net_worker = net_worker

        self.rect = pygame.Rect(0, 0, 0, 0)
        self.popup = PopupBox(
            self.screen,
            self.rm,
            "",
            ["찾기", "취소"],
            self.WINDOW_WIDTH,
            self.WINDOW_HEIGHT,
            UI_WINDOW_BACKGROUND,
            self.BUTTON_WIDTH,
            self.BUTTON_HEIGHT,
        )
        self.options: list[ToggleButton] = []
        self.option_val = None

        self.set_layout()

    def set_layout(self):
        rect = self.popup.message_window.rect.copy() # 메세지 띄우는 영역에 대신 토글 버튼으로 선택하도록
        option_texts = ["전체", "2인", "5인"]
        option_num = len(option_texts) # 옵션 수
        padding_num = option_num + 1 # 사용될 패딩 수
        option_w = self.BUTTON_WIDTH
        option_h = self.BUTTON_HEIGHT
        padding_w = (rect.w - option_w * option_num) // padding_num
        option_y = rect.y + (rect.h - option_h) // 2
        
        for i in range(len(option_texts)):
            option_x = rect.x + option_w*i + padding_w*(i + 1) # 첫 패딩은 적용된 상태로 그려야함
            option_rect = pygame.Rect(option_x, option_y, option_w, option_h)
            option = ToggleButton(self.screen, option_rect, self.rm, option_texts[i])
            self.options.append(option)

    def set_option_val(self):
        for option in self.options:
            if option.text == self.option_val: option.set_pressed(True)
            else: option.set_pressed(False)

    def send_fast_matching(self):
        max_user = 0
        if self.option_val == "2인": max_user = 2
        elif self.option_val == "5인": max_user = 5
        elif self.option_val == "전체": max_user = 0
        else: return
        packet = self.net_worker.builder.build_fast_matching(max_user)
        self.net_worker.send_packet(packet)
        
    def handle_event(self, ev: pygame.event.Event) -> Optional[str]:
        for option in self.options:
            if option.handle_event(ev): 
                self.option_val = option.text
                self.set_option_val()
                break
        
        res = self.popup.handle_event(ev)
        if res != None:
            if res == "찾기":
                if self.option_val != None: self.send_fast_matching()

            return res
        return None
                
    def draw(self):
        self.popup.draw()

        for option in self.options:
            option.draw()

        
