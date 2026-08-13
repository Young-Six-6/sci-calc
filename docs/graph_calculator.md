# Graph Calculator

`Graph Calc` is a standalone program loaded from `/graphcalc.bin` on the SD
card. Hold `esc` for about one second to restore `/main.bin` and return to the
main calculator firmware.

## Edit Page

The display keeps the `f(x)=` prefix fixed. Enter only the expression on the
right side. The initial F1 slot is blank. Up to four function slots can be kept
and edited independently.

- `Tab`: switch between the edit and graph pages
- `Enter`: validate the expression and open the graph page
- `RPN/Norm`: select the next function slot
- `Shift + RPN/Norm`: add a blank function slot
- `Shift + 8` / `Shift + 2`: select the previous or next function slot
- `Shift + del`: delete the selected function slot
- `Shift + 5`: insert `x`
- `Shift + /` and `Shift + *`: insert `(` and `)`
- `Shift + Left` and `Shift + Right`: move the cursor
- `del`: delete; hold to repeat
- `ang`: switch radians and degrees

The title shows the selected slot and total count, for example `F2/3`. Adding a
slot selects it immediately. Deleting shifts later slots down; deleting the only
remaining F1 clears it instead. Empty slots are allowed and are not drawn. If a
slot has invalid syntax, graphing stops and the editor selects that slot with an
error position.

The parser supports implicit multiplication such as `2x`, `2(x+1)`, and
`(x+1)(x-1)`. Scientific functions include `sin`, `cos`, `tan`, `sec`, `csc`,
`cot`, inverse trigonometric functions, `sqrt`, `ln`, `log`, `exp`, and `abs`,
plus the constants `pi` and `e`.

Scientific key layer:

- `Shift + sin/cos/tan`: `asin(`/`acos(`/`atan(`
- `Shift + ^`: `sqrt(`
- `Shift + exp`: `ln(`
- `Shift + sec`: `abs(`
- `Shift + csc`: `log(`
- `Shift + cot`: `pi`
- `Shift + 9`: `e`

## Graph Page

- `Shift + 4` / `Shift + 6`: pan left or right
- `Shift + 8` / `Shift + 2`: pan up or down
- `+` / `-`: zoom in or out
- `5`: reset the view
- `Enter`: toggle coordinate tracing
- `4` / `6`: move the trace cursor
- `8` / `2`: select the previous/next function for tracing
- `.`: toggle the grid
- `RPN/Norm`: toggle small numeric axis labels
- `ang`: switch radians and degrees
- `Tab`: return to the edit page

The graph renderer skips undefined values and avoids drawing lines across
large discontinuities. F1 uses a solid line; F2 uses a dotted line; F3 and F4
use different dash patterns. The upper-right status identifies the selected
trace function and whether the grid and labels are enabled. Axis labels use a
small font and automatically follow pan and zoom.

## Rendering Quality And Speed

The graph page caches its grid, axes, labels, and curves. It rebuilds that base
image only when a function, angle mode, pan, zoom, grid, or label setting changes.
Moving the trace cursor and leaving the graph idle therefore avoid full function
re-evaluation.

Each function is sampled at integer, half-pixel, and alternating subpixel
positions. Curved, rapidly changing, turning, or partly undefined intervals are
subdivided adaptively, down to roughly 1/32 horizontal pixel when needed. A line
is drawn only when all intermediate samples are valid and sufficiently close to
the local curve, reducing false connections across vertical asymptotes.

A graph rebuild is capped at 4096 function evaluations and each function is capped
at 1536. With four functions, the available work is divided fairly, so a difficult
first curve cannot consume the complete rendering budget.
