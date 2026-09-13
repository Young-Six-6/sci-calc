# USB 串口键盘共享

[English](README.md)

USB Key Share 通过计算器的 CH340C USB 串口，把实体按键状态变化发送给 Windows 电脑。附带的 Python 客户端只会显示或记录这些事件，不会把计算器变成 USB 键盘，也不会向 Windows 注入按键。

## 安装

1. 将 [`../../sci_calc_code/bin/serialkeyboard.bin`](../../sci_calc_code/bin/serialkeyboard.bin) 复制到 SD 卡根目录，保存为 `/serialkeyboard.bin`。
2. 在 SD 卡根目录保留当前 [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin)，文件名为 `/main.bin`。
3. 进入 `Programs > Tools > USB Key Share`。

长按计算器的 `ESC` 约一秒，可恢复 `/main.bin` 并返回主计算器。

## 设置 Windows 电脑

建议使用 Python 3.10 或更高版本。电脑端只有 PySerial 一个依赖：

```powershell
cd games\SerialKeyboard
python -m pip install -r pc\requirements.txt
python pc\serial_keyboard.py --list-ports
```

连接前请关闭 PlatformIO Monitor 和其他串口终端。同一时间只能有一个程序打开目标 COM 口。

## 记录按键

请把 `COM7` 换成 `--list-ports` 显示的实际端口：

```powershell
python pc\serial_keyboard.py --port COM7
```

使用下面的命令可打开 5x6 实时面板，持续显示每个按键的 `UP` 或 `DOWN` 状态：

```powershell
python pc\serial_keyboard.py --port COM7 --monitor
```

按下的按键会立即高亮。每条按键事件都带有完整的 30 键状态，周期状态包还会在串口偶然漏包后自动修正面板。`--monitor` 可以和 `--log` 一起使用。

默认会输出容易阅读的事件：

```text
    12.340s  #00018  PRESS   key=SHIFT        id=24 pos=R5C1 mods=SHIFT held=0ms
    12.412s  #00019  PRESS   key=3            id=20 pos=R4C3 mods=SHIFT held=0ms
    12.598s  #00020  RELEASE key=3            id=20 pos=R4C3 mods=SHIFT held=186ms
```

加上 `--log` 后，会在保留终端输出的同时，向 UTF-8 JSON Lines 文件追加机器可读记录：

```powershell
python pc\serial_keyboard.py --port COM7 --log logs\keys.jsonl
```

常用选项：

| 选项 | 用途 |
| --- | --- |
| `--list-ports` | 列出可用串口 |
| `--monitor` | 用 5x6 实时面板显示每个按键当前的 `UP` 或 `DOWN` 状态 |
| `--log 路径` | 向 JSONL 文件追加设备信息和按键事件 |
| `--show-state` | 显示每秒一次的完整按键状态和丢失计数 |
| `--connect-timeout 秒数` | 修改初次等待数据包的超时，默认为 6 秒 |
| `--duration 秒数` | 测试指定时长后自动结束 |

按 `Ctrl+C` 停止。最后一行会汇总收到的按键事件数、串口序号缺口和设备事件队列丢失数。

## 事件规则

- 每次经过消抖的实体按下和松开都会发送。
- 松开事件包含该键持续按住的时间。
- 实体 Shift 键按住时，记录中会出现 `mods=SHIFT`。
- 固件修订版 1 不自动生成连发事件；协议为以后扩展保留了连发动作编号。
- 按键使用固定的行优先 ID，范围为 0 至 29。JSON 和协议中的行列从 0 开始，终端会显示成更直观的 `R1C1` 至 `R5C6`。
- 某些 ESP32 开发板在串口打开时会复位。客户端会识别新的启动 ID，并自动开始一段新的序号统计。

### 实体按键矩阵

| 行 | C1 | C2 | C3 | C4 | C5 | C6 |
| --- | --- | --- | --- | --- | --- | --- |
| R1 | ESC | / | * | - | SIN | SEC |
| R2 | 7 | 8 | 9 | + | COS | CSC |
| R3 | 4 | 5 | 6 | ^ | TAN | COT |
| R4 | 1 | 2 | 3 | ENTER | EXP | RPN/NORM |
| R5 | SHIFT | 0 | . | BACKSPACE | ANGLE | TAB |

这些名称对应计算器上实际印刷的按键。电脑收到的是实体键身份，不是主计算器经过 Shift 键层解释后的字符。

## 扩展接收器

`SerialKeyboardReceiver.records()` 会提供带类型的记录，而且不依赖任何 Windows 按键注入库。以后可以让 MIDI 或自动化模块只消费 `KeyEventRecord`，自行决定每个实体键的动作：

```python
from pc.serial_keyboard import ACTION_PRESS, KeyEventRecord, SerialKeyboardReceiver

with SerialKeyboardReceiver("COM7") as keyboard:
    for record in keyboard.records():
        if isinstance(record, KeyEventRecord) and record.action_id == ACTION_PRESS:
            handle_press(record.key_id, record.modifiers)
```

动作映射应放在 `serial_keyboard.py` 之外。这样基础记录器始终只是实体按键的只读来源，MIDI、快捷动作或软件控制可以分别增加。

## 串口协议

串口参数为 `115200 8N1`。每个二进制帧的结构如下：

| 字节数 | 字段 |
| --- | --- |
| 4 | ASCII 魔数 `SKB1` |
| 1 | 协议版本，目前为 `1` |
| 1 | 数据包类型 |
| 2 | 小端序包序号 |
| 4 | 小端序设备毫秒时间 |
| 2 | 小端序负载长度 |
| N | 负载，最多 64 字节 |
| 2 | 对包头和负载计算的 CRC-16/CCITT-FALSE，小端序 |

数据包类型：

| 类型 | 负载 |
| --- | --- |
| `0x01` 设备信息 | 固件修订、矩阵大小、按键数、波特率、能力位和启动 ID |
| `0x02` 按键事件 | 按键 ID、动作、行、列、修饰键位、按住毫秒数和完整按键状态位图 |
| `0x03` 状态 | 完整按键状态位图、已生成事件数和设备队列丢失数 |

ESP32 的串口路径只发送、不接收，固件中没有命令接收器。设备信息和状态帧只是数据流元信息，实体输入只由按键事件帧承载。解码器会检查长度和 CRC，丢弃启动文字或损坏的数据帧，并从下一个 `SKB1` 魔数处恢复同步。

## 故障排查

- `No serial ports found`：重新连接 USB；如果 Windows 没有生成 COM 口，请安装 CH340 驱动后再次运行 `--list-ports`。
- `no valid SKB1 packets received`：先在计算器上打开 USB Key Share，确认 COM 口，并关闭其他串口工具。
- 每个按键似乎出现两次：一次普通点击本来就会产生一条 `PRESS` 和一条 `RELEASE`。
- 自定义动作在断线后仍认为某键被按住：串口关闭或启动 ID 改变时，应清空该动作模块维护的活动按键状态。
- OLED 显示 `SD UNAVAILABLE`：键盘记录仍能使用，但恢复 `/main.bin` 前需要让 SD 卡恢复可用。

## 硬件

| 设备 | 引脚 |
| --- | --- |
| USB 串口 | UART0，通过板载 CH340C |
| OLED | CS GPIO5、DC GPIO16、RST GPIO17、MOSI GPIO23、SCK GPIO18 |
| SD 卡 | CS GPIO4、MOSI GPIO23、MISO GPIO19、SCK GPIO18 |
| 键盘行 | GPIO33、GPIO32、GPIO35、GPIO34、GPIO39 |
| 键盘列 | GPIO13、GPIO12、GPIO14、GPIO27、GPIO26、GPIO25 |

## 编译与测试

```powershell
cd games\SerialKeyboard
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
python -m unittest discover -s tests -v
Copy-Item .pio\build\serial_keyboard\firmware.bin ..\..\sci_calc_code\bin\serialkeyboard.bin -Force
```

生成的 `serialkeyboard.bin` 是通过 SD 卡加载的应用分区镜像，不能作为地址 `0x0` 的全量刷机镜像使用。
