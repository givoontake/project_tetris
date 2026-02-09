import struct
import queue

from tetris.net.define_format import *             # 포맷 문자열 모음 (MAX_* 포함)
from tetris.net.packet_types import *    
from tetris.net.packet_structs import *
from tetris.net.packet_registry import *

MAX_QUEUE_SIZE = 1024

class PacketManager:
    def __init__(self):
        # 내부 누적 버퍼 (TCP 수신 조각 저장)
        self.recv_buffer = bytearray()
        # 메인 스레드로 넘길 '완성 패킷' 큐 (thread-safe)
        self.queue: "queue.Queue[dict]"= queue.Queue(maxsize=MAX_QUEUE_SIZE)

    def str_to_bytes(self, s: str, array_len: int) -> bytearray:
        return str(s).encode("utf-8")[:array_len].ljust(array_len, b"\x00")
    
    def struct_to_values(self, data: SendPacketStruct):
        values: list = []
        flds = fields(data)
        for fld in flds:
            value = getattr(data, fld.name)
            values.append(value)

        return values

    def struct_to_bytes(self, data: SendPacketStruct) -> bytes:
        fmts = data.FMT 
        flds = fields(data) # 들어온 필드의 변수 선언 순서를 따른다.
        if len(flds) != len(fmts):
            raise ValueError(f"필드 수와 포맷 수 불일치")
        values: list = []
        
        full_fmt: str = "<"

        for fld, fmt in zip(flds, fmts):
            value = getattr(data, fld.name)
            full_fmt += fmt

            if fmt.endswith("s"):
                length = int(fmt[:-1]) # 포맷 문자 제거 및 나머지 숫자 문자열 부분 실제 숫자로 변경
                # lJust는 현재 문자열을 받아 length만큼 새 바이트 버퍼를 할당하고, 복사한 다음 빈 공간을 널로 채운다는 것
                # 우선 길이 검증을 위해 바이트로 변환한 문자열을 최대 길이만큼 자른다. 부족하면 ljust(left justify) 함수에서 널로 채울 것.

                bytes_str = str(value).encode("utf-8")[:length].ljust(length, b"\x00")
                
                values.append(bytes_str)
            elif fmt in ("b", "h", "i", "q"):
                values.append(int(value))
            else:
                raise ValueError(f"지원하지 않는 포맷: {fmt}")
            
        return struct.pack(full_fmt, *values) # * 연산자는 리스트를 풀어 각 값을 위치 인자로 전달한다 *[1,2] -> 1, 2


    def bytes_to_struct(self, pkt: bytes) -> RecvPacketStruct:
        pkt_type = pkt[2]
        struct_name = PACKET_REGISTRY[pkt_type]
        packet_struct = struct_name()

        bytes_data = struct.unpack(struct_name.FMT, pkt)
        packet_struct.fill_data(bytes_data)

        return packet_struct
    
    # ---- 병합 단계 ----
    def merge_packet(self, pkt: bytes) -> None:
        """TCP로 받은 조각(chunk)을 내부 버퍼에 병합."""
        self.recv_buffer.extend(pkt)

    # ---- 커팅 단계 (큐에 적재) ----
    def process_packet(self) -> None:
        """
        내부 버퍼에서 완성된 패킷을 하나씩 잘라 '큐에 넣기만' 한다.
        handle_packet()은 호출하지 않는다(메인 스레드에서 호출).
        """
        buf = self.recv_buffer
        buflen = len(buf)
        offset = 0

        while True:
            # 남은 데이터가 size(2바이트)보다 적으면 중단
            if buflen - offset < 2:
                break

            size = struct.unpack_from("<H", buf, offset)[0]

            # 패킷이 완성되지 않았으면 중단
            if buflen - offset < size:
                break

            # 완성된 패킷 바이트 '복사본' 확보 (메인 스레드로 넘길 것)
            pkt_bytes = bytes(buf[offset:offset + size])

            # 스레드 세이프 큐에 적재 (꽉 차면 최신성 보존을 위해 가장 오래된 것을 드롭)
            data = self.bytes_to_struct(pkt_bytes)
            
            while True:
                try:
                    self.queue.put_nowait(data)
                    break
                except self.queue.full():
                    continue

            offset += size

        # 처리 완료 후, 사용한 부분 제거
        if offset:
            del buf[:offset]
            #buf = buf[offset:]

        
    
