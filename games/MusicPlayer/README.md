# MusicPlayer
# 音乐播放器

MusicPlayer is a standalone SD-card audio firmware for the ESP32 calculator and PCM5102A DAC.

MusicPlayer 是适用于 ESP32 计算器和 PCM5102A DAC 的独立 SD 卡音频固件。

The current image uses wired I2S only and contains no Bluetooth player, pairing page, A2DP task, or Bluetooth audio buffer.

当前镜像仅使用有线 I2S，不包含蓝牙播放器、配对页面、A2DP 任务或蓝牙音频缓冲区。

Keeping the player separate from the main calculator firmware leaves the normal calculator image unchanged.

播放器采用独立固件，因此不会增加普通计算器主固件的体积。

## Hardware
## 硬件

- `GPIO21 -> PCM5102A BCK`
  `GPIO21 -> PCM5102A BCK`
- `GPIO22 -> PCM5102A LCK/LRCK/WS`
  `GPIO22 -> PCM5102A LCK/LRCK/WS`
- `GPIO15 -> PCM5102A DIN`
  `GPIO15 -> PCM5102A DIN`
- `PCM5102A SCK -> GND`
  `PCM5102A SCK -> GND`
- `Calculator GND -> PCM5102A GND`
  `计算器 GND -> PCM5102A GND`

Do not leave PCM5102A SCK floating.

不要让 PCM5102A 的 SCK 悬空。

Grounding SCK selects the module's three-wire I2S internal-PLL mode and prevents scrambled audio.

将 SCK 接地可选择模块的三线 I2S 内部 PLL 模式，并避免声音错乱。

If the module exposes FMT and XSMT, keep FMT low for standard I2S and XSMT high for normal output.

如果模块引出了 FMT 和 XSMT，请将 FMT 保持低电平以使用标准 I2S，并将 XSMT 保持高电平以正常输出。

PCM5102A produces line-level output and must feed an amplifier, powered speaker, or suitable line input.

PCM5102A 输出线路电平信号，应连接功放、有源音箱或合适的线路输入。

Do not connect a passive speaker directly to PCM5102A L/R.

不要把无源扬声器直接连接到 PCM5102A 的 L/R 输出。

## Supported files
## 支持的文件

The player scans `/music` and up to three levels of subfolders, or the SD root when `/music` is absent.

播放器扫描 `/music` 及最多三级子目录；如果没有 `/music`，则扫描 SD 卡根目录。

Up to 96 files are sorted by their complete paths.

最多读取 96 个文件，并按完整路径排序。

Supported extensions are MP3, WAV, AAC, M4A, MID, and MIDI.

支持 MP3、WAV、AAC、M4A、MID 和 MIDI 扩展名。

FLAC is excluded because its decoder buffer is unsuitable for this WROOM-32 without PSRAM.

本机 WROOM-32 没有 PSRAM，因此不包含需要较大解码缓冲区的 FLAC。

Use FAT32, with 8 GB to 32 GB cards recommended.

请使用 FAT32，推荐使用 8GB 至 32GB 的存储卡。

Larger SDXC cards usually need to be reformatted from exFAT to FAT32 before use.

容量更大的 SDXC 卡通常需要从 exFAT 重新格式化为 FAT32 后才能使用。

## MIDI engine
## MIDI 引擎

Standard MIDI File format 0 and format 1 with PPQN timing are supported.

支持使用 PPQN 时基的标准 MIDI 文件 Format 0 和 Format 1。

SMPTE timing is not supported.

不支持 SMPTE 时基。

One file may contain up to 24 tracks and all 16 MIDI channels.

单个文件最多可包含 24 个音轨和全部 16 个 MIDI 通道。

The synthesizer provides 32 simultaneous full-quality voices at 44.1 kHz through PCM5102A.

合成器通过 PCM5102A 以 44.1kHz 提供 32 路完整音质复音。

The earlier experimental 40-voice mixed mode was removed after real-device testing showed overload and confused audio in dense passages.

早期实验性的 40 路混合复音模式已移除，因为实机测试显示密集段会过载并产生混乱声音。

It handles Program Change, channel volume, expression, pan, sustain, pitch bend, tempo changes, and channel-10 percussion.

它支持 Program Change、通道音量、表情、声像、延音、弯音、速度变化和第 10 通道打击乐。

General MIDI programs are mapped to compact synthesized instrument families instead of SoundFont samples.

General MIDI 音色编号映射为紧凑的合成音色族，不使用 SoundFont 采样。

Four selectable tone profiles provide practical customization without loading sample libraries.

播放器提供四种可选音色方案，无需加载采样音色库即可进行实用自定义。

`GM` follows Program Change events and uses a velocity-sensitive dynamic piano waveform for GM programs 0 through 7.

`GM` 遵循 Program Change 事件，并为 GM 0 至 7 号音色使用力度相关的动态钢琴波形。

`PIANO` forces every melodic channel to the dynamic piano patch while leaving channel-10 percussion unchanged.

`PIANO` 强制所有旋律通道使用动态钢琴音色，同时保持第 10 通道打击乐不变。

The piano model combines a short velocity-dependent hammer transient, rapidly decaying upper harmonics, and key-dependent decay and release times.

钢琴模型结合了随力度变化的短促击弦瞬态、快速衰减的高次谐波，以及随音高变化的衰减与释音时间。

`MELLOW` forces a softer sine-based patch with a gentle attack and decay.

`MELLOW` 强制使用较柔和的正弦波音色，并采用温和的起音与衰减。

`SYNTH` forces the brighter mixed waveform used by the earlier electronic-sounding implementation.

`SYNTH` 强制使用旧版偏电子感的明亮混合波形。

The selected profile remains active while changing tracks and returns to `GM` after a firmware restart.

切换曲目时会保留所选方案，固件重启后恢复为 `GM`。

Each MIDI track has a fixed 256-byte read cache to reduce SD seeks without dynamic allocation during playback.

每个 MIDI 音轨具有固定的 256 字节读取缓存，可减少 SD 寻址，并避免播放期间动态分配。

Channel volume, expression, and stereo pan gains are precomputed when controller events arrive.

通道音量、表情和立体声声像增益会在控制器事件到达时预先计算。

Envelope, velocity, and channel gains are cached per voice and refreshed only on envelope ticks or controller changes.

包络、力度和通道增益按复音缓存，仅在包络节拍或控制器变化时刷新。

Overlapping Note On events for the same channel and pitch share one oscillator while retaining a balanced Note Off count.

同通道同音高的重叠 Note On 会共用一个振荡器，同时保留对应的 Note Off 计数关系。

Audio is synthesized in 256-frame batches into a 20-by-512-frame I2S DMA queue.

音频按每批 256 帧合成，并写入 20×512 帧的 I2S DMA 队列。

The queue stores about 232 ms of stereo audio and absorbs short SD-card or display latency spikes.

该队列可保存约 232 毫秒立体声音频，用于吸收 SD 卡或屏幕刷新造成的短暂延迟峰值。

A stereo-linked limiter controls complex chord peaks without independently shifting the left and right image.

双声道联动限制器会控制复杂和弦的峰值，同时不会使左右声像单独漂移。

When all voices are occupied, the quietest release-preferred voice is reused with a short decay tail to avoid clicks.

复音全部占用时，优先复用最安静且处于释放阶段的声音，并通过短暂衰减尾音避免爆声。

If all 32 slots contain held melodic notes, a new note replaces one only when its MIDI velocity is at least 12 levels stronger than the quietest held note.

如果全部 32 个复音槽都包含仍按住的旋律音，新音符只有在 MIDI 力度至少比最弱按键音高 12 级时才会替换它。

Weaker excess notes are ignored, and percussion never displaces a held melodic note.

较弱的超额音符会被忽略，打击乐也绝不会替换仍按住的旋律音。

This overload policy preserves a continuous musical foreground instead of repeatedly cutting voices and producing stutter-like artifacts.

这种超限策略会保持连续的音乐主体，避免反复切断声部而产生类似卡顿的听感。

OLED refresh changes from 160 ms to 320 ms only while all voice slots are occupied.

仅在全部复音槽占满时，OLED 刷新间隔会从 160 毫秒调整为 320 毫秒。

Normal release-tail reuse does not remove a held note; if an extreme file requires more than 32 genuinely held voices, the OLED reports `MIDI polyphony limit` once for that track.

正常复用释放尾音不会移除仍按住的音符；如果极端文件需要超过 32 个真正按住的复音，OLED 会为该曲目提示一次 `MIDI polyphony limit`。

When a MIDI file is opened, the player shows an accurate total-duration preview after parsing the tempo map.

打开 MIDI 文件时，播放器会解析速度图并显示准确的总时长预览。

The result is stored in `/.sci_midi_cache` on the SD card and validated by path, size, and a file signature, so repeat openings do not rescan the MIDI event stream.

结果会存入 SD 卡上的 `/.sci_midi_cache`，并通过路径、大小和文件指纹校验，因此再次打开同一 MIDI 时无需重新扫描事件流。

The cache is capped at 16 KB and is rebuilt automatically when it grows beyond that limit.

缓存上限为 16KB，超过限制后会自动重建。

High saw and square notes automatically use gentler waveforms to reduce aliasing.

锯齿波和方波的高音会自动换用更平滑的波形，以减少混叠杂音。

When switching from compressed audio to MIDI, decoder work buffers are released while the reusable input ring remains allocated.

从压缩音频切换到 MIDI 时会释放解码器工作区，同时保留可复用的输入环形缓冲区。

This keeps MIDI memory high while avoiding repeated ring-buffer allocation and heap fragmentation.

这样既可为 MIDI 保留较多内存，又能避免反复申请环形缓冲区造成堆碎片。

## Controls
## 按键

### Now playing
### 正在播放

- `5` or `ENTER`: play or pause.
  `5` 或 `ENTER`：播放或暂停。
- `8` / `2`: previous or next track.
  `8` / `2`：上一曲或下一曲。
- `4` / `6`: seek backward or forward 10 seconds for decoded audio.
  `4` / `6`：压缩音频后退或前进 10 秒。
- `SHIFT+4` / `SHIFT+6`: seek 60 seconds for decoded audio, or transpose MIDI by one semitone.
  `SHIFT+4` / `SHIFT+6`：压缩音频后退或前进 60 秒，播放 MIDI 时降低或升高一个半音。
- `7` / `9` or `-` / `+`: decrease or increase volume.
  `7` / `9` 或 `-` / `+`：降低或提高音量。
- `1` or `RPN/NORM`: cycle repeat off, all, and one.
  `1` 或 `RPN/NORM`：切换关闭循环、列表循环和单曲循环。
- `SHIFT+1`: cycle `GM`, `PIANO`, `MELLOW`, and `SYNTH` while playing MIDI.
  `SHIFT+1`：播放 MIDI 时依次切换 `GM`、`PIANO`、`MELLOW` 和 `SYNTH`。
- `3` or `ANG`: toggle shuffle.
  `3` 或 `ANG`：切换随机播放。
- `*`: toggle mute.
  `*`：切换静音。
- `.`, `0`, or `TAB`: open the playlist.
  `.`、`0` 或 `TAB`：打开播放列表。
- `SHIFT+TAB`: confirm that PCM5102A is the only output.
  `SHIFT+TAB`：提示 PCM5102A 是唯一音频输出。
- Hold `ESC` for about one second: install `/main.bin` and return to the calculator.
  长按 `ESC` 约一秒：安装 `/main.bin` 并返回计算器主程序。

### Playlist
### 播放列表

- `8` / `2`: move one item.
  `8` / `2`：移动一项。
- `7` / `9`: move one page.
  `7` / `9`：翻动一页。
- `5`, `*`, or `ENTER`: play the selected item.
  `5`、`*` 或 `ENTER`：播放选中项目。
- `.`, `/`, `TAB`, or Backspace: return to now playing.
  `.`、`/`、`TAB` 或退格键：返回正在播放页面。

## Diagnostics
## 诊断

The serial port runs at 115200 baud and reports reset reason, heap state, decoder information, and MIDI performance every five seconds.

串口使用 115200 波特率，并报告复位原因、堆状态、解码器信息和每五秒一次的 MIDI 性能数据。

`load` is the average MIDI synthesis time as a percentage of available real-time audio duration.

`load` 表示 MIDI 合成时间占可用实时音频时长的平均百分比。

`peak` is the slowest 256-frame render batch in microseconds.

`peak` 表示最慢一批 256 帧合成所用的微秒数。

`limiter` and `reuse` count limiter activity and voice-slot reuse during the reporting interval.

`limiter` 和 `reuse` 表示报告周期内限制器动作次数和复音槽复用次数。

`sustain` and `held` distinguish pedal-held notes from keys that were still logically pressed when reuse became unavoidable.

`sustain` 和 `held` 用于区分延音踏板保留的音符，以及复用不可避免时逻辑上仍处于按下状态的音符。

`drop` counts weaker excess Note On events that were deliberately ignored to keep playback continuous.

`drop` 表示为了保持连续播放而有意忽略的较弱超额 Note On 事件数量。

For ordinary material, `held=0` confirms that the polyphony allocator did not remove a pressed note.

对于普通曲目，`held=0` 表示复音分配器没有移除仍按住的音符。

`i2s` must remain zero; a nonzero value means a timed-out or incomplete DMA write.

`i2s` 应始终为零；非零表示 DMA 写入超时或不完整。

## Build and install
## 编译与安装

Build from this directory with PlatformIO.

请在本目录中使用 PlatformIO 编译。

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
Copy-Item .pio\build\music_player\firmware.bin .\firmware.bin
Copy-Item .pio\build\music_player\firmware.bin ..\..\sci_calc_code\bin\music.bin
```

Place the published image on the SD-card root as `/music.bin` and keep `/main.bin` there as well.

将发布镜像以 `/music.bin` 放在 SD 卡根目录，并同时保留 `/main.bin`。

The image is an OTA application and must not be flashed at address `0x0` by itself.

该镜像是 OTA 应用程序，不能单独刷写到地址 `0x0`。

The current 4 MB dual-OTA `min_spiffs` layout provides enough room for this approximately 817 KB image.

当前 4MB Flash 的双 OTA `min_spiffs` 分区足以容纳约 817KB 的镜像。

The checked hardware uses the active application slot at `0x1F0000`, but this offset must not be assumed for a different partition table.

已检查的这台硬件使用位于 `0x1F0000` 的活动应用槽，但不能对其他分区表直接假定此地址。

## Verified configuration
## 已验证配置

The final 32-voice build uses 53,392 bytes of static RAM and 816,597 bytes of program flash before image wrapping.

最终 32 音构建使用 53,392 字节静态 RAM 和 816,597 字节程序 Flash，数值为镜像封装前统计。

The previous ESP32-D0WD-V3 baseline reached all 32 full-quality voices with `held=0`, zero I2S write faults, and about 46 percent peak reporting-window load.

此前 ESP32-D0WD-V3 基准实测达到全部 32 个完整音质复音，同时保持 `held=0`、I2S 写入错误为零，报告周期最高负载约为 46%。

The final dynamic-piano build reached all 32 voices and 66.6 percent peak reporting-window load with `drop=0`, `held=0`, and `i2s=0`.

最终动态钢琴版本实测达到全部 32 复音，报告周期最高负载为 66.6%，同时保持 `drop=0`、`held=0` 和 `i2s=0`。

MIDI total-duration preview is enabled, while MIDI seeking remains intentionally unavailable.

MIDI 总时长预览已经启用，但仍有意不提供 MIDI 快进退。

The one-time duration scan does not affect steady-state synthesis, and seeking is omitted because it would require replaying controller, program, sustain, and note state from the start.

一次性时长扫描不会影响持续合成；不加入跳转是因为它需要从文件开头重放控制器、音色、延音和音符状态。

Two 320 kbps, 44.1 kHz stereo MP3 files opened correctly across repeated MP3-to-MIDI transitions without panic or watchdog reset.

两首 320kbps、44.1kHz 立体声 MP3 在多次 MP3 与 MIDI 往返切换中均正常打开，未发生 panic 或看门狗复位。
