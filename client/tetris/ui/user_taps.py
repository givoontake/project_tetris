# tetris/ui/user_taps.py
import pygame
from typing import cast

from tetris.models.dataclass import EventFriend
from tetris.ui.button import Button
from tetris.ui.user_list import UserList
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.define_colors import *
from tetris.net.packet_types import *
from tetris.net.packet_structs import *

TAB_HEIGHT = 40
TAB_GAP = 5

USER_TAB_NAME = "유저"
FRIEND_TAB_NAME = "친구"


class UserTabs:
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager, tab_names: list[str]):
        self.screen = screen
        self.rect = rect
        self.rm = rm

        self.tab_names = tab_names
        self.cur_tab_index = 0

        self.tab_buttons: list[Button] = []
        self.tab_rects: list[pygame.Rect] = []
        self.user_lists: dict[str, UserList] = {}

        self.set_layout()

    def set_layout(self):
        tab_rect = self.rect.copy()
        tab_rect.h = TAB_HEIGHT

        list_rect = self.rect.copy()
        list_rect.y += TAB_HEIGHT + TAB_GAP
        list_rect.h -= (TAB_HEIGHT + TAB_GAP)

        self.tab_buttons.clear()
        self.tab_rects.clear()
        self.user_lists.clear()

        tab_count = len(self.tab_names)
        if tab_count <= 0:
            return

        total_gap = TAB_GAP * (tab_count - 1)
        tab_w = (tab_rect.w - total_gap) // tab_count

        draw_x = tab_rect.x
        for tab_name in self.tab_names:
            button_rect = pygame.Rect(draw_x, tab_rect.y, tab_w, tab_rect.h)
            button = Button(self.screen, button_rect, self.rm, None, tab_name, 1)
            self.tab_buttons.append(button)
            self.tab_rects.append(button_rect)
            self.user_lists[tab_name] = UserList(self.screen, list_rect, self.rm)
            draw_x += tab_w + TAB_GAP

    def get_cur_user_list(self) -> UserList:
        return self.user_lists[self.tab_names[self.cur_tab_index]]
    
    def get_cur_tab_name(self) -> str:
        return self.tab_names[self.cur_tab_index]

    def add_user(self, tab_name: str, id: int, nickname: str, state: bool = False):
        if tab_name not in self.user_lists:
            return
        self.user_lists[tab_name].add_user(id, nickname, state)

    def delete_user(self, tab_name: str, id: int):
        if tab_name not in self.user_lists:
            return
        self.user_lists[tab_name].delete_user(id)

    def update_user(self, tab_name: str, id: int, nickname: str, state: bool = False):
        if tab_name not in self.user_lists:
            return
        self.user_lists[tab_name].update_user(id, nickname, state)

    def update_user_state(self, tab_name: str, id: int, state: bool = False):
        if tab_name not in self.user_lists:
            return
        self.user_lists[tab_name].update_user_state(id, state)

    def clear(self, tab_name: str = None):
        if tab_name is None:
            for user_list in self.user_lists.values():
                user_list.clear()
            return

        if tab_name not in self.user_lists:
            return
        self.user_lists[tab_name].clear()

    def handle_packet(self, data: RecvPacketStruct):
        if data is None:
            return None

        if data.type == S2C_LOBBY_USER_INFO:
            user_data = cast(S2C_LOBBY_USER_INFO_PACKET, data)
            self.add_user("유저", user_data.user_pk, user_data.nickname, True)

        elif data.type == S2C_FRIEND_INFO:
            friend_data = cast(S2C_FRIEND_INFO_PACKET, data)
            self.add_user("친구", friend_data.user_pk, friend_data.nickname, friend_data.is_lobby)

        elif data.type == S2C_ADD_FRIEND:
            add_data = cast(S2C_ADD_FRIEND_PACKET, data)
            self.add_user("친구", add_data.friend_id, add_data.friend_nickname, True) # 나중에 서버에서 확인하고 패킷에 정보 추가해서 보내줘야 할 듯

        elif data.type == S2C_DELETE_FRIEND:
            delete_data = cast(S2C_DELETE_FRIEND_PACKET, data)
            self.delete_user("친구", delete_data.target_pk)

        elif data.type == S2C_REQUEST_FRIEND:
            request_data = cast(S2C_REQUEST_FRIEND_PACKET, data)
            return request_data

        return None

    def handle_event(self, ev: pygame.event.Event):
        for index in range(len(self.tab_buttons)):
            if self.tab_buttons[index].handle_event(ev):
                self.cur_tab_index = index
                return None

        result = self.get_cur_user_list().handle_event(ev)
        if result is None:
            return None

        cur_tab_name = self.get_cur_tab_name()
        if cur_tab_name == USER_TAB_NAME:
            result.ev_type = "add"
        elif cur_tab_name == FRIEND_TAB_NAME:
            result.ev_type = "delete"
        else:
            return None

        return result

    def draw(self):
        for index in range(len(self.tab_buttons)):
            self.tab_buttons[index].draw()

            if index == self.cur_tab_index:
                pygame.draw.rect(self.screen, ORANGE, self.tab_rects[index], 2)

        self.get_cur_user_list().draw()