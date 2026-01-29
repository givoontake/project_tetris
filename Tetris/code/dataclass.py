from dataclasses import dataclass

@dataclass
class RoomData:
    room_id: int = -1
    locked: str = "방 제목"
    title: str = "공개"
    cur_user: str = "현재 인원"
    max_user: str = "최대 인원"
    status: str = "방 상태"