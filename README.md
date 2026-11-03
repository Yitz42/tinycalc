# TinySci

A touch calculator for the Pebble Time 2 (emery) with algebraic entry and an f(x)=0 solver.

## Build and install (Mac)

    cd ~/Downloads/pebble-calc
    pebble build
    pebble install --phone <IP>        # or: pebble install --emulator emery

## Using it

Two keypad pages, 123 and f(x): swipe left or right to switch between them. Below each is another page: swipe up from 123 for the solver keys, or up from f(x) for more functions. From those, swipe down to go back up, or left/right for the 123 keypad. The Up button steps through all four pages.

| Page | Keys |
|---|---|
| 123 | digits, + - × ÷, = |
| f(x) | INV, DEG/RAD, ( ), sin cos tan, x^y, ln log, √, x², π, e, EE, Ans |
| SOLVE (below 123) | x, y, z, Clr xyz, ∫dx, d/dx, Factor, Expand, Graph, STO, RCL, History |
| MORE (below f(x)) | INV, sinh cosh tanh, nCr, nPr, mod, the comma, gcd, lcm, round, floor, n!, \|x\|, %, 1/x |
| TOOLS (below MORE) | S<>D, MODE, logb, ⁿ√, Σ, Π, the comma, i, Pol, Rec, MEM, Stat, Units, Const, Base, x |

INV turns on inverse functions for one key: asin/acos/atan, e^x, 10^x, ∛ and x³, and asinh/acosh/atanh.

**More functions:** `5 nCr 2` = 10 and `5 nPr 2` = 20 (combinations and permutations; they bind tighter than × and ÷, so `2×5 nCr 2` = 20, and choosing more than there are gives 0), `7 mod 3` = 1 (it takes the divisor's sign, so `-7 mod 3` = 2, and it's exact even for big numbers: `1E20 mod 7` = 2). gcd and lcm take two numbers separated by the comma key: `gcd(12, 18)` = 6. round rounds halves away from zero; floor rounds down. These whole-number functions first round their input to 12 digits, so `floor((0.7+0.1)×10)` is 8, as the display suggests, not 7. `n!` works for fractions too (the gamma function): `0.5!` = 0.886… = √π/2.

**TOOLS keypad** (swipe up from MORE; a green bar at the bottom of MORE hints at it):
- **S<>D** shows the answer in exact form (3/4, 2√3, 3π/4, or the exact root after solving); press again for the decimals.
- **MODE** steps the number format: normal, SCI (1.2340e3), ENG (powers of 1000: 12.340e3) and FIX (a set number of decimals). The digits, and the starting format, are set in the phone settings. SCI4, FIX2 and so on show in the corner. In normal mode small numbers show plainly down to 10 decimal places: 0.00001, not 1e-5. FIX, SCI and ENG round the way you would on paper (FIX 2 of 1.005 is 1.01).
- **logb(b, x)** is the log of x to base b: `logb(2, 8)` = 3. **ⁿ√** is `root(n, x)`: `root(3, -8)` = −2. Both typeset properly (log₂, ³√).
- **Σ and Π:** `sum(f, a, b)` adds f for x = a, a+1, …, b (`sum(x², 1, 10)` = 385); `prod` multiplies. Use the x key on this page for the counter; up to 10,000 terms. They typeset with the sign and limits.
- **i** types the imaginary unit (see Complex numbers below); hold it for conj, re, im, arg, abs and the answer in polar form.
- **Exact angles:** in DEG, sin 30, cos 60, tan 45 and the like are exact (sin 30 − 0.5 = 0), tan 90 is a Math error, asin 0.5 is exactly 30, and huge angles (sin(10^15 + 30)) lose no accuracy.
- **Pol / Rec** convert between rectangular and polar: type `3, 4` and Pol gives r = 5 and the angle 53.13° (in DEG; radians in RAD); Rec goes back. With nothing typed they use x and y. The results go into x and y.
- **MEM** lists memories A–F with their values. Tap one to type it into the expression; after STO or RCL, tap one to store into it or recall it.
- **Stat** collects data: "+ Ans" adds the number typed (or the answer), "+ (x, y)" adds the pair in x and y. Then tap n, mean, sum, s, σ, min, max or median, and with pairs: mean y and the regression y = ax + b (a, b, r). Up to 40 points, kept when you exit. (s and σ are worked out about the mean, so data like 1E9+1, 1E9+2, 1E9+3 keeps its precision.)
- **Probability** (the last row of Stat) types **Ran#** (a random number from 0 to 1, new each time it's worked out), **RanInt(a, b)** (a random whole number from a to b), the standard normal **normcdf(x)** = P(Z < x), its inverse **invnorm(p)** and the density **normpdf(x)**; for a normal with mean μ and spread σ use (x − μ)/σ. These work in ordinary calculations (they're worked out with the complex numbers, to save room in the app), not in graphs, the solver or integrals. **Prime factors** shows the answer as a product of primes (360 → 2^3 * 3^2 * 5); complete below 10^12, and a leftover that might not be prime is marked (?).
- **Units** opens a list of groups; tap one for its conversions, both ways, of the number typed (or the answer), each previewing its result. Back goes up to the groups. About 90 conversions:
  - Angle: degrees in d m s (12.5 → 12d 30m 0s), radians, gradians, turns, arcminutes, arcseconds
  - Length: in, mm, ft, yd, mi, nautical mi, mil, light year
  - Area: acre, sq ft, sq in, sq yd, sq mi
  - Volume: US gal, qt, pt, cup, fl oz, tbsp, tsp; UK gal and pt; cu ft, cu in; oil barrel
  - Mass: lb, oz, stone, US and UK ton, troy oz, carat, grain
  - Temperature: °F, °C, K, Rankine
  - Time: s, min, h, day, week, year
  - Speed: mph, knot, m/s, ft/s, Mach; Fuel economy: mpg (US and UK) to km/L and L/100 km
  - Energy and power: kcal, cal, BTU, kWh, eV, ft·lbf, therm, hp, metric hp, BTU/h
  - Pressure: psi, atm, bar, mmHg, inHg, kgf/cm²; Force: lbf, kgf, dyn; Torque: lb·ft, lb·in, kgf·m
  - Flow: gal/min, cfm, L/s; Data: bits and bytes, kB to TB, KiB to GiB
- **Const** types a constant into the expression (its full value; the list shows 6 digits), in groups: Mathematical (φ, γ, √2, ln 2, ζ(3), Catalan), Universal (c, G, h, ħ, Planck length, mass and time), Electromagnetic (e, ε₀, μ₀, Coulomb's kₑ, Z₀, flux quantum, Bohr and nuclear magnetons, Josephson and von Klitzing), Atomic and nuclear (mₑ, mₚ, mₙ, u, muon, deuteron, mₚ/mₑ, α, a₀, rₑ, Compton wavelength, Rydberg, eV), Physico-chemical (NA, k, R, F, σ, Wien, molar volume, Loschmidt, 0 °C in K), Earth (g, atm, Earth and Moon mass, radius and distance, speed of sound, day and year) and Astronomy (AU, light year, parsec, Sun mass, radius and power, Jupiter, Hubble). SI units, CODATA 2018.
- **Base** shows the answer in hex, binary or octal (0x1F, 0b11111, 0o37), and types bit operations: and, or, xor, not, shl, shr (on whole numbers, two's complement). Big whole numbers are converted exactly (2^50+1 is 0x4000000000001); a number too long to show says so rather than being cut off.

**Complex numbers:** anything with i is worked out in complex numbers: `(2+3i)(1-i)` = 5+i, `i^i` = 0.2078795764, `e^(iπ)` = −1. Things with no real answer give a complex one instead of Math error: `√(-4)` = 2i, `ln(-1)` = πi, `asin(2)` = 90−75.45613i (in DEG). All the functions work on complex numbers (powers, roots, logs, trig and hyperbolic functions and their inverses), plus conj, re, im and arg (hold the i key). Ans keeps both parts, so you can keep calculating. S<>D shows exact parts (√i → √2/2 + √2/2 i), Pol on a complex answer gives its size and angle, and the history keeps both parts. With both parts the answer shows 7 digits of each, to fit. x, y and the memories hold real numbers: storing a complex answer keeps its real part. Graphs, the solver and the formula keys (Factor, ∫dx…) stay real; the solver already lists complex roots of polynomials.

**Lists with the buttons:** in History and the TOOLS lists, Up and Down move a highlight (the first press shows it), holding them moves a page, Select picks the highlighted row, and Back goes up a level or closes the list.

**Typing numbers:** a 0 followed by another digit is dropped: 0 then 1 gives 1 (0.5 and 10 stay as typed).

**Roots of negatives:** an odd root of a negative number is real: `(-8)^(1/3)` = −2, `(-8)^(2/3)` = 4, `(-32)^0.2` = −2. Even roots of negatives are still an error.

**Accuracy details:** sin(π), cos(π/2) and tan(π) show 0 (the tiny leftover is only π being stored rounded). Trig of very large angles (up to 10¹⁶) is reduced exactly, so sin(1E10) is right to all digits. sinh, tanh, asinh and atanh keep full precision near 0, and sinh/cosh/asinh/acosh work out to the ends of the number range.

- **Evaluate:** type the expression and tap the yellow **=** (or press Select). Precedence is normal, `2π` and `3(4+1)` multiply implicitly, and missing `)` at the end are closed for you: a bracket you haven't closed yet is drawn grey.
- **Continue from the answer:** if you start with an operator after a result, it uses `Ans`.
- **The display:** what you type is shown as typeset math as you go: a real × sign, π, √ with its bar, fractions stacked, powers raised. Answers show in big digits underneath. RAD shows in the corner when you're in radians.
- **The one = key:** if what you've typed has x or y in it and no equals sign yet, = types the equals sign (`x^2` = gives `x^2=`). Type the right side and press = again to solve. Pressing = straight after the equals sign takes it back out and just calculates with the stored x (so `2x` = = gives 2 times x). Without x or y, = just calculates.
- **Solve:** for example `x^2` = `2` =: the solver sheet slides up with the answers (see below). To find where an expression is 0, type `=0`.
- **Store and recall:** STO, then tap x, y or z, saves the current result there (the two keys light up while it waits). RCL, then x, y or z, shows what's stored. Clr xyz sets all three back to 0. Any other key cancels STO / RCL.
- **Fractions:** ÷ follows the usual order of operations: 1+2 then ÷ 3 is 1 + 2/3, with only the 2 on top. To put more on top, use brackets: (1+2) ÷ 3.
- **Cursor:** hold your finger still on the display for a moment. It buzzes once and a blue cursor appears; slide to move it: it follows your finger left and right, and up and down into the top or bottom of a fraction or into a power. Then let go and type. Keys go in at the cursor in order, and DEL deletes before it. The cursor turns off when it reaches the end of the text (slide it there, or type up to it), when you tap the display, or on =, AC and the other commands. Holding on an answer brings its expression back to edit.
- **Buttons:** Down is AC, Back is DEL (on an answer it brings the expression back to edit; it exits only when the display is empty), Select is =, Up changes page. Hold Down for the formula view, hold Up for History, hold Back to exit.
- If touch is turned off (or a button is set to move the key highlight), Up and Down move a highlight across the keys and Select presses the highlighted key.

**History:** hold Up, or tap History on the solver keys. It lists your last 8 answers, newest first, each with what you typed. Tap one (or Up/Down + Select) to bring it back as the current answer: it's also Ans, so you can keep calculating, and Back brings its expression back to edit. History is saved when you exit.

**Settings (on your phone):** in the Pebble app, open TinySci and tap its settings (gear). Pick your choices and tap Save; they go straight to the watch, which keeps its own copy, so nothing on the watch side waits on the phone. The settings page lives in `src/pkjs/index.js`.

| Setting | Choices |
|---|---|
| Buttons | A list of actions for each of Up, Up held, Down, Down held, Select and Select held: next page, previous page, =, AC, DEL, formula view, history, solver keys, highlight previous/next key, press highlighted key, Ans, or nothing. Defaults: Up next page (held: History), Down AC (held: formula view), Select = (held: AC). Back always deletes and holding it exits. |
| Solver graph | Off, or On: the solver sheet graphs both sides of a one-variable equation |
| Key buzz | Off, Light, Strong |
| Backlight | Watch (the watch's own setting), On press (lights up with each key), Always (on while the app is open) |
| Clear the answer history | a checkbox, applied on Save |

**Formula view:** pull down from the top quarter of the screen (the display area), or hold Down, and a sheet slides down showing your expression as typeset math: stacked fractions, raised exponents, square and cube root signs, |x| bars and parentheses that grow to fit. The answer, or a live preview while you're typing, is shown underneath. Half-typed parts show as empty boxes. If a formula is too big for the screen, drag it to see the rest. Tap, push the sheet up, or press Back/Select to close it.

**Solver keys:** swipe up on the 123 keypad for the solver keys: x, y, z, Clr xyz, ∫dx, d/dx, Factor, Expand, Graph, STO, RCL and History. Powers, roots and brackets are on the f(x) page, and the equals sign comes from =. A green bar at the bottom edge of the keypad pages hints that you can swipe up. Swipe left, right or down to go back to 123.

**Symbolic math (formulas):**
- **d/dx** with nothing typed at "at x =" gives the derivative as a formula: x²·sin x → 2x·sin x + x²·cos x.
- **∫dx** with nothing typed at "from =" gives an antiderivative: x·sin x → sin x − x·cos x + C. It knows powers, 1/x, e^x, a^x, sin, cos, tan, sinh, cosh, tanh, ln, √ and |x| of a linear argument (like cos(3x+1)), sums and constant multiples, substitution (x·cos(x²), 2x·e^(x²), x/√(x²+1)), integration by parts for x^n times sin, cos, sinh, cosh or e^x, for x^n·ln x, and for atan, asin, acos, asinh and atanh; sin², cos², tan², 1/cos², 1/sin², sinh² and cosh²; x√x and other powers hidden in roots; powers of polynomials like (x²+1)²; any quotient of polynomials over a linear or quadratic bottom (by division and partial fractions: 1/(x²−1), (2x+3)/(x²+3x+2), 1/(x²+2x+2) → atan(x+1), x/(x+1)); and 1/√(quadratic) as asin, asinh or a log. When it can't find one it says "No formula found"; type limits to get the number instead.
- **Factor** factors a polynomial in x (up to degree 12) over the rationals, into linear and quadratic factors: x² − x − 6 → (x + 2)(x − 3), x⁴ − 1 → (x − 1)(x + 1)(x² + 1), x⁴ − 5x² + 6 → (x² − 2)(x² − 3), x⁴ + 4 → (x² − 2x + 2)(x² + 2x + 2). On an equation it factors left side minus right side: x² = 4 → (x − 2)(x + 2) = 0.
- **Expand** multiplies out and collects like terms: (x + 1)³ → x³ + 3x² + 3x + 1, up to powers like (x + 1)¹² (beyond what fits, it says so rather than giving the input back). Polynomials are shown in falling powers. On an equation it expands each side.

The formula shows big, with what it came from above it. Pull down (or hold Down) to see it full size. DEL or = brings the formula back to edit (so you can evaluate it, graph it, or take its derivative), and an operator goes on from it: + 1 gives (formula) + 1. y counts as a constant. In DEG mode trig is in degrees, so its derivatives and integrals carry π/180 or 180/π factors; switch to RAD for the usual forms. Every answer is checked numerically in the tests (`test/test_cas.c`).

**Graph:** type something and tap **Graph** on the solver keys. It graphs:
- y = f(x), or just f(x)
- both sides of an equation in x as two curves (they cross at the solutions)
- the curve of an equation in x and y, like x² + y² = 25

Drag to move around, hold Up or Down (or tap + / −) to zoom in or out, tap to mark a point and read its x and y, then Up/Down step the point along the curve. Select refits the view and Back closes it. The grid lines are labelled.

**Solver sheet (the answer to = on an equation):** it lists the roots of the equation in the search range (up to 20), no starting guess needed. Roots that only touch zero, like those of (x²−2)² or cos x = 1, are found and pinned down to about 10 digits, as are pairs closer together than the search grid (1 and 1.0000001), and a near miss like x² = −10⁻¹² is not taken for a root. It searches x from −10 to 10 around the current x and widens the search up to three times (10× wider each time) if it finds nothing, so `sin(x)=0.5` in degrees still finds 30 and 150. Tap **-** to search wider or **+** to search narrower (or hold Down/Up). Tap a root, or highlight it with Up/Down and press Select, to use it: it goes into x and Ans. Pressing = afterwards opens the sheet again. Without the graph, the equation is shown at full size and the sheet scrolls: drag it up and down (or step through the answers with Up/Down) to see everything; dragging down at the top closes it. With the graph on, the sheet is just the graph and the answers. Push the sheet down or press Back to close it.

**Two equations (x and y):** if the equation has a **y** in it, = asks for a second equation ("2nd equation?"; the first one stays on the small line). Type it and press = again: the sheet shows both equations with a brace and every (x, y) solution in the search box (x and y within ±10 to start). Two linear equations are solved directly, so their one answer is found wherever it is. Tap one to use it: x and y are both stored, and the calculator shows x on the small line and y on the big one. DEL on an empty second equation goes back to editing the first. The formula view shows the pair with a brace too.

**Three equations (x, y and z):** if the equation has a **z** in it, = asks for a second equation and then a third ("3rd equation?"). Press = after the third: the calculator solves the system and shows x and y on the small line and z on the big one; all three are stored. The three equations must be linear (otherwise "3 unknowns: linear only"); parallel or dependent equations give "No solution found". DEL on the result goes back to editing the third equation, and DEL on an empty equation goes back to the one before it. The formula view shows the three equations stacked, with the answer under them.

**Exact answers:** when an equation is really a polynomial (up to degree 4, rational coefficients), the solver shows it simplified under what you typed. For example, (x+1)^2-3=2x becomes x^2-2=0. Each root is shown in exact form, typeset, next to its decimal: x = ±√2, x = (1±√5)/2, x = ∛2 for x³ = 2, ±√2 and ±√3 for x⁴ − 5x² + 6 = 0, and complex ones like (−1±√3 i)/2 (listed, not usable). Coefficients have to match a fraction to about 12 digits, so x² = 0.3333333333 is not taken for x² = 1/3; the same goes for the exact forms of plain answers (3.1415926535 is not π). A linear pair in x and y gets exact fractions, and the formula view shows exact forms of plain answers, like √12 = 2√3, 0.75 = 3/4 and 3π/4. Anything else gets decimals only. See `EXACT_FORMS` below.

**Key buzz:** every key press gives a short buzz: 15 ms on Light (`KEY_BUZZ_MS` in config.h), 40 ms on Strong, or none (phone settings).

**Integral and derivative:** type a function of x (no `=`), for example `sin(x²)`.

- **∫dx** asks for "from =" and then "to =". Type each limit (an expression like `pi/2` is fine) and press =.
- **d/dx** asks for "at x =". Type the point (type `x` for the current x) and press =. With nothing typed you get the formula instead (see Symbolic math).

The answer goes into Ans, so you can keep calculating with it. Pressing ∫dx or d/dx again on a result reuses the same function with new limits or a new point. DEL steps back through the prompts, and then back to editing the function. In DEG mode trig works in degrees, so `d/dx sin(x)` is cos(x)·π/180. The formula view shows the integral sign with its limits, or d/dx with "| x = a" after it.

The integral uses adaptive 15-point Gauss–Kronrod. When that runs out of room it looks for a singular point and switches to the tanh-sinh (double exponential) rule on each side of it, which handles strong end singularities like x^−0.9 or ln x on [0, 1] to full accuracy. A divergent integral (1/x or 1/x² across 0, tan x past π/2) is a Math error, not a big number. A node that lands on a removable hole (sin(x)/x at 0) takes the value beside it. The tanh-sinh answer is only used when f really is unbounded there; a jump or wiggles (floor x, sin(1/x)) keep the Gauss–Kronrod total. The total is also checked against an independent split, so a wave far faster than the pieces (sin x from 0 to 10^6) can't fool it: that gives "Integral won't settle" instead of a wrong number. If it can't reach full accuracy (about 10 digits), the result line shows `~` instead of `=`. The derivative uses Ridders' method (central differences, extrapolated), accurate to about 10 digits; it first shrinks its step until the function looks smooth, so a nearby pole (1/x at 0.001) is no problem, and a kink, jump or vertical tangent (|x| or ∛x at 0, floor at 1), or a step that never settles (x·|x| at 0), is a Math error rather than a made-up number. Tiny slopes are kept: d/dx(1E-20·x) is 1E-20.

**Solver graph:** the solver sheet can also graph a one-variable equation (both sides, with the roots marked, drag to pan). Turn it on in the settings on your phone.

**Memory:** the app's code and static data (`.text + .data + .bss`) must fit in 64 KB (65,535 bytes); the build stops with "App virtual size ... must be 65535 bytes or smaller" otherwise. Resources (256 KB) and the heap don't count. So most of the code lives in *overlays*: code kept as resources and copied onto the heap when needed:
- `fmt.ovl` (10 KB): the typesetter, loaded when the app starts and kept
- `math.ovl` (38 KB): the root finder, exact answers, x/y systems, 3×3 linear systems, and the logic of the formula and solver sheets (what to typeset, the search for answers); loaded on first use
- `graph.ovl` (24 KB): sampling and drawing graphs, and the graph sheet's zoom, trace and pan; loaded on first use
- `cas.ovl` (32 KB): symbolic math; loaded for each use and freed straight after (the others are freed first to make room)
- `tools.ovl` (27 KB): the TOOLS lists (memories, statistics and probability, units, constants, bases, complex); loaded while a list is open
- `calcu.ovl` (21 KB): integrals and derivatives; loaded for ∫dx or d/dx with numbers
- `cplx.ovl` (22 KB): complex evaluation, Ran# and the normal distribution; loaded for = on something complex or random

If the heap runs short, the loaded overlays are freed and loaded again later; the same happens when a running overlay needs working memory (an integral's pieces, the root finder's samples): the idle ones make way. That keeps the app itself at about 59 KB, with roughly 6 KB free. Data that only matters while the app runs (formula trees, buffers, history, display lines) is on the heap too.

The app saves its state when you exit (the expression, x, Ans, the angle mode and history; settings are saved as soon as they arrive).

**Store listing:** `store/` has the description (`description.md`), six 200×228 screenshots, a 720×320 banner and 144/48 px icons for the app store.

## Layout

- `src/c/calc.c`: the parser, number formatting and key logic. It's plain C and doesn't use pebble.h.
- `src/c/mmath.c`: double-precision math functions. They don't depend on libm, and they're checked against libm to within about 1e-15.
- `src/c/main.c`: drawing, touch, buttons, the formula, solver and graph sheets, history, and receiving settings.
- `src/pkjs/index.js`: the settings page shown in the Pebble app on the phone.
- `src/ovl/`: overlay code (built into `resources/ovl/*.ovl`, not the app):
  - `mathfmt.c`: the formula typesetter. It turns tokens into a tree, lays it out, and draws through callbacks.
  - `calcx.c`: the all-roots finder (sampling, bracketing at each sign change, a closer look and Newton at each dip of |f|, the turning point for touching roots), x/y systems (2-D Newton from a grid of starts), 3×3 linear systems (a linearity check, then Gaussian elimination with partial pivoting), integrals (adaptive Gauss–Kronrod, tanh-sinh at singular points) and derivatives (Ridders, after a smoothness check).
  - `cplx.c`: complex evaluation: the same grammar as calc.c's parser on a + bi values.
  - `tools.c`: the TOOLS lists: their rows and what picking one does.
  - `sheet.c`: the formula and solver sheets' logic (moved out of main.c to save app space): what to typeset and at what size, exact answers on the sheet, the search for answers and which one to highlight.
  - `exact.c`: exact answers: spotting polynomials, simplifying them, exact roots, exact linear systems, recognizing p/q, surds and multiples of π.
  - `cas.c`: symbolic math: expression trees, simplification, derivatives, antiderivatives, factoring, expanding.
  - `graph.c`: graph sampling, fitting the view, and drawing (curves, implicit curves by marching squares, grid and labels, marked points).
  - `ovl_*.c`: each overlay's entry point; `ovl_rt.c`: the C library bits overlays need.
- `src/c/ovl.c`: loads an overlay from resources onto the heap and fixes up its addresses. `src/c/ovl_calls.c`: the app-side versions of the overlays' functions (same names as the real ones, so the PC tests link the real code directly).
- `tools/build_overlay.py`: compiles an overlay with the SDK's compiler. `pebble build` runs it (see `wscript`) whenever overlay sources change.
- `src/c/config.h`: build options: `EXACT_FORMS` (on; in the overlay, so no app space), `SOLVER_GRAPH` (the solver sheet's graph; turned on in the phone settings), `KEY_BUZZ_MS`. Set either option to 0 to make the app smaller.
- `test/`: PC tests, a stand-in pebble.h and a simulator that replays taps and renders a preview (`sh test/run.sh`).
- `test/emu/`: differential fuzzing. `build.sh` builds `fuzz.c` twice: on the PC with everything linked directly (with AddressSanitizer), and for ARM, where it loads the real overlay files through `ovl.c` onto a capped heap and runs under `qemu-arm`. Random key sequences and generated expressions go through the calculator, typesetter, root finder, exact answers, symbolic math and graphs, and the two outputs must match line for line.
