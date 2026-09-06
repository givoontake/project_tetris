import struct
from dataclasses import dataclass, field, fields
from typing import ClassVar

MAX_INPUT = 16
MAX_USER_ID = 16 * 3
MAX_USER_PASSWORD = 16 * 3
MAX_USER_NAME = 16 * 3
MAX_ROOM_NAME = 16 * 3
MAX_ROOM_PASSWORD = 16 * 3
MAX_CHAT_INPUT = 256
MAX_CHAT_BYTES = MAX_CHAT_INPUT * 3


@dataclass
class PacketHeader:
    size: int = -1
    type: int = -1

    BODY_FMT: ClassVar[str] = "HB"
    FMT: ClassVar[str] = f"<{BODY_FMT}"
    SIZE: ClassVar[int] = struct.calcsize(FMT)


@dataclass
class RecvPacketStruct:
    header: PacketHeader = field(default_factory=PacketHeader)

    HEADER_FMT: ClassVar[str] = PacketHeader.FMT

    @property
    def size(self) -> int:
        return self.header.size

    @size.setter
    def size(self, value: int) -> None:
        self.header.size = int(value)

    @property
    def type(self) -> int:
        return self.header.type

    @type.setter
    def type(self, value: int) -> None:
        self.header.type = int(value)

    def set_header(self, packet_type: int, packet_size: int | None = None) -> None:
        self.header.type = int(packet_type)
        self.header.size = int(packet_size if packet_size is not None else struct.calcsize(self.FMT))

    def to_values(self) -> list:
        values = [self.header.size, self.header.type]
        for f in fields(self):
            if f.name == "header":
                continue
            values.append(getattr(self, f.name))
        return values

    def fill_data(self, values: tuple) -> None:
        body_fields = [f for f in fields(self) if f.name != "header"]
        if len(values) != len(body_fields) + 2:
            raise ValueError(
                f"Field count mismatch tuple={len(values)} / dataclass_fields={len(body_fields) + 2}"
            )

        self.header.size = int(values[0])
        self.header.type = int(values[1])

        for f, v in zip(body_fields, values[2:]):
            if f.type is str and isinstance(v, (bytes, bytearray)):
                head, _, _ = v.partition(b"\x00")
                v = head.decode("utf-8", errors="ignore")
            setattr(self, f.name, v)


@dataclass
class IngamePacket(RecvPacketStruct):
    id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    HEADER_FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class SendPacketStruct:
    header: PacketHeader = field(default_factory=PacketHeader)

    HEADER_FMT: ClassVar[str] = PacketHeader.FMT

    @property
    def size(self) -> int:
        return self.header.size

    @size.setter
    def size(self, value: int) -> None:
        self.header.size = int(value)

    @property
    def type(self) -> int:
        return self.header.type

    @type.setter
    def type(self, value: int) -> None:
        self.header.type = int(value)

    def set_header(self, packet_type: int, packet_size: int | None = None) -> None:
        self.header.type = int(packet_type)
        self.header.size = int(packet_size if packet_size is not None else struct.calcsize(self.FMT))

    def to_values(self) -> list:
        values = [self.header.size, self.header.type]
        for f in fields(self):
            if f.name == "header":
                continue
            values.append(getattr(self, f.name))
        return values


@dataclass
class S2C_ERROR_PACKET(RecvPacketStruct):
    error_code: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_LOGIN_PACKET(RecvPacketStruct):
    id: int = -1
    max_score: int = -1
    win_count: int = -1
    lose_count: int = -1
    user_name: str = ""

    BODY_FMT: ClassVar[str] = f"iiii{MAX_USER_NAME}s"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_MESSAGE_PACKET(RecvPacketStruct):
    id: int = -1
    user_name: str = ""
    message: str = ""

    BODY_FMT: ClassVar[str] = f"i{MAX_USER_NAME}s"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_DISCONNECT_PACKET(RecvPacketStruct):
    id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_ADD_OPEN_ROOM_PACKET(RecvPacketStruct):
    room_key: int = 0
    max_user: int = -1
    room_name: str = ""

    BODY_FMT: ClassVar[str] = f"Qb{MAX_ROOM_NAME}s"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_ADD_LOCK_ROOM_PACKET(RecvPacketStruct):
    room_key: int = 0
    max_user: int = -1
    room_name: str = ""
    room_password: str = ""

    BODY_FMT: ClassVar[str] = f"Qb{MAX_ROOM_NAME}s{MAX_ROOM_PASSWORD}s"
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


@dataclass
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
    room_key: int = 0
    room_name: str = ""
    max_user: int = -1
    cur_user: int = -1
    is_private: bool = False
    is_play: bool = False

    BODY_FMT: ClassVar[str] = f"Q{MAX_ROOM_NAME}sbb??"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_INFO_PACKET(RecvPacketStruct):
    info_type: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_REQUEST_FRIEND_PACKET(RecvPacketStruct):
    requester_id: int = -1
    requester_nickname: str = ""

    BODY_FMT: ClassVar[str] = f"i{MAX_USER_NAME}s"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_DELETE_FRIEND_PACKET(RecvPacketStruct):
    target_id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_ADD_FRIEND_PACKET(RecvPacketStruct):
    friend_id: int = -1
    friend_nickname: str = ""

    BODY_FMT: ClassVar[str] = f"i{MAX_USER_NAME}s"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_LOBBY_USER_INFO_PACKET(RecvPacketStruct):
    user_id: int = -1
    nickname: str = ""

    BODY_FMT: ClassVar[str] = f"i{MAX_USER_NAME}s"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_FRIEND_INFO_PACKET(RecvPacketStruct):
    user_id: int = -1
    nickname: str = ""
    is_lobby: bool = False

    BODY_FMT: ClassVar[str] = f"i{MAX_USER_NAME}s?"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class S2C_RANKING_INFO_PACKET(RecvPacketStruct):
    nickname: str = ""
    score: int = -1

    BODY_FMT: ClassVar[str] = f"{MAX_USER_NAME}si"
    FMT: ClassVar[str] = RecvPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_LOGIN_PACKET(SendPacketStruct):
    user_id: bytes = b""
    user_password: bytes = b""

    BODY_FMT: ClassVar[str] = f"{MAX_USER_ID}s{MAX_USER_PASSWORD}s"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_MESSAGE_PACKET(SendPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_DISCONNECT_PACKET(SendPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_ADD_OPEN_ROOM_PACKET(SendPacketStruct):
    max_user: int = -1
    room_name: bytes = b""

    BODY_FMT: ClassVar[str] = f"b{MAX_ROOM_NAME}s"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_ADD_LOCK_ROOM_PACKET(SendPacketStruct):
    max_user: int = -1
    room_name: bytes = b""
    room_password: bytes = b""

    BODY_FMT: ClassVar[str] = f"b{MAX_ROOM_NAME}s{MAX_ROOM_PASSWORD}s"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_JOIN_OPEN_ROOM_PACKET(SendPacketStruct):
    room_key: int = 0

    BODY_FMT: ClassVar[str] = "Q"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_JOIN_LOCK_ROOM_PACKET(SendPacketStruct):
    room_key: int = 0
    room_password: bytes = b""

    BODY_FMT: ClassVar[str] = f"Q{MAX_ROOM_PASSWORD}s"
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

    BODY_FMT: ClassVar[str] = "b"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_REQUEST_FRIEND_PACKET(SendPacketStruct):
    recver_id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_DELETE_FRIEND_PACKET(SendPacketStruct):
    target_id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_ACCEPT_FRIEND_PACKET(SendPacketStruct):
    requester_id: int = -1

    BODY_FMT: ClassVar[str] = "i"
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_REQUEST_LOBBY_USER_LIST_PACKET(SendPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_REQUEST_FRIEND_LIST_PACKET(SendPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT


@dataclass
class C2S_REQUEST_RANKING_PACKET(SendPacketStruct):
    BODY_FMT: ClassVar[str] = ""
    FMT: ClassVar[str] = SendPacketStruct.HEADER_FMT + BODY_FMT
