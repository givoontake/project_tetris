import pygame
from typing import Optional

from tetris.net.packet_types import *
from tetris.net.packet_structs import *
from tetris.net.network import NetworkWorker
from tetris.net.session import Session
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.define import *
from tetris.resources.define_colors import *
from tetris.ui.button import Button
from tetris.ui.rectangle import Rectangle
from tetris.states.base_state import BaseState

BACK_BUTTON_TEXT = "\uB3CC\uC544\uAC00\uAE30"
NAME_HEADER_TEXT = "\uB2C9\uB124\uC784"
SCORE_HEADER_TEXT = "\uC810\uC218"


class RankingState(BaseState):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager,
                 net_worker: NetworkWorker, session: Session):
        super().__init__(screen, rm, net_worker, session)
        self.btn_back: Optional[Button] = None
        self.panel: Optional[Rectangle] = None
        self.header_name: Optional[Rectangle] = None
        self.header_score: Optional[Rectangle] = None
        self.rows: list[tuple[Rectangle, Rectangle]] = []
        self.rankings: list[S2C_RANKING_INFO_PACKET] = []
        self.set_layout()
        self.request_ranking()

    def request_ranking(self):
        self.rankings.clear()
        self.rows.clear()
        packet = self.net_worker.builder.build_request_ranking_pkt()
        self.net_worker.send_packet(packet)

    def set_layout(self):
        from tetris.states.lobby_state import MENU_WIDTH, MENU_HEIGHT

        sw, sh = self.screen.get_size()
        back_rect = pygame.Rect(sw - MENU_WIDTH, 0, MENU_WIDTH, MENU_HEIGHT)
        self.btn_back = Button(
            self.screen,
            back_rect,
            self.rm,
            BACK_BUTTON_TEXT,
            0,
        )

        panel_w = int(sw * 0.75)
        panel_h = int((sh - MENU_HEIGHT) * 0.8)
        panel_x = (sw - panel_w) // 2
        panel_y = (sh - panel_h) // 2
        panel_rect = pygame.Rect(panel_x, panel_y, panel_w, panel_h)

        self.panel = Rectangle(self.screen, panel_rect, self.rm, False, None, "", 3)
        self.panel.set_background_color((0, 0, 0, 120))

        header_h = max(60, panel_h // 12)
        half_w = panel_w // 2
        header_name_rect = pygame.Rect(panel_x, panel_y, half_w, header_h)
        header_score_rect = pygame.Rect(panel_x + half_w, panel_y, panel_w - half_w, header_h)

        self.header_name = Rectangle(self.screen, header_name_rect, self.rm, False, None, NAME_HEADER_TEXT, 2)
        self.header_name.set_background_color((0, 0, 0, 120))
        self.header_score = Rectangle(self.screen, header_score_rect, self.rm, False, None, SCORE_HEADER_TEXT, 2)
        self.header_score.set_background_color((0, 0, 0, 120))

        self.rebuild_rows()

    def rebuild_rows(self):
        if self.panel is None:
            return

        self.rows.clear()
        panel_rect = self.panel.rect
        header_h = self.header_name.rect.h if self.header_name else 60
        body_y = panel_rect.y + header_h
        body_h = panel_rect.h - header_h
        row_count = 10
        row_h = max(40, body_h // row_count)
        half_w = panel_rect.w // 2

        for idx in range(row_count):
            rank_text = ""
            score_text = ""
            if idx < len(self.rankings):
                info = self.rankings[idx]
                rank_text = f"{idx + 1}. {info.nickname}"
                score_text = str(info.score)

            row_y = body_y + idx * row_h
            name_rect = pygame.Rect(panel_rect.x, row_y, half_w, row_h)
            score_rect = pygame.Rect(panel_rect.x + half_w, row_y, panel_rect.w - half_w, row_h)

            name_box = Rectangle(self.screen, name_rect, self.rm, False, None, rank_text, 1)
            score_box = Rectangle(self.screen, score_rect, self.rm, False, None, score_text, 1)
            name_box.set_background_color((0, 0, 0, 120))
            score_box.set_background_color((0, 0, 0, 120))
            name_box.set_text_size(26)
            score_box.set_text_size(26)
            self.rows.append((name_box, score_box))

    def handle_packet(self, data: Optional[RecvPacketStruct]):
        if data and data.type == S2C_RANKING_INFO:
            self.rankings.append(data)
            self.rebuild_rows()
        return self

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.QUIT:
            pygame.quit()
            raise SystemExit

        if self.btn_back and self.btn_back.handle_event(ev):
            from tetris.states.lobby_state import LobbyState
            self.queue_state(LobbyState(self.screen, self.rm, self.net_worker, self.session))

        return self

    def update(self, dt_ms, events):
        for ev in events:
            self.handle_event(ev)
        self.update_fade(dt_ms)
        return self.consume_state()

    def draw(self):
        self.screen.blit(self.rm.images.ui_images[UI_LOBBY_BACKGROUND], (0, 0))

        if self.btn_back:
            self.btn_back.draw()
        if self.panel:
            self.panel.draw()
        if self.header_name:
            self.header_name.draw()
        if self.header_score:
            self.header_score.draw()

        for name_box, score_box in self.rows:
            name_box.draw()
            score_box.draw()
