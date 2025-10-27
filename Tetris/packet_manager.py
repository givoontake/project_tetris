"""
수신 스트림 처리 파이프라인 (내부 버퍼 보유 버전)
- merge_packet:   recv()로 받은 조각을 내부 버퍼에 병합
- process_packet: 내부 버퍼에서 완성된 패킷을 잘라 '스레드 세이프 큐'에 넣기만 함
- handle_packet:  (그대로 유지) 바이너리 전체만 받고 내부에서 type 바이트를 추출해 판별
- register_handler: 패킷 타입별 처리 콜백 등록 (세션 등 외부 상태는 콜백에서 처리)

패킷 경계 판정은 'size(H, 2바이트)' 기반:
[ size: H ][ type: B ][ payload... ]
"""

import struct
import queue

from define_format import *             # 포맷 문자열 모음 (MAX_* 포함)
from packet_type import *    
from define import BUF_SIZE           # S2C_LOGIN, ...

MAX_QUEUE_SIZE = 1024

class PacketManager:
    def __init__(self):
        # 내부 누적 버퍼 (TCP 수신 조각 저장)
        self.recv_buffer = bytearray()
        # 메인 스레드로 넘길 '완성 패킷' 큐 (thread-safe)
        self.queue: "queue.Queue[dict]"= queue.Queue(maxsize=MAX_QUEUE_SIZE)

    def dic_to_bytes(self, data: dict) ->bytes: # 송신을 위한 데이터 변환
        packet_bytes = bytearray()

        for key, value in data.items():
            if key not in PACK_FIELD_FMT:
                raise KeyError(f"PACK_FIELD_FMT에 '{key}'가 정의되어 있지 않습니다.")
            
            fmt = PACK_FIELD_FMT[key]

            # ---- 문자열(char 배열) 처리 ----
            if fmt.endswith("s"):
                n = int(fmt[:-1])  
                val_bytes = str(value).encode("utf-8")
                val_bytes = val_bytes[:n].ljust(n, b"\x00")
                packet_bytes.extend(struct.pack(f"<{fmt}", val_bytes))

            # ---- bool, int, short, long long 등 ----
            elif fmt in ("b", "h", "i", "q"):
                packet_bytes.extend(struct.pack(f"<{fmt}", int(value)))

            else:
                raise ValueError(f"지원하지 않는 포맷: {fmt} (key='{key}')")

        return bytes(packet_bytes)
    
    def bytes_to_dict(self, pkt: bytes) -> dict:

        pkt_struct = PACKET_STRUCT[pkt[2]] # 패킷 구조체(리스트)
        
        offset = 0
        data = {}
        for field in range(len(pkt_struct)): #구조체 내 필드 명
            fmt = PACK_FIELD_FMT[field] # 필드 명에 따른 포맷
            size = FIELD_SIZE[field] # 필드 명에 따른 필드 크기
            value = struct.unpack_from("<" + fmt, pkt, offset)[0] 
            data[field] = value
            offset += size

        return data
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
            while True:
                try:
                    self.queue.put(pkt_bytes, block=True, timeout=0.1)
                    break  # 성공하면 빠져나감
                except queue.Full:
                    continue  # 큐가 꽉 차 있으면 계속 재시도

            offset += size

        # 처리 완료 후, 사용한 부분 제거
        if offset:
            del buf[:offset]
            #buf = buf[offset:]

    def handle_packet(self, pkt_bytes: bytes) -> None:
        pkt_type = pkt_bytes[2]  # type 바이트(3번째)

        data = self.bytes_to_dict(pkt_bytes)
        
        while True:
            try:
                self.queue.put_nowait(data)
                break
            except self.queue.full():
                continue
