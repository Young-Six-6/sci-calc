# USB Serial Display
# USB 串口显示器

USB Display turns the SCI-CALC OLED into a small telemetry and monochrome-media display over the existing USB serial connection.

USB Display 通过现有 USB 串口连接，将 SCI-CALC OLED 变为电脑遥测与单色媒体小屏。

It is a standalone application, so it does not consume RAM while the main calculator or music player is running.

它是独立应用，因此主计算器或音乐播放器运行时不会占用其 RAM。

## Installation
## 安装方法

Copy sci_calc_code/bin/main.bin and sci_calc_code/bin/serialdisplay.bin to the SD-card root.

将 sci_calc_code/bin/main.bin 与 sci_calc_code/bin/serialdisplay.bin 复制到 SD 卡根目录。

Main firmware version 1.15 includes USB Display under Programs > Tools.

主固件 1.15 版将 USB Display 放在 Programs > Tools 中。

For a complete USB reinstall, flash sci_calc_code/bin/sci-calc-full-1.15-min-spiffs.bin at address 0x0.

如果需要通过 USB 完整重装，请将 sci_calc_code/bin/sci-calc-full-1.15-min-spiffs.bin 刷写到地址 0x0。

Open USB Display before starting the computer client.

启动电脑客户端之前，请先在计算器中打开 USB Display。

The OLED initially shows the resolution, baud rate, protocol name, SD state, and return key.

OLED 初始页面会显示分辨率、波特率、协议名称、SD 状态和返回按键。

## Computer setup
## 电脑端设置

Python 3.10 or newer is recommended.

建议使用 Python 3.10 或更高版本。

Install the serial, system-telemetry, video, and image dependencies from the pc directory.

在 pc 目录中安装串口、系统遥测、视频和图像处理依赖。

```powershell
cd C:\Users\wang\Documents\camera\sci-calcme\games\SerialDisplay
python -m pip install -r pc\requirements.txt
```

List the available serial ports before connecting.

连接前可以列出当前可用串口。

```powershell
python pc\serial_display.py --list-ports
```

The serial port must not be open in PlatformIO Monitor or another terminal at the same time.

该串口不能同时被 PlatformIO Monitor 或其他串口终端占用。

Opening the port may briefly reset an ESP32 development board, so the client waits for the firmware to restart before handshaking.

打开串口可能使 ESP32 开发板短暂复位，因此客户端会等待固件重新启动后再握手。

## Common commands
## 常用命令

Send a test pattern to COM7.

向 COM7 发送测试图。

```powershell
python pc\serial_display.py --port COM7 --test-pattern
```

Show the native CPU/RAM dashboard at the default five updates per second.

以默认每秒五次更新显示原生 CPU/RAM 仪表。

```powershell
python pc\serial_display.py --port COM7 --stats
```

Play a video as monochrome frames and repeat it after the end.

将视频转换为单色帧播放，并在结束后循环。

```powershell
python pc\serial_display.py --port COM7 --video C:\Videos\status.mp4 --loop
```

Use --start 30 to begin a video at the thirty-second position, and use --fps 15 to override its frame rate.

使用 --start 30 可以从视频第 30 秒开始，使用 --fps 15 可以覆盖视频帧率。

Send one image and leave it displayed after the client exits.

发送一张图片，并在客户端退出后继续保留画面。

```powershell
python pc\serial_display.py --port COM7 --image C:\Pictures\status.png --fit contain
```

Use --invert to exchange black and white.

使用 --invert 可以交换黑白颜色。

Hard threshold conversion is the default, and --threshold 128 selects its black-and-white boundary.

默认采用硬阈值转换，--threshold 128 用于设置黑白分界。

Use --dither only when grayscale detail is more important than sharp edges.

仅在灰度细节比锐利边缘更重要时使用 --dither。

Use --fit contain, cover, or stretch to select the aspect-ratio behavior.

使用 --fit contain、cover 或 stretch 可以选择画面比例处理方式。

Use --contrast from 0 through 255 to adjust OLED contrast.

使用 0 到 255 范围的 --contrast 可以调整 OLED 对比度。

Press Ctrl+C to stop the CPU/RAM dashboard or video without clearing the last frame.

按 Ctrl+C 可以停止 CPU/RAM 仪表或视频，并保留最后一帧。

Send short text directly when a custom status message is sufficient.

需要显示简短自定义状态时，可以直接发送文字。

```powershell
python pc\serial_display.py --port COM7 --text "BUILD COMPLETE"
```

Hold the calculator ESC key for about one second, or use --exit-device, to install /main.bin and return.

长按计算器 ESC 键约一秒，或使用 --exit-device，可安装 /main.bin 并返回主程序。

## Custom Python API
## 自定义 Python 接口

The pc/serial_display.py module exposes SerialDisplayClient for other programs.

pc/serial_display.py 模块提供 SerialDisplayClient，供其他程序直接调用。

The class performs startup waiting, HELLO negotiation, sequence management, CRC validation, RLE selection, and acknowledgement waiting.

该类会自动处理启动等待、HELLO 协商、序号管理、CRC 校验、RLE 选择和确认等待。

send_frame accepts exactly 2048 packed bytes and returns raw or rle to report the selected transport.

send_frame 接收恰好 2048 个打包字节，并返回 raw 或 rle 表示实际选择的传输方式。

```python
from pc.serial_display import FRAME_BYTES, SerialDisplayClient

frame = bytearray(FRAME_BYTES)
for y in range(64):
    x = y * 4
    frame[y * 32 + x // 8] |= 1 << (x & 7)

with SerialDisplayClient("COM7") as display:
    display.set_contrast(220)
    display.send_frame(bytes(frame))
```

clear clears the screen, set_contrast changes OLED contrast, and exit_device returns to /main.bin.

clear 用于清屏，set_contrast 用于修改 OLED 对比度，exit_device 用于返回 /main.bin。

The lower-level build_packet, parse_packet_bytes, crc16_ccitt, rle_encode, and rle_decode functions are also available for custom transports and tests.

更底层的 build_packet、parse_packet_bytes、crc16_ccitt、rle_encode 和 rle_decode 函数也可用于自定义传输与测试。

## Display format
## 显示格式

The logical screen is 256 pixels wide and 64 pixels high with one bit per pixel.

逻辑屏幕宽 256 像素、高 64 像素，每像素使用一位。

A complete frame is exactly 2048 bytes.

完整一帧固定为 2048 字节。

Bytes are stored from left to right and then top to bottom, with the least significant bit representing the leftmost pixel in each eight-pixel group.

字节按从左到右、再从上到下排列，每组八个像素中最低有效位代表最左侧像素。

The computer client converts color images to grayscale, resizes them, applies optional Floyd-Steinberg dithering, and packs this bitmap automatically.

电脑客户端会自动将彩色图像转换为灰度、调整大小、选择性应用 Floyd-Steinberg 抖动，并完成位图打包。

## Wire protocol
## 通信协议

The serial link uses 921600 baud, eight data bits, no parity, and one stop bit.

串口使用 921600 波特率、八个数据位、无校验和一个停止位。

Every packet uses this byte layout.

每个数据包使用以下字节布局。

```text
Offset  Size  Field
0       4     ASCII magic "SCD1"
4       1     Protocol version, currently 1
5       1     Command
6       2     Sequence, little-endian
8       2     Payload length, little-endian
10      N     Payload
10+N    2     CRC16-CCITT, little-endian
```

CRC uses polynomial 0x1021 and initial value 0xFFFF over the version-through-payload bytes, excluding the four-byte magic.

CRC 使用多项式 0x1021 和初始值 0xFFFF，计算范围从版本字段到负载末尾，不包含四字节魔数。

Payload length cannot exceed 4096 bytes.

负载长度不能超过 4096 字节。

Command 0x01 HELLO has no payload and returns 0x81 INFO.

命令 0x01 HELLO 不带负载，并返回 0x81 INFO。

Command 0x02 FRAME_RAW carries exactly 2048 display bytes.

命令 0x02 FRAME_RAW 固定携带 2048 字节显示数据。

Command 0x03 FRAME_RLE carries count-and-value byte pairs.

命令 0x03 FRAME_RLE 携带“计数、数值”字节对。

Each RLE count is from 1 through 255, and the decoded frame must be exactly 2048 bytes.

每个 RLE 计数范围为 1 到 255，解压后的帧必须恰好为 2048 字节。

Command 0x04 CLEAR carries one byte, where zero means black and any nonzero value means white.

命令 0x04 CLEAR 携带一个字节，零表示全黑，非零表示全白。

Command 0x05 CONTRAST carries one contrast byte.

命令 0x05 CONTRAST 携带一个对比度字节。

Command 0x06 PING and command 0x07 EXIT have no payload.

命令 0x06 PING 与命令 0x07 EXIT 均不带负载。

Response 0x80 ACK carries the original command and status zero.

响应 0x80 ACK 携带原始命令编号和零状态码。

Response 0xFF NACK carries the original command and an error code.

响应 0xFF NACK 携带原始命令编号和错误码。

The device echoes the request sequence in every response.

设备会在每个响应中原样返回请求序号。

An incomplete packet is discarded after 500 milliseconds, and the receiver then searches for the next SCD1 marker.

不完整数据包会在 500 毫秒后被丢弃，接收器随后重新搜索下一个 SCD1 魔数。

## Performance and memory
## 性能与内存

The CPU/RAM dashboard defaults to five updates per second, which keeps the graph readable and system overhead low.

CPU/RAM 仪表默认每秒更新五次，使曲线易读并降低系统开销。

Video uses its source frame rate by default and accepts an override up to 40 FPS, but the actual rate is limited by decoding, conversion, serial acknowledgement, and OLED refresh time.

视频默认使用源帧率，并允许最高每秒 40 帧的覆盖值，但实际速度受解码、转换、串口确认和 OLED 刷新时间限制。

RLE is selected only when it is smaller than a raw frame, avoiding compression overhead on noisy video.

只有 RLE 小于原始帧时才会使用它，从而避免高噪声视频产生额外压缩开销。

On the tested CH340 connection, revision 3 sustained 23.19 FPS for incompressible raw frames and 66.45 FPS for highly compressible RLE frames without packet loss.

在测试所用的 CH340 连接上，修订版 3 对不可压缩原始帧持续达到 23.19 FPS，对高度可压缩 RLE 帧达到 66.45 FPS，且没有丢包。

Use 20 to 23 FPS for worst-case full-screen video, while dashboards and simple animation can normally use 30 or 40 FPS.

最复杂的全屏视频建议使用 20 至 23 FPS，仪表盘和简单动画通常可使用 30 或 40 FPS。

The no-PSRAM firmware uses fixed 2048-byte frame storage and a fixed 4096-byte packet buffer.

无 PSRAM 固件使用固定的 2048 字节帧存储和固定的 4096 字节数据包缓冲区。

The host waits for ACK after each frame, preventing an unbounded queue inside the ESP32.

电脑端会在每帧后等待 ACK，防止 ESP32 内部形成无界队列。

Firmware revision 3 reserves an 8 KiB UART receive buffer, reads serial data in blocks, and converts frames directly into the native display buffer.

固件修订版 3 预留 8 KiB UART 接收缓冲区、批量读取串口数据，并将画面直接转换到显示器原生缓冲区。

When revision 1 firmware is detected, the PC client automatically sends large packets in paced chunks.

检测到修订版 1 固件时，电脑端会自动将大数据包分块限速发送。

## Hardware
## 硬件连接

USB communication uses the board's existing UART0 USB bridge and does not consume another project GPIO.

USB 通信使用开发板现有的 UART0 USB 转串口，不额外占用项目 GPIO。

OLED uses CS GPIO5, DC GPIO16, reset GPIO17, MOSI GPIO23, and clock GPIO18.

OLED 使用 CS GPIO5、DC GPIO16、复位 GPIO17、MOSI GPIO23 和时钟 GPIO18。

SD uses CS GPIO4, MOSI GPIO23, MISO GPIO19, and clock GPIO18.

SD 卡使用 CS GPIO4、MOSI GPIO23、MISO GPIO19 和时钟 GPIO18。

## Build and test
## 编译与测试

Build the standalone application with PlatformIO.

使用 PlatformIO 编译独立应用。

```powershell
cd C:\Users\wang\Documents\camera\sci-calcme\games\SerialDisplay
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

Run the protocol tests without requiring a serial device.

无需串口设备即可运行协议测试。

```powershell
python -m unittest discover -s tests -v
```

serialdisplay.bin is an OTA application image and must not be flashed at address 0x0.

serialdisplay.bin 是 OTA 应用镜像，不能刷写到地址 0x0。
