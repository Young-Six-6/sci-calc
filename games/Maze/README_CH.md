# SCI-CALC 迷宫

[English](README.md)

迷宫是面向 SCI-CALC 256x64 OLED 的独立游戏。将小球从左上角格子移动到右下角格子即可过关。

每次过关都会自动开始一张新生成的迷宫。随着关卡提升，正确路线会变长，连续直线通道会变短。

## 按键

| 按键 | 功能 |
| --- | --- |
| `8` | 向上移动 |
| `2` | 向下移动 |
| `4` | 向左移动 |
| `6` | 向右移动 |
| 按住方向键 | 沿畅通通道连续移动 |
| `5` | 用新迷宫替换当前关卡 |
| `0` | 显示或隐藏足迹点 |
| 长按 `ESC` 一秒 | 恢复 `/main.bin` 并返回计算器 |

只有当目标方向没有墙时，移动才会生效。

## 关卡

- 每关都是 32x8 迷宫，起点与终点之间始终存在路线。
- 第一关的解答路线至少需要 80 步。
- 每关要求的路线长度增加四步，最高达到 156 步。
- 允许的最长直线段会从十格逐渐降到五格。
- 通过的正确路线会使用全部四个方向，不会出现只按一个键就能过关的情况。
- 游戏运行时会记住最近 256 关的布局，立即重复时会重新生成。

## 安装

1. 将 [`../../sci_calc_code/bin/maze.bin`](../../sci_calc_code/bin/maze.bin) 复制到 SD 卡根目录，保存为 `/maze.bin`。
2. 将当前 [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) 复制到 SD 卡根目录，保存为 `/main.bin`。
3. 进入 `Programs > Games > Maze`。

如需通过 USB 完整安装，请将 [`../../sci_calc_code/bin/sci-calc-full-1.15-min-spiffs.bin`](../../sci_calc_code/bin/sci-calc-full-1.15-min-spiffs.bin) 刷写到地址 `0x0`。

## 编译与测试

```powershell
cd games\Maze
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
python -m unittest discover -s tests -v
Copy-Item .pio\build\maze\firmware.bin ..\..\sci_calc_code\bin\maze.bin -Force
```

生成的 `maze.bin` 通过计算器的应用加载器安装。在地址 `0x0` 刷写时请使用合并的全量镜像。
