# SCI-CALC

[English](README.md)

<a href="https://github.com/shaoxiongduan/sci-calc/">
  <img height="400" alt="SCI-CALC 计算器" src="https://github.com/user-attachments/assets/1d2f0c47-30ca-4eb9-b335-ce44204528ab">
</a>

SCI-CALC 是一台基于 ESP32 的手持计算器和可扩展应用平台，集成科学计算器、方程与图形工具、可配置的蓝牙 HID 宏键盘、独立程序启动器、小型游戏机，以及通过 USB 连接电脑的音乐和显示工具。

本仓库基于上游 [SCI-CALC 项目](https://github.com/shaoxiongduan/sci-calc/)。上游的计算器、键盘层、SD 卡程序加载方式和原有游戏仍是基础；下面归纳本仓库在此基础上维护的新增内容。

## 相对上游新增

- **主固件：**方程求解、图形计算器启动入口、五类 `Programs` 菜单、更稳定的二级菜单导航、通过明确清单自动发现 SD 应用，以及可选的自动息屏和自动关闭。
- **独立计算工具：**经典数码管风格计算器，适合日常四则运算、百分比、税率、记忆、纸带和累计合计；图形计算器支持同时绘制最多四个函数，并提供平移、缩放、网格、刻度和追踪。
- **独立游戏：**2048、保证可解的随机迷宫，以及从 SD 卡 `.ch8` 目录加载游戏的 Super-CHIP 模拟器，同时保留上游游戏移植。
- **有线音乐应用：**支持 SD 卡音乐、MP3 等压缩音频、流式 MIDI、总时长缓存和可选合成音色，面向无 PSRAM 的 ESP32 提供最多 32 路复音；通过 PCM5102A 有线 I2S 输出。
- **音乐键盘：**独立的实时乐器，提供计算器键位布局、多种音色、和弦、录音、MIDI 导出，以及面向无乐理基础用户的 MIDI 转计算器简谱工具。
- **USB 工具：**串口显示器可显示 CPU/RAM 仪表盘和其他单色图形；串口键盘共享会把计算器按下、松开和持续状态事件发送给电脑，但不会模拟 Windows 键盘输入。
- **Wi-Fi 存储工具：**设备启动临时热点，并通过带认证的网页管理 SD 卡文件。
- **桌面工具：**Electron 版 Falling Keys 和高级音乐键盘工作站，支持 MIDI 导入、计算器控制、练习、录音、合成和多轨编曲。
- **文档与发布：**补充中英文说明、通用简谱和 MIDI 指引、清单式应用命名，以及各独立应用的构建和测试说明。

主固件仍保留上游的蓝牙 HID 宏键盘能力。音乐播放器是独立应用镜像，使用下方说明的有线 PCM5102A 输出。

## 硬件

- 主控：ESP32-WROOM-32E
- 屏幕：256x64 SSD1322 单色 OLED
- USB 转串口：CH340C
- 键盘：5x6 矩阵，使用 Kailh Choc V1 轴体
- 存储：FAT32 microSD 卡
- 电池：按照原始硬件设计和充电电路，使用合适的 1000 mAh 以下 LiPo 电池

### 主要引脚

| 设备 | 引脚 |
| --- | --- |
| OLED | CS GPIO5、DC GPIO16、RST GPIO17、MOSI GPIO23、SCK GPIO18 |
| SD 卡 | CS GPIO4、MOSI GPIO23、MISO GPIO19、SCK GPIO18 |
| 键盘行 | GPIO33、GPIO32、GPIO35、GPIO34、GPIO39 |
| 键盘列 | GPIO13、GPIO12、GPIO14、GPIO27、GPIO26、GPIO25 |

OLED 与 SD 卡共用 SPI 的 MOSI 和时钟线，并分别使用独立的 CS 引脚。

### 可选 PCM5102A 音频接线

独立音乐播放器使用以下有线 I2S 接线：

| 计算器 | PCM5102A |
| --- | --- |
| GPIO21 | BCK / BCLK |
| GPIO22 | LCK / LRCK / WS |
| GPIO15 | DIN |
| GND | GND |
| GND | SCK |

将 `SCK` 接地以使用 PCM5102A 三线 I2S 模式。DAC 的线路输出应连接有源音箱、功放或线路输入，不要直接连接无源扬声器。音频接线不要占用键盘列引脚。

## 安装与更新

当前主固件版本为 1.22。本仓库使用定制的 `min_spiffs` 分区布局来安排应用分区。因此所有设备都必须先完成一次 USB 完整安装，包括从上游项目或本分支旧版本迁移的设备。SD 卡更新不能无缝迁移分区表。

发布目录中的完整固件文件名保留真实版本号，例如 `sci-calc-full-1.22-min-spiffs.bin`。下文使用的 `sci-calc-full-1x.xx-min-spiffs.bin` 只是占位写法，请在 [`sci_calc_code/bin`](sci_calc_code/bin/) 中选择实际版本文件。

### 必须进行的 USB 完整刷写

首次安装或迁移时使用合并完整固件，它会同时写入匹配的 bootloader、分区表、OTA 布局和主应用。

1. 通过 USB 连接计算器，确认串口号，例如 `COM7`。
2. 从 [`sci_calc_code/bin`](sci_calc_code/bin/) 选择 `sci-calc-full-1x.xx-min-spiffs.bin`。
3. 如未安装 `esptool`，先执行 `python -m pip install esptool`，然后在 PowerShell 执行下面命令，并按实际情况替换 `COM7` 和文件名：

```powershell
python -m esptool --chip esp32 --port COM7 --baud 460800 write-flash 0x0 "sci_calc_code/bin/sci-calc-full-1x.xx-min-spiffs.bin"
```

完整固件必须写入 `0x0` 地址。不要把 `main.bin`、`music.bin` 或 `partitions-min-spiffs.bin` 写入 `0x0`；它们不是完整刷写镜像。旧版 `esptool` 4.x 使用带下划线的 `write_flash`。如果当前 USB 连接在高速率下不稳定，可将 `460800` 改为 `115200` 重试。

USB 刷写完成后，如有需要将 SD 卡格式化为 FAT32，把 `/main.bin` 和需要的独立应用镜像复制到根目录，然后重启计算器。首次启动完成前请保持供电。

### 完成首次安装后的 SD 卡更新

设备已经安装本仓库的分区布局后，日常更新独立应用可以沿用上游的 SD 卡流程，不需要 IDE：

1. 从 [`sci_calc_code/bin`](sci_calc_code/bin/) 复制需要的镜像到 SD 卡根目录，替换同用途的旧镜像。
2. 更新主固件时，将 `main.bin` 放在根目录并命名为 `/main.bin`；只更新需要的独立应用，例如 `/music.bin` 或 `/graphcalc.bin`。
3. 打开 `Settings > Update from SD`，并保持供电直到设备重启。

如果已经使用兼容分区布局的旧版主固件没有 `Update from SD`，可以进入任意已安装的独立应用，按住 `ESC` 约一秒恢复 `/main.bin`。这只是兼容布局设备的后续更新方式，不能替代首次 USB 完整安装。返回键细节以各应用说明为准，完整流程参见 [软件更新](docs/updating_software.md)。

程序菜单不会把所有裸 `.bin` 文件都列出来。要为自定义应用设置友好名称，请把固件放到 SD 卡并创建明确的 `/apps/*.app` 清单，详见 [SD 应用自动发现](docs/sd_apps.md)。

## 主要按键

| 按键 | 功能 |
| --- | --- |
| `8` / `2` | 在菜单中上下移动 |
| `Enter` | 选择项目 |
| `6` 或 `Enter` | 展开 `Programs` 中的分类 |
| `4` 或 `ESC` | 返回上一级菜单 |
| 按住 `Shift` | 使用第二键层 |
| 双击 `Shift` | 锁定或解锁第二键层 |
| `ANG` | 切换计算器角度模式 |
| `TAB` | 在计算器输入框与历史记录之间切换 |

在计算器编辑框中，使用 `Shift+4` 和 `Shift+6` 移动光标。在菜单中使用方向键时不需要按住 `Shift`。

## 程序分类

主固件内置科学计算、方程求解、RPN、历史记录、秒表、设置和更新功能。`Programs` 启动器分为五类：

- `Calculation`：Classic Calc、Graph Calc
- `Games`：2048、Maze、Super-CHIP、Chess、Spacetrash、Snake、Tetris、Catacombs of the Damned 和 Squario
- `Music`：Music Player、Music Keyboard
- `Tools`：SD Manager、USB Display、USB Key Share
- `Other`：Wifi Rickroll 以及从 SD 清单自动发现的应用

外部应用使用应用分区镜像，通过 `Programs` 启动，通常通过恢复 `/main.bin` 返回计算器；具体按键请查看对应应用说明。SD 自动发现系统只读取明确的 `.app` 清单，屏幕显示名来自 `name=`，不会从 `.bin` 文件名猜测。

## 应用说明

- [2048](games/Game2048/README_CH.md)
- [经典计算器](games/ClassicCalculator/README_CH.md)
- [图形计算器](games/GraphCalculator/README_CH.md)
- [音乐播放器](games/MusicPlayer/README_CH.md)
- [高级音乐键盘](games/MusicKeyboard/README_CH.md)
- [Falling Keys Desktop 桌面下落音游](desktop/FallingKeysDesktop/README_CH.md)
- [高级音乐键盘桌面版](desktop/MusicKeyboardDesktop/README_CH.md)
- [迷宫](games/Maze/README_CH.md)
- [Super-CHIP](games/SuperChip/README_CH.md)
- [Wi-Fi SD 管理器](games/SDManager/README_CH.md)
- [USB 串口显示器](games/SerialDisplay/README_CH.md)
- [USB 串口键盘共享](games/SerialKeyboard/README_CH.md)
- [贪吃蛇](games/snake-game/README_CH.md)
- [Squario](games/Squario/README_CH.md)

## 更多文档

- [组装视频](https://youtu.be/RY_riS_9hHA?si=YKYpsfk6U_JUeETe)
- [键盘布局](docs/layout.md)
- [故障排查](docs/troubleshooting.md)
- [软件更新](docs/updating_software.md)
- [SD 应用自动发现](docs/sd_apps.md)
- [开发环境](docs/environment_setup.md)
- [代码结构](docs/code_structure.md)
- [通用简谱读法](docs/numbered_notation_CH.md)

## 编译

主固件使用 PlatformIO：

```powershell
cd sci_calc_code
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

每个持续维护的独立应用都在自身目录中提供构建说明；桌面应用位于 `desktop/`，使用各自的 Electron 和 Python 桥接环境。
