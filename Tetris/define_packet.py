# packet_structs.py
"""
C 스타일 패킷 '구조체' 정의만 보관하는 파일.
직렬화/역직렬화 로직은 handle_packet.py의 HandlePacket에서 담당한다.
"""
from dataclasses import dataclass

# 공통 헤더
@dataclass
class HEADER:
    size: int   # short
    type: int   # char

# ----- 개별 패킷들 -----

# struct S2C_LOGIN_PACKET {
#     short size;
#     char  type;
#     int   id;
# };
@dataclass
class S2C_LOGIN_PACKET:
    size: int
    type: int
    id: int
