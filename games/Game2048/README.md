# SCI-CALC 2048

[Chinese](README_CH.md)

2048 is a standalone, saveable version of the classic sliding-number puzzle for the SCI-CALC 256x64 OLED. The board occupies the full height of the display while score, best score, move count, and largest tile remain visible beside it.

## Controls

| Key | Action |
| --- | --- |
| `8` | Slide up |
| `2` | Slide down |
| `4` | Slide left |
| `6` | Slide right |
| Hold `8`, `2`, `4`, or `6` | Continue making moves; one next move is buffered during animation |
| `Backspace` or `Extra` | Undo the latest successful move |
| `ESC` | Open or close the pause menu |
| `Shift` + `Enter` | Ask to start a new game |
| Hold `ESC` for one second | Save the game, restore `/main.bin`, and return to the calculator |

In menus, use `8` and `2` to select an item and `Enter` to activate it. Use `4` or `6` on the Animation row to switch movement animation on or off. `ESC` or `Backspace` cancels the new-game confirmation.

## Rules and game states

- Every game starts with two tiles. A newly added tile is `2` most of the time and `4` occasionally.
- A move first slides every tile toward the chosen edge. Two adjacent equal tiles then combine and add their value to the score.
- A tile created by a merge cannot merge again during the same move. For example, `2 2 2 2` becomes `4 4`, not `8`.
- A new tile appears only after a move that actually changes the board.
- Reaching `2048` opens a win screen. Choose Continue to keep building larger tiles or start a new game.
- When no empty cell and no legal merge remain, the game-over screen offers Undo or New Game.

## Saving

The current board, score, move count, best score, win state, and animation preference are stored automatically. Writes are delayed until approximately 1.5 seconds after play stops, or forced after 20 moves, to avoid writing flash for every key press. Returning to the main firmware saves immediately.

One in-memory undo is available for the latest successful move. The undo snapshot is intentionally not retained across a restart. Saved data includes a format marker and checksum; invalid data starts a clean game instead of loading a damaged board.

## Install

1. Copy [`../../sci_calc_code/bin/2048.bin`](../../sci_calc_code/bin/2048.bin) to the SD-card root as `/2048.bin`.
2. Copy the current [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) to the SD-card root as `/main.bin`.
3. Open `Programs > Games > 2048`.

For a complete USB installation, flash [`../../sci_calc_code/bin/sci-calc-full-1x.xx-min-spiffs.bin`](../../sci_calc_code/bin/sci-calc-full-1x.xx-min-spiffs.bin) at address `0x0`.

## Build and test

```powershell
cd games\Game2048
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
python -m unittest discover -s tests -v
Copy-Item .pio\build\game_2048\firmware.bin ..\..\sci_calc_code\bin\2048.bin -Force
```

The resulting `2048.bin` is installed through the calculator's application loader. Use the merged full image for flashing at address `0x0`.
