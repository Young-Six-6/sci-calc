# Snake

[Chinese](README_CH.md)

Snake is a standalone SCI-CALC game for the 256x64 OLED. Guide the snake to each fruit, grow the body, and avoid hitting the border or yourself.

## Controls

| Key | Action |
| --- | --- |
| `8` | Turn up |
| `2` | Turn down |
| `4` | Turn left |
| `6` | Turn right |
| Any key after game over | Start a new game |
| `ESC` | Restore `/main.bin` and return to the calculator |

## Install

1. Copy [`../../sci_calc_code/bin/snake.bin`](../../sci_calc_code/bin/snake.bin) to the SD-card root as `/snake.bin`.
2. Copy the current [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) to the SD-card root as `/main.bin`.
3. Open `Programs > Games > Snake`.

## Hardware

| Device | Pins |
| --- | --- |
| OLED | CS GPIO5, DC GPIO16, RST GPIO17, MOSI GPIO23, SCK GPIO18 |
| SD card | CS GPIO4, MOSI GPIO23, MISO GPIO19, SCK GPIO18 |
| Keyboard rows | GPIO33, GPIO32, GPIO35, GPIO34, GPIO39 |
| Keyboard columns | GPIO13, GPIO12, GPIO14, GPIO27, GPIO26, GPIO25 |

The Arduino sketch entry point is [`snake-game.ino`](snake-game.ino). Publish the compiled ESP32 application image as `/snake.bin` for the calculator's application loader.

See [`LICENSE`](LICENSE) for licensing information.
