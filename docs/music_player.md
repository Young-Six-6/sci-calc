# Music player integration
# 音乐播放器集成说明

The calculator uses `games/MusicPlayer` as an independent application image named `music.bin`.

计算器将 `games/MusicPlayer` 作为名为 `music.bin` 的独立应用镜像使用。

The application is intentionally wired-only, and all Bluetooth UI and runtime paths have been removed from the linked firmware.

该应用有意仅支持有线输出，所有蓝牙界面和运行路径都已从最终链接固件中移除。

The PlatformIO dependency graph contains U8g2, ESP32-audioI2S, FS, SD, SPI, and Update, with no ESP32-A2DP dependency.

PlatformIO 依赖图包含 U8g2、ESP32-audioI2S、FS、SD、SPI 和 Update，不包含 ESP32-A2DP 依赖。

The framework may retain the small `esp_bt_controller_mem_release` startup symbol because it releases unused Bluetooth memory rather than starting Bluetooth.

框架可能保留很小的 `esp_bt_controller_mem_release` 启动符号，因为它用于释放未使用的蓝牙内存，而不是启动蓝牙。

## Runtime design
## 运行设计

PCM5102A receives standard stereo I2S on BCK GPIO21, LRCK GPIO22, and DIN GPIO15.

PCM5102A 通过 BCK GPIO21、LRCK GPIO22 和 DIN GPIO15 接收标准立体声 I2S。

OLED and SD share the calculator SPI bus, while audio uses the independent I2S peripheral and DMA.

OLED 与 SD 共用计算器 SPI 总线，音频则使用独立的 I2S 外设和 DMA。

Compressed audio is decoded by the vendored ESP32-audioI2S library.

压缩音频由项目内置的 ESP32-audioI2S 库解码。

MIDI is streamed by `MidiPlayer` using one open file and bounded fixed-size track caches.

MIDI 由 `MidiPlayer` 使用一个打开的文件和有界固定大小音轨缓存进行流式播放。

The MIDI engine supports 24 tracks, 16 channels, 32 full-quality voices, controller state, percussion, pitch bend, and compact synthesized instrument families.

MIDI 引擎支持 24 音轨、16 通道、32 路完整音质复音、控制器状态、打击乐、弯音和紧凑合成音色族。

`SHIFT+1` cycles the `GM`, `PIANO`, `MELLOW`, and `SYNTH` tone profiles during MIDI playback.

播放 MIDI 时，`SHIFT+1` 可依次切换 `GM`、`PIANO`、`MELLOW` 和 `SYNTH` 音色方案。

The GM piano family uses a velocity-dependent hammer transient, decaying upper harmonics, and key-dependent envelopes instead of the previous fixed harmonic mixture.

GM 钢琴组现在使用随力度变化的击弦瞬态、会衰减的高次谐波和随音高变化的包络，不再使用固定的谐波混合。

The forced profiles affect melodic channels only, so MIDI channel 10 keeps its percussion mapping.

强制音色方案仅影响旋律通道，因此 MIDI 第 10 通道仍保持打击乐映射。

The patch definitions in `MidiPlayer::patchFor` are the source-level customization point for waveform, attack, decay, release, and sustain.

`MidiPlayer::patchFor` 中的音色定义是源码级自定义入口，可调整波形、起音、衰减、释音和延音电平。

The player does not load a complete MIDI file or a SoundFont into RAM.

播放器不会把整个 MIDI 文件或 SoundFont 载入 RAM。

## Stability policy
## 稳定性策略

The 20-by-512-frame I2S DMA queue is deliberately larger than the render batch and provides about 232 ms of latency reserve.

20×512 帧的 I2S DMA 队列有意大于合成批次，可提供约 232 毫秒延迟余量。

MIDI renders 256 frames per call, and all channel gain and pan coefficients are updated only on MIDI controller events.

MIDI 每次合成 256 帧，所有通道增益和声像系数仅在 MIDI 控制器事件发生时更新。

Each of the 24 MIDI tracks receives a 256-byte read cache to limit SD seek spikes.

24 个 MIDI 音轨各自具有 256 字节读取缓存，以限制 SD 寻址峰值。

All 32 voices retain the complete instrument waveform.

全部 32 个复音均保留完整乐器波形。

An experimental 32-plus-8 mixed mode was removed after the no-PSRAM test unit reached 107 percent synthesis load and produced unstable audio in a dense passage.

实验性的 32+8 混合模式已移除，因为无 PSRAM 实机在密集段达到 107% 合成负载并产生不稳定声音。

Envelope and gain products are cached at the lower envelope update rate, while oscillator rendering remains sample-accurate.

包络与增益乘积以较低的包络更新频率缓存，振荡器合成仍保持逐采样精度。

The allocator distinguishes release-tail reuse, sustain-held reuse, and genuinely held-note reuse in serial diagnostics.

分配器会在串口诊断中区分释放尾音复用、延音踏板音符复用和真正按住音符的复用。

At the physical ceiling, held notes are ranked by target MIDI velocity instead of their instantaneous attack envelope.

达到物理上限时，仍按住的音符按目标 MIDI 力度排序，而不是按起音瞬间的包络电平排序。

A new melodic note must exceed the quietest held note by 12 velocity levels before replacement is allowed.

新的旋律音必须比最弱按键音高 12 级力度，才允许执行替换。

Lower-priority excess notes and percussion that would displace held melody are counted as `drop` and ignored.

优先级较低的超额音符，以及会替换按键旋律的打击乐，会计入 `drop` 并被忽略。

Display refresh is reduced to 320 ms while all 32 voices are active, preventing non-audio SPI work from consuming overload margin.

全部 32 复音活动时，屏幕刷新间隔降低至 320 毫秒，避免非音频 SPI 工作占用超限余量。

The OLED reports `MIDI polyphony limit` once when an extreme track exceeds the physical 32-voice ceiling.

当极端曲目超过物理 32 复音上限时，OLED 会提示一次 `MIDI polyphony limit`。

MIDI duration preview is calculated from the complete tempo map when a track is first opened and then stored in `/.sci_midi_cache` on the SD card.

首次打开曲目时会根据完整速度图计算 MIDI 总时长，随后将结果存入 SD 卡的 `/.sci_midi_cache`。

Cache records are validated using the path, file size, and a head-and-tail signature; the cache is capped at 16 KB and rebuilt automatically when oversized.

缓存记录会通过路径、文件大小和首尾指纹校验；缓存上限为 16KB，超限后会自动重建。

MIDI seeking remains disabled because accurate jumping requires reconstructing controller, program, sustain, and active-note state from the beginning.

MIDI 快进退仍然关闭，因为准确跳转需要从开头重建控制器、音色、延音和活动音符状态。

The stereo-linked limiter, quietest-voice reuse, and short steal tail preserve dense chords while reducing clipping and clicks.

双声道联动限制器、最安静复音复用和短暂抢音尾音会保留密集和弦，同时减少削波与爆声。

Decoder buffers are released on a compressed-audio-to-MIDI transition, while the small reusable input ring remains allocated to avoid heap churn.

从压缩音频切换到 MIDI 时会释放解码器缓冲区，同时保留较小的可复用输入环形缓冲区，以避免堆内存反复变化。

## SD and OTA contract
## SD 与 OTA 约定

The SD card should use FAT32 and contain `/music.bin`, `/main.bin`, and a `/music` directory for media.

SD 卡应使用 FAT32，并包含 `/music.bin`、`/main.bin` 和用于存放媒体的 `/music` 目录。

The player scans at most three directory levels and indexes at most 96 supported files.

播放器最多扫描三级目录，并索引最多 96 个受支持文件。

Holding ESC for about one second installs `/main.bin` through the Arduino Update API and restarts the calculator.

长按 ESC 约一秒会通过 Arduino Update API 安装 `/main.bin` 并重启计算器。

`music.bin` is an application image for the dual-OTA layout and is not a complete flash image.

`music.bin` 是用于双 OTA 分区的应用镜像，不是完整 Flash 镜像。

Do not write it at offset `0x0`.

不要把它写入地址 `0x0`。

## Release procedure
## 发布流程

Build the standalone player and copy the same binary to both release locations.

编译独立播放器，并把同一个二进制文件复制到两个发布位置。

```powershell
cd C:\Users\wang\Documents\camera\sci-calcme\games\MusicPlayer
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
Copy-Item .pio\build\music_player\firmware.bin .\firmware.bin -Force
Copy-Item .pio\build\music_player\firmware.bin ..\..\sci_calc_code\bin\music.bin -Force
```

Verify that the two published files have the same SHA-256 hash as the PlatformIO output.

请确认两个发布文件的 SHA-256 与 PlatformIO 输出完全一致。

For direct service flashing on the verified calculator only, the active `app1` slot starts at `0x1F0000`.

仅在已验证的这台计算器上进行直接维护刷写时，活动 `app1` 槽起始地址为 `0x1F0000`。

Read the partition table and OTA data again before using a raw offset on another unit.

在其他设备上使用裸地址之前，请重新读取分区表和 OTA 数据。

## Acceptance checks
## 验收检查

Confirm that an MP3 reports channels, sample rate, bit depth, and bitrate without `Could not open this track`.

确认 MP3 能报告通道数、采样率、位深和码率，并且不会出现 `Could not open this track`。

Switch repeatedly between MP3 and MIDI and confirm that the boot banner does not reappear.

在 MP3 与 MIDI 之间反复切换，并确认启动信息不会再次出现。

During a demanding MIDI, confirm that `i2s=0` and that `load` remains comfortably below 100 percent.

播放高负载 MIDI 时，请确认 `i2s=0`，并且 `load` 明显低于 100%。

Listen for clipped attacks, stuck notes, crackles, or gaps during dense chords and track changes.

请在密集和弦与切歌期间留意削波起音、持续不消失的音符、爆裂声或声音间断。

The August 13, 2026 COM7 test passed these software checks with 320 kbps MP3 files and a representative MIDI reaching all 32 voices with `held=0` and `i2s=0`.

2026 年 8 月 13 日的 COM7 实测使用 320kbps MP3 和达到全部 32 复音的代表性 MIDI，并以 `held=0`、`i2s=0` 通过上述软件检查。

The final August 14, 2026 dynamic-piano test reached all 32 voices and 66.6 percent peak reporting-window load with `drop=0`, `held=0`, and `i2s=0`.

2026 年 8 月 14 日最终动态钢琴版本实测达到全部 32 复音，报告周期最高负载为 66.6%，并保持 `drop=0`、`held=0` 和 `i2s=0`。

The MIDI duration cache was also confirmed on COM7 with `duration=362s cache=hit`, while free heap remained stable at about 231 KB.

COM7 实机还确认 MIDI 时长缓存显示 `duration=362s cache=hit`，空闲堆内存稳定在约 231KB。
