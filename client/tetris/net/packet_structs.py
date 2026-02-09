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


class RecvPacketStruct:
    def fill_data(self, values: tuple) -> None:
        """
        struct.unpack(_from) 결과 튜플을 dataclass 필드 선언 순서대로
        자기 자신의 멤버 변수에 대입한다.
        """
        # 필드 변수의 메타데이터를 객체로 반환한다. 반환값은 리스트이다.
        # 안전하게 내부 필드를 선언 순서로 보장하여 접근하는 유일한 방법이다.
        fs = fields(self) 

        if len(values) != len(fs): raise ValueError(f"필드 개수 불일치: tuple={len(values)} / dataclass_fields={len(fs)}")

        for f, v in zip(fs, values): # zip은 여러 컨테이너를 묶어 순회할 수 있게 해준다. index range로 여러 인덱스에 접근하는 거랑 기능상 차이는 없다.
            # dataclass 필드가 str인데, unpack 결과가 bytes 계열이면 수행
            if f.type is str and isinstance(v, (bytes, bytearray)):
                head, _, _ = v.partition(b"\x00") # 처음 만나는 b"\x00"(널)을 기준으로 해당 문자의 앞, b"\x00", 뒤 3개로 나눈다. 널을 제거하는 용도
                v = head.decode("utf-8", errors="ignore")

            setattr(self, f.name, v) # (객체, 변수명, 값) -> 객체의 f.name라는 이름의 변수를 v로 바꾼다.


class SendPacketStruct:
    pass


# ---------------------------
# S2C (수신) : RecvPacketStruct
# ---------------------------

@dataclass
class S2C_LOGIN_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    max_score: int = -1
    win_count: int = -1
    lose_count: int = -1
    user_name: str = ""

    FMT: ClassVar[str] = f"<hbi iii {MAX_USER_NAME}s".replace(" ", "")


@dataclass
class S2C_MESSAGE_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    user_name: str = ""
    message: str = ""

    FMT: ClassVar[str] = f"<hbi{MAX_USER_NAME}s{MAX_CHAT_BYTES}s"


@dataclass
class S2C_DISCONNECT_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1

    FMT: ClassVar[str] = "<hbi"


@dataclass
class S2C_ADD_OPEN_ROOM_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    max_user: int = -1
    room_name: str = ""

    FMT: ClassVar[str] = f"<hbib{MAX_ROOM_NAME}s"


@dataclass
class S2C_ADD_LOCK_ROOM_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    max_user: int = -1
    room_name: str = ""
    room_password: str = ""

    FMT: ClassVar[str] = f"<hbib{MAX_ROOM_NAME}s{MAX_ROOM_PASSWORD}s"


@dataclass
class S2C_ADD_USER_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    is_add: int = -1
    name: str = ""

    FMT: ClassVar[str] = f"<hbib{MAX_USER_NAME}s"


@dataclass
class S2C_DELETE_USER_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    new_host_id: int = -1

    FMT: ClassVar[str] = "<hbii"


@dataclass
class S2C_READY_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    is_ready: int = -1

    FMT: ClassVar[str] = "<hbib"


@dataclass
class S2C_START_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    is_start: int = -1
    score: int = -1

    FMT: ClassVar[str] = "<hbbi"


@dataclass
class S2C_KICK_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    kick_user_id: int = -1

    FMT: ClassVar[str] = "<hbi"


@dataclass
class S2C_MOVE_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    move_type: int = -1

    FMT: ClassVar[str] = "<hbib"


@dataclass
class S2C_SPAWN_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    tetromino_type: int = -1
    next_tetromino_type: int = -1
    spawn_x: int = -1
    spawn_y: int = -1

    FMT: ClassVar[str] = "<hbibbbb"


@dataclass
class S2C_FIX_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    fixed_x: int = -1
    fixed_y: int = -1

    FMT: ClassVar[str] = "<hbibb"


@dataclass
class S2C_CLEARLINE_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    line_index: int = -1
    score: int = -1

    FMT: ClassVar[str] = "<hbibi"


@dataclass
class S2C_ADDLINE_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    hole_x: int = -1

    FMT: ClassVar[str] = "<hbib"


@dataclass
class S2C_GAMEOVER_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1

    FMT: ClassVar[str] = "<hbi"


@dataclass
class S2C_GAMEEND_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1

    FMT: ClassVar[str] = "<hbi"


@dataclass
class S2C_UPDATE_SCORE_PACKET(RecvPacketStruct):
    size: int = -1
    type: int = -1
    max_score: int = -1

    FMT: ClassVar[str] = "<hbi"


# ---------------------------
# C2S (송신) : SendPacketStruct
# - FMT를 리스트로 유지
# ---------------------------

@dataclass
class C2S_LOGIN_PACKET(SendPacketStruct):
    size: int = -1
    type: int = -1
    user_id: str = ""
    user_password: str = ""

    FMT: ClassVar[str] = f"<hb{MAX_USER_ID}s{MAX_USER_PASSWORD}s"


@dataclass
class C2S_MESSAGE_PACKET(SendPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    message: str = ""

    FMT: ClassVar[str] = f"<hbi{MAX_CHAT_BYTES}s"


@dataclass
class C2S_DISCONNECT_PACKET(SendPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1

    FMT: ClassVar[str] = "<hbi"


@dataclass
class C2S_ADD_OPEN_ROOM_PACKET(SendPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    max_user: int = -1
    room_name: str = ""

    FMT: ClassVar[str] = f"<hbib{MAX_ROOM_NAME}s"


@dataclass
class C2S_ADD_LOCK_ROOM_PACKET(SendPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    max_user: int = -1
    room_name: str = ""
    room_password: str = ""

    FMT: ClassVar[str] = f"<hbib{MAX_ROOM_NAME}s{MAX_ROOM_PASSWORD}s"


@dataclass
class C2S_ADD_USER_PACKET(SendPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    room_id: int = -1
    name: str = ""

    FMT: ClassVar[str] = f"<hbii{MAX_USER_NAME}s"


@dataclass
class C2S_DELETE_USER_PACKET(SendPacketStruct):
    size: int = -1
    type: int = -1

    FMT: ClassVar[str] = "<hb"


@dataclass
class C2S_READY_PACKET(SendPacketStruct):
    size: int = -1
    type: int = -1

    FMT: ClassVar[str] = "<hb"


@dataclass
class C2S_START_PACKET(SendPacketStruct):
    size: int = -1
    type: int = -1

    FMT: ClassVar[str] = "<hb"


@dataclass
class C2S_KICK_PACKET(SendPacketStruct):
    size: int = -1
    type: int = -1
    id: int = -1
    kick_user_id: int = -1

    FMT: ClassVar[str] = "<hbii"


@dataclass
class C2S_MOVE_PACKET(SendPacketStruct):
    size: int = -1
    type: int = -1
    move_type: int = -1

    FMT: ClassVar[str] = "<hbb"