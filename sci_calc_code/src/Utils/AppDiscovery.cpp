#include "AppDiscovery.h"

#include <Arduino.h>
#include <FS.h>
#include <SD.h>

#include <new>
#include <cstring>
#include <string>

#include "../UIElements/BinLink.h"
#include "../UIElements/Menu.h"
#include "../UIElements/Text.h"

namespace {
constexpr char APP_DIRECTORY[] = "/apps";
constexpr size_t MAX_DISCOVERED_APPS = 16;
constexpr size_t MAX_DIRECTORY_ENTRIES = 128;
constexpr size_t MAX_MANIFEST_BYTES = 768;
constexpr size_t MAX_DISPLAY_NAME_BYTES = 24;
constexpr size_t MAX_FIRMWARE_PATH_BYTES = 80;
constexpr size_t MAX_FIRMWARE_BYTES = 0x1E0000;
constexpr uint8_t ESP_IMAGE_MAGIC = 0xE9;

const char *BUILTIN_NAMES[] = {
    "Classic Calc", "Graph Calc", "Music Player", "Music Keyboard", "SD Manager", "USB Display",
    "Maze", "Falling Keys", "Super-CHIP", "Chess", "Spacetrash", "Snake", "Tetris",
    "Catacombs of the damned", "Squario", "Wifi Rickroll"
};

const char *BUILTIN_FIRMWARE[] = {
    "/classiccalc.bin", "/graphcalc.bin", "/music.bin", "/musickeyboard.bin", "/sdmanager.bin",
    "/serialdisplay.bin", "/maze.bin", "/fallingkeys.bin", "/chip8.bin", "/chess.bin",
    "/spacetrash.bin", "/snake.bin", "/tetris.bin", "/COTD.bin", "/squario.bin",
    "/wifirickroll.bin"
};

struct AppDescriptor {
    String name;
    String firmware;
    int order = 100;
};

bool endsWithIgnoreCase(const String &value, const char *suffix) {
    const size_t suffixLength = strlen(suffix);
    if (value.length() < suffixLength) return false;
    return value.substring(value.length() - suffixLength).equalsIgnoreCase(suffix);
}

bool isManifestFileName(const String &fileName) {
    return endsWithIgnoreCase(fileName, ".app");
}

bool isValidDisplayName(const String &name) {
    if (!name.length() || name.length() > MAX_DISPLAY_NAME_BYTES) return false;
    String lower = name;
    lower.toLowerCase();
    if (lower.indexOf(".bin") >= 0) return false;
    for (size_t index = 0; index < name.length(); ++index) {
        const uint8_t value = static_cast<uint8_t>(name[index]);
        if (value < 0x20 || value > 0x7E || value == '/' || value == '\\') return false;
    }
    return true;
}

bool isValidFirmwarePath(const String &path) {
    if (path.length() < 6 || path.length() > MAX_FIRMWARE_PATH_BYTES || path[0] != '/') return false;
    if (!endsWithIgnoreCase(path, ".bin") || path.equalsIgnoreCase("/main.bin")) return false;
    if (path.indexOf("..") >= 0 || path.indexOf("//") >= 0 || path.indexOf('\\') >= 0 ||
        path.indexOf(':') >= 0) {
        return false;
    }
    for (size_t index = 0; index < path.length(); ++index) {
        const uint8_t value = static_cast<uint8_t>(path[index]);
        if (value < 0x20 || value > 0x7E) return false;
    }
    return true;
}

bool isBuiltIn(const AppDescriptor &app) {
    for (const char *name : BUILTIN_NAMES) {
        if (app.name.equalsIgnoreCase(name)) return true;
    }
    for (const char *firmware : BUILTIN_FIRMWARE) {
        if (app.firmware.equalsIgnoreCase(firmware)) return true;
    }
    return false;
}

bool firmwareLooksValid(const String &path) {
    File firmware = SD.open(path.c_str(), FILE_READ);
    if (!firmware || firmware.isDirectory()) {
        if (firmware) firmware.close();
        return false;
    }
    const size_t size = firmware.size();
    const int magic = firmware.read();
    firmware.close();
    return size >= 32 && size <= MAX_FIRMWARE_BYTES && magic == ESP_IMAGE_MAGIC;
}

bool parseEnabled(const String &value, bool &enabled) {
    if (value == "1" || value.equalsIgnoreCase("true") || value.equalsIgnoreCase("yes")) {
        enabled = true;
        return true;
    }
    if (value == "0" || value.equalsIgnoreCase("false") || value.equalsIgnoreCase("no")) {
        enabled = false;
        return true;
    }
    return false;
}

bool parseOrder(const String &value, int &order) {
    if (!value.length()) return false;
    size_t start = value[0] == '-' || value[0] == '+' ? 1 : 0;
    if (start == value.length()) return false;
    for (size_t index = start; index < value.length(); ++index) {
        if (value[index] < '0' || value[index] > '9') return false;
    }
    const long parsed = value.toInt();
    if (parsed < -1000 || parsed > 1000) return false;
    order = static_cast<int>(parsed);
    return true;
}

bool parseManifest(File &file, AppDescriptor &app) {
    if (!file || file.isDirectory() || !file.size() || file.size() > MAX_MANIFEST_BYTES) return false;
    bool validFormat = false;
    bool enabled = true;
    bool valid = true;
    bool firstLine = true;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (firstLine && line.length() >= 3 &&
            static_cast<uint8_t>(line[0]) == 0xEF &&
            static_cast<uint8_t>(line[1]) == 0xBB &&
            static_cast<uint8_t>(line[2]) == 0xBF) {
            line.remove(0, 3);
        }
        firstLine = false;
        line.trim();
        if (!line.length() || line[0] == '#' || line[0] == ';') continue;
        const int separator = line.indexOf('=');
        if (separator <= 0) {
            valid = false;
            continue;
        }
        String key = line.substring(0, separator);
        String value = line.substring(separator + 1);
        key.trim();
        key.toLowerCase();
        value.trim();

        if (key == "format") validFormat = value == "1";
        else if (key == "name") app.name = value;
        else if (key == "firmware") app.firmware = value;
        else if (key == "enabled") valid = parseEnabled(value, enabled) && valid;
        else if (key == "order") valid = parseOrder(value, app.order) && valid;
    }
    return valid && validFormat && enabled && isValidDisplayName(app.name) &&
           isValidFirmwarePath(app.firmware);
}

bool duplicatesDiscoveredApp(const AppDescriptor *apps, size_t count, const AppDescriptor &candidate) {
    for (size_t index = 0; index < count; ++index) {
        if (apps[index].name.equalsIgnoreCase(candidate.name) ||
            apps[index].firmware.equalsIgnoreCase(candidate.firmware)) {
            return true;
        }
    }
    return false;
}

void sortApps(AppDescriptor *apps, size_t count) {
    for (size_t right = 1; right < count; ++right) {
        AppDescriptor current = apps[right];
        size_t left = right;
        String currentName = current.name;
        currentName.toLowerCase();
        while (left > 0) {
            String previousName = apps[left - 1].name;
            previousName.toLowerCase();
            if (apps[left - 1].order < current.order ||
                (apps[left - 1].order == current.order && previousName.compareTo(currentName) <= 0)) {
                break;
            }
            apps[left] = apps[left - 1];
            --left;
        }
        apps[left] = current;
    }
}
}

size_t discoverSdApplications(Menu &menu) {
    static bool discoveryComplete = false;
    if (discoveryComplete) return 0;
    discoveryComplete = true;

    File directory = SD.open(APP_DIRECTORY);
    if (!directory || !directory.isDirectory()) {
        if (directory) directory.close();
        return 0;
    }

    AppDescriptor apps[MAX_DISCOVERED_APPS];
    size_t appCount = 0;
    size_t entryCount = 0;
    while (entryCount++ < MAX_DIRECTORY_ENTRIES && appCount < MAX_DISCOVERED_APPS) {
        File entry = directory.openNextFile();
        if (!entry) break;
        const String descriptorFileName = entry.name();
        if (entry.isDirectory() || !isManifestFileName(descriptorFileName)) {
            entry.close();
            continue;
        }

        AppDescriptor candidate;
        const bool parsed = parseManifest(entry, candidate);
        entry.close();
        if (!parsed || isBuiltIn(candidate) ||
            duplicatesDiscoveredApp(apps, appCount, candidate) ||
            !firmwareLooksValid(candidate.firmware)) {
            continue;
        }
        apps[appCount++] = candidate;
    }
    directory.close();
    sortApps(apps, appCount);

    size_t inserted = 0;
    for (size_t index = 0; index < appCount; ++index) {
        Text *label = new (std::nothrow) Text(std::string(apps[index].name.c_str()));
        BinLink *link = new (std::nothrow) BinLink(std::string(apps[index].firmware.c_str()));
        if (label == nullptr || link == nullptr) {
            delete label;
            delete link;
            break;
        }
        menu.appendElement(label, link);
        ++inserted;
    }
    Serial.printf("SD app manifests loaded: %u\n", static_cast<unsigned>(inserted));
    return inserted;
}
