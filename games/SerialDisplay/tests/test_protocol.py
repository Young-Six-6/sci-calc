import importlib.util
import random
import struct
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).parents[1] / "pc" / "serial_display.py"
SPEC = importlib.util.spec_from_file_location("serial_display", MODULE_PATH)
serial_display = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(serial_display)


class FakeDevicePort:
    def __init__(self):
        self.responses = bytearray()
        self.requests = bytearray()
        self.commands = []

    def write(self, data):
        self.requests.extend(data)
        while len(self.requests) >= 10:
            length = struct.unpack("<H", self.requests[8:10])[0]
            packet_size = 12 + length
            if len(self.requests) < packet_size:
                break
            packet = bytes(self.requests[:packet_size])
            del self.requests[:packet_size]
            command, sequence, payload = serial_display.parse_packet_bytes(packet)
            self.commands.append((command, payload))
            if command == serial_display.CMD_HELLO:
                info = struct.pack(
                    "<HHBBIHBB",
                    serial_display.SCREEN_WIDTH,
                    serial_display.SCREEN_HEIGHT,
                    1,
                    3,
                    serial_display.BAUD_RATE,
                    serial_display.MAX_PAYLOAD,
                    serial_display.PROTOCOL_VERSION,
                    3,
                )
                response = serial_display.build_packet(serial_display.CMD_INFO, sequence, info)
            else:
                response = serial_display.build_packet(
                    serial_display.CMD_ACK, sequence, bytes((command, 0))
                )
            self.responses.extend(response)
        return len(data)

    def read(self, size):
        count = min(size, 3, len(self.responses))
        output = bytes(self.responses[:count])
        del self.responses[:count]
        return output

    def flush(self):
        pass

    def reset_input_buffer(self):
        self.responses.clear()


class ProtocolTests(unittest.TestCase):
    def test_crc_reference_vector(self):
        self.assertEqual(serial_display.crc16_ccitt(b"123456789"), 0x29B1)

    def test_packet_round_trip(self):
        payload = bytes(range(64))
        packet = serial_display.build_packet(serial_display.CMD_FRAME_RLE, 0xBEEF, payload)
        command, sequence, decoded = serial_display.parse_packet_bytes(packet)
        self.assertEqual(command, serial_display.CMD_FRAME_RLE)
        self.assertEqual(sequence, 0xBEEF)
        self.assertEqual(decoded, payload)

    def test_packet_detects_corruption(self):
        packet = bytearray(serial_display.build_packet(serial_display.CMD_PING, 7))
        packet[-1] ^= 0x40
        with self.assertRaises(serial_display.ProtocolError):
            serial_display.parse_packet_bytes(bytes(packet))

    def test_rle_round_trip_for_sparse_frame(self):
        frame = bytes([0] * 1000 + [255] * 800 + [0] * 248)
        encoded = serial_display.rle_encode(frame)
        self.assertLess(len(encoded), len(frame))
        self.assertEqual(serial_display.rle_decode(encoded), frame)

    def test_rle_round_trip_for_noisy_frame(self):
        generator = random.Random(20260906)
        frame = bytes(generator.randrange(256) for _ in range(serial_display.FRAME_BYTES))
        self.assertEqual(serial_display.rle_decode(serial_display.rle_encode(frame)), frame)

    def test_rle_rejects_incomplete_frame(self):
        with self.assertRaises(serial_display.ProtocolError):
            serial_display.rle_decode(bytes((10, 0)))

    def test_virtual_device_handshake_and_frame_acknowledgements(self):
        client = serial_display.SerialDisplayClient.__new__(serial_display.SerialDisplayClient)
        client.port = FakeDevicePort()
        client.response_timeout = 0.2
        client.sequence = 0

        info = client._handshake()
        self.assertEqual(info["width"], serial_display.SCREEN_WIDTH)
        self.assertEqual(info["height"], serial_display.SCREEN_HEIGHT)
        self.assertEqual(client.send_frame(bytes(serial_display.FRAME_BYTES)), "rle")

        noisy = bytes((index * 73) & 0xFF for index in range(serial_display.FRAME_BYTES))
        self.assertEqual(client.send_frame(noisy), "raw")
        self.assertEqual(
            [command for command, _ in client.port.commands],
            [serial_display.CMD_HELLO, serial_display.CMD_FRAME_RLE, serial_display.CMD_FRAME_RAW],
        )


if __name__ == "__main__":
    unittest.main()
