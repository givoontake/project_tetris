from dataclasses import dataclass

@dataclass
class RoomData:
    room_gen: int = -1
    is_private: str = "공개여부"
    title: str = "방 제목"
    cur_user: str = "현재 인원"
    max_user: str = "최대 인원"
    is_play: str = "방 상태"


@dataclass
class EventFriend:
    ev_type: str
    target_id: int
    pos: tuple
