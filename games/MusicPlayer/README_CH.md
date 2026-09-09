# 音乐播放器

[English](README.md)

音乐播放器通过 PCM5102A DAC 播放计算器 SD 卡中的音频文件，支持常见压缩音频以及带可选合成音色的多轨 MIDI 播放。

## PCM5102A 接线

| 计算器 | PCM5102A |
| --- | --- |
| GPIO21 | BCK / BCLK |
| GPIO22 | LCK / LRCK / WS |
| GPIO15 | DIN |
| GND | GND |
| GND | SCK |

不要让 `SCK` 悬空。将它接地可选择模块的三线 I2S 模式，并避免播放不稳定或声音错乱。

如果模块引出了 `FMT` 和 `XSMT`，请将 `FMT` 保持低电平以使用标准 I2S，并将 `XSMT` 保持高电平以正常输出。

PCM5102A 的 L/R 是线路电平输出，请连接有源音箱、功放或合适的线路输入，不要直接驱动无源扬声器。

## 准备 SD 卡

1. 将 SD 卡格式化为 FAT32，推荐使用 8GB 至 32GB 存储卡。
2. 在 SD 卡根目录创建 `/music`。
3. 将音乐放入 `/music` 或其下最多三级子目录。
4. 在 SD 卡根目录保留 `/music.bin` 和 `/main.bin`。

曲库最多读取 96 个文件，并按完整路径排序。如果没有 `/music` 目录，播放器会改为扫描 SD 卡根目录。

支持的文件扩展名：

- MP3
- WAV
- AAC
- M4A
- MID
- MIDI

更大容量的 SDXC 卡可在格式化为 FAT32 后使用。

## MIDI 播放

为了获得更好的兼容性，建议使用 PPQN 时基的标准 MIDI Format 0 或 Format 1 文件。单个文件最多可包含 24 个音轨和全部 16 个 MIDI 通道。

播放器以 44.1kHz 提供最多 32 路同时发声，支持 Program Change、通道音量、表情、声像、延音、弯音、速度变化和第 10 通道打击乐。

播放 MIDI 时使用 `Shift+1` 循环切换以下音色：

| 音色 | 效果 |
| --- | --- |
| `GM` | 跟随 MIDI Program Change，钢琴类程序使用动态钢琴音色 |
| `PIANO` | 旋律通道统一使用动态钢琴音色 |
| `MELLOW` | 使用起音和衰减更柔和的音色 |
| `SYNTH` | 使用更明亮的电子音色 |

切换曲目时会保留当前音色，重启播放器后恢复为 `GM`。

打开 MIDI 文件时会计算并显示总时长。再次打开同一文件时，会使用 SD 卡上自动维护的 `/.sci_midi_cache` 缓存。

如果极端复杂的片段需要超过 32 个同时按住的音，屏幕可能显示 `MIDI polyphony limit`。播放会继续，并优先保留较强的音乐主体。

## 按键

### 正在播放

| 按键 | 功能 |
| --- | --- |
| `5` 或 `Enter` | 播放或暂停 |
| `8` / `2` | 上一曲或下一曲 |
| `4` / `6` | MP3、WAV、AAC 或 M4A 后退或前进 10 秒 |
| `Shift+4` / `Shift+6` | 普通音频后退或前进 60 秒，或将 MIDI 降低/升高一个半音 |
| `7` / `9` 或 `-` / `+` | 降低或提高音量 |
| `1` 或 `RPN/NORM` | 循环切换不循环、列表循环和单曲循环 |
| `Shift+1` | 切换 MIDI 音色 |
| `3` 或 `ANG` | 开关随机播放 |
| `*` | 开关静音 |
| `.`、`0` 或 `TAB` | 打开播放列表 |
| 长按 `ESC` 约一秒 | 恢复 `/main.bin` 并返回计算器 |

### 播放列表

| 按键 | 功能 |
| --- | --- |
| `8` / `2` | 移动一项 |
| `7` / `9` | 翻动一页 |
| `5`、`*` 或 `Enter` | 播放选中项目 |
| `.`、`/`、`TAB` 或退格键 | 返回正在播放页面 |

## 安装

1. 将 [`../../sci_calc_code/bin/music.bin`](../../sci_calc_code/bin/music.bin) 复制到 SD 卡根目录，保存为 `/music.bin`。
2. 将当前 [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) 复制到 SD 卡根目录，保存为 `/main.bin`。
3. 进入 `Programs > Music > Music Player`。

`music.bin` 通过计算器的应用加载器安装。完整 USB 安装时，请在地址 `0x0` 使用合并固件。

## 故障排查

- 看不到文件：确认 SD 卡为 FAT32，文件使用上述扩展名。
- 没有声音：检查共地、GPIO21/22/15 接线、`SCK -> GND` 以及功放或有源音箱。
- 声音错乱：检查 BCK、LRCK 和 DIN 是否接反，并确认 SCK 已接地。
- PCM5102A 无输出：如果模块引出了相应引脚，将 `XSMT` 置高、`FMT` 置低。
- 曲目打开失败：简化异常文件名，在电脑上检查文件，并尝试新格式化的 FAT32 卡。

串口诊断波特率为 115200。正常播放时 `i2s` 计数应保持为零。

## 编译

使用 PlatformIO 编译并发布应用：

```powershell
cd games\MusicPlayer
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
Copy-Item .pio\build\music_player\firmware.bin ..\..\sci_calc_code\bin\music.bin -Force
```
