# ===== FILE BEGIN: tetris\net\packet_structs.py =====
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
class SendPacketStruct:
    size: int = -1
    type: int = -1

    HEADER_FMT: ClassVar[str] = "<hb"


# ---------------------------
# S2C (수신) : RecvPacketStruct
# ---------------------------

@dataclass
class S2C_LOGIN_PACKET(RecvPacketStruct):
    id: int = -1
    max_score: int = -1
    win_count: int = -1
    lose_count: int = -1
    user_name: str = ""

    BODY_FMT: ClassVar[str] = f"i iii {MAX_USER_NAME}s".replace(" ", "")
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_MESSAGE_PACKET(RecvPacketStruct):
    id: int = -1
    user_name: str = ""
    message: str = ""

    BODY_FMT: ClassVar[str] = f"i{MAX_USER_NAME}s{MAX_CHAT_BYTES}s"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_DISCONNECT_PACKET(RecvPacketStruct):
    # (기존 코드에서는 "<hbi"였지만 필드/의도와 불일치였음)
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
    is_add: int = -1
    name: str = ""

    BODY_FMT: ClassVar[str] = f"ib{MAX_USER_NAME}s"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_DELETE_USER_PACKET(RecvPacketStruct):
    id: int = -1
    new_host_id: int = -1

    BODY_FMT: ClassVar[str] = "ii"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_READY_PACKET(RecvPacketStruct):
    id: int = -1
    is_ready: int = -1

    BODY_FMT: ClassVar[str] = "ib"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_START_PACKET(RecvPacketStruct):
    is_start: int = -1
    score: int = -1

    BODY_FMT: ClassVar[str] = "bi"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_KICK_PACKET(RecvPacketStruct):
    kick_user_id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_MOVE_PACKET(RecvPacketStruct):
    id: int = -1
    move_type: int = -1

    BODY_FMT: ClassVar[str] = "ib"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_SPAWN_PACKET(RecvPacketStruct):
    id: int = -1
    tetromino_type: int = -1
    next_tetromino_type: int = -1
    spawn_x: int = -1
    spawn_y: int = -1

    BODY_FMT: ClassVar[str] = "ibbbb"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_FIX_PACKET(RecvPacketStruct):
    id: int = -1
    fixed_x: int = -1
    fixed_y: int = -1

    BODY_FMT: ClassVar[str] = "ibb"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_CLEARLINE_PACKET(RecvPacketStruct):
    id: int = -1
    line_index: int = -1
    combo: int = -1
    score: int = -1

    BODY_FMT: ClassVar[str] = "ibbi"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_ADDLINE_PACKET(RecvPacketStruct):
    id: int = -1
    hole_x: int = -1

    BODY_FMT: ClassVar[str] = "ib"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_GAMEOVER_PACKET(RecvPacketStruct):
    id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_GAMEEND_PACKET(RecvPacketStruct):
    id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_UPDATE_SCORE_PACKET(RecvPacketStruct):
    max_score: int = -1

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
    id: int = -1
    message: str = ""

    BODY_FMT: ClassVar[str] = f"i{MAX_CHAT_BYTES}s"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_DISCONNECT_PACKET(SendPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_ADD_OPEN_ROOM_PACKET(SendPacketStruct):
    id: int = -1
    max_user: int = -1
    room_name: str = ""

    BODY_FMT: ClassVar[str] = f"ib{MAX_ROOM_NAME}s"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_ADD_LOCK_ROOM_PACKET(SendPacketStruct):
    id: int = -1
    max_user: int = -1
    room_name: str = ""
    room_password: str = ""

    BODY_FMT: ClassVar[str] = f"ib{MAX_ROOM_NAME}s{MAX_ROOM_PASSWORD}s"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_ADD_USER_PACKET(SendPacketStruct):
    id: int = -1
    room_id: int = -1
    name: str = ""

    BODY_FMT: ClassVar[str] = f"ii{MAX_USER_NAME}s"
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
    id: int = -1
    kick_user_id: int = -1

    BODY_FMT: ClassVar[str] = "ii"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_MOVE_PACKET(SendPacketStruct):
    move_type: int = -1

    BODY_FMT: ClassVar[str] = "b"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT

# ===== FILE END: tetris\net\packet_structs.py =====