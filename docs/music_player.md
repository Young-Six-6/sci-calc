# Music player
# 音乐播放器

The ESP32-WROOM-32E has enough CPU, RAM, SPI bandwidth, and a hardware I2S peripheral to decode common compressed audio from the microSD card while driving a PCM5102A.

ESP32-WROOM-32E 具备足够的 CPU 性能、RAM、SPI 带宽和硬件 I2S 外设，可以从 microSD 卡解码常见压缩音频并驱动 PCM5102A。

The player is a standalone firmware image (`/music.bin`), so its audio decoder does not consume space in the normal calculator firmware.

播放器使用独立固件镜像（`/music.bin`），因此音频解码器不会占用普通计算器主固件的空间。

Bluetooth headphone output is integrated into this same image rather than duplicated in a second player.

蓝牙耳机输出已整合到同一个镜像中，没有再复制出第二套播放器。

The SD scanner, decoders, playlist, transport controls, repeat state, shuffle state, and volume control are shared by both outputs.

SD 扫描、解码器、播放列表、播放控制、循环状态、随机状态和音量控制均由两种输出共用。

## One-time partition update
## 一次性分区更新

The Bluetooth-enabled player is about 1.63 MB.

启用蓝牙后的播放器约为 1.63MB。

The repository now uses the standard `min_spiffs.csv` dual-OTA layout, which provides two 1.875 MB application slots on the 4 MB flash chip.

项目现在使用标准 `min_spiffs.csv` 双 OTA 布局，在 4MB Flash 芯片上提供两个 1.875MB 应用槽。

The previous default layout provides only 1.25 MB per slot, so replacing only `/music.bin` on an old installation will fail with an insufficient-space update error.

以前的默认布局每个应用槽只有 1.25MB，因此在旧安装上只替换 `/music.bin` 会因为升级空间不足而失败。

Connect the calculator over USB and perform one complete upload from the main-firmware directory.

请通过 USB 连接计算器，并在主固件目录中执行一次完整上传。

```powershell
cd C:\Users\wang\Documents\camera\sci-calcme\sci_calc_code
pio run -t upload
```

PlatformIO writes the bootloader at `0x1000`, the new partition table at `0x8000`, the OTA initializer at `0xE000`, and the main firmware at `0x10000`.

PlatformIO 会把引导程序写入 `0x1000`，新分区表写入 `0x8000`，OTA 初始化数据写入 `0xE000`，主固件写入 `0x10000`。

The repository also includes the merged image `sci_calc_code/bin/sci-calc-full-1.8-min-spiffs.bin` for flash tools that accept one file.

项目还提供合并镜像 `sci_calc_code/bin/sci-calc-full-1.8-min-spiffs.bin`，供支持单文件刷写的工具使用。

Flash that merged image at offset `0x0`.

请把该合并镜像刷写到地址 `0x0`。

Do not flash `music.bin` at `0x0`, because it is an OTA application image intended for SD-card switching.

不要把 `music.bin` 刷写到 `0x0`，因为它是供 SD 卡切换使用的 OTA 应用镜像。

This is required only once unless the partition table is later changed again.

只要以后不再更改分区表，这个完整刷写步骤只需执行一次。

After the upload, use SD-card application switching normally.

完整上传后，即可照常使用 SD 卡切换应用。

## PCM5102A connection
## PCM5102A 接线

| Calculator<br>计算器 | PCM5102A |
| --- | --- |
| GPIO15 | DIN |
| GPIO22 | LCK、LRCK 或 WS |
| GPIO21 | BCK |
| GND | SCK |
| GND | GND |

No MCLK signal is required for the usual PCM5102A breakout.

常见的 PCM5102A 模块不需要 MCLK 信号。

In this three-wire I2S configuration, SCK must be connected to GND so the DAC uses its internal PLL with BCK as the reference.

在这种三线 I2S 配置中，SCK 必须接 GND，使 DAC 使用内部 PLL，并以 BCK 作为参考时钟。

Do not leave SCK floating: this can produce recognizable rhythm with badly scrambled audio.

不要让 SCK 悬空，否则可能出现能听到节拍但声音严重混乱的情况。

Connect the DAC board's power pin according to that particular module's marking; its digital input signals are 3.3 V.

请按照具体模块上的标识连接 DAC 电源；其数字输入信号为 3.3V 电平。

If exposed, set FMT low for standard I2S and keep XSMT high to unmute the DAC.

如果模块引出了这些配置脚，请将 FMT 置低以选择标准 I2S，并将 XSMT 保持高电平以解除静音。

The DAC outputs are line level.

DAC 输出为线路电平。

Feed L/R into an amplifier, powered speaker, or line input rather than connecting a passive loudspeaker directly.

请将 L/R 接入功放、有源音箱或线路输入，不要直接连接无源扬声器。

GPIO21/22 are otherwise unused in the current firmware.

GPIO21 和 GPIO22 在当前固件中没有其他用途。

GPIO15 is a boot-strapping pin, so it is assigned to the PCM5102A high-impedance DIN input rather than a clock line.

GPIO15 是启动配置脚，因此将它连接到 PCM5102A 的高阻 DIN 输入，而不是时钟线。

Do not add an external pull-up or pull-down to GPIO15.

不要给 GPIO15 增加外部上拉或下拉电阻。

This connection leaves GPIO25/26/27 free for the keyboard's last three columns, so the complete keyboard remains usable during playback.

这种接线会保留 GPIO25、GPIO26 和 GPIO27 给键盘后三列使用，因此播放音乐时整个键盘均可操作。

## Bluetooth behavior
## 蓝牙工作方式

The ESP32-WROOM-32E radio supports Classic Bluetooth BR/EDR and the A2DP source profile needed to send SBC audio to headphones.

ESP32-WROOM-32E 无线电支持经典蓝牙 BR/EDR，以及向耳机发送 SBC 音频所需的 A2DP Source 协议。

Bluetooth Low Energy by itself is not an audio transport for this player.

单独的低功耗蓝牙 BLE 不能作为本播放器的音频传输方式。

The device picker shows only discovered devices whose Bluetooth class advertises an audio-rendering service.

设备选择器只显示蓝牙类别中声明了音频播放服务的设备。

Names that the compact OLED font cannot represent are displayed with question marks, but selection still uses the device address during that scan.

紧凑 OLED 字体无法显示的名称字符会显示为问号，但本次扫描中的选择仍使用设备地址。

The chosen name is stored in ESP32 NVS and is matched during later scans.

选中的名称会保存在 ESP32 NVS 中，并在以后扫描时进行匹配。

Only one Bluetooth output device is used at a time.

一次只会使用一个蓝牙输出设备。

A2DP pulls stereo 16-bit PCM at 44.1 kHz.

A2DP 以 44.1kHz 拉取双声道 16 位 PCM。

The player intercepts the decoder's volume-adjusted PCM, converts other sampling rates with linear interpolation, and places the result in a bounded 2048-frame ring buffer.

播放器会截取解码器完成音量调整后的 PCM，使用线性插值转换其他采样率，并将结果放入容量为 2048 帧的有界环形缓冲区。

The bounded buffer keeps playback time synchronized with the Bluetooth consumer instead of decoding the file as fast as possible.

有界缓冲区会让播放时间与蓝牙接收端保持同步，避免文件被不受限制地快速解码。

If the headphones disconnect while Bluetooth is selected, the main decoder waits and the OLED shows `WAIT BT`.

选择蓝牙后如果耳机断开，主解码器会等待，OLED 会显示 `WAIT BT`。

Selecting PCM5102A immediately restores wired output.

选择 PCM5102A 会立即恢复有线输出。

Pressing `POWER` on the output page fully stops A2DP discovery or connection and returns to PCM5102A to save power.

在输出页面按 `POWER` 会完整停止 A2DP 扫描或连接并切回 PCM5102A，以节省电量。

## Preparing the SD card
## 准备 SD 卡

1. Keep the card FAT32 and keep the current `/main.bin` in its root.
   SD 卡保持 FAT32 格式，并在根目录保留当前的 `/main.bin`。
2. Copy the generated player image to `/music.bin`.
   将生成的播放器固件复制为 `/music.bin`。
3. Create `/music` and copy songs into it. Subfolders are supported to depth 3.
   创建 `/music` 目录并把歌曲复制进去，最多支持三级子目录。
4. Use short ASCII filenames for the clearest display.
   使用简短的 ASCII 文件名可以获得最清晰的显示效果。

The scanner accepts up to 96 MP3, WAV, AAC, or M4A files.

播放器最多读取 96 个 MP3、WAV、AAC 或 M4A 文件。

When `/music` does not exist, it scans the SD root instead.

当 `/music` 不存在时，播放器会改为扫描 SD 卡根目录。

FLAC is not listed because the installed WROOM-32E module has no PSRAM and the decoder requires a larger frame buffer.

没有列出 FLAC，是因为当前 WROOM-32E 模块没有 PSRAM，而该解码器需要更大的帧缓冲区。

## Controls
## 按键操作

| Key<br>按键 | Now playing<br>正在播放页面 | Playlist<br>播放列表页面 |
| --- | --- | --- |
| `5`, `ENTER` | Play/pause<br>播放或暂停 | Play selected track<br>播放选中的歌曲 |
| `8`, `2` | Previous/next track<br>上一曲或下一曲 | Move selection up/down<br>向上或向下移动选择项 |
| `4`, `6` | Seek -10/+10 seconds<br>后退或前进 10 秒 | - |
| `SHIFT+4`, `SHIFT+6` | Seek -60/+60 seconds<br>后退或前进 60 秒 | - |
| `7`, `9` or `-`, `+` | Volume down/up<br>降低或提高音量 | Page up/down<br>向上或向下翻页 |
| `1` or `RPN/Norm` | Repeat off/all/one<br>关闭循环、列表循环或单曲循环 | - |
| `3` or `ANG` | Toggle shuffle<br>切换随机播放 | - |
| `*` | Mute/unmute<br>静音或恢复声音 | Play selected track<br>播放选中的歌曲 |
| `.`, `0`, `TAB` | Open playlist<br>打开播放列表 | Return to now playing<br>返回正在播放页面 |
| `SHIFT+TAB` | Open audio outputs<br>打开音频输出页面 | Open audio outputs<br>打开音频输出页面 |
| `/` | - | Return to now playing<br>返回正在播放页面 |
| del<br>退格键 | - | Return to now playing<br>返回正在播放页面 |
| Hold `ESC`<br>长按 `ESC` | Restore `/main.bin` and reboot<br>恢复 `/main.bin` 并重启 | Same<br>相同操作 |

Audio-output page controls:

音频输出页面按键：

| Key<br>按键 | Action<br>操作 |
| --- | --- |
| `8`, `2` | Select PCM5102A or Bluetooth<br>选择 PCM5102A 或蓝牙 |
| `5`, `ENTER` | Apply the selected output<br>启用选中的输出 |
| `0` | Forget the current target and scan for another device<br>忘记当前目标并扫描其他设备 |
| `POWER` | Stop Bluetooth and use PCM5102A<br>关闭蓝牙并使用 PCM5102A |
| `TAB`, `.`, del | Return to now playing<br>返回正在播放页面 |

Bluetooth-device picker controls:

蓝牙设备选择页面按键：

| Key<br>按键 | Action<br>操作 |
| --- | --- |
| `8`, `2` | Move through discovered devices<br>在扫描到的设备中移动选择项 |
| `5`, `ENTER` | Connect to the selected device<br>连接选中的设备 |
| `0` | Clear the list and restart discovery<br>清空列表并重新扫描 |
| `TAB`, `.`, del | Cancel and return to PCM5102A<br>取消并切回 PCM5102A |

The OLED shows the playing state, track number, format, filename, progress, elapsed and total time, volume, repeat mode, and shuffle state.

OLED 会显示播放状态、曲目编号、格式、文件名、进度、已播放时间、总时间、音量、循环模式和随机播放状态。

Long filenames scroll.

较长的文件名会滚动显示。

## Troubleshooting Bluetooth
## 蓝牙故障排查

If no device appears, confirm that the headphones are in pairing mode and are not already connected to a phone.

如果没有出现设备，请确认耳机已经进入配对模式，并且没有连接到手机。

If the OLED remains on `CONNECTING`, press `0`, return the headphones to pairing mode, and select them again.

如果 OLED 一直显示 `CONNECTING`，请按 `0`，让耳机重新进入配对模式，然后再次选择。

If music plays too fast or jumps ahead, the Bluetooth connection was lost; switch to PCM5102A or reconnect before continuing.

如果音乐播放过快或进度跳跃，说明蓝牙连接已经丢失；请切回 PCM5102A，或重新连接后再继续。

If installation reports insufficient space, the old 1.25 MB partition table is still installed and the one-time USB upload has not been completed.

如果安装提示空间不足，说明设备仍使用旧的 1.25MB 分区表，尚未完成一次性 USB 完整上传。

Bluetooth adds normal A2DP latency, so this output is intended for music rather than real-time sound effects.

蓝牙会带来正常的 A2DP 延迟，因此该输出适合听音乐，不适合实时音效。
