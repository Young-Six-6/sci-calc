# MusicPlayer
# 音乐播放器

Standalone SD-card music player for the ESP32 calculator and a PCM5102A I2S DAC.

适用于 ESP32 计算器和 PCM5102A I2S DAC 的独立 SD 卡音乐播放器。

The same player can send decoded music to Classic Bluetooth A2DP headphones without a separate `btmusic.bin`.

同一个播放器可以把解码后的音乐发送到经典蓝牙 A2DP 耳机，不需要单独的 `btmusic.bin`。

PCM5102A remains the default output after every launch.

每次启动后仍默认使用 PCM5102A 输出。

## Wiring
## 接线

- GPIO15 -> PCM5102A DIN
  GPIO15 接 PCM5102A DIN。
- GPIO22 -> PCM5102A LCK/LRCK/WS
  GPIO22 接 PCM5102A LCK、LRCK 或 WS。
- GPIO21 -> PCM5102A BCK
  GPIO21 接 PCM5102A BCK。
- PCM5102A SCK -> GND (required for 3-wire I2S/internal PLL mode)
  PCM5102A SCK 接 GND，这是三线 I2S 内部 PLL 模式的必要接线。
- Calculator GND -> PCM5102A GND
  计算器 GND 接 PCM5102A GND。
- Power the PCM5102A breakout according to the voltage printed on that module.
  请按照模块上印刷的电压要求为 PCM5102A 模块供电。

Do not leave SCK floating.

不要让 SCK 悬空。

A floating SCK can make the DAC mis-detect an external system clock, producing recognizable rhythm but badly scrambled audio.

SCK 悬空会使 DAC 错误识别外部系统时钟，表现为能听到节拍但声音严重混乱。

If exposed, set FMT low for standard I2S and keep XSMT high to unmute the DAC.

如果模块引出了这些配置脚，请将 FMT 置低以使用标准 I2S，并将 XSMT 保持高电平以解除静音。

PCM5102A provides line-level audio.

PCM5102A 输出的是线路电平音频。

Connect its L/R outputs to an amplifier, powered speaker, or suitable line input; do not connect a passive speaker directly.

请将 L/R 输出接入功放、有源音箱或合适的线路输入，不要直接连接无源扬声器。

These pins are separate from the keyboard matrix, OLED, and SD-card SPI pins, so all six keyboard columns remain available during playback.

这些引脚与键盘矩阵、OLED 和 SD 卡 SPI 引脚分开，因此播放音乐时六列键盘均可使用。

GPIO15 is used only as the PCM5102A data output; the DAC input is high impedance and must not pull this ESP32 boot-strapping pin externally during reset.

GPIO15 仅作为 PCM5102A 数据输出使用；DAC 输入为高阻状态，复位时不能从外部拉高或拉低这个 ESP32 启动配置脚。

## Bluetooth headphones
## 蓝牙耳机

Use headphones or speakers that support Classic Bluetooth A2DP audio.

请使用支持经典蓝牙 A2DP 音频的耳机或音箱。

BLE-only devices and Bluetooth Low Energy hearing-aid profiles are not supported by this ESP32 A2DP source.

仅支持 BLE 的设备以及低功耗蓝牙助听器协议不能作为本机的 A2DP 输出设备。

Put the headphones into pairing mode before starting a scan.

开始扫描前，请先让耳机进入配对模式。

The selected device name is saved in NVS and is used for later reconnection attempts.

选中的设备名称会保存在 NVS 中，供以后重新连接时使用。

The Bluetooth link uses SBC at 44.1 kHz, and the player linearly resamples other source rates so 48 kHz files keep the correct speed.

蓝牙链路使用 44.1kHz SBC，播放器会对其他源采样率进行线性重采样，因此 48kHz 文件仍能保持正确速度。

When the selected headphones disconnect, decoding waits instead of skipping ahead through the track.

选中的耳机断开时，解码会等待，不会在歌曲中快速跳过一大段。

Supported AVRCP buttons on the headphones can control play, pause, stop, previous track, and next track.

耳机支持的 AVRCP 按键可以控制播放、暂停、停止、上一曲和下一曲。

## Required flash layout
## 必需的 Flash 分区

The integrated Bluetooth image is about 1.63 MB and requires the included `min_spiffs` dual-OTA layout with two 1.875 MB application slots.

整合蓝牙后的镜像约为 1.63MB，需要使用项目配置的 `min_spiffs` 双 OTA 分区，每个应用槽为 1.875MB。

The older default layout has only 1.25 MB per slot and cannot install this `music.bin`.

旧的默认分区每个应用槽只有 1.25MB，无法安装这个 `music.bin`。

Perform one full USB upload of the main calculator firmware after this update.

本次更新后必须通过 USB 完整上传一次计算器主固件。

```powershell
cd C:\Users\wang\Documents\camera\sci-calcme\sci_calc_code
pio run -t upload
```

This upload writes the bootloader, new partition table, OTA initializer, and main firmware together.

这次上传会同时写入引导程序、新分区表、OTA 初始化数据和主固件。

For a flash utility, the ready-to-write merged image is `sci_calc_code/bin/sci-calc-full-1.8-min-spiffs.bin` and its flash offset is `0x0`.

如果使用刷写工具，可以直接选择已经合并好的 `sci_calc_code/bin/sci-calc-full-1.8-min-spiffs.bin`，刷写地址为 `0x0`。

Do not write `music.bin` at offset `0x0`; it is an OTA application image for SD-card switching.

不要把 `music.bin` 写到地址 `0x0`；它是供 SD 卡切换使用的 OTA 应用镜像。

After that one-time repartition, normal SD-card switching between `main.bin`, `music.bin`, and the other application images works again.

完成这一次分区更新后，便可继续通过 SD 卡在 `main.bin`、`music.bin` 和其他应用镜像之间正常切换。

## SD card
## SD 卡

Create `/music` and place audio files inside it.

在 SD 卡中创建 `/music` 目录，并把音频文件放入其中。

Subfolders up to three levels are scanned.

播放器会扫描最多三级子目录。

If `/music` is absent, the SD root is scanned.

如果没有 `/music` 目录，播放器将扫描 SD 卡根目录。

Up to 96 files are sorted by path.

播放器最多读取 96 个文件，并按路径排序。

Supported extensions are MP3, WAV, AAC, and M4A.

支持 MP3、WAV、AAC 和 M4A 格式。

ASCII filenames display best with the compact OLED font.

使用 ASCII 文件名时，小型 OLED 字体的显示效果最好。

FLAC is intentionally excluded because this WROOM-32E board has no PSRAM for the decoder's large frame buffer.

播放器有意不支持 FLAC，因为这块 WROOM-32E 没有 PSRAM，无法容纳解码器所需的大型帧缓冲区。

Keep `/main.bin` on the SD root.

请在 SD 卡根目录保留 `/main.bin`。

Hold ESC for about one second to restore the main firmware.

长按 ESC 约一秒可恢复主固件。

Put this project's compiled image on the SD root as `/music.bin`.

请将本项目编译生成的固件以 `/music.bin` 文件名放在 SD 卡根目录。

## Controls
## 按键操作

Now-playing page:

正在播放页面：

- `5`: play/pause
  `5`：播放或暂停。
- `ENTER`: play/pause
  `ENTER`：播放或暂停。
- `8` / `2`: previous/next track
  `8` / `2`：上一曲或下一曲。
- `4` / `6`: seek backward/forward 10 seconds
  `4` / `6`：后退或前进 10 秒。
- `SHIFT+4` / `SHIFT+6`: seek backward/forward 60 seconds
  `SHIFT+4` / `SHIFT+6`：后退或前进 60 秒。
- `7` / `9`: volume down/up (hold for repeat)
  `7` / `9`：降低或提高音量，长按可连续调整。
- `-` / `+`: volume down/up (hold for repeat)
  `-` / `+`：降低或提高音量，长按可连续调整。
- `1`: repeat off/all/one (`R-`, `RA`, `R1`)
  `1`：切换关闭循环、列表循环或单曲循环（`R-`、`RA`、`R1`）。
- `RPN/NORM`: repeat off/all/one
  `RPN/NORM`：切换关闭循环、列表循环或单曲循环。
- `3`: shuffle on/off
  `3`：开启或关闭随机播放。
- `ANG`: shuffle on/off
  `ANG`：开启或关闭随机播放。
- `*`: mute/unmute
  `*`：静音或恢复声音。
- `.`, `0`, or `TAB`: playlist page
  `.`、`0` 或 `TAB`：进入播放列表页面。
- `SHIFT+TAB`: audio output page
  `SHIFT+TAB`：进入音频输出页面。

Playlist page:

播放列表页面：

- `8` / `2`: move one item
  `8` / `2`：向上或向下移动一项。
- `7` / `9`: move one page
  `7` / `9`：向上或向下翻一页。
- `5`, `*`, or `ENTER`: play the selected item
  `5`、`*` 或 `ENTER`：播放选中的歌曲。
- `.`, `/`, `TAB`, or Backspace: return to now playing
  `.`、`/`、`TAB` 或退格键：返回正在播放页面。

Audio-output page:

音频输出页面：

- `8` / `2`: select PCM5102A or Bluetooth
  `8` / `2`：选择 PCM5102A 或蓝牙。
- `5` or `ENTER`: apply the selected output
  `5` 或 `ENTER`：启用选中的输出。
- `0`: forget the current target and scan for another Bluetooth device
  `0`：忘记当前目标并扫描其他蓝牙设备。
- `POWER`: turn off the Bluetooth radio and return to PCM5102A
  `POWER`：关闭蓝牙无线电并切回 PCM5102A。
- `TAB`, `.`, or Backspace: return to now playing
  `TAB`、`.` 或退格键：返回正在播放页面。

Bluetooth-device picker:

蓝牙设备选择页面：

- `8` / `2`: select a discovered audio device
  `8` / `2`：选择扫描到的音频设备。
- `5` or `ENTER`: connect to the selected device
  `5` 或 `ENTER`：连接选中的设备。
- `0`: restart discovery
  `0`：重新开始扫描。
- `TAB`, `.`, or Backspace: cancel Bluetooth selection and use PCM5102A
  `TAB`、`.` 或退格键：取消蓝牙选择并使用 PCM5102A。

## Build
## 编译

Run the following commands in this directory.

请在本目录中运行以下命令。

```powershell
pio run
Copy-Item .pio\build\music_player\firmware.bin .\firmware.bin
```

The vendored ESP32-A2DP 1.8.11 source is under `lib/ESP32-A2DP`, so this project does not need to download that library while building.

项目已在 `lib/ESP32-A2DP` 中保存 ESP32-A2DP 1.8.11 源码，因此编译时不需要另外下载这个库。
