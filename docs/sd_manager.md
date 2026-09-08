# Wi-Fi SD Manager
# Wi-Fi SD 卡管理器

The standalone manager is built from games/SDManager and published as sci_calc_code/bin/sdmanager.bin.

独立管理器由 games/SDManager 构建，并发布为 sci_calc_code/bin/sdmanager.bin。

Copy both the version 1.13 main.bin and sdmanager.bin from sci_calc_code/bin to the SD-card root before opening it from Programs.

从 Programs 菜单打开管理器之前，请把 sci_calc_code/bin 中 1.13 版 main.bin 与 sdmanager.bin 一同复制到 SD 卡根目录。

The merged sci-calc-full-1.13-min-spiffs.bin image is available for a complete USB reinstall at flash address 0x0.

合并镜像 sci-calc-full-1.13-min-spiffs.bin 可用于通过 USB 在 Flash 地址 0x0 完整重装。

It provides an offline browser interface at http://192.168.4.1 over the calculator's own WPA2 access point.

它通过计算器自身的 WPA2 热点，在 http://192.168.4.1 提供离线浏览器界面。

The OLED displays the unique SSID, per-start eight-character hotspot password, service address, connected-client count, latest operation, and an independent six-digit WEB CODE.

OLED 会显示唯一 SSID、每次启动生成的八字符热点密码、服务地址、已连接客户端数量、最近一次操作，以及独立的六位 WEB CODE。

Files are uploaded and downloaded as streams, keeping memory use bounded on the no-PSRAM ESP32.

文件采用流式上传与下载，使无 PSRAM ESP32 的内存占用保持有界。

All file endpoints remain locked until the six-digit physical-access code is exchanged for a random 128-bit session token.

只有使用设备屏幕上的六码换取随机 128 位会话令牌后，所有文件接口才会解锁。

Five failed entries trigger a 30-second login pause, and the Lock command revokes every session and rotates both credentials.

连续五次输入错误会暂停登录 30 秒，Lock 命令会撤销所有会话并轮换六码与会话令牌。

Uploads use temporary and backup files so an interrupted replacement normally leaves the previous file available.

上传使用临时文件和备份文件，因此替换中断时通常仍能保留旧文件。

The manager protects /main.bin from delete and rename operations while still allowing it to be replaced by a successful upload.

管理器禁止删除和重命名 /main.bin，但仍允许通过成功上传来替换它。

Holding ESC or pressing the web return button installs /main.bin and restarts the calculator.

长按 ESC 或点击网页返回按钮会安装 /main.bin 并重新启动计算器。

For complete operating and build instructions, see games/SDManager/README.md.

完整操作与编译说明请参阅 games/SDManager/README.md。
