# GraphCalculator

Standalone graphing firmware for the ESP32 calculator with a 256x64 SSD1322 OLED.

## Hardware

- OLED: hardware SPI, CS 5, DC 16, reset 17
- SD card: CS 4
- Keyboard rows: 33, 32, 35, 34, 39
- Keyboard columns: 13, 12, 14, 27, 26, 25

## Build

```powershell
pio run
Copy-Item .pio\build\graph_calculator\firmware.bin .\firmware.bin
```

Put `firmware.bin` on the SD card as `/graphcalc.bin`. The main calculator can
launch that image through its SD firmware loader. Keep the main calculator image
on the card as `/main.bin`; hold ESC for about one second to restore it and reboot.

## Keys

The physical layout follows the main calculator. TAB switches pages. ENTER on the
editor validates the expression and opens the graph. ANG toggles radians/degrees.

Editor mappings:

- RPN/NORM: select the next function slot
- SHIFT+RPN/NORM: add a function slot (maximum four)
- SHIFT+8/2: select the previous/next function slot
- SHIFT+Backspace: delete the selected function; F1 is cleared when it is the only slot
- SHIFT+5: `x`
- SHIFT+/: `(`, SHIFT+*: `)`
- SHIFT+4/6: move cursor left/right
- SIN/COS/TAN: direct functions; with SHIFT: ASIN/ACOS/ATAN
- SEC/CSC/COT: direct functions; SHIFT gives `abs(` / `log(` / `pi`
- EXP: `exp(`; SHIFT+EXP: `ln(`
- `^`: power; SHIFT+`^`: `sqrt(`
- SHIFT+9: `e`
- Hold backspace for repeated deletion

Graph mappings:

- SHIFT+4/6/8/2: pan left/right/up/down
- `+` / `-`: zoom in/out
- `5`: reset view
- ENTER: toggle trace; plain 4/6 moves the trace cursor
- Plain 8/2: select the previous/next function for tracing
- `.`: toggle grid
- RPN/NORM: toggle small axis labels
- ANG: toggle radians/degrees
- TAB: return to editor

Up to four non-empty functions are drawn together. F1 is solid; F2 is dotted;
F3 and F4 use different dash patterns. Empty slots are ignored. A syntax error
returns to the corresponding function slot so it can be corrected.

Expressions support `+ - * / ^`, unary signs, parentheses, decimal numbers, `x`,
`pi`, `e`, implicit multiplication, and these functions: `sin`, `cos`, `tan`,
`sec`, `csc`, `cot`, `asin`, `acos`, `atan`, `sqrt`, `ln`, `log`, `exp`, `abs`.
