# Updating Software

> **Important for this repository:** The current fork uses a customized `min_spiffs` partition layout. Every device must first receive the merged full image through USB at address `0x0`, including devices coming from upstream or an older fork version. The SD-card procedures below are only for routine updates after that installation; they cannot migrate an old partition table.
>
> **重要：**当前分支使用定制的 `min_spiffs` 分区布局。所有设备，包括从上游项目或本分支旧版本迁移的设备，都必须先通过 USB 将合并完整固件刷写到 `0x0` 地址。下面的 SD 卡流程只适用于完成该安装后的日常更新，不能迁移旧分区表。

For the required first installation, follow the [complete USB flash instructions in the root README](../README.md#required-complete-usb-flash).

首次安装请先按照根目录 [README 中的 USB 完整刷写说明](../README_CH.md#必须进行的-usb-完整刷写) 操作。

## How It Works

The SCI-CALC can load external programs through a SD card.
Each time you load a program, the SCI-CALC writes the contents of the corresponding ```.bin``` file to the ESP32's ROM. For instance, when you load up Chess, it overwrites the existing ```main.bin``` in the ROM to ```chess.bin```. When you press esc to go back, The ```main.bin``` from the SD-card is written back into the ROM. This allows us to update programs without having to use an IDE to upload. The idea is to update the ```.bin``` file of the program in the SD-card, and then load it to the ESP32.

## Updating Software After Initial Installation
To update the software for your sci-calc, follow these steps:
1. Get the .bin file you want to update from the ```/bin``` folder.
2. Copy that file over to the microSD card, replacing the old .bin file you want to update.
3. Navigate to the settings menu and activate Update from SD.
4. After it is done writing to ROM it should be properly updated!

## Updating Software on Older Compatible Main Firmware
If you do not see the Update From SD option in the settings menu, follow these steps:
1. Get the .bin file you want to update from the ```/bin``` folder from this repo.
2. Copy that file over to the microSD card, replacing the old .bin file you want to update.
3. Navigate to the programs menu and enter any one of them.
4. once the game is loaded press esc to load the updated main.bin file back to the ESP32.
5. After it is done writing to ROM it should be properly updated!

For updating the main software, do these steps with the ```main.bin``` file. Or you can just replace everything in the SD-card with the contents in the ```/bin``` folder.
