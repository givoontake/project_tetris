import struct
from typing import Optional

from tetris.net.packet_structs import *
from tetris.net.packet_types import *

class PacketBuilder:
    def __init__(self):
        pass

    def struct_to_values(self, data: SendPacketStruct):
        values: list = []
        flds = fields(data)
        for fld in flds:
            value = getattr(data, fld.name)
            values.append(value)

        return values
    
    def str_to_bytes(self, s: str, array_len: int) -> bytearray:
        return str(s).encode("utf-8")[:array_len].ljust(array_len, b"\x00")
    
    # --------------- builders ----------------

    # login
    def build_login_pkt(self, id: str, pw: str) -> bytes:
        data = C2S_LOGIN_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_LOGIN
        data.user_id = self.str_to_bytes(id, MAX_USER_ID)
        data.user_password = self.str_to_bytes(pw, MAX_USER_PASSWORD)
        values = self.struct_to_values(data)

        return struct.pack(data.FMT, *values)

    # lobby
    def build_message_pkt(self, message: str) -> bytes: # 메세지는 가변이라 문자열 포맷을 크기만큼 만들어 직접 전송
        if len(message) == 0: return
        data = C2S_MESSAGE_PACKET()
        data.type = C2S_MESSAGE
        encoded = message.encode("utf-8")
        message_bytes = len(encoded)
        data.size = struct.calcsize(data.FMT) + message_bytes
        values = self.struct_to_values(data)
        fmt = data.FMT
        header = struct.pack(fmt, *values)
        message = struct.pack(f"{message_bytes}s", encoded)

        return header + message

    def build_join_room_pkt(self, room_id: int, room_pw: Optional[str]) -> bytes:
        if room_pw: 
            data = C2S_JOIN_LOCK_ROOM_PACKET()
            data.size = struct.calcsize(data.FMT)
            data.type = C2S_JOIN_LOCK_ROOM
            data.room_id = room_id
            data.room_password = self.str_to_bytes(room_pw, MAX_ROOM_PASSWORD)
        else: 
            data = C2S_JOIN_OPEN_ROOM_PACKET()
            data.size = struct.calcsize(data.FMT)
            data.type = C2S_JOIN_OPEN_ROOM
            data.room_id = room_id

        values = self.struct_to_values(data)

        return struct.pack(data.FMT, *values)

    def build_request_room_list_pkt(self) -> bytes:
        data = C2S_REQUEST_ROOM_LIST_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_REQUEST_ROOM_LIST
        values = self.struct_to_values(data)

        return struct.pack(data.FMT, *values)

    def build_disconnect_pkt(self) -> bytes:
        data = C2S_DISCONNECT_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_DISCONNECT
        values = self.struct_to_values(data)

        return struct.pack(data.FMT, *values)
    
    # controller
    def build_move_pkt(self, move_type) -> bytes:
        data = C2S_MOVE_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_MOVE
        data.move_type = move_type
        values = self.struct_to_values(data)

        return struct.pack(data.FMT, *values)
    
    # create_room_window
    def build_create_room(self, title: str, players_num: int, is_open: bool, pw: Optional[str] = None):

        title = self.str_to_bytes(title, MAX_ROOM_NAME)
        if is_open:
            data = C2S_ADD_OPEN_ROOM_PACKET()
            data.size = struct.calcsize(data.FMT)
            data.type = C2S_ADD_OPEN_ROOM
            data.max_user = players_num
            data.room_name = title

        else:
            data = C2S_ADD_LOCK_ROOM_PACKET()
            data.size = struct.calcsize(data.FMT)
            data.type = C2S_ADD_LOCK_ROOM
            data.max_user = players_num
            data.room_name = title
            data.room_password = self.str_to_bytes(pw, MAX_ROOM_PASSWORD)
            
        values = self.struct_to_values(data)

        return struct.pack(data.FMT, *values)
    
    # fast_matching_window
    def build_fast_matching(self, max_user: int):
        data = C2S_FAST_MATCHING_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_FAST_MATCHING
        data.max_user = max_user
        values = self.struct_to_values(data)

        return struct.pack(data.FMT, *values)
    
    # tetris_session
    def build_start_pkt(self) -> bytes:
        data = C2S_START_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_START
        values = self.struct_to_values(data)

        return struct.pack(data.FMT, *values)

    def build_ready_pkt(self) -> bytes:
        data = C2S_READY_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_READY
        values = self.struct_to_values(data)

        return struct.pack(data.FMT, *values)
    
    # play 공통
    def build_delete_user_pkt(self) -> bytes:
        data = C2S_DELETE_USER_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_DELETE_USER
        values = self.struct_to_values(data)

        return struct.pack(data.FMT, *values)

    # single 전용
    def build_giveup_pkt(self) -> bytes:
        data = C2S_GIVEUP_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_GIVEUP
        values = self.struct_to_values(data)

        return struct.pack(data.FMT, *values)

    # multi 전용
    def build_kick_pkt(self, target_id: int) -> bytes:
        data = C2S_KICK_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_KICK
        data.kick_user_id = target_id
        values = self.struct_to_values(data)

        return struct.pack(data.FMT, *values)
    
    def build_request_friend_pkt(self, recver_id: int) -> bytes:
        data = C2S_REQUEST_FRIEND_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_REQUEST_FRIEND
        data.recver_id = recver_id
        values = self.struct_to_values(data)
        return struct.pack(data.FMT, *values)

    def build_delete_friend_pkt(self, target_id: int) -> bytes:
        data = C2S_DELETE_FRIEND_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_DELETE_FRIEND
        data.target_id = target_id
        values = self.struct_to_values(data)
        return struct.pack(data.FMT, *values)

    def build_accept_friend_pkt(self, requester_id: int) -> bytes:
        data = C2S_ACCEPT_FRIEND_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_ACCEPT_FRIEND
        data.requester_id = requester_id
        values = self.struct_to_values(data)
        return struct.pack(data.FMT, *values)

    def build_request_lobby_user_list_pkt(self) -> bytes:
        data = C2S_REQUEST_LOBBY_USER_LIST_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_REQUEST_LOBBY_USER_LIST
        values = self.struct_to_values(data)
        return struct.pack(data.FMT, *values)

    def build_request_friend_list_pkt(self) -> bytes:
        data = C2S_REQUEST_FRIEND_LIST_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_REQUEST_FRIEND_LIST
        values = self.struct_to_values(data)
        return struct.pack(data.FMT, *values)

    def build_request_ranking_pkt(self) -> bytes:
        data = C2S_REQUEST_RANKING_PACKET()
        data.size = struct.calcsize(data.FMT)
        data.type = C2S_REQUEST_RANKING
        values = self.struct_to_values(data)
        return struct.pack(data.FMT, *values)
