#ifndef EQUATION_SOLVER_H
#define EQUATION_SOLVER_H

#include "../UIElements/InputBox.h"
#include "Expression.h"

class EquationSolver : public UIElement {
    public:
        EquationSolver(int x, int y, int width, int height);

        void init();
        void activate();
        void draw();
        void update();

    private:
        InputBox equationInput;
        InputBox guessInput;
        int activeField;
        int delPressedTime;
        int lastDeleteTime;
        bool hasResult;
        bool moreRoots;
        bool resultClipped;
        int rootCount;
        std::string resultText;
        std::string statusText;

        InputBox* activeInput();
        void markEdited();
        void insertInput(std::string value);
        void solve();

        bool parseToRpn(std::string expression, std::vector<Node>& rpn);
        bool evaluateRpn(const std::vector<Node>& rpn, long double x, long double& result);
        bool evaluateEquation(const std::vector<Node>& left, const std::vector<Node>& right, long double x, long double& result);
        bool findRoot(const std::vector<Node>& left, const std::vector<Node>& right, long double initialGuess, long double& root, long double& residual);
        void findRoots(const std::vector<Node>& left, const std::vector<Node>& right, long double initialGuess, std::vector<long double>& roots);
        bool addRoot(std::vector<long double>& roots, long double root);
        std::string formatRoots(const std::vector<long double>& roots);
        bool bisect(const std::vector<Node>& left, const std::vector<Node>& right, long double low, long double high, long double& root, long double& residual);
};

#endif
