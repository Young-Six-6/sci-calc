#include "RomLibrary.h"

#include <FS.h>
#include <SD.h>

#include <cstring>

constexpr char RomLibrary::DIRECTORY[];

namespace {
bool endsWithIgnoreCase(const String &value, const char *suffix) {
    const size_t suffixLength = strlen(suffix);
    if (value.length() < suffixLength) return false;
    return value.substring(value.length() - suffixLength).equalsIgnoreCase(suffix);
}

int compareIgnoreCase(const char *left, const char *right) {
    while (*left && *right) {
        char leftChar = *left++;
        char rightChar = *right++;
        if (leftChar >= 'A' && leftChar <= 'Z') leftChar += 'a' - 'A';
        if (rightChar >= 'A' && rightChar <= 'Z') rightChar += 'a' - 'A';
        if (leftChar != rightChar) return static_cast<uint8_t>(leftChar) - static_cast<uint8_t>(rightChar);
    }
    return static_cast<uint8_t>(*left) - static_cast<uint8_t>(*right);
}

void makeTitle(const String &path, char *destination, size_t capacity) {
    int start = path.lastIndexOf('/');
    start = start < 0 ? 0 : start + 1;
    size_t output = 0;
    for (size_t index = static_cast<size_t>(start);
         index + 4 < path.length() && output + 1 < capacity; ++index) {
        char value = path[index];
        if (value == '_') value = ' ';
        if (value < 0x20 || value > 0x7E) value = '?';
        destination[output++] = value;
    }
    while (output && destination[output - 1] == ' ') --output;
    destination[output] = '\0';
    if (!output) strlcpy(destination, "Untitled", capacity);
}
}

bool RomLibrary::scan() {
    count_ = 0;
    truncated_ = false;
    if (!SD.exists(DIRECTORY) && !SD.mkdir(DIRECTORY)) return false;

    File directory = SD.open(DIRECTORY, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        if (directory) directory.close();
        return false;
    }

    size_t scanned = 0;
    while (scanned < MAX_SCAN_ENTRIES) {
        File entry = directory.openNextFile();
        if (!entry) break;
        ++scanned;
        String path = entry.name();
        if (!path.startsWith("/")) path = String(DIRECTORY) + "/" + path;
        const bool valid = !entry.isDirectory() && endsWithIgnoreCase(path, ".ch8") &&
                           entry.size() >= 2 && entry.size() <= MAX_ROM_BYTES &&
                           path.length() < sizeof(entries_[0].path);
        const size_t size = entry.size();
        entry.close();
        if (!valid) continue;
        if (count_ == MAX_ROMS) {
            truncated_ = true;
            continue;
        }
        RomEntry &rom = entries_[count_++];
        strlcpy(rom.path, path.c_str(), sizeof(rom.path));
        makeTitle(path, rom.title, sizeof(rom.title));
        rom.size = static_cast<uint16_t>(size);
    }
    if (scanned == MAX_SCAN_ENTRIES) {
        File extra = directory.openNextFile();
        if (extra) {
            truncated_ = true;
            extra.close();
        }
    }
    directory.close();

    for (size_t right = 1; right < count_; ++right) {
        const RomEntry current = entries_[right];
        size_t left = right;
        while (left && compareIgnoreCase(entries_[left - 1].title, current.title) > 0) {
            entries_[left] = entries_[left - 1];
            --left;
        }
        entries_[left] = current;
    }
    return true;
}

const RomEntry *RomLibrary::at(size_t index) const {
    return index < count_ ? &entries_[index] : nullptr;
}
