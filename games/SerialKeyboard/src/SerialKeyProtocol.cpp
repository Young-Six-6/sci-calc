#include "SerialKeyProtocol.h"

namespace SerialKeyProtocol {
namespace {
constexpr uint8_t MAGIC[4] = {'S', 'K', 'B', '1'};
constexpr uint16_t CAP_PRESS_RELEASE = 0x0001;
constexpr uint16_t CAP_HELD_DURATION = 0x0002;
constexpr uint16_t CAP_STATE_SNAPSHOT = 0x0004;
constexpr size_t HEADER_BYTES = 10;

enum class PacketType : uint8_t {
    DeviceInfo = 0x01,
    KeyEvent = 0x02,
    State = 0x03
};

uint16_t sequence = 0;

uint16_t crc16Update(uint16_t crc, uint8_t value) {
    crc ^= static_cast<uint16_t>(value) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
        crc = (crc & 0x8000U) ? static_cast<uint16_t>((crc << 1) ^ 0x1021U)
                              : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

void writeLe16(uint8_t *target, uint16_t value) {
    target[0] = static_cast<uint8_t>(value & 0xFFU);
    target[1] = static_cast<uint8_t>(value >> 8);
}

void writeLe32(uint8_t *target, uint32_t value) {
    target[0] = static_cast<uint8_t>(value & 0xFFU);
    target[1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    target[2] = static_cast<uint8_t>((value >> 16) & 0xFFU);
    target[3] = static_cast<uint8_t>((value >> 24) & 0xFFU);
}

uint16_t packetCrc(const uint8_t *header, const uint8_t *payload, uint16_t length) {
    uint16_t crc = 0xFFFFU;
    for (size_t index = 0; index < HEADER_BYTES; ++index) {
        crc = crc16Update(crc, header[index]);
    }
    for (uint16_t index = 0; index < length; ++index) {
        crc = crc16Update(crc, payload[index]);
    }
    return crc;
}

void sendPacket(PacketType type, uint32_t timestampMs, const uint8_t *payload,
                uint16_t length) {
    uint8_t header[HEADER_BYTES];
    header[0] = PROTOCOL_VERSION;
    header[1] = static_cast<uint8_t>(type);
    writeLe16(header + 2, sequence++);
    writeLe32(header + 4, timestampMs);
    writeLe16(header + 8, length);

    const uint16_t crc = packetCrc(header, payload, length);
    const uint8_t checksum[2] = {
        static_cast<uint8_t>(crc & 0xFFU),
        static_cast<uint8_t>(crc >> 8)
    };

    Serial.write(MAGIC, sizeof(MAGIC));
    Serial.write(header, sizeof(header));
    if (length) Serial.write(payload, length);
    Serial.write(checksum, sizeof(checksum));
}
}

void reset() {
    sequence = 0;
}

void sendDeviceInfo(uint32_t timestampMs, uint32_t bootId) {
    uint8_t payload[14] = {};
    payload[0] = FIRMWARE_REVISION;
    payload[1] = KeyboardMatrix::ROWS;
    payload[2] = KeyboardMatrix::COLS;
    payload[3] = KeyboardMatrix::KEY_COUNT;
    writeLe32(payload + 4, BAUD_RATE);
    writeLe16(payload + 8, CAP_PRESS_RELEASE | CAP_HELD_DURATION | CAP_STATE_SNAPSHOT);
    writeLe32(payload + 10, bootId);
    sendPacket(PacketType::DeviceInfo, timestampMs, payload, sizeof(payload));
}

void sendKeyEvent(const KeyEvent &event) {
    uint8_t payload[14] = {};
    payload[0] = KeyboardMatrix::keyId(event.key);
    payload[1] = static_cast<uint8_t>(event.action);
    payload[2] = event.row;
    payload[3] = event.col;
    payload[4] = (event.stateMask & (1UL << KeyboardMatrix::keyId(KeyCode::Shift))) ? 0x01 : 0x00;
    payload[5] = 0;
    writeLe32(payload + 6, event.heldMs);
    writeLe32(payload + 10, event.stateMask);
    sendPacket(PacketType::KeyEvent, event.timestampMs, payload, sizeof(payload));
}

void sendState(uint32_t timestampMs, uint32_t stateMask, uint32_t eventCount,
               uint32_t droppedEvents) {
    uint8_t payload[12] = {};
    writeLe32(payload, stateMask);
    writeLe32(payload + 4, eventCount);
    writeLe32(payload + 8, droppedEvents);
    sendPacket(PacketType::State, timestampMs, payload, sizeof(payload));
}

}
