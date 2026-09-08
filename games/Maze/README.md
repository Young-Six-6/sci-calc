# SCI-CALC Maze
# SCI-CALC 迷宫

Maze is a standalone game for the 256 by 64 SCI-CALC OLED.

迷宫是面向 SCI-CALC 256×64 OLED 的独立游戏。

The player starts in the upper-left cell and clears the level by reaching the lower-right cell.

玩家从左上角格子出发，到达右下角格子即可过关。

Every clear automatically advances to a newly generated level.

每次过关都会自动进入新生成的关卡。

Difficulty rises automatically by requiring a longer and less straight solution after each clear.

难度会自动提升，每次过关后都会要求正确路线更长、直线段更短。

## Controls
## 操作

Use 8, 2, 4, and 6 to move up, down, left, and right.

使用 8、2、4、6 分别向上、向下、向左、向右移动。

Hold a movement key to move repeatedly through a corridor.

长按方向键可以在通道中连续移动。

Press 5 to replace the current level with another new maze.

按 5 可以用另一张新迷宫替换当前关卡。

Press 0 to show or hide breadcrumb dots.

按 0 可以显示或隐藏足迹点。

Hold ESC for one second to reinstall /main.bin from the SD card and return to the calculator.

长按 ESC 一秒会从 SD 卡安装 /main.bin 并返回计算器。

## Generation and rules
## 生成与规则

Each level is a 32 by 8 perfect maze produced by an iterative randomized depth-first search.

每关都是通过迭代式随机深度优先搜索生成的 32×8 完美迷宫。

All 256 cells belong to one connected spanning tree, so the finish is always reachable and there are no isolated regions.

全部 256 个格子构成一棵连通生成树，因此终点始终可达，也不会出现孤立区域。

The firmware performs a breadth-first connectivity check after generation before accepting a level.

固件会在生成后执行广度优先连通性检查，确认通过后才接受该关卡。

The minimum solution starts at 80 moves, increases by four moves per level, and reaches a cap of 156 moves.

最短正确路线从 80 步开始，每关增加 4 步，最高达到 156 步。

The longest permitted straight section gradually falls from ten cells to five cells.

正确路线允许的最长直线段会从十格逐渐降到五格。

Every accepted solution must use all four movement directions, preventing a right-only or monotonic route.

每条被接受的正确路线都必须使用四个移动方向，从而杜绝只按右键或单调移动的路线。

Fingerprints for the latest 256 levels are retained in RAM and duplicate candidates are regenerated.

最近 256 关的指纹会保存在 RAM 中，候选关卡重复时会重新生成。

A hardware random seed makes repetition across restarts extremely unlikely.

硬件随机种子使重启后的关卡重复概率也极低。

Movement is accepted only when the current cell has no wall in the requested direction.

只有当前格在目标方向没有墙时，移动才会被接受。

## Installation
## 安装

Copy maze.bin and the latest main.bin to the root of the SD card.

将 maze.bin 和最新版 main.bin 复制到 SD 卡根目录。

Install the v1.13 main firmware, then open Programs and select Maze.

安装 v1.13 主固件，然后进入 Programs 并选择 Maze。

For a complete USB reinstall, flash sci_calc_code/bin/sci-calc-full-1.13-min-spiffs.bin at address 0x0.

如果需要通过 USB 完整重装，请将 sci_calc_code/bin/sci-calc-full-1.13-min-spiffs.bin 刷写到地址 0x0。

## Build
## 构建

Run the following command from this directory.

在本目录运行以下命令。

```powershell
pio run
```

The output application image is .pio/build/maze/firmware.bin.

输出的应用镜像位于 .pio/build/maze/firmware.bin。

The game uses fixed-size buffers and does not require PSRAM.

游戏只使用固定大小缓冲区，不需要 PSRAM。
