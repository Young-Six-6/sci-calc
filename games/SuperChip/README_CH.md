# Super-CHIP 模拟器

[English](README.md)

Super-CHIP 是面向 SCI-CALC ESP32-WROOM-32E 的独立 CHIP-8 与 SUPER-CHIP 模拟器。可从 SD 卡选择 ROM，并使用计算器实体键盘游玩。

## 准备 SD 卡

在 FAT32 SD 卡中创建以下结构：

```text
/
|- main.bin
|- chip8.bin
`- chip8/
   |- game-one.ch8
   |- game-two.ch8
   `- demo.ch8
```

需要时，模拟器会在首次启动时创建 `/chip8`。请将 `.ch8` 文件直接放在该目录中，不要放入子目录。

ROM 浏览器最多读取 64 个文件，支持大小为 2 至 3584 字节的 ROM。

## 安装

1. 将 [`../../sci_calc_code/bin/chip8.bin`](../../sci_calc_code/bin/chip8.bin) 复制到 SD 卡根目录，保存为 `/chip8.bin`。
2. 将当前 [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) 复制到 SD 卡根目录，保存为 `/main.bin`。
3. 将需要的 `.ch8` 文件放入 `/chip8`。
4. 进入 `Programs > Games > Super-CHIP`。

如需通过 USB 完整安装，请将 [`../../sci_calc_code/bin/sci-calc-full-1.15-min-spiffs.bin`](../../sci_calc_code/bin/sci-calc-full-1.15-min-spiffs.bin) 刷写到地址 `0x0`。

## ROM 浏览器

| 按键 | 功能 |
| --- | --- |
| `8` / `2` | 选择上一个或下一个 ROM |
| `Enter` 或 `5` | 启动选中 ROM |
| `4` / `6` | 选择上一个或下一个兼容模式 |
| `-` / `+` | 降低或提高模拟速度 |
| `.` | 开关声音 |
| 退格键 | 重新扫描 `/chip8` |
| 长按 `ESC` 一秒 | 恢复 `/main.bin` 并返回计算器 |

## CHIP-8 十六进制键盘

印刷的数字键保持对应的十六进制数值，六个运算符键对应 A 至 F。

| 计算器按键 | CHIP-8 按键 |
| --- | --- |
| `0` 至 `9` | `0` 至 `9` |
| `/` | `A` |
| `*` | `B` |
| `-` | `C` |
| `+` | `D` |
| `^` | `E` |
| 退格键 | `F` |

这种映射使 ROM 说明中的“按 5”或“使用 A 和 F”可直接对应计算器的印刷键。

## 游戏中的控制

短按 `Enter` 或 `ESC` 会打开暂停菜单，不会向 CHIP-8 虚拟键盘发送按键。

请先按住 `Shift` 再按快捷键：

| 快捷键 | 功能 |
| --- | --- |
| `Shift+4` / `Shift+6` | 降低或提高速度 |
| `Shift+7` / `Shift+9` | 选择上一个或下一个模式并重启 ROM |
| `Shift+0` | 开关侧边状态显示 |
| `Shift+.` | 开关声音 |
| `Shift+Enter` | 重启当前 ROM |
| `Shift+退格` | 返回 ROM 浏览器 |
| `Shift+5` | 打开暂停菜单 |

在暂停菜单中，用 `8` 和 `2` 选择项目，用 `4` 和 `6` 修改设置，用 `Enter` 执行。

## 兼容模式

| 模式 | 建议用途 |
| --- | --- |
| `MODERN` | 较新的 CHIP-8 软件和 Octo 风格 ROM |
| `SUPER` | 使用 SCHIP 指令或显示模式的 SUPER-CHIP 软件 |
| `CLASSIC` | 依赖早期时序和指令行为的老式 CHIP-8 软件 |

切换兼容模式会重启已加载的 ROM，速度可在 ROM 运行时调整。如果游戏过快、过慢或漏掉输入，请先调整速度，再尝试其他兼容模式。

## 显示与声音

SUPER-CHIP 的 128x64 画面位于 256x64 OLED 中央。标准 CHIP-8 的 64x32 画面会放大到相同的中央 128x64 区域。可选侧边状态显示会显示模式、速度、声音状态、ROM 名称和返回提示。

CHIP-8 声音通过以下接线输出到 PCM5102A：

| 计算器 | PCM5102A |
| --- | --- |
| GPIO21 | BCK / BCLK |
| GPIO22 | LCK / LRCK / WS |
| GPIO15 | DIN |
| GND | GND |
| GND | SCK |

请将 PCM5102A 的 L/R 线路输出连接到有源音箱、功放或线路输入。

## 故障排查

- 列表中没有 ROM：将 `.ch8` 文件直接放在 `/chip8`，然后按退格键重新扫描。
- ROM 启动后黑屏或画面异常：选择其他兼容模式。
- 移动过快或漏掉输入：用 `-`、`Shift+4` 或暂停菜单降低速度。
- 没有声音：检查 GPIO21/22/15、共地、`SCK -> GND` 和外部功放。
- 无法返回：确认 SD 卡根目录中存在 `/main.bin`。

## 编译与测试

```powershell
cd games\SuperChip
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
python -m unittest discover -s tests -v
Copy-Item .pio\build\superchip\firmware.bin ..\..\sci_calc_code\bin\chip8.bin -Force
```

生成的 `chip8.bin` 通过计算器的应用加载器安装。在地址 `0x0` 刷写时请使用合并的全量镜像。

## 许可证

CPU 与图形解释器使用面向微控制器的 [OctEmu](https://github.com/Cycloctane/octemu) 核心，固定于提交 `e33e3250f7668c721a63a5f03d83e95cd982fa25`。其 MIT 许可证位于 [`LICENSE.OctEmu`](LICENSE.OctEmu)，引入的源码位于 `src/vendor/octemu`。

检查 ROM 兼容性时，可参考 [CHIP-8 扩展说明](https://chip-8.github.io/extensions/) 和 [Timendus CHIP-8 测试套件](https://github.com/Timendus/chip8-test-suite)。
