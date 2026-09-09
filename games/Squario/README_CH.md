# Squario

[English](README.md)

Squario 是移植到 SCI-CALC 键盘和屏幕的横版平台游戏。控制角色奔跑、跳跃、收集金币和道具，并通过逐渐变长的关卡。

## 按键

### 标题页

| 按键 | 功能 |
| --- | --- |
| `8` / `2` | 增加或减少世界种子 |
| `TAN` 或 `COT` | 开始游戏 |
| `ESC` | 恢复 `/main.bin` 并返回计算器 |

### 游戏中

| 按键 | 功能 |
| --- | --- |
| `4` / `6` | 向左或向右移动 |
| `8` 或 `COT` | 跳跃 |
| `TAN` | 奔跑 |
| `2` | 在关卡允许时向下移动 |
| `ESC` | 恢复 `/main.bin` 并返回计算器 |

蘑菇会让 Squario 变大，已变大时可以补充状态。每收集 20 枚金币会获得一条额外生命。关卡后半段可能出现星星和闪电道具，后续关卡会更长，并包含更多地下区域。

## 安装

1. 将 [`../../sci_calc_code/bin/squario.bin`](../../sci_calc_code/bin/squario.bin) 复制到 SD 卡根目录，保存为 `/squario.bin`。
2. 将当前 [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) 复制到 SD 卡根目录，保存为 `/main.bin`。
3. 进入 `Programs > Games > Squario`。

游戏使用计算器现有的 SD 卡、屏幕和 5x6 键盘接线。Arduino 工程入口为 [`Squario.ino`](Squario.ino)。
