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
from typing import Callable, Dict

from define_format import *             # 포맷 문자열 모음 (MAX_* 포함)
from packet_type import *               # S2C_LOGIN, ...


class PacketManager:
    def __init__(self):
        # 내부 누적 버퍼 (TCP 수신 조각 저장)
        self._recv_buffer = bytearray()
        # 타입별 처리 콜백: {ptype: Callable[[object], None]}
        self._handlers: Dict[int, Callable[[object], None]] = {}
        # 메인 스레드로 넘길 '완성 패킷' 큐 (thread-safe)
        self.queue: "queue.Queue[bytes]" = queue.Queue(maxsize=1024)

    # ---- 콜백 등록 ----
    def register_handler(self, ptype: int, handler: Callable[[object], None]) -> None:
        """특정 패킷 타입에 대한 처리 콜백을 등록한다."""
        self._handlers[ptype] = handler

    # ---- 병합 단계 ----
    def merge_packet(self, chunk: bytes) -> None:
        """TCP로 받은 조각(chunk)을 내부 버퍼에 병합."""
        self._recv_buffer.extend(chunk)

    # ---- 커팅 단계 (큐에 적재) ----
    def process_packet(self) -> None:
        """
        내부 버퍼에서 완성된 패킷을 하나씩 잘라 '큐에 넣기만' 한다.
        handle_packet()은 호출하지 않는다(메인 스레드에서 호출).
        """
        view = memoryview(self._recv_buffer)
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
            try:
                self.queue.put_nowait(pkt_bytes)
            except queue.Full:
                # 오래된 것 하나 버리고 다시 넣기
                try:
                    _ = self.queue.get_nowait()
                except Exception:
                    pass
                # 재시도 (실패해도 그냥 스킵)
                try:
                    self.queue.put_nowait(pkt_bytes)
                except Exception:
                    pass

            offset += size

        # 처리 완료 후, 사용한 부분 제거
        if offset:
            del self._recv_buffer[:offset]

    # ---- 실제 패킷 처리 (메인 스레드에서 호출) ----
    def handle_packet(self, pkt_bytes: bytes) -> None:
        ptype = pkt_bytes[2]  # type 바이트(3번째)

        def slice_packet(off: int, n: int) -> bytes:
            return pkt_bytes[off:off+n]

        offset = 0

        if ptype == S2C_LOGIN:
            print("로그인 패킷 도착")
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);  offset += 4
            data = {"size": size, "type": type, "id": id}

        elif ptype == S2C_MESSAGE:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);  offset += 4
            data = {"size": size, "type": type, "id": id}

        elif ptype == S2C_DISCONNECT:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);  offset += 4
            data = {"size": size, "type": type, "id": id}

        elif ptype == S2C_ADD_OPEN_ROOM:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);        offset += 4
            maxuser = slice_packet(offset, 1);   offset += 1
            roomname = slice_packet(offset, MAX_ROOM_NAME); offset += MAX_ROOM_NAME
            data = {
                "size": size, "type": type, "id": id,
                "maxuser": maxuser, "roomname": roomname
            }

        elif ptype == S2C_ADD_LOCK_ROOM:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);        offset += 4
            maxuser = slice_packet(offset, 1);   offset += 1
            roomname = slice_packet(offset, MAX_ROOM_NAME); offset += MAX_ROOM_NAME
            roompassword = slice_packet(offset, MAX_ROOM_PASSWORD); offset += MAX_ROOM_PASSWORD
            data = {
                "size": size, "type": type, "id": id,
                "maxuser": maxuser, "roomname": roomname, "roompassword": roompassword
            }

        elif ptype == S2C_ADD_USER:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);        offset += 4
            isadd = slice_packet(offset, 1);     offset += 1
            name = slice_packet(offset, MAX_USER_NAME); offset += MAX_USER_NAME
            data = {"size": size, "type": type, "id": id, "isadd": isadd, "name": name}

        elif ptype == S2C_DELETE_USER:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);          offset += 4
            newhostid = slice_packet(offset, 4);   offset += 4
            data = {"size": size, "type": type, "id": id, "newhostid": newhostid}

        elif ptype == S2C_READY:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            id = slice_packet(offset, 4);        offset += 4
            isready = slice_packet(offset, 1);   offset += 1
            data = {"size": size, "type": type, "id": id, "isready": isready}

        elif ptype == S2C_START:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            isstart = slice_packet(offset, 1); offset += 1
            data = {"size": size, "type": type, "isstart": isstart}

        elif ptype == S2C_KICK:
            size = slice_packet(offset, 2); offset += 2
            type = slice_packet(offset, 1); offset += 1
            kickuserid = slice_packet(offset, 4); offset += 4
            data = {"size": size, "type": type, "kickuserid": kickuserid}

        else:
            return  # S2C 외 타입 무시

        handler = self._handlers.get(ptype)
        if handler:
            handler(data)
