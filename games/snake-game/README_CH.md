# 贪吃蛇

[English](README.md)

贪吃蛇是面向 SCI-CALC 256x64 OLED 的独立游戏。控制蛇吃到水果并不断变长，同时避免撞上边界或自己的身体。

## 按键

| 按键 | 功能 |
| --- | --- |
| `8` | 向上转向 |
| `2` | 向下转向 |
| `4` | 向左转向 |
| `6` | 向右转向 |
| 游戏结束后按任意键 | 开始新游戏 |
| `ESC` | 恢复 `/main.bin` 并返回计算器 |

## 安装

1. 将 [`../../sci_calc_code/bin/snake.bin`](../../sci_calc_code/bin/snake.bin) 复制到 SD 卡根目录，保存为 `/snake.bin`。
2. 将当前 [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) 复制到 SD 卡根目录，保存为 `/main.bin`。
3. 进入 `Programs > Games > Snake`。

## 硬件

| 设备 | 引脚 |
| --- | --- |
| OLED | CS GPIO5、DC GPIO16、RST GPIO17、MOSI GPIO23、SCK GPIO18 |
| SD 卡 | CS GPIO4、MOSI GPIO23、MISO GPIO19、SCK GPIO18 |
| 键盘行 | GPIO33、GPIO32、GPIO35、GPIO34、GPIO39 |
| 键盘列 | GPIO13、GPIO12、GPIO14、GPIO27、GPIO26、GPIO25 |

Arduino 工程入口为 [`snake-game.ino`](snake-game.ino)。编译 ESP32 应用后，将镜像发布为 `/snake.bin`，即可通过计算器应用加载器启动。

许可证信息请查看 [`LICENSE`](LICENSE)。
