import pygame
from typing import Optional

from tetris.resources.define import *
from tetris.resources.resource_manager import ResourceManager
from tetris.ui.popupbox import PopupBox

QUICK_START_MESSAGE = "모드를 선택하세요"
QUICK_START_SINGLE_TEXT = "싱글"
QUICK_START_MULTI_TEXT = "멀티"
QUICK_START_CANCEL_TEXT = "취소"


class QuickStartWindow:
    WINDOW_WIDTH = 500
    WINDOW_HEIGHT = 500
    BUTTON_WIDTH = 120
    BUTTON_HEIGHT = 60

    def __init__(self, screen: pygame.Surface, rm: ResourceManager):
        self.screen = screen
        self.rm = rm
        self.popup = PopupBox(
            self.screen,
            self.rm,
            QUICK_START_MESSAGE,
            [QUICK_START_SINGLE_TEXT, QUICK_START_MULTI_TEXT, QUICK_START_CANCEL_TEXT],
            self.WINDOW_WIDTH,
            self.WINDOW_HEIGHT,
            UI_WINDOW_BACKGROUND,
            self.BUTTON_WIDTH,
            self.BUTTON_HEIGHT,
        )

    def handle_event(self, ev: pygame.event.Event) -> Optional[str]:
        return self.popup.handle_event(ev)

    def draw(self):
        self.popup.draw()
