#include "EquationSolver.h"

#include <algorithm>
#include <cstdlib>

namespace {
const long double ROOT_TOLERANCE = 1e-7L;
const int MAX_ROOTS = 8;
const int MAX_RESULT_CHARS = 25;

bool isFinite(long double value) {
    return isfinite(static_cast<double>(value));
}

bool hasSignChange(long double a, long double b) {
    return (a < 0 && b > 0) || (a > 0 && b < 0);
}
}

EquationSolver::EquationSolver(int x, int y, int width, int height) :
    UIElement(x, y, width, height),
    equationInput(17, 19, 191, 12, 37),
    guessInput(17, 34, 52, 12, 9) {
    this -> activeField = 0;
    this -> delPressedTime = 0;
    this -> lastDeleteTime = 0;
    this -> hasResult = false;
    this -> moreRoots = false;
    this -> resultClipped = false;
    this -> rootCount = 0;
    this -> statusText = "Enter an equation";
}

void EquationSolver::init() {
    this -> equationInput.init();
    this -> guessInput.init();
    if (this -> guessInput.getStr().empty()) {
        this -> guessInput.insertStr("0");
    }
}

void EquationSolver::activate() {
    currentElement = this;
}

InputBox* EquationSolver::activeInput() {
    return this -> activeField == 0 ? &this -> equationInput : &this -> guessInput;
}

void EquationSolver::markEdited() {
    this -> hasResult = false;
    this -> moreRoots = false;
    this -> resultClipped = false;
    this -> rootCount = 0;
    this -> resultText = "";
    this -> statusText = "Ready";
}

void EquationSolver::insertInput(std::string value) {
    if (value.empty()) return;

    if (this -> activeField == 1) {
        if (value.size() != 1 || std::string("0123456789.+-").find(value[0]) == std::string::npos) {
            return;
        }
    }

    this -> activeInput() -> insertStr(value);
    markEdited();
}

bool EquationSolver::parseToRpn(std::string expression, std::vector<Node>& rpn) {
    if (expression.empty()) return false;

    Expression parser;
    std::vector<Node> nodes = parser.parseString(expression);
    if (nodes.empty() || !parser.checkSyntax(nodes)) return false;

    std::queue<Node> queue = parser.shuntingYard(nodes);
    while (!queue.empty()) {
        rpn.push_back(queue.front());
        queue.pop();
    }
    return !rpn.empty();
}

bool EquationSolver::evaluateRpn(const std::vector<Node>& rpn, long double x, long double& result) {
    std::vector<long double> values;

    for (Node node : rpn) {
        Token token = node.getToken();
        if (token == NUM) {
            values.push_back(node.getValue());
            continue;
        }
        if (token == VAR) {
            values.push_back(x);
            continue;
        }

        if (node.isUnary()) {
            if (values.empty()) return false;
            long double value = values.back();
            values.pop_back();
            long double angle = (angleMode ? 0.017453292519943295L : 1.0L) * value;
            long double output = 0;

            switch (token) {
                case NEGATIVE: output = -value; break;
                case SQRT: output = sqrtl(value); break;
                case LN: output = logl(value); break;
                case EXP: output = expl(value); break;
                case SIN: output = sinl(angle); break;
                case COS: output = cosl(angle); break;
                case TAN: output = tanl(angle); break;
                case SEC: output = 1.0L / cosl(angle); break;
                case CSC: output = 1.0L / sinl(angle); break;
                case COT: output = 1.0L / tanl(angle); break;
                case ASIN: output = (angleMode ? 57.29577951308232L : 1.0L) * asinl(value); break;
                case ACOS: output = (angleMode ? 57.29577951308232L : 1.0L) * acosl(value); break;
                case ATAN: output = (angleMode ? 57.29577951308232L : 1.0L) * atanl(value); break;
                default: return false;
            }
            if (!isFinite(output)) return false;
            values.push_back(output);
            continue;
        }

        if (node.isBinary()) {
            if (values.size() < 2) return false;
            long double right = values.back();
            values.pop_back();
            long double left = values.back();
            values.pop_back();
            long double output = 0;

            switch (token) {
                case ADD: output = left + right; break;
                case SUB: output = left - right; break;
                case MUL: output = left * right; break;
                case DIV: output = left / right; break;
                case POW: output = powl(left, right); break;
                default: return false;
            }
            if (!isFinite(output)) return false;
            values.push_back(output);
            continue;
        }

        return false;
    }

    if (values.size() != 1 || !isFinite(values.back())) return false;
    result = values.back();
    return true;
}

bool EquationSolver::evaluateEquation(const std::vector<Node>& left, const std::vector<Node>& right, long double x, long double& result) {
    long double leftValue = 0;
    long double rightValue = 0;
    if (!evaluateRpn(left, x, leftValue) || !evaluateRpn(right, x, rightValue)) return false;
    result = leftValue - rightValue;
    return isFinite(result);
}

bool EquationSolver::bisect(const std::vector<Node>& left, const std::vector<Node>& right, long double low, long double high, long double& root, long double& residual) {
    long double lowValue = 0;
    long double highValue = 0;
    if (!evaluateEquation(left, right, low, lowValue) || !evaluateEquation(left, right, high, highValue)) return false;

    for (int i = 0; i < 80; i++) {
        long double mid = (low + high) / 2.0L;
        long double midValue = 0;
        if (!evaluateEquation(left, right, mid, midValue)) return false;

        if (fabsl(midValue) < ROOT_TOLERANCE) {
            root = mid;
            residual = fabsl(midValue);
            return true;
        }

        if (hasSignChange(lowValue, midValue)) {
            high = mid;
            highValue = midValue;
        }
        else {
            low = mid;
            lowValue = midValue;
        }
    }

    root = (low + high) / 2.0L;
    long double value = 0;
    if (!evaluateEquation(left, right, root, value)) return false;
    residual = fabsl(value);
    return residual < ROOT_TOLERANCE;
}

bool EquationSolver::findRoot(const std::vector<Node>& left, const std::vector<Node>& right, long double initialGuess, long double& root, long double& residual) {
    long double x = initialGuess;

    // Start near x0 so equations with several roots select the intended one.
    for (int i = 0; i < 40; i++) {
        long double value = 0;
        if (!evaluateEquation(left, right, x, value)) break;
        if (fabsl(value) < ROOT_TOLERANCE) {
            root = x;
            residual = fabsl(value);
            return true;
        }

        long double h = 1e-5L * (1.0L + fabsl(x));
        long double above = 0;
        long double below = 0;
        if (!evaluateEquation(left, right, x + h, above) || !evaluateEquation(left, right, x - h, below)) break;

        long double derivative = (above - below) / (2.0L * h);
        if (!isFinite(derivative) || fabsl(derivative) < 1e-12L) break;

        long double step = value / derivative;
        long double maxStep = 10.0L * (1.0L + fabsl(x));
        if (fabsl(step) > maxStep) step = step < 0 ? -maxStep : maxStep;

        long double next = x - step;
        if (!isFinite(next) || fabsl(next) > 1e12L) break;
        x = next;
    }

    const long double ranges[] = {1.0L, 10.0L, 100.0L, 1000.0L, 10000.0L};
    long double bestLow = 0;
    long double bestHigh = 0;
    bool foundBracket = false;

    // Fall back to a bracketed search when Newton's method cannot progress.
    for (long double range : ranges) {
        const int segments = 200;
        long double previousX = initialGuess - range;
        long double previousValue = 0;
        bool previousValid = evaluateEquation(left, right, previousX, previousValue);
        long double nearestDistance = 1e30L;

        for (int i = 1; i <= segments; i++) {
            long double currentX = initialGuess - range + (2.0L * range * i / segments);
            long double currentValue = 0;
            bool currentValid = evaluateEquation(left, right, currentX, currentValue);

            if (currentValid && fabsl(currentValue) < ROOT_TOLERANCE) {
                root = currentX;
                residual = fabsl(currentValue);
                return true;
            }
            if (previousValid && currentValid && hasSignChange(previousValue, currentValue)) {
                long double distance = fabsl((previousX + currentX) / 2.0L - initialGuess);
                if (distance < nearestDistance) {
                    nearestDistance = distance;
                    bestLow = previousX;
                    bestHigh = currentX;
                    foundBracket = true;
                }
            }

            previousX = currentX;
            previousValue = currentValue;
            previousValid = currentValid;
        }

        if (foundBracket) break;
    }

    return foundBracket && bisect(left, right, bestLow, bestHigh, root, residual);
}

bool EquationSolver::addRoot(std::vector<long double>& roots, long double root) {
    for (long double existing : roots) {
        long double tolerance = 1e-4L * (1.0L + fabsl(existing));
        if (fabsl(existing - root) < tolerance) return false;
    }

    if (roots.size() >= MAX_ROOTS) {
        this -> moreRoots = true;
        return false;
    }
    roots.push_back(root);
    return true;
}

void EquationSolver::findRoots(const std::vector<Node>& left, const std::vector<Node>& right, long double initialGuess, std::vector<long double>& roots) {
    long double root = 0;
    long double residual = 0;
    if (findRoot(left, right, initialGuess, root, residual)) {
        addRoot(roots, root);
    }

    const long double ranges[] = {10.0L, 100.0L, 1000.0L};
    for (long double range : ranges) {
        const int segments = 400;
        long double previousX = initialGuess - range;
        long double previousValue = 0;
        bool previousValid = evaluateEquation(left, right, previousX, previousValue);

        if (previousValid && fabsl(previousValue) < ROOT_TOLERANCE) {
            addRoot(roots, previousX);
        }

        for (int i = 1; i <= segments; i++) {
            long double currentX = initialGuess - range + (2.0L * range * i / segments);
            long double currentValue = 0;
            bool currentValid = evaluateEquation(left, right, currentX, currentValue);

            if (currentValid && fabsl(currentValue) < ROOT_TOLERANCE) {
                addRoot(roots, currentX);
            }
            else if (previousValid && currentValid && hasSignChange(previousValue, currentValue)) {
                long double bracketRoot = 0;
                long double bracketResidual = 0;
                if (bisect(left, right, previousX, currentX, bracketRoot, bracketResidual)) {
                    addRoot(roots, bracketRoot);
                }
            }

            previousX = currentX;
            previousValue = currentValue;
            previousValid = currentValid;
        }

        if (this -> moreRoots || roots.size() >= 2) break;
    }

    std::sort(roots.begin(), roots.end());
}

std::string EquationSolver::formatRoots(const std::vector<long double>& roots) {
    std::string output = "x=";
    this -> resultClipped = false;

    for (int i = 0; i < roots.size(); i++) {
        std::string part = (i == 0 ? "" : ",") + numToStr(roots[i]);
        if (output.size() + part.size() > MAX_RESULT_CHARS) {
            output += ",...";
            this -> resultClipped = true;
            break;
        }
        output += part;
    }
    return output;
}

void EquationSolver::solve() {
    std::string equation = this -> equationInput.getStr();
    size_t equals = equation.find('=');
    if (equals != std::string::npos && equation.find('=', equals + 1) != std::string::npos) {
        this -> hasResult = false;
        this -> statusText = "Use only one =";
        return;
    }

    std::string leftText = equals == std::string::npos ? equation : equation.substr(0, equals);
    std::string rightText = equals == std::string::npos ? "0" : equation.substr(equals + 1);
    std::vector<Node> left;
    std::vector<Node> right;
    if (!parseToRpn(leftText, left) || !parseToRpn(rightText, right)) {
        this -> hasResult = false;
        this -> statusText = "Invalid equation";
        return;
    }

    std::string guessText = this -> guessInput.getStr();
    char* end = nullptr;
    long double initialGuess = strtold(guessText.c_str(), &end);
    if (guessText.empty() || end == guessText.c_str() || *end != '\0' || !isFinite(initialGuess)) {
        this -> hasResult = false;
        this -> statusText = "Invalid initial x0";
        return;
    }

    if (equation.find('x') == std::string::npos) {
        long double value = 0;
        if (!evaluateEquation(left, right, 0, value)) {
            this -> hasResult = false;
            this -> statusText = "Equation is undefined";
        }
        else if (fabsl(value) < ROOT_TOLERANCE) {
            this -> hasResult = false;
            this -> statusText = "Identity: any x";
        }
        else {
            this -> hasResult = false;
            this -> statusText = "No solution (no x)";
        }
        return;
    }

    this -> moreRoots = false;
    std::vector<long double> roots;
    findRoots(left, right, initialGuess, roots);
    if (roots.empty()) {
        this -> hasResult = false;
        this -> statusText = "No root near x0";
        return;
    }

    this -> hasResult = true;
    this -> rootCount = roots.size();
    this -> resultText = formatRoots(roots);
    this -> statusText = std::to_string(this -> rootCount) + (this -> moreRoots ? "+ roots near x0" : (this -> rootCount == 1 ? " root near x0" : " roots near x0"));
}

void EquationSolver::draw() {
    u8g2.drawStr(2, 7, "Equation solver");
    u8g2.drawStr(2, 19, "f:");
    this -> equationInput.draw();
    u8g2.drawStr(2, 34, "x0:");
    this -> guessInput.draw();

    if (this -> hasResult) {
        u8g2.drawStr(76, 34, this -> resultText.c_str());
    }
    u8g2.drawStr(2, 48, this -> statusText.c_str());
    if (this -> hasResult) {
        u8g2.drawStr(2, 60, (this -> moreRoots || this -> resultClipped) ? "More roots not shown" : "Roots sorted low to high");
    }
    else {
        u8g2.drawStr(2, 60, "S+5:x  S+Enter:=");
    }

    this -> activeInput() -> drawCursor();
    u8g2.drawStr(215, 21, this -> activeField == 0 ? "IN:EQ" : "IN:x0");
    u8g2.drawStr(215, 33, ("L:" + calcLayout.getLayout().getName()).c_str());
    u8g2.drawStr(215, 45, angleMode ? "DEG" : "RAD");
}

void EquationSolver::update() {
    std::string key = calcLayout.updateString();
    bool shifted = calcLayout.getLayout().getName() == "2";

    if (key == "ESC") {
        goBack();
        return;
    }
    if (key == "TAB" || key == "UP" || key == "DOWN") {
        this -> activeField = !this -> activeField;
    }
    else if (key == "LEFT") {
        this -> activeInput() -> scrollLeft(1);
    }
    else if (key == "RIGHT") {
        this -> activeInput() -> scrollRight(1);
    }
    else if (key == "BKSP") {
        this -> activeInput() -> deleteStr();
        markEdited();
    }
    else if (key == "MODE SWITCH") {
        angleMode = !angleMode;
        markEdited();
    }
    else if (key == "ENTER") {
        if (shifted && this -> activeField == 0) insertInput("=");
        else solve();
    }
    else if (key == "5" && shifted && this -> activeField == 0) {
        insertInput("x");
    }
    else if (key != "" && key != "LAYER SWITCH" && key != "RPN SWITCH") {
        insertInput(key);
    }

    if (kb.getRisingEdgeKey() == std::make_pair(4, 3)) {
        this -> delPressedTime = millis();
    }
    if (kb.getKey(4, 3).getIsPressed() && millis() - this -> delPressedTime > 750 && millis() - this -> lastDeleteTime > 100) {
        this -> activeInput() -> deleteStr();
        this -> lastDeleteTime = millis();
        markEdited();
    }

    draw();
}
