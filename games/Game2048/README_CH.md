# SCI-CALC 2048

[English](README.md)

2048 是面向 SCI-CALC 256x64 OLED 的可存档独立游戏。棋盘占满屏幕高度，右侧持续显示得分、最高分、移动次数和当前最大数字。

## 按键

| 按键 | 功能 |
| --- | --- |
| `8` | 向上滑动 |
| `2` | 向下滑动 |
| `4` | 向左滑动 |
| `6` | 向右滑动 |
| 按住 `8`、`2`、`4` 或 `6` | 连续移动；动画期间会缓存下一次方向输入 |
| `Backspace` 或 `Extra` | 撤销最近一次有效移动 |
| `ESC` | 打开或关闭暂停菜单 |
| `Shift` + `Enter` | 确认后开始新游戏 |
| 长按 `ESC` 一秒 | 保存游戏，恢复 `/main.bin` 并返回计算器 |

菜单中使用 `8`、`2` 选择，使用 `Enter` 执行。在 Animation 一项上按 `4` 或 `6` 可以开关移动动画。在新游戏确认页按 `ESC` 或 `Backspace` 取消。

## 规则与状态

- 每局以两个数字块开始。新数字通常为 `2`，偶尔为 `4`。
- 每次操作先将全部数字块推向所选边缘，再合并相邻且相同的数字；合并所得数字会计入得分。
- 本次合并产生的新数字不能在同一次移动中再次合并。例如，`2 2 2 2` 会变成 `4 4`，而不是 `8`。
- 只有棋盘真正发生变化时才会生成新数字。
- 达到 `2048` 后进入胜利页面，可以选择继续挑战更大数字，也可以开始新游戏。
- 棋盘没有空格且无法继续合并时进入结束页面，可以撤销一步或开始新游戏。

## 存档

当前棋盘、得分、移动次数、最高分、胜利状态与动画选项都会自动保存。为避免每次按键都写入闪存，游戏会在停止操作约 1.5 秒后保存，连续达到 20 步时也会强制保存；返回主固件前则会立即保存。

游戏在内存中保留最近一次有效移动，供单步撤销使用。撤销快照不会跨重启保存。存档带有格式标记和校验值；如果数据不完整或损坏，程序会开始一局新游戏，而不会载入异常棋盘。

## 安装

1. 将 [`../../sci_calc_code/bin/2048.bin`](../../sci_calc_code/bin/2048.bin) 复制到 SD 卡根目录，保存为 `/2048.bin`。
2. 将当前 [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) 复制到 SD 卡根目录，保存为 `/main.bin`。
3. 进入 `Programs > Games > 2048`。

如需通过 USB 完整安装，请将 [`../../sci_calc_code/bin/sci-calc-full-1x.xx-min-spiffs.bin`](../../sci_calc_code/bin/sci-calc-full-1x.xx-min-spiffs.bin) 刷写到地址 `0x0`。

## 编译与测试

```powershell
cd games\Game2048
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
python -m unittest discover -s tests -v
Copy-Item .pio\build\game_2048\firmware.bin ..\..\sci_calc_code\bin\2048.bin -Force
```

生成的 `2048.bin` 通过计算器的应用加载器安装。在地址 `0x0` 刷写时请使用合并的全量镜像。
