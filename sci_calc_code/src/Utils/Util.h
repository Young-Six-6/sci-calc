#ifndef UTIL_H
#define UTIL_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <U8x8lib.h>
#include <BleKeyboard.h>
#include <string>
#include <cmath>
#include <vector>
#include <queue>
#include <stack>
#include <sstream>
#include <list>
#include <map>

#include <Update.h>
#include <FS.h>
#include <SD.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define SPI_CLK 18
#define SPI_DATA 23
#define SPI_CS 5
#define SPI_DC 16
#define SPI_RESET 17

extern U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2;

/*
VERSION HISTORY

v1.1

v1.2
Fixed Macropad bug where in some layers row 0 and 1 were swapped.

v1.3
Added a one-variable equation solver with Shift-based x and equals input.

v1.4
Fixed the battery sidebar layout and added multiple-root equation results.

v1.5
Added a standalone graphing calculator program.

v1.6
Added multi-function graphing and optional axis labels.

v1.7
Improved graph rendering and added a standalone PCM5102A music player.

v1.8
Integrated Bluetooth headphone output into the music player and expanded both OTA application slots.

v1.9
Added a standalone Wi-Fi SD card manager with a mobile web interface.

v1.10
Protected the Wi-Fi SD manager with a rotating six-digit physical-access code.

v1.11
Added a standalone USB serial display for PC telemetry and monochrome video.

v1.12
Added a standalone procedurally generated maze game.

v1.13
Added manifest-only SD application discovery with explicit display names.

v1.14
Added a standalone CHIP-8 and SUPER-CHIP emulator with an SD ROM library.

v1.15
Grouped standalone programs into expandable category menus with directional navigation.

v1.16
Kept sliding menus inside the main panel and refreshed the sidebar during transitions.

v1.17
Added a standalone polyphonic music keyboard with visualizers and MIDI recording.

v1.18
Added a standalone falling-tile rhythm game with built-in and SD-card charts.

v1.19
Added a standalone seven-segment classic calculator for everyday and accounting work.

*/

const std::string VERSION = "1.19";
const int REFRESH_DURATION = 10; // The refresh rate of the the program (in milliseconds)
const float DELTA_TIME = 100; // in milliseconds
extern bool sample;
const int MAXOP = 25; // The maximum number of operators, if you want to implement your own operator, remember to increase this accordingly
const int MAXNODES = 1000; // The maximum number of nodes when evaluating expressions
const int MAXANI = 100; // The maximum number of animation instances
const long double EPSILON = 1e-8; // Used in evaluating derivatives

const int WIDTH = 256; // The width of the screen (in pixels)
const int HEIGHT = 64; // The height of the screen (in pixels)

// The key ids for the calculator
const uint16_t KEY_LAYER_SWITCH = 0xFC;
const uint16_t KEY_EXP = 0xFD;
const uint16_t KEY_LN = 0xFE;
const uint16_t KEY_SIN = 0xFF;
const uint16_t KEY_COS = 0x100;
const uint16_t KEY_TAN = 0x101;
const uint16_t KEY_SEC = 0x102;
const uint16_t KEY_CSC = 0x103;
const uint16_t KEY_COT = 0x104;
const uint16_t KEY_ASIN = 0x105;
const uint16_t KEY_ACOS = 0x106;
const uint16_t KEY_ATAN = 0x107;
const uint16_t KEY_SQRT = 0x108;
const uint16_t KEY_MODE_SWITCH = 0x109;
const uint16_t KEY_RPN_SWITCH = 0x10A;

// Keyboard matrix dimensions
const byte ROWCNT = 5;
const byte COLCNT = 6;
// Keyboard matrix GPIOs
extern byte ROW_MISO[ROWCNT];
extern byte COL_MOSI[COLCNT];

// Hide sidebar
extern bool HIDE_SIDEBAR;

extern char keys[ROWCNT][COLCNT];
//extern Keypad keypad;

enum KeyStatus {
    NOT_PRESSED, PRESSED, RISING_EDGE, FALLING_EDGE
};

enum Element {
    MENU, TEXT
};

enum AnimationType {
    LINEAR, INDENT, INDENTINV, BOUNCE
};

extern AnimationType menuScroll;
extern AnimationType menuEntry;
extern AnimationType cursorScroll;
extern AnimationType sliderMove;


extern BleKeyboard bleKeyboard;

extern int lastNode;

enum Token {
    ADD, SUB, MUL, DIV, POW, SQRT, LN, EXP, SIN, COS, TAN, SEC, CSC, COT, ASIN, ACOS, ATAN, LPARA, RPARA, EQUALS, NEGATIVE, NUM, VAR
};

//settings
extern bool cursorMode;

extern bool angleMode;
extern bool RPNMode;
extern std::string tokenStr[MAXOP];

// Various util functions
std::string getKeyString(uint16_t x);
bool strIsOperator(std::string str);
bool strIsNum(std::string str);
bool cmpLongDouble(long double a, long double b);
int getPrecedenceToken(Token token);
Token convertToEnum(std::string s);
std::string convertToString(Token t);
std::string numToStr(long double x);
std::string numToStrRaw(long double x);
void performUpdate(Stream &updateSource, size_t updateSize);
void updateFromFS(fs::FS &fs, std::string);
void rebootEspWithReason(std::string reason);
bool checkSyntax(std::string str);

// Used for program loading
void initFromFile();
#endif
