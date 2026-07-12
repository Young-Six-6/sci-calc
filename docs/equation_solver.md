# Equation Solver

The `Equation` item in the main menu solves one real variable, `x`.

## Keys

- `Shift + 5`: insert `x`
- `Shift + Enter`: insert `=`
- `Enter`: solve
- `Tab`, `Shift + Up`, or `Shift + Down`: switch between the equation and `x0`
- `Shift + Left` / `Shift + Right`: move the cursor
- `ang`: switch radians and degrees
- `esc`: return to the main menu

Enter either a complete equation such as `x^2=4`, or an expression such as
`x^2-4`, which is treated as `x^2-4=0`. The initial value `x0` selects which
root to find when an equation has more than one real solution.

The solver also scans around `x0` for additional real roots. Roots are sorted
from low to high. Up to eight are collected; the screen shows an ellipsis and
`More roots not shown` when the result list is longer than the display.

Examples:

- `x^2=4`, `x0=1` gives `x=2`
- `x^2=4`, `x0=-1` gives `x=-2`
- `x^2=9`, `x0=0` gives `x=-3,3`
- `sin(x)=0.5`, `x0=0` gives the root nearest zero in the current angle mode
