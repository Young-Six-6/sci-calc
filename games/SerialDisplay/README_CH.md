# USB 串口显示器

[English](README.md)

USB Display 可将计算器的 256x64 OLED 变为电脑驱动的小屏幕，用于显示 CPU/RAM 仪表、单色视频、图片、测试图和简短状态文字。

## 安装

1. 将 [`../../sci_calc_code/bin/serialdisplay.bin`](../../sci_calc_code/bin/serialdisplay.bin) 复制到 SD 卡根目录，保存为 `/serialdisplay.bin`。
2. 将当前 [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) 复制到 SD 卡根目录，保存为 `/main.bin`。
3. 启动电脑客户端之前，先进入 `Programs > Tools > USB Display`。

如需通过 USB 完整安装，请将 [`../../sci_calc_code/bin/sci-calc-full-1.15-min-spiffs.bin`](../../sci_calc_code/bin/sci-calc-full-1.15-min-spiffs.bin) 刷写到地址 `0x0`。

## 设置电脑

建议使用 Python 3.10 或更高版本。

```powershell
cd games\SerialDisplay
python -m pip install -r pc\requirements.txt
python pc\serial_display.py --list-ports
```

连接前请关闭 PlatformIO Monitor 和其他串口终端。同一时间只能有一个程序占用目标 COM 口。

## 常用命令

请将 `COM7` 替换为 `--list-ports` 显示的端口。

### CPU 和 RAM 仪表

```powershell
python pc\serial_display.py --port COM7 --stats
```

仪表默认每秒更新五次，可用 `--fps 20` 选择其他速率。

### 测试图

```powershell
python pc\serial_display.py --port COM7 --test-pattern
```

### 视频

```powershell
python pc\serial_display.py --port COM7 --video C:\Videos\status.mp4 --loop
```

使用 `--start 30` 从第 30 秒开始，使用 `--fps 15` 设置输出帧率。

### 图片

```powershell
python pc\serial_display.py --port COM7 --image C:\Pictures\status.png --fit contain
```

### 文字

```powershell
python pc\serial_display.py --port COM7 --text "BUILD COMPLETE"
```

### 返回计算器

```powershell
python pc\serial_display.py --port COM7 --exit-device
```

也可长按计算器的 `ESC` 约一秒。两种方式都会恢复 `/main.bin` 并重启计算器。

按 `Ctrl+C` 可停止仪表或视频，并保留最后一帧。

## 图像选项

| 选项 | 用途 |
| --- | --- |
| `--fit contain` | 保留完整图像，必要时留出空白边缘 |
| `--fit cover` | 填满屏幕并裁剪超出部分 |
| `--fit stretch` | 不保持长宽比，直接填满屏幕 |
| `--threshold 0..255` | 设置黑白转换阈值 |
| `--dither` | 保留更多视觉灰度细节 |
| `--invert` | 交换黑白颜色 |
| `--contrast 0..255` | 设置 OLED 对比度 |
| `--loop` | 循环播放视频 |

硬阈值转换通常更适合文字和 UI 图形，抖动处理则更适合照片和带阴影的视频。

## 帧率建议

- CPU/RAM 仪表：5 至 20 FPS
- 简单动画和稀疏图形：30 至 40 FPS
- 细节较多的全屏视频：20 至 23 FPS

实际速率取决于视频解码、单色转换、USB 串口传输和 OLED 刷新时间。

## Python 接口

`pc/serial_display.py` 提供 `SerialDisplayClient` 供自定义程序使用。`send_frame()` 接收一帧已打包的 256x64 单色画面，长度固定为 2048 字节。

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

客户端还提供 `clear()`、`set_contrast()` 和 `exit_device()`。

## 故障排查

- `serial response timed out`：先在计算器中打开 USB Display，确认 COM 口，并关闭所有其他串口程序。
- 打开串口时 ESP32 重启：等待客户端在启动后重新连接，这在 USB 转串口会切换复位线的开发板上属于正常现象。
- 视频跟不上：降低 `--fps`，使用更低分辨率的视频，或用硬阈值转换代替抖动。
- 画面难以辨认：调整 `--threshold`、`--contrast`、`--fit` 或 `--invert`。
- 返回命令失败：确认 SD 卡中存在 `/main.bin`。

## 硬件

USB 数据通过开发板的 UART0 USB 转串口连接传输。

| 设备 | 引脚 |
| --- | --- |
| OLED | CS GPIO5、DC GPIO16、RST GPIO17、MOSI GPIO23、SCK GPIO18 |
| SD 卡 | CS GPIO4、MOSI GPIO23、MISO GPIO19、SCK GPIO18 |
| ESC 键盘行/列 | GPIO33 / GPIO13 |

## 编译与测试

```powershell
cd games\SerialDisplay
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
python -m unittest discover -s tests -v
Copy-Item .pio\build\serial_display\firmware.bin ..\..\sci_calc_code\bin\serialdisplay.bin -Force
```

生成的 `serialdisplay.bin` 通过计算器的应用加载器安装。在地址 `0x0` 刷写时请使用合并的全量镜像。
