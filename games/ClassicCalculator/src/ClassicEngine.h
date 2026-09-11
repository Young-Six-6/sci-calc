#pragma once

#include <stddef.h>
#include <stdint.h>

enum class CalcOperation : uint8_t { None, Add, Subtract, Multiply, Divide };
enum class DecimalMode : uint8_t { Auto, Fixed0, Fixed2, Fixed4 };
enum class RoundingMode : uint8_t { Nearest, Truncate, AwayFromZero };

struct TapeRecord {
    char text[72] = {};
    double result = 0.0;
};

class ClassicEngine {
public:
    static constexpr uint8_t MAX_ENTRY_DIGITS = 15;
    static constexpr uint8_t MAX_TAPE_RECORDS = 24;

    ClassicEngine();

    void inputDigit(uint8_t digit);
    void inputDoubleZero();
    void inputDecimal();
    void backspace();
    void toggleSign();
    void clearEntry();
    void allClear();

    void setOperation(CalcOperation operation);
    void equals();
    void percent();
    void applyTax(bool addTax);
    bool undo();

    void memoryAdd();
    void memorySubtract();
    void memoryRecall();
    void memoryClear();
    void setStoredMemory(double value, bool active);
    double memoryValue() const;
    bool memoryActive() const;

    void grandTotalRecall();
    void grandTotalClear();
    double grandTotalValue() const;
    bool grandTotalActive() const;

    void clearTape();
    uint8_t tapeCount() const;
    const TapeRecord *tapeRecord(uint8_t logicalIndex) const;
    bool recallTape(uint8_t logicalIndex);

    void setDecimalMode(DecimalMode mode);
    DecimalMode decimalMode() const;
    void setRoundingMode(RoundingMode mode);
    RoundingMode roundingMode() const;
    void setGrouping(bool enabled);
    bool grouping() const;
    bool setTaxRate(double rate);
    double taxRate() const;

    bool hasError() const;
    const char *errorText() const;
    bool undoAvailable() const;
    CalcOperation pendingOperation() const;
    double currentValue() const;
    void formatDisplay(char *output, size_t size) const;
    void formatValue(double value, char *output, size_t size, bool grouped) const;
    void contextText(char *output, size_t size) const;

    static const char *operationSymbol(CalcOperation operation);

private:
    static constexpr size_t ENTRY_SIZE = 32;

    struct UndoState {
        bool valid = false;
        char entry[ENTRY_SIZE] = {};
        double accumulator = 0.0;
        double repeatOperand = 0.0;
        double grandTotal = 0.0;
        CalcOperation pending = CalcOperation::None;
        CalcOperation repeat = CalcOperation::None;
        bool entering = true;
        bool waitingForOperand = false;
        bool resultShown = false;
        bool grandTotalActive = false;
        uint8_t tapeStart = 0;
        uint8_t tapeCount = 0;
        bool overwritten = false;
        uint8_t overwrittenIndex = 0;
        TapeRecord overwrittenRecord;
    };

    char entry_[ENTRY_SIZE] = {};
    double accumulator_ = 0.0;
    double repeatOperand_ = 0.0;
    double memory_ = 0.0;
    double grandTotal_ = 0.0;
    double taxRate_ = 13.0;
    CalcOperation pending_ = CalcOperation::None;
    CalcOperation repeat_ = CalcOperation::None;
    DecimalMode decimalMode_ = DecimalMode::Auto;
    RoundingMode roundingMode_ = RoundingMode::Nearest;
    bool entering_ = true;
    bool waitingForOperand_ = false;
    bool resultShown_ = false;
    bool error_ = false;
    bool memoryActive_ = false;
    bool grandTotalActive_ = false;
    bool grouping_ = true;
    char errorText_[24] = {};

    TapeRecord tape_[MAX_TAPE_RECORDS];
    uint8_t tapeStart_ = 0;
    uint8_t tapeCount_ = 0;
    UndoState undo_;

    void beginFreshEntry();
    void setEntryFromValue(double value);
    void setError(const char *message);
    uint8_t entryDigitCount() const;
    double roundedResult(double value) const;
    bool calculate(double left, CalcOperation operation, double right, double &result);
    void finishCalculation(double left, CalcOperation operation, double right,
                           double result, bool addToGrandTotal, const char *rightSuffix = nullptr);
    void snapshotUndo();
    void invalidateUndo();
    void appendTape(const char *text, double result);
    void appendBinaryTape(double left, CalcOperation operation, double right,
                          double result, const char *rightSuffix = nullptr);
    void appendTaxTape(double source, double result, bool addTax);
    void appendUnaryTape(double source, const char *operation, double result);
    void formatEntryText(char *output, size_t size) const;
};
