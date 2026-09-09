# SCI-CALC

[English](README.md)

<a href="https://github.com/shaoxiongduan/sci-calc/">
  <img height="400" alt="SCI-CALC 计算器" src="https://github.com/user-attachments/assets/1d2f0c47-30ca-4eb9-b335-ce44204528ab">
</a>

SCI-CALC 是一台基于 ESP32 的手持设备，集成科学计算器、方程求解、RPN 计算器、可配置的蓝牙 HID 宏键盘、独立程序启动器、秒表和小型游戏机。

## 硬件

- 主控：ESP32-WROOM-32E
- 屏幕：256x64 SSD1322 单色 OLED
- USB 转串口：CH340C
- 键盘：5x6 矩阵，使用 Kailh Choc V1 轴体
- 存储：FAT32 microSD 卡

### 主要引脚

| 设备 | 引脚 |
| --- | --- |
| OLED | CS GPIO5、DC GPIO16、RST GPIO17、MOSI GPIO23、SCK GPIO18 |
| SD 卡 | CS GPIO4、MOSI GPIO23、MISO GPIO19、SCK GPIO18 |
| 键盘行 | GPIO33、GPIO32、GPIO35、GPIO34、GPIO39 |
| 键盘列 | GPIO13、GPIO12、GPIO14、GPIO27、GPIO26、GPIO25 |

OLED 与 SD 卡共用 SPI 的 MOSI 和时钟线，并分别使用独立的 CS 引脚。

## 安装与更新

当前主固件版本为 1.16。

如需通过 USB 完整安装，请将 [`sci_calc_code/bin/sci-calc-full-1.16-min-spiffs.bin`](sci_calc_code/bin/sci-calc-full-1.16-min-spiffs.bin) 刷写到地址 `0x0`。

通过 SD 卡更新：

1. 将 [`sci_calc_code/bin/main.bin`](sci_calc_code/bin/main.bin) 复制到 SD 卡根目录，保存为 `/main.bin`。
2. 替换需要更新的独立应用镜像。
3. 在计算器中打开 `Settings > Update from SD`。
4. 保持供电，直到计算器自动重启。

独立应用是应用分区镜像，请从 `Programs` 启动。返回方法请查看各应用说明，设备会从 SD 卡恢复 `/main.bin`。

完整流程参见 [软件更新](docs/updating_software.md)。

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

`Programs` 分为五类：

- `Calculation`：Graph Calc
- `Games`：Maze、Super-CHIP、Chess、Spacetrash、Snake、Tetris、Catacombs of the Damned 和 Squario
- `Music`：Music Player
- `Tools`：SD Manager 和 USB Display
- `Other`：Wifi Rickroll 以及从 SD 清单自动发现的应用

可以通过明确的 `/apps/*.app` 清单添加应用。屏幕显示名来自清单，而不是固件文件名。详见 [SD 应用自动发现](docs/sd_apps.md)。

## 应用说明

- [图形计算器](games/GraphCalculator/README_CH.md)
- [音乐播放器](games/MusicPlayer/README_CH.md)
- [迷宫](games/Maze/README_CH.md)
- [Super-CHIP](games/SuperChip/README_CH.md)
- [Wi-Fi SD 管理器](games/SDManager/README_CH.md)
- [USB 串口显示器](games/SerialDisplay/README_CH.md)
- [贪吃蛇](games/snake-game/README_CH.md)
- [Squario](games/Squario/README_CH.md)

## 更多文档

- [组装视频](https://youtu.be/RY_riS_9hHA?si=YKYpsfk6U_JUeETe)
- [键盘布局](docs/layout.md)
- [故障排查](docs/troubleshooting.md)
- [开发环境](docs/environment_setup.md)
- [代码结构](docs/code_structure.md)

## 编译

主固件使用 PlatformIO：

```powershell
cd sci_calc_code
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

每个持续维护的独立应用都在自身目录中提供编译说明。
