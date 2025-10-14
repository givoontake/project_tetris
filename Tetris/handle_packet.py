# handle_packet.py
"""
패킷 직렬화/역직렬화와 스트림 파싱을 담당.
- C 구조체 레이아웃에 맞춘 struct.unpack/pack
- Little-endian + 1바이트 정렬(pack(1)) 가정
"""
import struct
from typing import List, Tuple, Union
from packet_type import *
from define_packet import*

# 공통 헤더 (size: short, type: char)
_HEADER_FMT_LE = '<Hb'
_HEADER_SIZE   = struct.calcsize(_HEADER_FMT_LE)

# 개별 패킷 포맷
# S2C_LOGIN_PACKET: short size; char type; int id;
_S2C_LOGIN_FMT_LE = '<HbI'
_S2C_LOGIN_SIZE   = struct.calcsize(_S2C_LOGIN_FMT_LE)

PacketObj = Union[S2C_LOGIN_PACKET]

class HandlePacket:
    """
    - parse_stream(rx_buf): bytearray 스트림에서 가능한 모든 패킷을 파싱해 (ptype, packet_obj) 리스트로 반환.
      처리한 바이트는 rx_buf에서 제거한다.
    - pack_*/unpack_*: 필요 시 송수신용 헬퍼 제공(현재는 S2C_LOGIN만 사용).
    """

    def parse_stream(self, rx_buf: bytearray) -> List[Tuple[int, PacketObj]]:
        results: List[Tuple[int, PacketObj]] = []
        view = memoryview(rx_buf)
        offset = 0
        buflen = len(view)

        while True:
            # 헤더 확인
            if buflen - offset < _HEADER_SIZE:
                break
            size, ptype = struct.unpack_from(_HEADER_FMT_LE, view, offset)

            # 전체 패킷 도착 확인
            if buflen - offset < size:
                break

            # 패킷 슬라이스
            pkt_bytes = view[offset:offset + size].tobytes()

            # 타입별 언패킹
            if ptype == S2C_LOGIN:
                pkt = self.unpack_S2C_LOGIN(pkt_bytes)
                results.append((ptype, pkt))
            else:
                # 아직 미구현 타입은 스킵(필요 시 추가)
                pass

            offset += size

        # 소비한 바이트 제거
        if offset:
            del rx_buf[:offset]
        return results

    # ---------- Unpackers ----------
    def unpack_S2C_LOGIN(self, buf: bytes) -> S2C_LOGIN_PACKET:
        if len(buf) != _S2C_LOGIN_SIZE:
            raise ValueError(f"S2C_LOGIN_PACKET size mismatch: {len(buf)} != {_S2C_LOGIN_SIZE}")
        size, ptype, pid = struct.unpack(_S2C_LOGIN_FMT_LE, buf)
        return S2C_LOGIN_PACKET(size=size, type=ptype, id=pid)

    # ---------- Packers (필요 시 사용) ----------
    def pack_S2C_LOGIN(self, pkt: S2C_LOGIN_PACKET) -> bytes:
        return struct.pack(_S2C_LOGIN_FMT_LE, pkt.size, pkt.type, pkt.id)
