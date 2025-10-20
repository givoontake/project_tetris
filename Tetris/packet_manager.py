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
from packet_type import *               # S2C_LOGIN, ...

MAX_QUEUE_SIZE = 1024

class PacketManager:
    def __init__(self):
        # 내부 누적 버퍼 (TCP 수신 조각 저장)
        self.recv_buffer = bytearray()
        # 메인 스레드로 넘길 '완성 패킷' 큐 (thread-safe)
        self.queue: "queue.Queue[dict]"= queue.Queue(maxsize=MAX_QUEUE_SIZE)

    # ---- 병합 단계 ----
    def merge_packet(self, chunk: bytes) -> None:
        """TCP로 받은 조각(chunk)을 내부 버퍼에 병합."""
        self.recv_buffer.extend(chunk)

    # ---- 커팅 단계 (큐에 적재) ----
    def process_packet(self) -> None:
        """
        내부 버퍼에서 완성된 패킷을 하나씩 잘라 '큐에 넣기만' 한다.
        handle_packet()은 호출하지 않는다(메인 스레드에서 호출).
        """
        view = memoryview(self.recv_buffer)
        buflen = len(view)
        offset = 0

        while True:
            # 남은 데이터가 size(2바이트)보다 적으면 중단
            if buflen - offset < 2:
                break

            size = struct.unpack_from("<H", view, offset)[0]

            # 패킷이 완성되지 않았으면 중단
            if buflen - offset < size:
                break

            # 완성된 패킷 바이트 '복사본' 확보 (메인 스레드로 넘길 것)
            pkt_bytes = view[offset:offset + size].tobytes()

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
            del self.recv_buffer[:offset]

    def handle_packet(self, pkt_bytes: bytes) -> None:
        pkt_type = pkt_bytes[2]  # type 바이트(3번째)

        def slice_packet(off: int, n: int) -> bytes:
            return pkt_bytes[off:off+n]

        offset = 0

        data = {}

        if pkt_type == S2C_LOGIN:
            print("로그인 패킷 도착")
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);  offset += 4
            data = {"size": size, "type": type, "id": id}

        elif pkt_type == S2C_MESSAGE:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);  offset += 4
            data = {"size": size, "type": type, "id": id}

        elif pkt_type == S2C_DISCONNECT:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);  offset += 4
            data = {"size": size, "type": type, "id": id}

        elif pkt_type == S2C_ADD_OPEN_ROOM:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);        offset += 4
            max_user = slice_packet(offset, 1);   offset += 1
            room_name = slice_packet(offset, MAX_ROOM_NAME); offset += MAX_ROOM_NAME
            data = {
                "size": size, "type": type, "id": id,
                "max_user": max_user, "room_name": room_name
            }

        elif pkt_type == S2C_ADD_LOCK_ROOM:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);        offset += 4
            max_user = slice_packet(offset, 1);   offset += 1
            room_name = slice_packet(offset, MAX_ROOM_NAME); offset += MAX_ROOM_NAME
            room_password = slice_packet(offset, MAX_ROOM_PASSWORD); offset += MAX_ROOM_PASSWORD
            data = {
                "size": size, "type": type, "id": id,
                "max_user": max_user, "room_name": room_name, "room_password": room_password
            }

        elif pkt_type == S2C_ADD_USER:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);        offset += 4
            is_add = slice_packet(offset, 1);     offset += 1
            name = slice_packet(offset, MAX_USER_NAME); offset += MAX_USER_NAME
            data = {"size": size, "type": type, "id": id, "is_add": is_add, "name": name}

        elif pkt_type == S2C_DELETE_USER:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);          offset += 4
            new_host_id = slice_packet(offset, 4);   offset += 4
            data = {"size": size, "type": type, "id": id, "new_host_id": new_host_id}

        elif pkt_type == S2C_READY:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);        offset += 4
            is_ready = slice_packet(offset, 1);   offset += 1
            data = {"size": size, "type": type, "id": id, "is_ready": is_ready}

        elif pkt_type == S2C_START:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            is_start = slice_packet(offset, 1); offset += 1
            data = {"size": size, "type": type, "is_start": is_start}

        elif pkt_type == S2C_KICK:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            kick_user_id = slice_packet(offset, 4); offset += 4
            data = {"size": size, "type": type, "kick_user_id": kick_user_id}

        while True:
            try:
                self.queue.put_nowait(data)
                break
            except self.queue.full():
                continue
