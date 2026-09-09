# 图形计算器

[English](README.md)

图形计算器可在 SCI-CALC 256x64 OLED 上同时绘制最多四个函数，并提供表达式编辑、平移缩放、可选网格与坐标刻度，以及追踪光标。

## 安装

1. 将 [`../../sci_calc_code/bin/graphcalc.bin`](../../sci_calc_code/bin/graphcalc.bin) 复制到 SD 卡根目录，保存为 `/graphcalc.bin`。
2. 将当前 [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) 复制到 SD 卡根目录，保存为 `/main.bin`。
3. 进入 `Programs > Calculation > Graph Calc`。

长按 `ESC` 约一秒可恢复 `/main.bin` 并返回计算器。

## 输入函数

编辑器启动时会显示一个空函数槽。输入 `x^2-9` 之类的表达式，再按 `Enter` 或 `TAB` 打开图像页。

最多可同时绘制四个非空函数：

| 按键 | 功能 |
| --- | --- |
| `RPN/NORM` | 选择下一个函数槽 |
| `Shift+RPN/NORM` | 新增函数槽，最多四个 |
| `Shift+8` / `Shift+2` | 选择上一个或下一个函数槽 |
| `Shift+退格` | 删除选中槽；只剩一个时会将它清空 |
| `Shift+5` | 输入 `x` |
| `Shift+/` / `Shift+*` | 输入 `(` 或 `)` |
| `Shift+4` / `Shift+6` | 左右移动文本光标 |
| `SIN`、`COS`、`TAN` | 输入键帽上的函数 |
| `Shift+SIN`、`Shift+COS`、`Shift+TAN` | 输入 `asin(`、`acos(` 或 `atan(` |
| `SEC`、`CSC`、`COT` | 输入 `sec(`、`csc(` 或 `cot(` |
| `Shift+SEC`、`Shift+CSC`、`Shift+COT` | 输入 `abs(`、`log(` 或 `pi` |
| `EXP` / `Shift+EXP` | 输入 `exp(` 或 `ln(` |
| `^` / `Shift+^` | 输入幂或 `sqrt(` |
| `Shift+9` | 输入 `e` |
| `ANG` | 切换弧度与角度 |

按住退格键可连续删除。如果表达式有语法错误，程序会返回对应的函数槽以便修改。

## 查看图像

| 按键 | 功能 |
| --- | --- |
| `Shift+4` / `Shift+6` | 向左或向右平移 |
| `Shift+8` / `Shift+2` | 向上或向下平移 |
| `+` / `-` | 放大或缩小 |
| `5` | 重置视图 |
| `Enter` | 开关追踪模式 |
| `4` / `6` | 移动追踪光标 |
| `8` / `2` | 选择上一个或下一个追踪函数 |
| `.` | 开关网格 |
| `RPN/NORM` | 开关坐标轴小字刻度 |
| `ANG` | 切换弧度与角度 |
| `TAB` | 返回编辑页 |

F1 使用实线，F2 使用点线，F3 和 F4 使用不同的虚线样式。空函数槽会被跳过。

## 表达式

支持以下语法：

- 运算符：`+ - * / ^`
- 一元 `+` 和 `-`
- 括号和小数
- 变量与常量：`x`、`pi`、`e`
- 隐式乘法，例如 `2x` 或 `3(x+1)`
- 函数：`sin`、`cos`、`tan`、`sec`、`csc`、`cot`、`asin`、`acos`、`atan`、`sqrt`、`ln`、`log`、`exp`、`abs`

绘图会在曲线、转折点和快速变化区域自适应增加采样。无定义或不连续区域会保持断开，避免绘制误导性连线。

## 硬件

| 设备 | 引脚 |
| --- | --- |
| OLED | CS GPIO5、DC GPIO16、RST GPIO17、MOSI GPIO23、SCK GPIO18 |
| SD 卡 | CS GPIO4、MOSI GPIO23、MISO GPIO19、SCK GPIO18 |
| 键盘行 | GPIO33、GPIO32、GPIO35、GPIO34、GPIO39 |
| 键盘列 | GPIO13、GPIO12、GPIO14、GPIO27、GPIO26、GPIO25 |

## 编译

```powershell
cd games\GraphCalculator
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
Copy-Item .pio\build\graph_calculator\firmware.bin ..\..\sci_calc_code\bin\graphcalc.bin -Force
```

生成的 `graphcalc.bin` 通过计算器的应用加载器安装。在地址 `0x0` 刷写时请使用合并的全量镜像。
