# SD Manager
# SD 卡管理器

SD Manager is a standalone Wi-Fi file-management firmware for the SCI-CALC SD card.

SD Manager 是用于管理 SCI-CALC SD 卡文件的独立 Wi-Fi 固件。

It creates its own access point, so a router, mobile application, FTP client, and Internet connection are not required.

它会自行创建无线热点，因此不需要路由器、手机应用、FTP 客户端或互联网连接。

## Installation
## 安装方法

Copy sci_calc_code/bin/main.bin and sci_calc_code/bin/sdmanager.bin to the root of the calculator's SD card.

将 sci_calc_code/bin/main.bin 与 sci_calc_code/bin/sdmanager.bin 复制到计算器 SD 卡根目录。

Replace the old main.bin with version 1.13 so the displayed version and program menu remain in sync.

请用 1.13 版替换旧的 main.bin，使显示版本与程序菜单保持一致。

The existing Update from SD command can install the new main.bin without a USB cable.

可以使用现有的 Update from SD 命令安装新版 main.bin，无需连接 USB 数据线。

For a complete USB reinstall, flash sci_calc_code/bin/sci-calc-full-1.13-min-spiffs.bin at address 0x0.

如果需要通过 USB 完整重装，请将 sci_calc_code/bin/sci-calc-full-1.13-min-spiffs.bin 刷写到地址 0x0。

Do not flash main.bin or sdmanager.bin at address 0x0 because they are application-slot images.

不要把 main.bin 或 sdmanager.bin 刷写到地址 0x0，因为它们是应用分区镜像。

## Connection
## 连接方法

Open SD Manager from the calculator's Programs menu.

在计算器的 Programs 菜单中打开 SD Manager。

Connect a phone or computer to the SCI-CALC-XXXX Wi-Fi network shown on the OLED.

用手机或电脑连接 OLED 所显示的 SCI-CALC-XXXX Wi-Fi。

Enter the eight-character Wi-Fi password shown on the OLED.

输入 OLED 上显示的八字符 Wi-Fi 密码。

The Wi-Fi password is regenerated on every start, independently of the six-digit WEB CODE.

Wi-Fi 密码每次启动都会重新生成，并且与六位 WEB CODE 相互独立。

Open http://192.168.4.1 in a browser.

在浏览器中打开 http://192.168.4.1。

Enter the six-digit WEB CODE shown on the OLED.

输入 OLED 上显示的六位 WEB CODE。

The WEB CODE is regenerated on every start and is never returned by an unauthenticated web request.

WEB CODE 每次启动都会重新生成，并且不会通过未认证的网页请求返回。

Some phones open the page automatically as a captive portal; opening the address manually provides the full browser interface.

部分手机会将页面自动显示为认证门户；手动打开上述地址可以使用完整浏览器界面。

The access point accepts at most four clients at once.

热点最多允许四台设备同时连接。

## File operations
## 文件操作

The web page supports folder navigation, multiple-file upload, drag-and-drop upload, download, folder creation, rename, and recursive deletion.

网页支持目录浏览、多文件上传、拖放上传、下载、新建文件夹、重命名和递归删除目录。

Uploads stream directly to the SD card instead of loading a complete file into ESP32 memory.

上传数据会直接流式写入 SD 卡，不会把完整文件载入 ESP32 内存。

Each upload is written to a .sdm.part file first.

每次上传都会先写入 .sdm.part 临时文件。

When replacing an existing file, the old copy is temporarily renamed to .sdm.bak and restored if finalization fails.

替换已有文件时，旧文件会临时改名为 .sdm.bak；如果最终替换失败，则恢复旧文件。

Temporary upload and backup files are hidden from the browser list.

上传临时文件和备份文件不会显示在浏览器列表中。

/main.bin can be replaced by a completed upload, but it cannot be deleted or renamed through the web interface.

网页允许通过完整上传替换 /main.bin，但不允许删除或重命名它。

This protection preserves the calculator's return path.

该保护用于保留返回计算器主固件的路径。

Every status, directory, download, and write endpoint requires an authenticated browser session.

状态、目录、下载和写入等所有接口都要求浏览器会话已经通过认证。

A successful code entry is exchanged for a random 128-bit session token, which is not displayed in the address bar.

六码验证成功后会换取随机的 128 位会话令牌，该令牌不会显示在浏览器地址栏中。

Five incorrect code attempts pause further login attempts for 30 seconds.

连续五次输入错误会暂停登录 30 秒。

The Lock button revokes all active browser sessions and immediately generates a new WEB CODE on the OLED.

Lock 按钮会撤销所有浏览器会话，并立即在 OLED 上生成新的 WEB CODE。

## Returning to the calculator
## 返回计算器

Use Return to calculator on the web page, or hold the physical ESC key for about one second.

可以点击网页上的 Return to calculator，或长按实体 ESC 键约一秒。

The manager installs /main.bin through the existing OTA application slot and then restarts.

管理器会通过现有 OTA 应用槽安装 /main.bin，随后重新启动。

An active upload must finish before either return method is accepted.

上传进行期间，两种返回方式都会等待上传完成后才允许执行。

## Hardware and storage
## 硬件与存储

OLED uses CS GPIO5, DC GPIO16, reset GPIO17, MOSI GPIO23, and clock GPIO18.

OLED 使用 CS GPIO5、DC GPIO16、复位 GPIO17、MOSI GPIO23 和时钟 GPIO18。

The SD card shares MOSI GPIO23 and clock GPIO18, and additionally uses MISO GPIO19 and CS GPIO4.

SD 卡与 OLED 共用 MOSI GPIO23 和时钟 GPIO18，并使用 MISO GPIO19 与 CS GPIO4。

The physical keyboard keeps the project's existing five-row by six-column GPIO matrix.

实体键盘继续使用项目现有的五行六列 GPIO 矩阵。

Use a FAT32-formatted SD card.

请使用 FAT32 格式的 SD 卡。

A single FAT32 file must be smaller than 4 GB.

FAT32 中的单个文件必须小于 4 GB。

## Troubleshooting
## 故障排查

If a phone reports that this Wi-Fi has no Internet, choose to remain connected and open the address manually.

如果手机提示此 Wi-Fi 无互联网，请选择仍然保持连接，并手动打开访问地址。

Temporarily disable a VPN or automatic mobile-data switching if the page is redirected away from 192.168.4.1.

如果页面被跳转而无法访问 192.168.4.1，请暂时关闭 VPN 或自动切换移动数据功能。

If the session expires, reload the page and enter the current WEB CODE again.

如果会话失效，请重新载入页面并再次输入当前的 WEB CODE。

Keep the calculator powered while replacing main.bin or while returning to the calculator firmware.

替换 main.bin 或返回计算器主固件时，请保持计算器持续供电。

## Build
## 编译

    cd C:\Users\wang\Documents\camera\sci-calcme\games\SDManager
    & "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
    Copy-Item .pio\build\sd_manager\firmware.bin ..\..\sci_calc_code\bin\sdmanager.bin -Force

sdmanager.bin is an OTA application image and must not be written at flash offset 0x0.

sdmanager.bin 是 OTA 应用镜像，不能写入 Flash 地址 0x0。
