# USB Serial Display
# USB 串口显示器

USB Display is a standalone SCI-CALC program published as sci_calc_code/bin/serialdisplay.bin.

USB Display 是独立的 SCI-CALC 程序，发布文件为 sci_calc_code/bin/serialdisplay.bin。

It receives framed 256 by 64 monochrome images from a computer at 921600 baud and renders them on the OLED.

它以 921600 波特率接收电脑发送的 256×64 单色帧，并将其显示在 OLED 上。

The supplied Python client renders a crisp CPU/RAM dashboard and can play monochrome video, images, text, or a generated test pattern.

随附的 Python 客户端会绘制清晰的 CPU/RAM 仪表，并可播放单色视频、图片、文字或生成的测试图。

Revision 3 measured 23.19 FPS for incompressible raw frames and 66.45 FPS for highly compressible RLE frames on the tested CH340 connection.

修订版 3 在测试所用的 CH340 连接上，不可压缩原始帧实测 23.19 FPS，高度可压缩 RLE 帧实测 66.45 FPS。

Each packet carries a sequence number, payload length, and CRC16-CCITT checksum.

每个数据包都带有序号、负载长度和 CRC16-CCITT 校验。

The client waits for an acknowledgement after every frame and automatically chooses raw or RLE transport.

客户端会在每帧后等待确认，并自动选择原始或 RLE 传输。

Firmware revision 3 reserves an 8 KiB UART receive buffer, reads serial data in blocks, and converts frames directly into the native display buffer.

固件修订版 3 预留 8 KiB UART 接收缓冲区、批量读取串口数据，并将画面直接转换到显示器原生缓冲区。

When revision 1 firmware is detected, the PC client automatically sends large packets in paced chunks.

检测到修订版 1 固件时，电脑端会自动将大数据包分块限速发送。

For complete installation, command, and wire-protocol details, see games/SerialDisplay/README.md.

完整安装、命令和通信协议说明请参阅 games/SerialDisplay/README.md。
