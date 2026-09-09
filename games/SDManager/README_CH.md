# Wi-Fi SD 卡管理器

[English](README.md)

SD Manager 会创建本地 Wi-Fi 热点和浏览器管理页面，用于管理计算器 SD 卡中的文件。

## 安装

1. 将 [`../../sci_calc_code/bin/sdmanager.bin`](../../sci_calc_code/bin/sdmanager.bin) 复制到 SD 卡根目录，保存为 `/sdmanager.bin`。
2. 将当前 [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) 复制到 SD 卡根目录，保存为 `/main.bin`。
3. 进入 `Programs > Tools > SD Manager`。

如需通过 USB 完整安装，请将 [`../../sci_calc_code/bin/sci-calc-full-1.15-min-spiffs.bin`](../../sci_calc_code/bin/sci-calc-full-1.15-min-spiffs.bin) 刷写到地址 `0x0`。

## 连接

启动后，OLED 会显示：

- Wi-Fi 名称（`SCI-CALC-XXXX`）
- 八位 Wi-Fi 密码
- 服务地址（`http://192.168.4.1`）
- 六位 `WEB CODE`
- 已连接设备数量和最近的文件操作

打开管理器：

1. 用手机或电脑连接屏幕上的 Wi-Fi。
2. 输入屏幕显示的 Wi-Fi 密码。
3. 在浏览器打开 `http://192.168.4.1`。
4. 输入计算器屏幕上的六位 `WEB CODE`。

SD Manager 每次启动都会更换两组凭据。热点最多可同时连接四台设备。

部分手机会自动打开简化的网络登录窗口。在普通浏览器中手动打开 `http://192.168.4.1` 可使用完整界面。

## 管理文件

浏览器页面支持：

- 浏览目录
- 多文件上传和拖放上传
- 下载
- 新建文件夹
- 重命名
- 删除文件和文件夹

上传或替换文件后，请等待成功提示再断电。

`/main.bin` 可以通过上传替换，并受到重命名与删除保护。请在卡中保留有效的 `/main.bin`，以便独立程序返回计算器。

将设备交给他人前，可点击 `Lock` 结束当前浏览器会话并生成新的 `WEB CODE`。连续五次输入错误后，登录会暂停 30 秒。

## 返回计算器

点击网页中的 `Return to calculator`，或长按实体 `ESC` 约一秒。SD Manager 会恢复 `/main.bin` 并重启 ESP32。

如果正在上传，需要等待上传完成后才能返回。

## 硬件与存储

| 设备 | 引脚 |
| --- | --- |
| OLED | CS GPIO5、DC GPIO16、RST GPIO17、MOSI GPIO23、SCK GPIO18 |
| SD 卡 | CS GPIO4、MOSI GPIO23、MISO GPIO19、SCK GPIO18 |
| 键盘行 | GPIO33、GPIO32、GPIO35、GPIO34、GPIO39 |
| 键盘列 | GPIO13、GPIO12、GPIO14、GPIO27、GPIO26、GPIO25 |

请使用 FAT32 格式的 SD 卡。FAT32 中的单个文件必须小于 4GB。

## 故障排查

- Wi-Fi 提示无互联网：选择继续保持连接，再手动打开 `http://192.168.4.1`。
- 浏览器离开管理页：暂时关闭 VPN 和自动切换移动数据功能。
- 登录过期：重新加载页面，输入当前 `WEB CODE`。
- 上传失败：检查 SD 卡剩余空间，简化异常路径，并尝试新格式化的 FAT32 卡。
- 无法返回：确认 `/main.bin` 存在，并等待当前上传完成。

替换 `/main.bin` 或返回主固件时，请保持计算器供电。

## 编译

```powershell
cd games\SDManager
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
Copy-Item .pio\build\sd_manager\firmware.bin ..\..\sci_calc_code\bin\sdmanager.bin -Force
```

生成的 `sdmanager.bin` 通过计算器的应用加载器安装。在地址 `0x0` 刷写时请使用合并的全量镜像。
