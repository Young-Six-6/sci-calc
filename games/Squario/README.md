# Squario

[Chinese](README_CH.md)

Squario is a side-scrolling platform game ported to the SCI-CALC keyboard and display. Run, jump, collect coins and power-ups, and continue through progressively longer levels.

## Controls

### Title screen

| Key | Action |
| --- | --- |
| `8` / `2` | Increase or decrease the world seed |
| `TAN` or `COT` | Start the game |
| `ESC` | Restore `/main.bin` and return to the calculator |

### Playing

| Key | Action |
| --- | --- |
| `4` / `6` | Move left or right |
| `8` or `COT` | Jump |
| `TAN` | Run |
| `2` | Move down when the level allows it |
| `ESC` | Restore `/main.bin` and return to the calculator |

Mushrooms increase Squario's size and can refill him while large. Coins award an extra life every 20 collected. Later parts of a level can contain stars and thunderbolts, while later levels become longer and use more underworld sections.

## Install

1. Copy [`../../sci_calc_code/bin/squario.bin`](../../sci_calc_code/bin/squario.bin) to the SD-card root as `/squario.bin`.
2. Copy the current [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) to the SD-card root as `/main.bin`.
3. Open `Programs > Games > Squario`.

The game uses the calculator's standard SD, display, and 5x6 keyboard connections. The Arduino sketch entry point is [`Squario.ino`](Squario.ino).
