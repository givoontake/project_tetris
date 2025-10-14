# handle_packet.py
"""
직렬화/역직렬화 + 스트림 파싱
변경 사항:
- 포맷 문자열은 모두 define_format 에서 가져와 사용.
- 구조체 크기 상수는 미리 정의하지 않고, 필요할 때 struct.calcsize로 계산.
- '헤더' 상수/포맷은 사용하지 않음. 스트림에서 먼저 2바이트(short) size만 읽어
  패킷 경계를 판정한 뒤, 해당 덩어리를 타입별 포맷으로 언팩한다.
"""

import struct
from typing import List, Tuple, Union

from packet_type import *                 # S2C_LOGIN 등 타입 상수
from define_packet import *               # dataclass 들 (예: S2C_LOGIN_PACKET)
import define_format as F                 # 포맷 문자열의 단일 출처


PacketObj = Union[S2C_LOGIN_PACKET]       # 필요 시 여기에 패킷 dataclass를 추가


class HandlePacket:
    """
    parse_stream(rx_buf):
        - bytearray 스트림에서 가능한 모든 패킷을 파싱해 (ptype, packet_obj) 리스트를 반환.
        - 처리한 바이트는 rx_buf에서 제거한다.
    """

    def parse_stream(self, rx_buf: bytearray) -> List[Tuple[int, PacketObj]]:
        results: List[Tuple[int, PacketObj]] = []
        view = memoryview(rx_buf)
        buflen = len(view)
        offset = 0

        while True:
            # 1) 최소한 size(short, 2바이트)를 읽을 수 있어야 함
            if buflen - offset < 2:
                break

            # 2) 첫 2바이트는 전체 패킷 길이(= size)로 사용
            size = struct.unpack_from('<H', view, offset)[0]

            # 3) size만큼 온전한 패킷이 도착했는지 확인
            if buflen - offset < size:
                break

            # 4) 패킷 바이트 확보 (이 시점에서만 슬라이스 복사)
            pkt_bytes = view[offset:offset + size].tobytes()

            # 5) 타입은 size 직후 1바이트 (H 다음 B) — 헤더 상수는 쓰지 않음
            #    (size=2바이트 뒤의 1바이트를 타입으로 해석)
            if len(pkt_bytes) < 3:
                # 이론상 도달하지 않지만, 방어 코드
                break
            ptype = pkt_bytes[2]  # unsigned byte(0~255)

            # 6) 타입별 언패킹 (포맷은 define_format의 상수 사용)
            if ptype == S2C_LOGIN:
                pkt = self._unpack_S2C_LOGIN(pkt_bytes)
                results.append((ptype, pkt))
            else:
                # 미지원/미구현 타입은 스킵(필요 시 elif 분기 추가)
                pass

            # 7) 다음 패킷으로 이동
            offset += size

        # 소비된 바이트 제거
        if offset:
            del rx_buf[:offset]
        return results

    # ---------- Unpackers ----------
    def _unpack_S2C_LOGIN(self, buf: bytes) -> S2C_LOGIN_PACKET:
        """
        포맷: define_format.S2C_LOGIN_PACKET_FMT  (예: "<HBI")
        - H: size(short)
        - B: type(uchar)
        - I: id(uint32)
        """
        fmt = F.S2C_LOGIN_PACKET_FMT
        expected = struct.calcsize(fmt)  # 필요 시에만 계산 (사전 상수 없음)
        if len(buf) != expected:
            raise ValueError(f"S2C_LOGIN size mismatch: {len(buf)} != {expected}")
        size, ptype, pid = struct.unpack(fmt, buf)
        return S2C_LOGIN_PACKET(size=size, type=ptype, id=pid)

    # ---------- Packers (필요 시 사용) ----------
    def pack_S2C_LOGIN(self, pkt: S2C_LOGIN_PACKET) -> bytes:
        """
        테스트/모의 송신 등에 사용.
        포맷은 define_format에서 가져오며, 크기는 struct.calcsize로 매번 확인 가능.
        """
        fmt = F.S2C_LOGIN_PACKET_FMT
        return struct.pack(fmt, pkt.size, pkt.type, pkt.id)
