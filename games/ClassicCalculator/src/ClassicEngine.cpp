#include "ClassicEngine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr double MAX_RESULT = 9.99999999999999e99;
constexpr double ZERO_CUTOFF = 1e-14;

void copyText(char *destination, size_t size, const char *source) {
    if (!destination || !size) return;
    std::snprintf(destination, size, "%s", source ? source : "");
}

void trimFixed(char *text) {
    char *exponent = std::strchr(text, 'E');
    char *end = exponent ? exponent - 1 : text + std::strlen(text) - 1;
    char *dot = std::strchr(text, '.');
    if (!dot || dot > end) return;
    while (end > dot && *end == '0') *end-- = '\0';
    if (end == dot) *end = '\0';
    if (exponent) {
        const size_t prefixLength = std::strlen(text);
        std::memmove(text + prefixLength, exponent, std::strlen(exponent) + 1);
    }
}

void normalizeExponent(char *text) {
    char *exponent = std::strchr(text, 'e');
    if (!exponent) exponent = std::strchr(text, 'E');
    if (!exponent) return;
    *exponent = 'E';
    char *read = exponent + 1;
    char sign = '\0';
    if (*read == '+' || *read == '-') sign = *read++;
    while (*read == '0' && read[1]) ++read;
    char *write = exponent + 1;
    if (sign == '-') *write++ = '-';
    std::memmove(write, read, std::strlen(read) + 1);
}

void groupNumberText(const char *source, char *output, size_t size) {
    if (!output || !size) return;
    output[0] = '\0';
    if (!source || !*source || std::strchr(source, 'E') || std::strchr(source, 'e')) {
        copyText(output, size, source);
        return;
    }

    const char *integer = source;
    const bool negative = *integer == '-';
    if (negative) ++integer;
    const char *dot = std::strchr(integer, '.');
    const size_t integerLength = dot ? static_cast<size_t>(dot - integer) : std::strlen(integer);
    size_t position = 0;
    if (negative && position + 1 < size) output[position++] = '-';
    for (size_t index = 0; index < integerLength && position + 1 < size; ++index) {
        if (index && (integerLength - index) % 3 == 0 && position + 1 < size) output[position++] = ',';
        if (position + 1 < size) output[position++] = integer[index];
    }
    if (dot) {
        for (const char *cursor = dot; *cursor && position + 1 < size; ++cursor) output[position++] = *cursor;
    }
    output[position] = '\0';
}
}

ClassicEngine::ClassicEngine() {
    allClear();
}

void ClassicEngine::invalidateUndo() {
    undo_.valid = false;
    undo_.overwritten = false;
}

void ClassicEngine::allClear() {
    copyText(entry_, sizeof(entry_), "0");
    accumulator_ = 0.0;
    repeatOperand_ = 0.0;
    pending_ = CalcOperation::None;
    repeat_ = CalcOperation::None;
    entering_ = true;
    waitingForOperand_ = false;
    resultShown_ = false;
    error_ = false;
    errorText_[0] = '\0';
    invalidateUndo();
}

void ClassicEngine::beginFreshEntry() {
    if (error_ || (resultShown_ && pending_ == CalcOperation::None)) allClear();
    if (waitingForOperand_ || !entering_) {
        copyText(entry_, sizeof(entry_), "0");
        waitingForOperand_ = false;
        entering_ = true;
    }
    repeat_ = CalcOperation::None;
    invalidateUndo();
}

uint8_t ClassicEngine::entryDigitCount() const {
    uint8_t count = 0;
    for (const char *cursor = entry_; *cursor; ++cursor) {
        if (*cursor >= '0' && *cursor <= '9') ++count;
    }
    return count;
}

void ClassicEngine::inputDigit(uint8_t digit) {
    if (digit > 9) return;
    beginFreshEntry();
    const bool negativeZero = std::strcmp(entry_, "-0") == 0;
    if (std::strcmp(entry_, "0") == 0 || negativeZero) {
        if (digit == 0) return;
        if (negativeZero) std::snprintf(entry_, sizeof(entry_), "-%u", digit);
        else std::snprintf(entry_, sizeof(entry_), "%u", digit);
        return;
    }
    if (entryDigitCount() >= MAX_ENTRY_DIGITS) return;
    const size_t length = std::strlen(entry_);
    if (length + 1 < sizeof(entry_)) {
        entry_[length] = static_cast<char>('0' + digit);
        entry_[length + 1] = '\0';
    }
}

void ClassicEngine::inputDoubleZero() {
    beginFreshEntry();
    if (std::strcmp(entry_, "0") == 0 || std::strcmp(entry_, "-0") == 0) return;
    inputDigit(0);
    inputDigit(0);
}

void ClassicEngine::inputDecimal() {
    beginFreshEntry();
    if (std::strchr(entry_, '.')) return;
    const size_t length = std::strlen(entry_);
    if (length + 1 < sizeof(entry_)) {
        entry_[length] = '.';
        entry_[length + 1] = '\0';
    }
}

void ClassicEngine::backspace() {
    if (error_) {
        allClear();
        return;
    }
    if (!entering_ || waitingForOperand_) return;
    invalidateUndo();
    size_t length = std::strlen(entry_);
    if (!length) {
        copyText(entry_, sizeof(entry_), "0");
        return;
    }
    entry_[--length] = '\0';
    if (!length || std::strcmp(entry_, "-") == 0) copyText(entry_, sizeof(entry_), "0");
}

void ClassicEngine::toggleSign() {
    if (error_) return;
    if (waitingForOperand_ || (!entering_ && !resultShown_)) {
        copyText(entry_, sizeof(entry_), "0");
        waitingForOperand_ = false;
    }
    entering_ = true;
    resultShown_ = false;
    repeat_ = CalcOperation::None;
    invalidateUndo();
    if (entry_[0] == '-') {
        std::memmove(entry_, entry_ + 1, std::strlen(entry_));
    } else {
        const size_t length = std::strlen(entry_);
        if (length + 1 < sizeof(entry_)) {
            std::memmove(entry_ + 1, entry_, length + 1);
            entry_[0] = '-';
        }
    }
}

void ClassicEngine::clearEntry() {
    if (error_) {
        allClear();
        return;
    }
    copyText(entry_, sizeof(entry_), "0");
    waitingForOperand_ = false;
    entering_ = true;
    resultShown_ = false;
    if (pending_ == CalcOperation::None) {
        accumulator_ = 0.0;
        repeat_ = CalcOperation::None;
    }
    invalidateUndo();
}

double ClassicEngine::currentValue() const {
    if (error_) return 0.0;
    if (!entering_ && (resultShown_ || waitingForOperand_)) return accumulator_;
    return std::strtod(entry_, nullptr);
}

uint8_t decimalPlaces(DecimalMode mode) {
    switch (mode) {
        case DecimalMode::Fixed0: return 0;
        case DecimalMode::Fixed2: return 2;
        case DecimalMode::Fixed4: return 4;
        default: return 0;
    }
}

double ClassicEngine::roundedResult(double value) const {
    if (decimalMode_ == DecimalMode::Auto || !std::isfinite(value)) return value;
    const uint8_t places = decimalPlaces(decimalMode_);
    const double factor = places == 0 ? 1.0 : places == 2 ? 100.0 : 10000.0;
    const double scaled = value * factor;
    if (!std::isfinite(scaled)) return value;

    double rounded = scaled;
    if (roundingMode_ == RoundingMode::Nearest) {
        rounded = std::round(scaled);
    } else if (roundingMode_ == RoundingMode::Truncate) {
        rounded = std::trunc(scaled);
    } else {
        const double magnitude = std::fabs(scaled);
        const double nearest = std::round(magnitude);
        const double adjusted = std::fabs(magnitude - nearest) < 1e-10 ? nearest : std::ceil(magnitude);
        rounded = std::copysign(adjusted, scaled);
    }
    const double result = rounded / factor;
    return std::fabs(result) < ZERO_CUTOFF ? 0.0 : result;
}

bool ClassicEngine::calculate(double left, CalcOperation operation, double right, double &result) {
    switch (operation) {
        case CalcOperation::Add: result = left + right; break;
        case CalcOperation::Subtract: result = left - right; break;
        case CalcOperation::Multiply: result = left * right; break;
        case CalcOperation::Divide:
            if (right == 0.0) {
                setError("DIVIDE BY ZERO");
                return false;
            }
            result = left / right;
            break;
        default:
            result = right;
            break;
    }
    result = roundedResult(result);
    if (!std::isfinite(result) || std::fabs(result) > MAX_RESULT) {
        setError("OVERFLOW");
        return false;
    }
    if (std::fabs(result) < ZERO_CUTOFF) result = 0.0;
    return true;
}

void ClassicEngine::setError(const char *message) {
    error_ = true;
    copyText(errorText_, sizeof(errorText_), message);
    pending_ = CalcOperation::None;
    repeat_ = CalcOperation::None;
    entering_ = false;
    waitingForOperand_ = false;
    resultShown_ = false;
}

void ClassicEngine::setEntryFromValue(double value) {
    formatValue(value, entry_, sizeof(entry_), false);
}

void ClassicEngine::snapshotUndo() {
    undo_.valid = true;
    copyText(undo_.entry, sizeof(undo_.entry), entry_);
    undo_.accumulator = accumulator_;
    undo_.repeatOperand = repeatOperand_;
    undo_.grandTotal = grandTotal_;
    undo_.pending = pending_;
    undo_.repeat = repeat_;
    undo_.entering = entering_;
    undo_.waitingForOperand = waitingForOperand_;
    undo_.resultShown = resultShown_;
    undo_.grandTotalActive = grandTotalActive_;
    undo_.tapeStart = tapeStart_;
    undo_.tapeCount = tapeCount_;
    undo_.overwritten = false;
}

void ClassicEngine::appendTape(const char *text, double result) {
    uint8_t index = 0;
    if (tapeCount_ < MAX_TAPE_RECORDS) {
        index = (tapeStart_ + tapeCount_) % MAX_TAPE_RECORDS;
        ++tapeCount_;
    } else {
        index = tapeStart_;
        if (undo_.valid) {
            undo_.overwritten = true;
            undo_.overwrittenIndex = index;
            undo_.overwrittenRecord = tape_[index];
        }
        tapeStart_ = (tapeStart_ + 1) % MAX_TAPE_RECORDS;
    }
    copyText(tape_[index].text, sizeof(tape_[index].text), text);
    tape_[index].result = result;
}

void ClassicEngine::appendBinaryTape(double left, CalcOperation operation, double right,
                                     double result, const char *rightSuffix) {
    char leftText[32];
    char rightText[32];
    char resultText[32];
    char line[72];
    formatValue(left, leftText, sizeof(leftText), false);
    formatValue(right, rightText, sizeof(rightText), false);
    formatValue(result, resultText, sizeof(resultText), false);
    std::snprintf(line, sizeof(line), "%s %s %s%s = %s", leftText, operationSymbol(operation),
                  rightText, rightSuffix ? rightSuffix : "", resultText);
    appendTape(line, result);
}

void ClassicEngine::appendTaxTape(double source, double result, bool addTax) {
    char sourceText[28];
    char resultText[28];
    char rateText[16];
    char line[72];
    formatValue(source, sourceText, sizeof(sourceText), false);
    formatValue(result, resultText, sizeof(resultText), false);
    std::snprintf(rateText, sizeof(rateText), "%.2f", taxRate_);
    trimFixed(rateText);
    std::snprintf(line, sizeof(line), "%s TAX%c%s%% = %s", sourceText, addTax ? '+' : '-', rateText, resultText);
    appendTape(line, result);
}

void ClassicEngine::appendUnaryTape(double source, const char *operation, double result) {
    char sourceText[28];
    char resultText[28];
    char line[72];
    formatValue(source, sourceText, sizeof(sourceText), false);
    formatValue(result, resultText, sizeof(resultText), false);
    std::snprintf(line, sizeof(line), "%s%s = %s", sourceText, operation, resultText);
    appendTape(line, result);
}

void ClassicEngine::finishCalculation(double left, CalcOperation operation, double right,
                                      double result, bool addToGrandTotal, const char *rightSuffix) {
    appendBinaryTape(left, operation, right, result, rightSuffix);
    accumulator_ = result;
    setEntryFromValue(result);
    repeat_ = operation;
    repeatOperand_ = right;
    pending_ = CalcOperation::None;
    entering_ = false;
    waitingForOperand_ = false;
    resultShown_ = true;
    error_ = false;
    if (addToGrandTotal) {
        grandTotal_ = roundedResult(grandTotal_ + result);
        if (std::isfinite(grandTotal_) && std::fabs(grandTotal_) <= MAX_RESULT) grandTotalActive_ = true;
        else {
            grandTotal_ = 0.0;
            grandTotalActive_ = false;
        }
    }
}

void ClassicEngine::setOperation(CalcOperation operation) {
    if (error_ || operation == CalcOperation::None) return;
    repeat_ = CalcOperation::None;
    if (pending_ != CalcOperation::None && entering_) {
        snapshotUndo();
        const double left = accumulator_;
        const double right = currentValue();
        double result = 0.0;
        if (!calculate(left, pending_, right, result)) return;
        appendBinaryTape(left, pending_, right, result);
        accumulator_ = result;
        setEntryFromValue(result);
    } else if (pending_ == CalcOperation::None) {
        accumulator_ = currentValue();
    }
    pending_ = operation;
    entering_ = false;
    waitingForOperand_ = true;
    resultShown_ = false;
}

void ClassicEngine::equals() {
    if (error_) return;
    if (pending_ != CalcOperation::None) {
        snapshotUndo();
        const double left = accumulator_;
        const double right = entering_ ? currentValue() : accumulator_;
        double result = 0.0;
        if (!calculate(left, pending_, right, result)) return;
        finishCalculation(left, pending_, right, result, true);
        return;
    }
    if (repeat_ != CalcOperation::None && resultShown_) {
        snapshotUndo();
        const double left = currentValue();
        double result = 0.0;
        if (!calculate(left, repeat_, repeatOperand_, result)) return;
        finishCalculation(left, repeat_, repeatOperand_, result, true);
    }
}

void ClassicEngine::percent() {
    if (error_) return;
    const double enteredPercent = entering_ ? currentValue() : accumulator_;
    snapshotUndo();
    if (pending_ != CalcOperation::None) {
        const double left = accumulator_;
        const double effectiveRight = (pending_ == CalcOperation::Add || pending_ == CalcOperation::Subtract)
            ? left * enteredPercent / 100.0
            : enteredPercent / 100.0;
        double result = 0.0;
        if (!calculate(left, pending_, effectiveRight, result)) return;

        char percentText[32];
        char leftText[32];
        char resultText[32];
        char line[72];
        formatValue(enteredPercent, percentText, sizeof(percentText), false);
        formatValue(left, leftText, sizeof(leftText), false);
        formatValue(result, resultText, sizeof(resultText), false);
        std::snprintf(line, sizeof(line), "%s %s %s%% = %s", leftText, operationSymbol(pending_),
                      percentText, resultText);
        appendTape(line, result);

        const CalcOperation completed = pending_;
        accumulator_ = result;
        setEntryFromValue(result);
        repeat_ = completed;
        repeatOperand_ = effectiveRight;
        pending_ = CalcOperation::None;
        entering_ = false;
        waitingForOperand_ = false;
        resultShown_ = true;
        grandTotal_ = roundedResult(grandTotal_ + result);
        if (std::isfinite(grandTotal_) && std::fabs(grandTotal_) <= MAX_RESULT) grandTotalActive_ = true;
        else {
            grandTotal_ = 0.0;
            grandTotalActive_ = false;
        }
        return;
    }

    const double result = roundedResult(currentValue() / 100.0);
    appendUnaryTape(currentValue(), "%", result);
    accumulator_ = result;
    setEntryFromValue(result);
    repeat_ = CalcOperation::None;
    entering_ = false;
    waitingForOperand_ = false;
    resultShown_ = true;
}

void ClassicEngine::applyTax(bool addTax) {
    if (error_) return;
    if (waitingForOperand_) {
        copyText(entry_, sizeof(entry_), "0");
        waitingForOperand_ = false;
        entering_ = true;
    }
    const double source = currentValue();
    const double divisor = 1.0 + taxRate_ / 100.0;
    snapshotUndo();
    double result = addTax ? source * divisor : source / divisor;
    result = roundedResult(result);
    if (!std::isfinite(result) || std::fabs(result) > MAX_RESULT) {
        setError("OVERFLOW");
        return;
    }
    appendTaxTape(source, result, addTax);
    setEntryFromValue(result);
    entering_ = pending_ != CalcOperation::None;
    waitingForOperand_ = false;
    resultShown_ = pending_ == CalcOperation::None;
    if (pending_ == CalcOperation::None) accumulator_ = result;
    repeat_ = CalcOperation::None;
}

bool ClassicEngine::undo() {
    if (!undo_.valid) return false;
    copyText(entry_, sizeof(entry_), undo_.entry);
    accumulator_ = undo_.accumulator;
    repeatOperand_ = undo_.repeatOperand;
    grandTotal_ = undo_.grandTotal;
    pending_ = undo_.pending;
    repeat_ = undo_.repeat;
    entering_ = undo_.entering;
    waitingForOperand_ = undo_.waitingForOperand;
    resultShown_ = undo_.resultShown;
    grandTotalActive_ = undo_.grandTotalActive;
    tapeStart_ = undo_.tapeStart;
    tapeCount_ = undo_.tapeCount;
    if (undo_.overwritten) tape_[undo_.overwrittenIndex] = undo_.overwrittenRecord;
    error_ = false;
    errorText_[0] = '\0';
    invalidateUndo();
    return true;
}

void ClassicEngine::memoryAdd() {
    if (error_) return;
    const double result = roundedResult(memory_ + currentValue());
    if (!std::isfinite(result) || std::fabs(result) > MAX_RESULT) return;
    memory_ = result;
    memoryActive_ = true;
}

void ClassicEngine::memorySubtract() {
    if (error_) return;
    const double result = roundedResult(memory_ - currentValue());
    if (!std::isfinite(result) || std::fabs(result) > MAX_RESULT) return;
    memory_ = result;
    memoryActive_ = true;
}

void ClassicEngine::memoryRecall() {
    if (error_) allClear();
    if (pending_ == CalcOperation::None && resultShown_) allClear();
    setEntryFromValue(memoryActive_ ? memory_ : 0.0);
    waitingForOperand_ = false;
    entering_ = true;
    resultShown_ = false;
    repeat_ = CalcOperation::None;
    invalidateUndo();
}

void ClassicEngine::memoryClear() {
    memory_ = 0.0;
    memoryActive_ = false;
}

void ClassicEngine::setStoredMemory(double value, bool active) {
    if (!std::isfinite(value) || std::fabs(value) > MAX_RESULT) {
        memory_ = 0.0;
        memoryActive_ = false;
        return;
    }
    memory_ = value;
    memoryActive_ = active;
}

double ClassicEngine::memoryValue() const { return memory_; }
bool ClassicEngine::memoryActive() const { return memoryActive_; }

void ClassicEngine::grandTotalRecall() {
    if (error_) allClear();
    if (pending_ == CalcOperation::None && resultShown_) allClear();
    setEntryFromValue(grandTotalActive_ ? grandTotal_ : 0.0);
    waitingForOperand_ = false;
    entering_ = true;
    resultShown_ = false;
    repeat_ = CalcOperation::None;
    invalidateUndo();
}

void ClassicEngine::grandTotalClear() {
    grandTotal_ = 0.0;
    grandTotalActive_ = false;
    invalidateUndo();
}

double ClassicEngine::grandTotalValue() const { return grandTotal_; }
bool ClassicEngine::grandTotalActive() const { return grandTotalActive_; }

void ClassicEngine::clearTape() {
    tapeStart_ = 0;
    tapeCount_ = 0;
    invalidateUndo();
}

uint8_t ClassicEngine::tapeCount() const { return tapeCount_; }

const TapeRecord *ClassicEngine::tapeRecord(uint8_t logicalIndex) const {
    if (logicalIndex >= tapeCount_) return nullptr;
    return &tape_[(tapeStart_ + logicalIndex) % MAX_TAPE_RECORDS];
}

bool ClassicEngine::recallTape(uint8_t logicalIndex) {
    const TapeRecord *record = tapeRecord(logicalIndex);
    if (!record) return false;
    const double value = record->result;
    allClear();
    setEntryFromValue(value);
    accumulator_ = value;
    entering_ = false;
    resultShown_ = true;
    return true;
}

void ClassicEngine::setDecimalMode(DecimalMode mode) {
    if (static_cast<uint8_t>(mode) <= static_cast<uint8_t>(DecimalMode::Fixed4)) decimalMode_ = mode;
}
DecimalMode ClassicEngine::decimalMode() const { return decimalMode_; }

void ClassicEngine::setRoundingMode(RoundingMode mode) {
    if (static_cast<uint8_t>(mode) <= static_cast<uint8_t>(RoundingMode::AwayFromZero)) roundingMode_ = mode;
}
RoundingMode ClassicEngine::roundingMode() const { return roundingMode_; }

void ClassicEngine::setGrouping(bool enabled) { grouping_ = enabled; }
bool ClassicEngine::grouping() const { return grouping_; }

bool ClassicEngine::setTaxRate(double rate) {
    if (!std::isfinite(rate) || rate < 0.0 || rate > 99.99) return false;
    taxRate_ = std::round(rate * 100.0) / 100.0;
    return true;
}
double ClassicEngine::taxRate() const { return taxRate_; }

bool ClassicEngine::hasError() const { return error_; }
const char *ClassicEngine::errorText() const { return errorText_; }
bool ClassicEngine::undoAvailable() const { return undo_.valid; }
CalcOperation ClassicEngine::pendingOperation() const { return pending_; }

const char *ClassicEngine::operationSymbol(CalcOperation operation) {
    switch (operation) {
        case CalcOperation::Add: return "+";
        case CalcOperation::Subtract: return "-";
        case CalcOperation::Multiply: return "x";
        case CalcOperation::Divide: return "/";
        default: return "";
    }
}

void ClassicEngine::formatValue(double value, char *output, size_t size, bool grouped) const {
    if (!output || !size) return;
    char raw[48];
    if (!std::isfinite(value)) {
        copyText(output, size, "ERROR");
        return;
    }
    if (std::fabs(value) < ZERO_CUTOFF) value = 0.0;

    const uint8_t places = decimalPlaces(decimalMode_);
    const bool fixedFits = decimalMode_ != DecimalMode::Auto && std::fabs(value) < 1e14;
    if (fixedFits) std::snprintf(raw, sizeof(raw), "%.*f", places, value);
    else std::snprintf(raw, sizeof(raw), "%.15g", value);
    normalizeExponent(raw);
    if (grouped && grouping_) groupNumberText(raw, output, size);
    else copyText(output, size, raw);
}

void ClassicEngine::formatEntryText(char *output, size_t size) const {
    if (grouping_) groupNumberText(entry_, output, size);
    else copyText(output, size, entry_);
}

void ClassicEngine::formatDisplay(char *output, size_t size) const {
    if (error_) {
        copyText(output, size, "ERROR");
        return;
    }
    if (entering_) formatEntryText(output, size);
    else formatValue(currentValue(), output, size, true);
}

void ClassicEngine::contextText(char *output, size_t size) const {
    if (!output || !size) return;
    if (pending_ != CalcOperation::None) {
        char left[32];
        formatValue(accumulator_, left, sizeof(left), true);
        std::snprintf(output, size, "%s  %s", left, operationSymbol(pending_));
        return;
    }
    if (tapeCount_) {
        copyText(output, size, tapeRecord(tapeCount_ - 1)->text);
        return;
    }
    copyText(output, size, "READY");
}
