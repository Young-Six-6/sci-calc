# SCI-CALC Maze

[Chinese](README_CH.md)

Maze is a standalone game for the SCI-CALC 256x64 OLED. Move the ball from the upper-left cell to the lower-right cell to complete each level.

Every completed level starts a newly generated maze automatically. The route becomes longer and uses shorter straight sections as the level increases.

## Controls

| Key | Action |
| --- | --- |
| `8` | Move up |
| `2` | Move down |
| `4` | Move left |
| `6` | Move right |
| Hold a direction | Continue moving through an open passage |
| `5` | Replace the current level with a new maze |
| `0` | Show or hide trail dots |
| Hold `ESC` for one second | Restore `/main.bin` and return to the calculator |

Movement is accepted only when no wall blocks the selected direction.

## Levels

- Every level is a 32x8 maze and always has a route from start to finish.
- The first level requires a solution of at least 80 steps.
- The required route length increases by four steps per level, up to 156 steps.
- The longest allowed straight part gradually decreases from ten cells to five.
- Accepted solution routes use all four directions, preventing a level from being solved by holding only one key.
- The latest 256 level layouts are remembered while the game is running so immediate repeats can be regenerated.

## Install

1. Copy [`../../sci_calc_code/bin/maze.bin`](../../sci_calc_code/bin/maze.bin) to the SD-card root as `/maze.bin`.
2. Copy the current [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) to the SD-card root as `/main.bin`.
3. Open `Programs > Games > Maze`.

For a complete USB installation, flash [`../../sci_calc_code/bin/sci-calc-full-1.18-min-spiffs.bin`](../../sci_calc_code/bin/sci-calc-full-1.18-min-spiffs.bin) at address `0x0`.

## Build and test

```powershell
cd games\Maze
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
python -m unittest discover -s tests -v
Copy-Item .pio\build\maze\firmware.bin ..\..\sci_calc_code\bin\maze.bin -Force
```

The resulting `maze.bin` is installed through the calculator's application loader. Use the merged full image for flashing at address `0x0`.
