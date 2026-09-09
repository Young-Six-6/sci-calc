# SCI-CALC Super-CHIP

Super-CHIP is a standalone CHIP-8 and SUPER-CHIP emulator for the SCI-CALC ESP32 WROOM-32E.

Super-CHIP 是面向 SCI-CALC ESP32 WROOM-32E 的独立 CHIP-8 与 SUPER-CHIP 模拟器。

It uses the calculator keyboard as a complete hexadecimal keypad and loads games only from the SD card.

它把计算器键盘用作完整的十六进制键盘，并且只从 SD 卡加载游戏。

The emulator firmware is installed as `/chip8.bin`, while game ROMs remain ordinary `.ch8` files and are never bundled into the firmware.

模拟器固件以 `/chip8.bin` 安装，而游戏 ROM 始终是普通 `.ch8` 文件，绝不会打包进固件。

## SD Card Layout

## SD 卡布局

Create the following layout on a FAT32 SD card.

请在 FAT32 SD 卡中建立以下结构。

```text
/
|-- main.bin
|-- chip8.bin
`-- chip8/
    |-- game_one.ch8
    `-- game_two.ch8
```

The firmware creates `/chip8` automatically if the directory is missing.

如果 `/chip8` 目录不存在，固件会自动创建它。

Only direct `.ch8` files inside `/chip8` are listed; subdirectories and every other extension are ignored.

程序只列出 `/chip8` 内第一层的 `.ch8` 文件；子目录和其他扩展名都会被忽略。

The browser can index up to 64 valid ROMs, scans at most 256 directory entries, and accepts ROMs from 2 through 3584 bytes.

浏览器最多索引 64 个有效 ROM，最多扫描 256 个目录项，并接受 2 至 3584 字节的 ROM。

No commercial or third-party game ROM is included in this repository.

本仓库不附带任何商业或第三方游戏 ROM。

## ROM Browser

## ROM 浏览器

Press `8` or `2` to move through the ROM list, then press `Enter` or `5` to start the selected game.

按 `8` 或 `2` 浏览 ROM 列表，再按 `Enter` 或 `5` 启动所选游戏。

Press `4` or `6` to select the previous or next compatibility profile.

按 `4` 或 `6` 选择上一个或下一个兼容配置。

Press `-` or `+` to reduce or increase the number of CPU cycles executed in every 60 Hz frame.

按 `-` 或 `+` 减少或增加每个 60 Hz 帧执行的 CPU 周期数。

Press `.` to enable or disable sound, and press `Backspace` to rescan the SD directory.

按 `.` 开关声音，按 `Backspace` 重新扫描 SD 目录。

Hold `ESC` for one second to reinstall `/main.bin` and return to the calculator.

长按 `ESC` 一秒会重新安装 `/main.bin` 并返回计算器。

## Hexadecimal Keypad

## 十六进制键盘

The printed number keys keep their hexadecimal values, and the six operator keys provide A through F.

印刷的数字键保持其十六进制数值，六个运算符键分别提供 A 至 F。

| Calculator key / 计算器按键 | CHIP-8 key / CHIP-8 键 |
|---|---|
| `0` through `9` / `0` 至 `9` | `0` through `9` / `0` 至 `9` |
| `/` | `A` |
| `*` | `B` |
| `-` | `C` |
| `+` | `D` |
| `^` | `E` |
| `.` | `F` |

This direct mapping makes ROM instructions such as “press 5” or “use A and F” match the printed calculator keys.

这种直接映射让 ROM 中“按 5”或“使用 A 和 F”之类的说明与计算器印刷按键一致。

## Running Controls

## 运行控制

A short press of `Enter` or `ESC` opens the pause menu without sending a virtual keypad key to the game.

短按 `Enter` 或 `ESC` 会打开暂停菜单，并且不会向游戏发送虚拟键值。

Hold `Shift` before pressing a shortcut key so that the shortcut cannot leak into the emulated keypad.

请先按住 `Shift` 再按快捷键，这样快捷键不会泄漏到模拟键盘中。

| Shortcut / 快捷键 | Action / 功能 |
|---|---|
| `Shift+4`, `Shift+6` | Lower or raise speed / 降低或提高速度 |
| `Shift+7`, `Shift+9` | Previous or next profile and restart / 上一个或下一个配置并重启游戏 |
| `Shift+0` | Toggle the side HUD / 开关两侧状态栏 |
| `Shift+.` | Toggle sound / 开关声音 |
| `Shift+Enter` | Restart the current ROM / 重启当前 ROM |
| `Shift+Backspace` | Return to the ROM browser / 返回 ROM 浏览器 |
| `Shift+5` | Open the pause menu / 打开暂停菜单 |

The pause menu provides resume, restart, ROM library, compatibility mode, speed, sound, and HUD controls.

暂停菜单提供继续、重启、ROM 库、兼容模式、速度、声音与状态栏控制。

Use `8` and `2` to select an item, `4` and `6` to change a setting, and `Enter` to activate it.

使用 `8` 与 `2` 选择项目，使用 `4` 与 `6` 修改设置，并用 `Enter` 执行。

## Compatibility Profiles

## 兼容配置

`MODERN` is the default Octo-style profile and is a useful first choice for recent CHIP-8 software.

`MODERN` 是默认的 Octo 风格配置，适合作为较新 CHIP-8 软件的首选。

`SUPER` enables the SUPER-CHIP instruction behavior and quirks expected by many SCHIP programs.

`SUPER` 启用许多 SCHIP 程序所需的 SUPER-CHIP 指令行为与兼容特性。

`CLASSIC` selects original CHIP-8 behavior, including its draw synchronization and older instruction quirks.

`CLASSIC` 选择原始 CHIP-8 行为，包括绘图同步和较旧的指令兼容特性。

Changing the profile while a ROM is loaded restarts that ROM because compatibility rules affect CPU state from the first instruction.

ROM 已加载时切换配置会重启该 ROM，因为兼容规则从第一条指令起就会影响 CPU 状态。

Speed is measured in emulated instructions per 60 Hz frame, and it can be changed without restarting the game.

速度以每个 60 Hz 帧执行的模拟指令数表示，修改速度不需要重启游戏。

The default is 200 cycles per frame for `MODERN` and `SUPER`, and 15 cycles per frame for `CLASSIC`.

`MODERN` 与 `SUPER` 默认每帧 200 个周期，`CLASSIC` 默认每帧 15 个周期。

If a game runs too quickly, too slowly, or misses input, adjust speed before changing compatibility profiles.

如果游戏运行过快、过慢或漏掉输入，请先调整速度，再尝试更换兼容配置。

## Display And Sound

## 显示与声音

SUPER-CHIP 128 by 64 output is drawn pixel-for-pixel in the center of the 256 by 64 OLED.

SUPER-CHIP 的 128×64 输出会逐像素绘制在 256×64 OLED 中央。

Standard CHIP-8 64 by 32 output is expanded by an exact two-times nearest-neighbor scale to the same 128 by 64 viewport.

标准 CHIP-8 的 64×32 输出会以精确的两倍最近邻方式放大到同一个 128×64 视口。

The optional side HUD shows profile, speed, sound state, ROM name, and return hints without covering game pixels.

可选的两侧状态栏会显示配置、速度、声音状态、ROM 名称和返回提示，不会遮挡游戏像素。

The CHIP-8 sound timer produces a low-volume square wave through the existing PCM5102A wiring.

CHIP-8 声音定时器会通过现有 PCM5102A 接线输出低音量方波。

Audio uses BCK GPIO21, LRCK GPIO22, and DIN GPIO15, and it runs in a small task on the other ESP32 core.

音频使用 BCK GPIO21、LRCK GPIO22 与 DIN GPIO15，并在 ESP32 的另一个核心上运行一个小任务。

The emulator remains usable when no PCM5102A module is attached.

即使没有连接 PCM5102A 模块，模拟器仍可正常使用。

## Build And Install

## 构建与安装

Build the standalone application with PlatformIO from this directory.

请在此目录中使用 PlatformIO 构建独立应用。

```powershell
pio run
```

The resulting application image is `.pio/build/superchip/firmware.bin`.

生成的应用镜像位于 `.pio/build/superchip/firmware.bin`。

For normal SD installation, copy the released `sci_calc_code/bin/chip8.bin` and version 1.15 `sci_calc_code/bin/main.bin` to the SD-card root.

正常通过 SD 安装时，请把发布的 `sci_calc_code/bin/chip8.bin` 与 1.15 版 `sci_calc_code/bin/main.bin` 放到 SD 卡根目录。

Open `Programs > Games`, select `Super-CHIP`, and allow the calculator to install the standalone image.

进入 `Programs > Games`，选择 `Super-CHIP`，然后让计算器安装独立镜像。

For a complete USB reinstall, flash `sci_calc_code/bin/sci-calc-full-1.15-min-spiffs.bin` at address `0x0`.

如需通过 USB 完整重装，请把 `sci_calc_code/bin/sci-calc-full-1.15-min-spiffs.bin` 刷写到地址 `0x0`。

## Core And License

## 核心与许可证

The CPU and graphics interpreter is the microcontroller-oriented [OctEmu](https://github.com/Cycloctane/octemu) core at commit `e33e3250f7668c721a63a5f03d83e95cd982fa25`.

CPU 与图形解释器采用面向微控制器的 OctEmu 核心，固定于提交 `e33e3250f7668c721a63a5f03d83e95cd982fa25`。

The vendored core is preserved under `src/vendor/octemu`, and its MIT license is stored in `LICENSE.OctEmu`.

引入的核心原样保存在 `src/vendor/octemu`，其 MIT 许可证保存在 `LICENSE.OctEmu`。

Compatibility behavior follows the established CHIP-8 and SUPER-CHIP extension families rather than a new instruction implementation written for this project.

兼容行为遵循成熟的 CHIP-8 与 SUPER-CHIP 扩展体系，而不是为本项目重新手写一套指令实现。

The [CHIP-8 extensions reference](https://chip-8.github.io/extensions/) documents the instruction families and historical compatibility differences.

[CHIP-8 扩展参考](https://chip-8.github.io/extensions/)记录了这些指令族与历史兼容差异。

The open [Timendus CHIP-8 test suite](https://github.com/Timendus/chip8-test-suite) is the recommended diagnostic ROM set when checking a new hardware build.

在检查新硬件时，建议使用开源的 [Timendus CHIP-8 测试套件](https://github.com/Timendus/chip8-test-suite)作为诊断 ROM 集。

The source contract tests pin the imported core hashes, all sixteen key bindings, ROM limits, display geometry, and required SUPER-CHIP opcode families.

源码契约测试会固定引入核心的哈希、全部十六个按键映射、ROM 限制、显示几何和必要的 SUPER-CHIP 指令族。
