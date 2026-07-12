#pragma once

#include <Arduino.h>

struct ParseError {
    int position = -1;
    String message;
};

class Parser {
public:
    bool compile(const String &expression, ParseError &error);
    bool evaluate(double x, bool degrees, double &result) const;

private:
    enum class Kind : uint8_t {
        Number, Variable, Add, Subtract, Multiply, Divide, Power, Negate, Function
    };
    enum class Function : uint8_t {
        Sin, Cos, Tan, Sec, Csc, Cot, Asin, Acos, Atan, Sqrt, Ln, Log, Exp, Abs
    };
    struct Node {
        Kind kind = Kind::Number;
        Function function = Function::Sin;
        double value = 0.0;
        int16_t left = -1;
        int16_t right = -1;
    };

    static constexpr uint8_t MAX_NODES = 128;
    Node nodes_[MAX_NODES];
    uint8_t nodeCount_ = 0;
    int16_t root_ = -1;
    String source_;
    uint16_t cursor_ = 0;
    ParseError *error_ = nullptr;

    void skipSpaces();
    char peek();
    bool take(char c);
    bool startsPrimary();
    int16_t parseExpression();
    int16_t parseTerm();
    int16_t parseUnary();
    int16_t parsePower();
    int16_t parsePrimary();
    int16_t addNode(Kind kind, int16_t left = -1, int16_t right = -1, double value = 0.0);
    int16_t addFunction(Function function, int16_t argument);
    bool fail(int position, const String &message);
    bool evalNode(int16_t index, double x, bool degrees, uint8_t depth, double &value) const;
};

