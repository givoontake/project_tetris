import pygame
from typing import Optional

from tetris.net.network import NetworkWorker
from tetris.net.packet_structs import RecvPacketStruct
from tetris.net.session import Session
from tetris.resources.define import UI_INGAME_BACKGROUND
from tetris.resources.resource_manager import ResourceManager
from tetris.states.base_state import BaseState
from tetris.ui.block_customize_panel import BlockShapePreviewPanel, BlockSkinList
from tetris.ui.button import Button
from tetris.ui.scroll_window_base import SCROLL_WIDTH

BACK_BUTTON_TEXT = "\uB3CC\uC544\uAC00\uAE30"
SAVE_BUTTON_TEXT = "\uC800\uC7A5"


class CustomizeState(BaseState):
    BUTTON_WIDTH = 200
    BUTTON_HEIGHT = 100
    LIST_WIDTH = 1000
    PREVIEW_WIDTH = 400
    PANEL_HEIGHT = 800
    PANEL_GAP = 40
    MIN_LIST_WIDTH = 400
    MIN_PANEL_HEIGHT = 400

    def __init__(self, screen: pygame.Surface, rm: ResourceManager,
                 net_worker: NetworkWorker, session: Session):
        super().__init__(screen, rm, net_worker, session)
        self.btn_back: Optional[Button] = None
        self.btn_save: Optional[Button] = None
        self.skin_list: Optional[BlockSkinList] = None
        self.preview_panel: Optional[BlockShapePreviewPanel] = None
        self.block_texture_keys = self.session.block_texture_keys.copy()
        self.set_layout()

    def set_layout(self):
        sw, sh = self.screen.get_size()

        back_rect = pygame.Rect(
            sw - self.BUTTON_WIDTH,
            0,
            self.BUTTON_WIDTH,
            self.BUTTON_HEIGHT,
        )
        save_rect = back_rect.copy()
        save_rect.x -= self.BUTTON_WIDTH

        self.btn_back = Button(self.screen, back_rect, self.rm, BACK_BUTTON_TEXT, 0)
        self.btn_save = Button(self.screen, save_rect, self.rm, SAVE_BUTTON_TEXT, 0)

        panel_h = min(self.PANEL_HEIGHT, max(self.MIN_PANEL_HEIGHT, sh - self.BUTTON_HEIGHT))
        list_w = min(
            self.LIST_WIDTH,
            max(
                self.MIN_LIST_WIDTH,
                sw - self.PREVIEW_WIDTH - self.PANEL_GAP - SCROLL_WIDTH - 40,
            ),
        )
        total_w = list_w + SCROLL_WIDTH + self.PANEL_GAP + self.PREVIEW_WIDTH
        start_x = max(0, (sw - total_w) // 2)
        start_y = self.BUTTON_HEIGHT + max(0, (sh - self.BUTTON_HEIGHT - panel_h) // 2)

        list_rect = pygame.Rect(start_x, start_y, list_w, panel_h)
        preview_rect = pygame.Rect(
            list_rect.right + SCROLL_WIDTH + self.PANEL_GAP,
            start_y,
            self.PREVIEW_WIDTH,
            panel_h,
        )

        self.skin_list = BlockSkinList(self.screen, list_rect, self.rm)
        self.preview_panel = BlockShapePreviewPanel(
            self.screen,
            preview_rect,
            self.rm,
            self.block_texture_keys,
        )
        self.skin_list.set_selected_block_key(self.preview_panel.get_selected_block_key())

    def save_block_settings(self):
        # Server persistence will be connected here later.
        pass

    def handle_packet(self, data: Optional[RecvPacketStruct]):
        return self

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.QUIT:
            pygame.quit()
            raise SystemExit

        if self.btn_back and self.btn_back.handle_event(ev):
            from tetris.states.lobby_state import LobbyState
            self.queue_state(LobbyState(self.screen, self.rm, self.net_worker, self.session))
            return

        if self.btn_save and self.btn_save.handle_event(ev):
            self.save_block_settings()
            return

        if self.preview_panel:
            selected_shape_key = self.preview_panel.handle_event(ev)
            if selected_shape_key and self.skin_list:
                self.skin_list.set_selected_block_key(
                    self.preview_panel.get_selected_block_key()
                )
                return

        if self.skin_list and self.preview_panel:
            selected_block_key = self.skin_list.handle_event(ev)
            if selected_block_key is not None:
                shape_key = self.preview_panel.selected_shape_key
                self.preview_panel.set_block_texture(shape_key, selected_block_key)
                self.session.set_block_texture(shape_key, selected_block_key)
                self.skin_list.set_selected_block_key(selected_block_key)

    def update(self, dt_ms, events):
        for ev in events:
            self.handle_event(ev)

        self.update_fade(dt_ms)
        return self.consume_state()

    def draw(self):
        self.screen.blit(self.rm.images.ui_images[UI_INGAME_BACKGROUND], (0, 0))

        if self.skin_list:
            self.skin_list.draw()
        if self.preview_panel:
            self.preview_panel.draw()
        if self.btn_save:
            self.btn_save.draw()
        if self.btn_back:
            self.btn_back.draw()
