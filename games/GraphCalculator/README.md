# Graph Calculator

[Chinese](README_CH.md)

Graph Calculator plots up to four functions together on the SCI-CALC 256x64 OLED. It provides an expression editor, pan and zoom controls, optional grid and axis labels, and a trace cursor.

## Install

1. Copy [`../../sci_calc_code/bin/graphcalc.bin`](../../sci_calc_code/bin/graphcalc.bin) to the SD-card root as `/graphcalc.bin`.
2. Copy the current [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) to the SD-card root as `/main.bin`.
3. Open `Programs > Calculation > Graph Calc`.

Hold `ESC` for about one second to restore `/main.bin` and return to the calculator.

## Enter functions

The editor starts with an empty function slot. Enter an expression such as `x^2-9`, then press `Enter` or `TAB` to open the graph.

Up to four non-empty functions can be displayed together:

| Key | Action |
| --- | --- |
| `RPN/NORM` | Select the next function slot |
| `Shift+RPN/NORM` | Add a function slot, up to four |
| `Shift+8` / `Shift+2` | Select the previous or next function slot |
| `Shift+Backspace` | Delete the selected slot; the only remaining slot is cleared |
| `Shift+5` | Insert `x` |
| `Shift+/` / `Shift+*` | Insert `(` or `)` |
| `Shift+4` / `Shift+6` | Move the text cursor left or right |
| `SIN`, `COS`, `TAN` | Insert the printed function |
| `Shift+SIN`, `Shift+COS`, `Shift+TAN` | Insert `asin(`, `acos(`, or `atan(` |
| `SEC`, `CSC`, `COT` | Insert `sec(`, `csc(`, or `cot(` |
| `Shift+SEC`, `Shift+CSC`, `Shift+COT` | Insert `abs(`, `log(`, or `pi` |
| `EXP` / `Shift+EXP` | Insert `exp(` or `ln(` |
| `^` / `Shift+^` | Insert a power or `sqrt(` |
| `Shift+9` | Insert `e` |
| `ANG` | Toggle radians and degrees |

Holding Backspace repeats deletion. A syntax error returns to the affected function slot for correction.

## View the graph

| Key | Action |
| --- | --- |
| `Shift+4` / `Shift+6` | Pan left or right |
| `Shift+8` / `Shift+2` | Pan up or down |
| `+` / `-` | Zoom in or out |
| `5` | Reset the view |
| `Enter` | Toggle trace mode |
| `4` / `6` | Move the trace cursor |
| `8` / `2` | Select the previous or next function for tracing |
| `.` | Toggle the grid |
| `RPN/NORM` | Toggle small axis labels |
| `ANG` | Toggle radians and degrees |
| `TAB` | Return to the editor |

F1 is solid, F2 is dotted, and F3/F4 use separate dash patterns. Empty slots are skipped.

## Expressions

Supported syntax includes:

- Operators: `+ - * / ^`
- Unary `+` and `-`
- Parentheses and decimal numbers
- Variables and constants: `x`, `pi`, `e`
- Implicit multiplication, such as `2x` or `3(x+1)`
- Functions: `sin`, `cos`, `tan`, `sec`, `csc`, `cot`, `asin`, `acos`, `atan`, `sqrt`, `ln`, `log`, `exp`, `abs`

The graph uses adaptive sampling around curves, turning points, and rapidly changing regions. Undefined or discontinuous regions are left separated instead of being joined by a misleading line.

## Hardware

| Device | Pins |
| --- | --- |
| OLED | CS GPIO5, DC GPIO16, RST GPIO17, MOSI GPIO23, SCK GPIO18 |
| SD card | CS GPIO4, MOSI GPIO23, MISO GPIO19, SCK GPIO18 |
| Keyboard rows | GPIO33, GPIO32, GPIO35, GPIO34, GPIO39 |
| Keyboard columns | GPIO13, GPIO12, GPIO14, GPIO27, GPIO26, GPIO25 |

## Build

```powershell
cd games\GraphCalculator
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
Copy-Item .pio\build\graph_calculator\firmware.bin ..\..\sci_calc_code\bin\graphcalc.bin -Force
```

The resulting `graphcalc.bin` is installed through the calculator's application loader. Use the merged full image for flashing at address `0x0`.
