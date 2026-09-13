import importlib.util
import struct
import sys
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).parents[1] / "pc" / "serial_keyboard.py"
SPEC = importlib.util.spec_from_file_location("serial_keyboard", MODULE_PATH)
serial_keyboard = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = serial_keyboard
SPEC.loader.exec_module(serial_keyboard)


class ProtocolTests(unittest.TestCase):
    def test_crc_reference_vector(self):
        self.assertEqual(serial_keyboard.crc16_ccitt(b"123456789"), 0x29B1)

    def test_fragmented_key_event_round_trip(self):
        state_mask = (1 << 20) | (1 << 24)
        payload = serial_keyboard.KEY_EVENT.pack(
            20,
            serial_keyboard.ACTION_PRESS,
            3,
            2,
            1,
            0,
            0,
            state_mask,
        )
        encoded = serial_keyboard.build_packet(
            serial_keyboard.PACKET_KEY_EVENT, 0xBEEF, 123456, payload
        )
        decoder = serial_keyboard.PacketDecoder()
        packets = []
        for byte in encoded:
            packets.extend(decoder.feed(bytes((byte,))))

        self.assertEqual(len(packets), 1)
        event = serial_keyboard.decode_packet(packets[0])
        self.assertIsInstance(event, serial_keyboard.KeyEventRecord)
        self.assertEqual(event.key, "3")
        self.assertEqual(event.action, "PRESS")
        self.assertEqual(event.modifiers, ("SHIFT",))
        self.assertEqual(event.state_mask, state_mask)
        self.assertEqual(event.sequence, 0xBEEF)
        self.assertEqual(event.device_ms, 123456)

    def test_release_carries_hold_duration(self):
        payload = serial_keyboard.KEY_EVENT.pack(
            21,
            serial_keyboard.ACTION_RELEASE,
            3,
            3,
            0,
            0,
            742,
            0,
        )
        packet = serial_keyboard.PacketDecoder().feed(
            serial_keyboard.build_packet(serial_keyboard.PACKET_KEY_EVENT, 8, 900, payload)
        )[0]
        event = serial_keyboard.decode_packet(packet)
        self.assertEqual(event.key, "ENTER")
        self.assertEqual(event.held_ms, 742)

    def test_decoder_discards_noise_and_recovers_after_bad_crc(self):
        info_payload = serial_keyboard.DEVICE_INFO.pack(
            1, 5, 6, 30, serial_keyboard.BAUD_RATE, 0x0007, 0x12345678
        )
        info = serial_keyboard.build_packet(
            serial_keyboard.PACKET_DEVICE_INFO, 10, 20, info_payload
        )
        corrupt = bytearray(
            serial_keyboard.build_packet(serial_keyboard.PACKET_STATE, 11, 30, bytes(12))
        )
        corrupt[-1] ^= 0x80
        state_payload = serial_keyboard.STATE.pack(3, 42, 0)
        state = serial_keyboard.build_packet(
            serial_keyboard.PACKET_STATE, 12, 40, state_payload
        )

        decoder = serial_keyboard.PacketDecoder()
        packets = decoder.feed(b"startup text\r\n" + info + corrupt + state)
        self.assertEqual([packet.sequence for packet in packets], [10, 12])
        self.assertEqual(decoder.invalid_packets, 1)
        self.assertGreater(decoder.discarded_bytes, 0)

        device = serial_keyboard.decode_packet(packets[0])
        snapshot = serial_keyboard.decode_packet(packets[1])
        self.assertEqual(device.boot_id, 0x12345678)
        self.assertEqual(snapshot.event_count, 42)

    def test_invalid_key_position_is_rejected(self):
        payload = serial_keyboard.KEY_EVENT.pack(
            6, serial_keyboard.ACTION_PRESS, 0, 0, 0, 0, 0, 1 << 6
        )
        packet = serial_keyboard.Packet(
            serial_keyboard.PROTOCOL_VERSION,
            serial_keyboard.PACKET_KEY_EVENT,
            1,
            1,
            payload,
        )
        with self.assertRaises(serial_keyboard.ProtocolError):
            serial_keyboard.decode_packet(packet)

    def test_future_packet_type_remains_available_to_extensions(self):
        packet = serial_keyboard.Packet(
            serial_keyboard.PROTOCOL_VERSION, 0x80, 3, 99, b"future"
        )
        self.assertIs(serial_keyboard.decode_packet(packet), packet)

    def test_reference_builder_rejects_oversize_payload(self):
        with self.assertRaises(ValueError):
            serial_keyboard.build_packet(1, 1, 1, bytes(65))

    def test_live_grid_shows_every_key_and_current_state(self):
        state_mask = (1 << 0) | (1 << 20) | (1 << 29)
        grid = serial_keyboard.format_key_grid(state_mask)
        self.assertEqual(len(grid.splitlines()), 5)
        self.assertEqual(grid.count("DOWN"), 3)
        self.assertEqual(grid.count("  UP"), 27)
        for key_name in serial_keyboard.KEY_NAMES:
            self.assertIn(key_name, grid)
        self.assertRegex(grid, r"ESC\s+DOWN")
        self.assertRegex(grid, r"3\s+DOWN")
        self.assertRegex(grid, r"TAB\s+DOWN")

    def test_monitor_frame_reports_transport_health(self):
        frame = serial_keyboard.format_monitor_frame(
            "COM7", True, 0x1234ABCD, 1250, 1 << 6, 9, 2, 1, None
        )
        self.assertIn("COM7  CONNECTED", frame)
        self.assertIn("boot=1234ABCD", frame)
        self.assertIn("down=01/30", frame)
        self.assertIn("events=9", frame)
        self.assertIn("gaps=2", frame)
        self.assertIn("dropped=1", frame)


if __name__ == "__main__":
    unittest.main()
