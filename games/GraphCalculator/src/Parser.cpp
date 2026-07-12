#include "Parser.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace {
constexpr double PI_VALUE = 3.14159265358979323846;
constexpr double E_VALUE = 2.71828182845904523536;
constexpr double SINGULAR_EPSILON = 1e-10;
}

bool Parser::fail(int position, const String &message) {
    if (error_ && error_->position < 0) {
        error_->position = position;
        error_->message = message;
    }
    return false;
}

void Parser::skipSpaces() {
    while (cursor_ < source_.length() && isspace(static_cast<unsigned char>(source_[cursor_]))) ++cursor_;
}

char Parser::peek() {
    skipSpaces();
    return cursor_ < source_.length() ? source_[cursor_] : '\0';
}

bool Parser::take(char c) {
    if (peek() != c) return false;
    ++cursor_;
    return true;
}

bool Parser::startsPrimary() {
    const char c = peek();
    return c == '(' || isalpha(static_cast<unsigned char>(c));
}

int16_t Parser::addNode(Kind kind, int16_t left, int16_t right, double value) {
    if (nodeCount_ >= MAX_NODES) {
        fail(cursor_, "expression too complex");
        return -1;
    }
    Node &node = nodes_[nodeCount_];
    node.kind = kind;
    node.left = left;
    node.right = right;
    node.value = value;
    return nodeCount_++;
}

int16_t Parser::addFunction(Function function, int16_t argument) {
    int16_t index = addNode(Kind::Function, argument);
    if (index >= 0) nodes_[index].function = function;
    return index;
}

int16_t Parser::parseExpression() {
    int16_t left = parseTerm();
    if (left < 0) return -1;
    while (true) {
        if (take('+')) {
            int16_t right = parseTerm();
            if (right < 0) return -1;
            left = addNode(Kind::Add, left, right);
        } else if (take('-')) {
            int16_t right = parseTerm();
            if (right < 0) return -1;
            left = addNode(Kind::Subtract, left, right);
        } else break;
    }
    return left;
}

int16_t Parser::parseTerm() {
    int16_t left = parseUnary();
    if (left < 0) return -1;
    while (true) {
        Kind kind;
        if (take('*')) kind = Kind::Multiply;
        else if (take('/')) kind = Kind::Divide;
        else if (startsPrimary()) kind = Kind::Multiply;
        else break;
        int16_t right = parseUnary();
        if (right < 0) return -1;
        left = addNode(kind, left, right);
    }
    return left;
}

int16_t Parser::parseUnary() {
    if (take('+')) return parseUnary();
    if (take('-')) {
        int16_t value = parseUnary();
        return value < 0 ? -1 : addNode(Kind::Negate, value);
    }
    return parsePower();
}

int16_t Parser::parsePower() {
    int16_t left = parsePrimary();
    if (left < 0) return -1;
    if (take('^')) {
        int16_t right = parseUnary();
        if (right < 0) return -1;
        return addNode(Kind::Power, left, right);
    }
    return left;
}

int16_t Parser::parsePrimary() {
    const int start = cursor_;
    const char c = peek();
    if (c == '\0') {
        fail(cursor_, "expected a value");
        return -1;
    }
    if (take('(')) {
        int16_t value = parseExpression();
        if (value < 0) return -1;
        if (!take(')')) {
            fail(cursor_, "expected ')'");
            return -1;
        }
        return value;
    }
    if (isdigit(static_cast<unsigned char>(c)) || c == '.') {
        const char *begin = source_.c_str() + cursor_;
        char *end = nullptr;
        double value = strtod(begin, &end);
        if (end == begin || !isfinite(value)) {
            fail(start, "invalid number");
            return -1;
        }
        cursor_ += static_cast<uint16_t>(end - begin);
        return addNode(Kind::Number, -1, -1, value);
    }
    if (isalpha(static_cast<unsigned char>(c))) {
        String name;
        while (isalpha(static_cast<unsigned char>(peek()))) {
            name += static_cast<char>(tolower(static_cast<unsigned char>(source_[cursor_++])));
        }
        if (name == "x") return addNode(Kind::Variable);
        if (name == "pi") return addNode(Kind::Number, -1, -1, PI_VALUE);
        if (name == "e") return addNode(Kind::Number, -1, -1, E_VALUE);

        Function function;
        if (name == "sin") function = Function::Sin;
        else if (name == "cos") function = Function::Cos;
        else if (name == "tan") function = Function::Tan;
        else if (name == "sec") function = Function::Sec;
        else if (name == "csc") function = Function::Csc;
        else if (name == "cot") function = Function::Cot;
        else if (name == "asin") function = Function::Asin;
        else if (name == "acos") function = Function::Acos;
        else if (name == "atan") function = Function::Atan;
        else if (name == "sqrt") function = Function::Sqrt;
        else if (name == "ln") function = Function::Ln;
        else if (name == "log") function = Function::Log;
        else if (name == "exp") function = Function::Exp;
        else if (name == "abs") function = Function::Abs;
        else {
            fail(start, String("unknown '") + name + "'");
            return -1;
        }
        if (!take('(')) {
            fail(cursor_, "expected '(' after function");
            return -1;
        }
        int16_t argument = parseExpression();
        if (argument < 0) return -1;
        if (!take(')')) {
            fail(cursor_, "expected ')' after function");
            return -1;
        }
        return addFunction(function, argument);
    }
    fail(start, String("unexpected '") + c + "'");
    return -1;
}

bool Parser::compile(const String &expression, ParseError &error) {
    source_ = expression;
    cursor_ = 0;
    nodeCount_ = 0;
    root_ = -1;
    error = ParseError();
    error_ = &error;
    if (source_.length() == 0) return fail(0, "expression is empty");
    root_ = parseExpression();
    if (root_ < 0) return false;
    if (peek() != '\0') return fail(cursor_, String("unexpected '") + peek() + "'");
    error_ = nullptr;
    return true;
}

bool Parser::evalNode(int16_t index, double x, bool degrees, uint8_t depth, double &value) const {
    if (index < 0 || index >= nodeCount_ || depth > 64) return false;
    const Node &node = nodes_[index];
    double a = 0.0;
    double b = 0.0;
    switch (node.kind) {
        case Kind::Number: value = node.value; break;
        case Kind::Variable: value = x; break;
        case Kind::Negate:
            if (!evalNode(node.left, x, degrees, depth + 1, a)) return false;
            value = -a;
            break;
        case Kind::Add: case Kind::Subtract: case Kind::Multiply:
        case Kind::Divide: case Kind::Power:
            if (!evalNode(node.left, x, degrees, depth + 1, a) ||
                !evalNode(node.right, x, degrees, depth + 1, b)) return false;
            if (node.kind == Kind::Add) value = a + b;
            else if (node.kind == Kind::Subtract) value = a - b;
            else if (node.kind == Kind::Multiply) value = a * b;
            else if (node.kind == Kind::Divide) {
                if (fabs(b) < SINGULAR_EPSILON) return false;
                value = a / b;
            } else value = pow(a, b);
            break;
        case Kind::Function: {
            if (!evalNode(node.left, x, degrees, depth + 1, a)) return false;
            const double radians = degrees ? a * PI_VALUE / 180.0 : a;
            switch (node.function) {
                case Function::Sin: value = sin(radians); break;
                case Function::Cos: value = cos(radians); break;
                case Function::Tan:
                    if (fabs(cos(radians)) < SINGULAR_EPSILON) return false;
                    value = tan(radians); break;
                case Function::Sec:
                    if (fabs(cos(radians)) < SINGULAR_EPSILON) return false;
                    value = 1.0 / cos(radians); break;
                case Function::Csc:
                    if (fabs(sin(radians)) < SINGULAR_EPSILON) return false;
                    value = 1.0 / sin(radians); break;
                case Function::Cot:
                    if (fabs(sin(radians)) < SINGULAR_EPSILON) return false;
                    value = cos(radians) / sin(radians); break;
                case Function::Asin:
                    if (a < -1.0 || a > 1.0) return false;
                    value = asin(a) * (degrees ? 180.0 / PI_VALUE : 1.0); break;
                case Function::Acos:
                    if (a < -1.0 || a > 1.0) return false;
                    value = acos(a) * (degrees ? 180.0 / PI_VALUE : 1.0); break;
                case Function::Atan:
                    value = atan(a) * (degrees ? 180.0 / PI_VALUE : 1.0); break;
                case Function::Sqrt:
                    if (a < 0.0) return false;
                    value = sqrt(a); break;
                case Function::Ln:
                    if (a <= 0.0) return false;
                    value = log(a); break;
                case Function::Log:
                    if (a <= 0.0) return false;
                    value = log10(a); break;
                case Function::Exp: value = exp(a); break;
                case Function::Abs: value = fabs(a); break;
            }
            break;
        }
    }
    return isfinite(value);
}

bool Parser::evaluate(double x, bool degrees, double &result) const {
    return root_ >= 0 && evalNode(root_, x, degrees, 0, result);
}
