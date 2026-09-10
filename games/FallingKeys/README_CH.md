# Falling Keys 下落式音游

[English](README.md)

Falling Keys 是 SCI-CALC 的独立下落式音游。默认玩法接近四列钢琴块，同时提供六列挑战、三档判定难度、长按块、两种音色、连击与准确率统计、结算评级和最高分保存。

固件内置三首无需 SD 谱面的曲目：

- `Classic Rush`：较长的街机风格生成谱，密度会随难度变化
- `Ode to Joy`（欢乐颂）
- `Twinkle Star`（小星星）

## 硬件接线

固件沿用计算器已经验证的接线：

| 设备 | 引脚 |
| --- | --- |
| OLED | CS `GPIO5`、MOSI `GPIO23`、DC `GPIO16`、CLK `GPIO18`、复位 `GPIO17` |
| SD 卡 | CS `GPIO4`、MOSI `GPIO23`、MISO `GPIO19`、CLK `GPIO18` |
| 键盘行 | `GPIO33`、`GPIO32`、`GPIO35`、`GPIO34`、`GPIO39` |
| 键盘列 | `GPIO13`、`GPIO12`、`GPIO14`、`GPIO27`、`GPIO26`、`GPIO25` |
| PCM5102A | BCK `GPIO21`、LCK/LRCK `GPIO22`、DIN `GPIO15`、SCK 接 GND |

OLED 与 SD 卡共用 SPI 总线，各自使用独立 CS。声音由核心 0 上的固定内存 8 复音合成器以 44.1 kHz 输出；游戏判定与屏幕绘制在核心 1 运行。

## 安装

1. 把 [`firmware.bin`](firmware.bin) 复制到 SD 卡根目录并命名为 `/fallingkeys.bin`。
2. 在 SD 卡根目录保留当前主固件 `/main.bin`。
3. 进入 `Programs > Games > Falling Keys`。

如需通过 USB 完整安装主计算器，请将 [`../../sci_calc_code/bin/sci-calc-full-1.18-min-spiffs.bin`](../../sci_calc_code/bin/sci-calc-full-1.18-min-spiffs.bin) 刷写到地址 `0x0`。

在任意页面长按 `ESC` 一秒会刷回 `/main.bin`。游玩时短按 `ESC` 是暂停。

## 操作

### 菜单

| 按键 | 功能 |
| --- | --- |
| `8` / `2` | 上移 / 下移选项 |
| `4` / `6` | 修改当前选项 |
| `Enter` | 开始或确认 |
| `Backspace` | 从暂停或结算页返回游戏菜单 |
| 长按 `ESC` | 返回主计算器 |

### 四列模式

每一整列实体按键都是同一条轨道，因此可以选择自己最舒服的一行来弹。最自然的四键组合是：

`1` `2` `3` `Enter`

下面两行的效果完全相同：

`4` `5` `6` `^`

`7` `8` `9` `+`

完整映射如下：

| 轨道 | 这一实体列内可用的按键 |
| --- | --- |
| 1 | `7`、`4`、`1` |
| 2 | `/`、`8`、`5`、`2`、`0` |
| 3 | `*`、`9`、`6`、`3`、`.` |
| 4 | `-`、`+`、`^`、`Enter`、`Backspace` |

`ESC` 和 `Shift` 保留为控制键，不会触发第一轨。遇到长按块时，持续按住该轨道里的任意一个按键，直到长条尾部到达判定线。

### 六列模式

六列模式会启用最右侧两列科学功能键。一整行中较方便的六键组合是：

`1` `2` `3` `Enter` `Exp` `Extra`

第 5 轨可使用 `Sin`、`Cos`、`Tan`、`Exp` 或 `Angle`；第 6 轨可使用 `Sec`、`Csc`、`Cot`、`Extra` 或 `Tab`。

## 游戏设置

- `TRACK`：选择内置谱或 SD 卡谱面。
- `LANES`：为内置曲目选择四列或六列；SD 谱面的列数由谱面固定。
- `LEVEL`：同时改变判定宽度和下落速度。`Easy` 较宽松，`Normal` 是默认档，`Hard` 更快且判定更紧。
- `TONE`：选择类钢琴音色或街机音色。
- `VOLUME`：把 PCM5102A 输出音量调到 0 至 10。

命中会得到 `PERFECT` 或 `GOOD`。漏掉块头、提前松开长按块、在附近没有块时误按，都会中断连击。结算页显示评级、准确率、最大连击、各类判定次数，以及当前曲目、列数和难度组合的历史最高分。

## SD 谱面格式

自定义谱面放在 SD 卡 `/fallingkeys` 目录。最多列出 16 个 `.ptc` 文件，加上三首内置曲目。菜单显示谱面中的 `title`，不会拿文件名充当曲名。

`PTC1` 是专门为 ESP32 稳定判定设计的轻量文本格式：

```text
PTC1
title=Example Song
artist=Example Artist
lanes=4
bpm=120
offset=0
# N,time_ms,lane,note,duration_ms
N,1000,0,60,0
N,1500,2,64,0
N,2000,1,67,700
```

格式规则：

- `lanes` 可设为 4、5 或 6。
- `time_ms` 是从谱面开始到该块判定点的毫秒数。
- `lane` 从 0 开始，范围是 `0` 到 `lanes - 1`。
- `note` 是 0 至 127 的 MIDI 音高，命中时用它发声。
- `duration_ms` 为 `0` 时是普通块，达到 300 ms 时显示成长按块。
- `offset` 会整体平移所有音符，可用范围为 -10000 至 10000 ms。
- 单个谱面最多 1024 个音符事件。事件行不必预先排序，加载器会按时间排序。

可编辑示例见 [`examples/example.ptc`](examples/example.ptc)。

## 从 MIDI 转换

附带的电脑端转换器会预先处理变速、运行状态、和弦、音符时值、轨道分配和密度。计算器只需直接载入整理好的 PTC1 事件，因此高密度段落也有稳定的判定时序。

转换器只使用 Python 标准库：

```powershell
cd games\FallingKeys
python tools\midi_to_tiles.py song.mid song.ptc --lanes 4 --difficulty normal
```

如果 SD 卡盘符是 `F:`，可以直接输出到卡上：

```powershell
python tools\midi_to_tiles.py song.mid F:\fallingkeys\song.ptc --lanes 6 --difficulty hard --title "曲名" --artist "作者"
```

三档转换会优先考虑游戏手感：

- `easy`：每个时点保留一个突出音符，并拉开过密段落。
- `normal`：最多保留二音和弦。
- `hard`：最多保留四音和弦，并允许更密的连续块。
- 长音自动变成长按块，短音保持普通块。
- 默认跳过 MIDI 第 10 通道的打击乐；需要把鼓点也变成方块时可加入 `--include-drums`。
- 超过 1024 个事件时会在整首歌范围内均匀精简，而不是直接砍掉后半首。

支持采用 PPQ 时间基准的标准 MIDI 0 型和 1 型文件。

## 编译

```powershell
cd games\FallingKeys
platformio run
```

应用镜像生成在 `.pio/build/falling_keys/firmware.bin`。
