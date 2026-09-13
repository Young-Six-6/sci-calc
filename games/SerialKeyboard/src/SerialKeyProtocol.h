#pragma once

#include <Arduino.h>

#include "KeyboardMatrix.h"

namespace SerialKeyProtocol {

constexpr uint32_t BAUD_RATE = 115200;
constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr uint8_t FIRMWARE_REVISION = 1;

void reset();
void sendDeviceInfo(uint32_t timestampMs, uint32_t bootId);
void sendKeyEvent(const KeyEvent &event);
void sendState(uint32_t timestampMs, uint32_t stateMask, uint32_t eventCount,
               uint32_t droppedEvents);

}
