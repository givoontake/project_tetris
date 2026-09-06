import struct
from typing import Optional

from tetris.net.packet_structs import *
from tetris.net.packet_types import *


class PacketBuilder:
    def __init__(self):
        pass

    def struct_to_values(self, data: SendPacketStruct) -> list:
        return data.to_values()

    def str_to_bytes(self, s: str, array_len: int) -> bytes:
        return str(s).encode("utf-8")[:array_len].ljust(array_len, b"\x00")

    def pack_struct(self, data: SendPacketStruct) -> bytes:
        return struct.pack(data.FMT, *self.struct_to_values(data))

    def build_login_pkt(self, id: str, pw: str) -> bytes:
        data = C2S_LOGIN_PACKET()
        data.set_header(C2S_LOGIN)
        data.user_id = self.str_to_bytes(id, MAX_USER_ID)
        data.user_password = self.str_to_bytes(pw, MAX_USER_PASSWORD)
        return self.pack_struct(data)

    def build_message_pkt(self, message: str) -> bytes:
        if len(message) == 0:
            return b""

        data = C2S_MESSAGE_PACKET()
        encoded = message.encode("utf-8")
        message_bytes = len(encoded)
        data.set_header(C2S_MESSAGE, struct.calcsize(data.FMT) + message_bytes)
        header = self.pack_struct(data)
        return header + struct.pack(f"<{message_bytes}s", encoded)

    def build_join_room_pkt(self, room_key: int, room_pw: Optional[str]) -> bytes:
        if room_pw:
            data = C2S_JOIN_LOCK_ROOM_PACKET()
            data.set_header(C2S_JOIN_LOCK_ROOM)
            data.room_key = room_key
            data.room_password = self.str_to_bytes(room_pw, MAX_ROOM_PASSWORD)
        else:
            data = C2S_JOIN_OPEN_ROOM_PACKET()
            data.set_header(C2S_JOIN_OPEN_ROOM)
            data.room_key = room_key

        return self.pack_struct(data)

    def build_request_room_list_pkt(self) -> bytes:
        data = C2S_REQUEST_ROOM_LIST_PACKET()
        data.set_header(C2S_REQUEST_ROOM_LIST)
        return self.pack_struct(data)

    def build_disconnect_pkt(self) -> bytes:
        data = C2S_DISCONNECT_PACKET()
        data.set_header(C2S_DISCONNECT)
        return self.pack_struct(data)

    def build_move_pkt(self, move_type: int) -> bytes:
        data = C2S_MOVE_PACKET()
        data.set_header(C2S_MOVE)
        data.move_type = move_type
        return self.pack_struct(data)

    def build_create_room(self, title: str, players_num: int, is_open: bool, pw: Optional[str] = None) -> bytes:
        title_bytes = self.str_to_bytes(title, MAX_ROOM_NAME)

        if is_open:
            data = C2S_ADD_OPEN_ROOM_PACKET()
            data.set_header(C2S_ADD_OPEN_ROOM)
            data.max_user = players_num
            data.room_name = title_bytes
        else:
            data = C2S_ADD_LOCK_ROOM_PACKET()
            data.set_header(C2S_ADD_LOCK_ROOM)
            data.max_user = players_num
            data.room_name = title_bytes
            data.room_password = self.str_to_bytes(pw or "", MAX_ROOM_PASSWORD)

        return self.pack_struct(data)

    def build_fast_matching(self, max_user: int) -> bytes:
        data = C2S_FAST_MATCHING_PACKET()
        data.set_header(C2S_FAST_MATCHING)
        data.max_user = max_user
        return self.pack_struct(data)

    def build_start_pkt(self) -> bytes:
        data = C2S_START_PACKET()
        data.set_header(C2S_START)
        return self.pack_struct(data)

    def build_ready_pkt(self) -> bytes:
        data = C2S_READY_PACKET()
        data.set_header(C2S_READY)
        return self.pack_struct(data)

    def build_delete_user_pkt(self) -> bytes:
        data = C2S_DELETE_USER_PACKET()
        data.set_header(C2S_DELETE_USER)
        return self.pack_struct(data)

    def build_giveup_pkt(self) -> bytes:
        data = C2S_GIVEUP_PACKET()
        data.set_header(C2S_GIVEUP)
        return self.pack_struct(data)

    def build_kick_pkt(self, target_id: int) -> bytes:
        data = C2S_KICK_PACKET()
        data.set_header(C2S_KICK)
        data.kick_user_id = target_id
        return self.pack_struct(data)

    def build_request_friend_pkt(self, recver_id: int) -> bytes:
        data = C2S_REQUEST_FRIEND_PACKET()
        data.set_header(C2S_REQUEST_FRIEND)
        data.recver_id = recver_id
        return self.pack_struct(data)

    def build_delete_friend_pkt(self, target_id: int) -> bytes:
        data = C2S_DELETE_FRIEND_PACKET()
        data.set_header(C2S_DELETE_FRIEND)
        data.target_id = target_id
        return self.pack_struct(data)

    def build_accept_friend_pkt(self, requester_id: int) -> bytes:
        data = C2S_ACCEPT_FRIEND_PACKET()
        data.set_header(C2S_ACCEPT_FRIEND)
        data.requester_id = requester_id
        return self.pack_struct(data)

    def build_request_lobby_user_list_pkt(self) -> bytes:
        data = C2S_REQUEST_LOBBY_USER_LIST_PACKET()
        data.set_header(C2S_REQUEST_LOBBY_USER_LIST)
        return self.pack_struct(data)

    def build_request_friend_list_pkt(self) -> bytes:
        data = C2S_REQUEST_FRIEND_LIST_PACKET()
        data.set_header(C2S_REQUEST_FRIEND_LIST)
        return self.pack_struct(data)

    def build_request_ranking_pkt(self) -> bytes:
        data = C2S_REQUEST_RANKING_PACKET()
        data.set_header(C2S_REQUEST_RANKING)
        return self.pack_struct(data)
