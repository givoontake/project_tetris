from dataclasses import dataclass, fields
from typing import ClassVar

MAX_INPUT = 16
MAX_USER_ID = 16 * 3
MAX_USER_PASSWORD = 16 * 3
MAX_USER_NAME = 16 * 3
MAX_ROOM_NAME = 16 * 3
MAX_ROOM_PASSWORD = 16 * 3
MAX_CHAT_INPUT = 256
MAX_CHAT_BYTES = MAX_CHAT_INPUT * 3


# ---------------------------
# Common Header
# - size: short (h)
# - type: byte  (b)
# - little endian
# ---------------------------

@dataclass
class RecvPacketStruct:
    size: int = -1
    type: int = -1

    HEADER_FMT: ClassVar[str] = "<hb"

    def fill_data(self, values: tuple) -> None:
        """
        struct.unpack(_from) 결과 튜플을 dataclass 필드 선언 순서대로
        자기 자신의 멤버 변수에 대입한다.
        """
        fs = fields(self)

        if len(values) != len(fs):
            raise ValueError(f"필드 개수 불일치: tuple={len(values)} / dataclass_fields={len(fs)}")

        for f, v in zip(fs, values):
            # dataclass 필드가 str인데, unpack 결과가 bytes 계열이면 수행
            if f.type is str and isinstance(v, (bytes, bytearray)):
                head, _, _ = v.partition(b"\x00")
                v = head.decode("utf-8", errors="ignore")

            setattr(self, f.name, v)
            

@dataclass
class IngamePacket(RecvPacketStruct): # id 공통필드
    id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    HEADER_FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class SendPacketStruct:
    size: int = -1
    type: int = -1

    HEADER_FMT: ClassVar[str] = "<hb"

# ---------------------------
# S2C (수신) : RecvPacketStruct
# ---------------------------

@dataclass
class S2C_ERROR_PACKET(RecvPacketStruct):
    error_code: int = -1

    BODY_FMT: ClassVar[str] = f"i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT

@dataclass
class S2C_LOGIN_PACKET(RecvPacketStruct):
    id: int = -1
    max_score: int = -1
    win_count: int = -1
    lose_count: int = -1
    user_name: str = ""

    BODY_FMT: ClassVar[str] = f"iiii{MAX_USER_NAME}s".replace(" ", "")
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_MESSAGE_PACKET(RecvPacketStruct):
    id: int = -1
    user_name: str = ""
    message: str = "" # 역직렬화 할 때 메세지 크기를 결정해야 하므로 포맷으로 따로 정의하지 않음

    BODY_FMT: ClassVar[str] = f"i{MAX_USER_NAME}s"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_DISCONNECT_PACKET(RecvPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_ADD_OPEN_ROOM_PACKET(RecvPacketStruct):
    id: int = -1
    max_user: int = -1
    room_name: str = ""

    BODY_FMT: ClassVar[str] = f"ib{MAX_ROOM_NAME}s"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_ADD_LOCK_ROOM_PACKET(RecvPacketStruct):
    id: int = -1
    max_user: int = -1
    room_name: str = ""
    room_password: str = ""

    BODY_FMT: ClassVar[str] = f"ib{MAX_ROOM_NAME}s{MAX_ROOM_PASSWORD}s"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_ADD_USER_PACKET(RecvPacketStruct):
    id: int = -1
    name: str = ""

    BODY_FMT: ClassVar[str] = f"i{MAX_USER_NAME}s"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_DELETE_USER_PACKET(RecvPacketStruct):
    id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_READY_PACKET(RecvPacketStruct):
    id: int = -1
    is_ready: bool = False

    BODY_FMT: ClassVar[str] = "i?"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_SINGLE_START_PACKET(RecvPacketStruct):
    score: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT

class S2C_MULTI_START_PACKET(RecvPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT

@dataclass
class S2C_KICK_PACKET(RecvPacketStruct):
    kick_user_id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_MOVE_PACKET(IngamePacket):
    move_type: int = -1

    BODY_FMT: ClassVar[str] = "b"
    FMT: ClassVar[str] = IngamePacket.HEADER_FMT + BODY_FMT


@dataclass
class S2C_SPAWN_PACKET(IngamePacket):
    tetromino_type: int = -1
    next_tetromino_type: int = -1
    spawn_x: int = -1
    spawn_y: int = -1

    BODY_FMT: ClassVar[str] = "bbbb"
    FMT: ClassVar[str] = IngamePacket.HEADER_FMT + BODY_FMT


@dataclass
class S2C_FIX_PACKET(IngamePacket):
    fixed_x: int = -1
    fixed_y: int = -1

    BODY_FMT: ClassVar[str] = "bb"
    FMT: ClassVar[str] = IngamePacket.HEADER_FMT + BODY_FMT


@dataclass
class S2C_CLEARLINE_PACKET(IngamePacket):
    line_index: int = -1
    combo: int = -1
    score: int = -1

    BODY_FMT: ClassVar[str] = "bbi"
    FMT: ClassVar[str] = IngamePacket.HEADER_FMT + BODY_FMT


@dataclass
class S2C_ADDLINE_PACKET(IngamePacket):
    hole_x: int = -1

    BODY_FMT: ClassVar[str] = "b"
    FMT: ClassVar[str] = IngamePacket.HEADER_FMT + BODY_FMT


@dataclass
class S2C_GAMEOVER_PACKET(IngamePacket):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = IngamePacket.HEADER_FMT + BODY_FMT


@dataclass
class S2C_GAMEEND_PACKET(RecvPacketStruct):
    winner_id: int = -1
    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_UPDATE_SCORE_PACKET(RecvPacketStruct):
    max_score: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT

@dataclass
class S2C_MATCH_RECORD_PACKET(RecvPacketStruct):
    win_count: int = -1
    lose_count: int = -1

    BODY_FMT: ClassVar[str] = "ii"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT

@dataclass
class S2C_UPDATE_HOST_PACKET(RecvPacketStruct):
    new_host_id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT

@dataclass
class S2C_ROOM_INFO_PACKET(RecvPacketStruct):
    room_id: int = -1
    room_name: str = ""
    max_user: int = -1
    cur_user: int = -1
    is_private: bool = False
    is_play: bool = False

    BODY_FMT: ClassVar[str] = f"i{MAX_ROOM_NAME}sbb??"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT

@dataclass
class S2C_INFO_PACKET(RecvPacketStruct):
    info_type: int = -1
    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


# ---------------------------
# C2S (송신) : SendPacketStruct
# ---------------------------

@dataclass
class C2S_LOGIN_PACKET(SendPacketStruct):
    user_id: str = ""
    user_password: str = ""

    BODY_FMT: ClassVar[str] = f"{MAX_USER_ID}s{MAX_USER_PASSWORD}s"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_MESSAGE_PACKET(SendPacketStruct):
    # message: str = ""

    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_DISCONNECT_PACKET(SendPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_ADD_OPEN_ROOM_PACKET(SendPacketStruct):
    max_user: int = -1
    room_name: str = ""

    BODY_FMT: ClassVar[str] = f"b{MAX_ROOM_NAME}s"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_ADD_LOCK_ROOM_PACKET(SendPacketStruct):
    max_user: int = -1
    room_name: str = ""
    room_password: str = ""

    BODY_FMT: ClassVar[str] = f"b{MAX_ROOM_NAME}s{MAX_ROOM_PASSWORD}s"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_JOIN_OPEN_ROOM_PACKET(SendPacketStruct):
    room_id: int = -1

    BODY_FMT: ClassVar[str] = f"i"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT

@dataclass
class C2S_JOIN_LOCK_ROOM_PACKET(SendPacketStruct):
    room_id: int = -1
    room_password: str = ""

    BODY_FMT: ClassVar[str] = f"i{MAX_ROOM_PASSWORD}s"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT

@dataclass
class C2S_DELETE_USER_PACKET(SendPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_READY_PACKET(SendPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_START_PACKET(SendPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_KICK_PACKET(SendPacketStruct):
    kick_user_id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_MOVE_PACKET(SendPacketStruct):
    move_type: int = -1

    BODY_FMT: ClassVar[str] = "b"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT

@dataclass
class C2S_GIVEUP_PACKET(SendPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT

@dataclass
class C2S_REQUEST_ROOM_LIST_PACKET(SendPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT

@dataclass
class C2S_FAST_MATCHING_PACKET(SendPacketStruct):
    max_user: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT