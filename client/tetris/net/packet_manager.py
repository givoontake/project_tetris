import queue
import struct

from tetris.net.packet_registry import PACKET_REGISTRY
from tetris.net.packet_structs import PacketHeader, RecvPacketStruct, SendPacketStruct
from tetris.net.packet_types import S2C_MESSAGE

MAX_QUEUE_SIZE = 1024


class PacketManager:
    def __init__(self):
        self.recv_buffer = bytearray()
        self.queue: "queue.Queue[RecvPacketStruct]" = queue.Queue(maxsize=MAX_QUEUE_SIZE)

    def struct_to_bytes(self, data: SendPacketStruct) -> bytes:
        return struct.pack(data.FMT, *data.to_values())

    def bytes_to_struct(self, pkt: bytes) -> RecvPacketStruct:
        packet_size, pkt_type = struct.unpack_from(PacketHeader.FMT, pkt, 0)
        struct_type = PACKET_REGISTRY[pkt_type]
        packet_struct = struct_type()

        if pkt_type == S2C_MESSAGE:
            front_values = struct.unpack_from(packet_struct.FMT, pkt, 0)
            message_size = packet_size - struct.calcsize(packet_struct.FMT)
            message_values = struct.unpack_from(f"<{message_size}s", pkt, struct.calcsize(packet_struct.FMT))
            unpacked_data = front_values + message_values
        else:
            unpacked_data = struct.unpack(struct_type.FMT, pkt)

        packet_struct.fill_data(unpacked_data)
        return packet_struct

    def merge_packet(self, pkt: bytes) -> None:
        self.recv_buffer.extend(pkt)

    def process_packet(self) -> None:
        buf = self.recv_buffer
        buflen = len(buf)
        offset = 0

        while True:
            if buflen - offset < PacketHeader.SIZE:
                break

            size = struct.unpack_from("<H", buf, offset)[0]
            if size < PacketHeader.SIZE:
                raise ValueError(f"Invalid packet size: {size}")

            if buflen - offset < size:
                break

            pkt_bytes = bytes(buf[offset:offset + size])
            data = self.bytes_to_struct(pkt_bytes)

            while True:
                try:
                    self.queue.put_nowait(data)
                    break
                except queue.Full:
                    continue

            offset += size

        if offset:
            del buf[:offset]
