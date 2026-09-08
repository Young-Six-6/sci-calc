# SD Application Discovery
# SD 应用自动发现

Main firmware version 1.13 can add standalone applications to Programs at startup without recompiling the main menu.

主固件 1.13 版可在启动时将独立应用加入 Programs，无需重新编译主菜单。

Discovery is manifest-only: the firmware reads files ending in .app inside /apps and never adds a raw .bin file to the menu.

自动发现只认清单：固件仅读取 /apps 中以 .app 结尾的文件，绝不会把裸 .bin 文件加入菜单。

The visible label comes exclusively from name= and is never copied or inferred from either the .app filename or the .bin filename.

屏幕上的名称只能来自 name=，绝不会复制或推断 .app 文件名或 .bin 文件名。

## Installation
## 安装

Create an /apps directory in the root of the FAT32 SD card.

在 FAT32 SD 卡根目录创建 /apps 目录。

Copy the standalone firmware to the SD card, for example /tools/unitconv.bin.

将独立应用固件复制到 SD 卡，例如 /tools/unitconv.bin。

Create a descriptor such as /apps/unit-converter.app with the following contents.

创建一个如 /apps/unit-converter.app 的描述文件，内容如下。

```ini
format=1
name=Unit Converter
firmware=/tools/unitconv.bin
order=100
enabled=true
```

Restart the calculator and open Programs; Unit Converter appears after the built-in entries.

重启计算器并打开 Programs；Unit Converter 会出现在内置项目之后。

Removing or disabling the descriptor removes the menu item after the next restart without deleting the application firmware.

删除清单或禁用它后，菜单项会在下次重启时消失，应用固件仍会保留。

## Manifest fields
## 清单字段

format=1 is required and identifies the current descriptor format.

format=1 为必填项，用于标识当前描述格式。

name is required, must contain 1 to 24 printable ASCII characters, and must not contain .bin, /, or a backslash.

name 为必填项，必须包含 1 至 24 个可显示 ASCII 字符，且不得包含 .bin、/ 或反斜杠。

firmware is required and must be an absolute SD path ending in .bin.

firmware 为必填项，必须是以 .bin 结尾的 SD 卡绝对路径。

The firmware path is internal metadata and is never rendered as a menu label.

固件路径只是内部元数据，永远不会被绘制为菜单名称。

order is optional, defaults to 100, accepts -1000 through 1000, and sorts discovered applications from lower to higher values.

order 为可选项，默认值为 100，可取 -1000 至 1000，自动发现的应用会按数值由小到大排序。

enabled is optional, defaults to true, and accepts true/false, yes/no, or 1/0.

enabled 为可选项，默认为 true，支持 true/false、yes/no 或 1/0。

Lines beginning with # or ; are comments, and unknown keys are ignored for forward compatibility.

以 # 或 ; 开头的行是注释，未知字段会被忽略以便以后扩展。

Both ordinary UTF-8 text and Windows UTF-8 text with a byte-order mark are accepted.

普通 UTF-8 文本和带字节顺序标记的 Windows UTF-8 文本均可使用。

## Validation and limits
## 校验与限制

An entry is shown only when its descriptor is valid and its target exists, is 32 bytes to 1,966,080 bytes long, and starts with the ESP32 image byte 0xE9.

只有在清单有效，且目标固件存在、大小为 32 至 1,966,080 字节、并以 ESP32 镜像字节 0xE9 开头时，菜单才会显示该项。

/main.bin, parent-directory paths, malformed paths, duplicate names, duplicate targets, and built-in applications are rejected.

/main.bin、父目录路径、错误路径、重复名称、重复目标和内置应用都会被拒绝。

At most 16 discovered applications are added, at most the first 128 /apps entries are inspected, and each descriptor may be at most 768 bytes.

最多可添加 16 个自动发现的应用，最多检查 /apps 中前 128 个项目，每个清单最大为 768 字节。

Invalid entries are omitted rather than shown under a guessed filename, and the serial log reports only the number successfully loaded.

无效项会被直接忽略，不会用猜测的文件名显示；串口日志只会报告成功加载的数量。

The scan runs once per boot, so restart after changing a descriptor or application file.

扫描每次启动只执行一次，修改清单或应用文件后请重启。
