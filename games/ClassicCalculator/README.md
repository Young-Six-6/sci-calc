# SCI-CALC Classic Calculator

[Chinese](README_CH.md)

Classic Calculator is a standalone everyday calculator for the SCI-CALC handheld. It uses immediate-execution desk-calculator behavior instead of a scientific expression editor, making repeated arithmetic, prices, percentages, tax, and running totals quick to enter.

The current value occupies most of the 256x64 OLED in a custom seven-segment display. Digits stay 35 pixels high for normal values and narrow progressively only when a long number needs more room. Thousands separators are shown without reducing stored precision.

## Basic controls

| Printed key | Classic Calculator action |
| --- | --- |
| `0` to `9` | Enter digits |
| `.` | Decimal point |
| `EXP` | Enter `00` |
| `+`, `-`, `*`, `/` | Select an arithmetic operation |
| `Enter` | Equals |
| `^` | Percent |
| `ANG` | Change the sign of the current value |
| `Backspace` | Delete the last entered digit |
| `COT` | CE: clear only the current entry |
| `ESC` | AC: clear the current calculation |
| `EXTRA` | Undo the last completed arithmetic step |
| `TAB` | Open or close the paper tape |
| Hold `ESC` for one second | Restore `/main.bin` and return to the main firmware |

The operator keys follow classic immediate execution. For example, `2 + 3 * 4 Enter` gives `20`: pressing `*` first completes `2 + 3`, then starts the multiplication. Pressing an operator again before entering the next number replaces that operator.

`Enter` remembers the last operation and operand. `10 + 5 Enter Enter` therefore produces `15`, then `20`. `5 + Enter` uses `5` as the missing second operand and produces `10`.

Input is limited to 15 significant digits, matching the useful precision of the ESP32 double-precision number type. Very large and very small results switch to seven-segment `E` notation. Division by zero and overflow show a clear error; enter a digit or press `ESC` to begin again.

## Percent and tax

Percent follows business-calculator rules and completes the pending operation immediately:

| Keys | Result |
| --- | --- |
| `200 + 10 ^` | `220` |
| `200 - 10 ^` | `180` |
| `200 * 10 ^` | `20` |
| `200 / 10 ^` | `2000` |
| `10 ^` | `0.1` |

Tax shortcuts use the saved tax rate:

| Keys | Action |
| --- | --- |
| `Shift` + `+` | Add tax to the current value |
| `Shift` + `-` | Remove included tax and recover the pre-tax value |
| `Shift` + `^` | Store the current value as the tax rate, from 0 to 99.99 percent |

With a 13 percent rate, enter `100`, then hold `Shift` and press `+` to get `113`. Enter `113`, then hold `Shift` and press `-` to recover `100`. Tax transformations can also be applied to the second operand of a pending calculation.

## Memory and grand total

The six printed trigonometric keys become memory controls in this application:

| Printed key | Action |
| --- | --- |
| `SIN` | M+: add the display value to memory |
| `COS` | M-: subtract the display value from memory |
| `TAN` | MR: recall memory into the current entry |
| `SEC` | MC: clear memory |
| `CSC` | Recall the grand total |
| `Shift` + `CSC` | Clear the grand total |

Memory and its active state survive restarts and application changes. The grand total adds every result completed by `Enter` or a pending percent calculation and lasts until it is cleared or the device restarts. `M`, `GT`, and `U` indicators appear in the top status row when memory, grand total, or undo is available.

## Paper tape

The in-memory paper tape keeps the most recent 24 completed operations without writing the SD card or wearing flash.

| Key | Tape action |
| --- | --- |
| `TAB` | Open or close the tape |
| `8` / `2` | Move to an older or newer entry |
| `Enter` | Recall the selected result to the calculator |
| `Backspace` or `ESC` | Return to the calculator |
| `Shift` + `Backspace` | Clear the complete tape |

## Accounting setup

Press `Shift` + `TAB` to open setup. Use `8` and `2` to select a row, `4` and `6` to change it, and `TAB`, `Backspace`, or `ESC` to return.

- Decimals: `AUTO`, `0`, `2`, or `4` decimal places.
- Rounding: nearest, truncate toward zero, or round away from zero.
- Separator: show or hide thousands separators.
- Tax rate: adjust from 0 to 99.99 percent in 0.25-point steps.

Fixed-decimal modes round every completed arithmetic step, which is useful when intermediate results must remain at cents precision. Settings and memory are stored in NVS; tape and grand total stay in RAM to avoid unnecessary flash writes.

## Install

1. Copy [`classiccalc.bin`](../../sci_calc_code/bin/classiccalc.bin) to the SD-card root as `/classiccalc.bin`.
2. Copy the current [`main.bin`](../../sci_calc_code/bin/main.bin) to the SD-card root as `/main.bin`.
3. Open `Programs > Calculation > Classic Calc`.

For a complete USB installation, flash [`sci-calc-full-1x.xx-min-spiffs.bin`](../../sci_calc_code/bin/sci-calc-full-1x.xx-min-spiffs.bin) at address `0x0`.

## Build and test

```powershell
cd games\ClassicCalculator
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
python -m unittest discover -s tests -v
Copy-Item .pio\build\classic_calculator\firmware.bin ..\..\sci_calc_code\bin\classiccalc.bin -Force
```

The resulting `classiccalc.bin` is an application-slot image. Use the merged full image only for a complete installation at address `0x0`.
