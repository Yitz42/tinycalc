#!/bin/sh
# PC checks: math accuracy, parser, solver, key logic, and a UI simulator run.
# Usage: sh test/run.sh   (from the project folder)
set -e
cd "$(dirname "$0")"
OUT=${TMPDIR:-/tmp}/solver-calc-test
mkdir -p "$OUT"
gcc -std=c99 -D_DEFAULT_SOURCE -O1 -Wall -Wextra -Wno-missing-field-initializers -Werror -o "$OUT/t" test_calc.c ../src/c/calc.c ../src/ovl/cplx.c ../src/ovl/calcx.c ../src/ovl/cas.c ../src/c/mmath.c -lm
"$OUT/t"
gcc -std=c99 -D_DEFAULT_SOURCE -O1 -Wall -Wextra -Wno-unused-parameter -Werror -o "$OUT/tm" test_math.c ../src/ovl/mathfmt.c ../src/c/calc.c ../src/ovl/cplx.c ../src/ovl/calcx.c ../src/ovl/cas.c ../src/c/mmath.c
"$OUT/tm"
gcc -std=c99 -D_DEFAULT_SOURCE -O1 -Wall -Wextra -Wno-missing-field-initializers -Werror -o "$OUT/te" test_exact.c ../src/ovl/exact.c ../src/c/calc.c ../src/ovl/cplx.c ../src/ovl/calcx.c ../src/ovl/cas.c ../src/c/mmath.c -lm
"$OUT/te" | tail -1
gcc -std=c99 -D_DEFAULT_SOURCE -O1 -Wall -Wextra -Wno-missing-field-initializers -Werror -o "$OUT/tc" test_cas.c ../src/ovl/cas.c ../src/c/calc.c ../src/ovl/cplx.c ../src/ovl/calcx.c ../src/c/mmath.c -lm
"$OUT/tc" | tail -1
gcc -std=gnu99 -Wall -Wno-unused-parameter -Werror -DSIM -Istub -I../src/c -o "$OUT/sim" sim.c sim_main.c ../src/ovl/graph.c ../src/ovl/sheet.c ../src/ovl/tools.c ../src/c/calc.c ../src/ovl/cplx.c ../src/ovl/calcx.c ../src/ovl/cas.c ../src/c/mmath.c ../src/ovl/mathfmt.c ../src/ovl/exact.c
# x^2 = 2 typed on the keys, = solves it, tap the second root
"$OUT/sim" "btn up" "btn up" "key 0 0" "btn up" "btn up" "btn up" "btn up" "key 2 3" "btn up" "btn up" "btn up" "btn up" \
           "key 3 2" "key 2 1" "key 3 2" anim "tap 100 158" anim frame | grep -q "x = 1.414213562" \
  && echo "UI solve flow: ok"
# pull up from the number keys brings in the solver keypad
"$OUT/sim" "down 100 200" "move 100 180" "move 100 120" "move 100 60" "lift 100 60" anim frame \
  | grep -q "B d/dx" && echo "Pull-up solver keys: ok"
# pull down the formula sheet for 1/2 and check it opened with the result
"$OUT/sim" "k 49" "k 47" "k 50" "pull 200" "lift 100 230" anim frame | grep -q "= 0.5" \
  && echo "Formula sheet: ok"
# swipes: left/right only between 123 and f(x); up from 123 = solver keys, up from f(x) = more
# functions; from those, down goes back up a level and left/right go to 123
# (pages are told apart by a key on them: 7 on 123, sin on f(x), d/dx on the solver keys, sinh on more)
UP='"down 100 200" "move 100 150" "move 100 80" "lift 100 80"'
"$OUT/sim" "swipe 170 140 30 140" "swipe 170 140 30 140" frame | grep -q "B 7$" \
  && eval "\"$OUT/sim\" $UP frame" | grep -q "B d/dx" \
  && eval "\"$OUT/sim\" \"swipe 170 140 30 140\" $UP frame" | grep -q "B sinh" \
  && eval "\"$OUT/sim\" $UP \"swipe 30 140 170 140\" frame" | grep -q "B 7$" \
  && eval "\"$OUT/sim\" \"swipe 170 140 30 140\" $UP \"down 100 20\" \"move 100 50\" \"move 100 120\" \"lift 100 120\" anim frame" | grep -q "B sin$" \
  && eval "\"$OUT/sim\" $UP \"down 100 20\" \"move 100 50\" \"move 100 120\" \"lift 100 120\" anim frame" | grep -q "B 7$" \
  && echo "Swipe navigation: ok"
# pulling down from the middle of the screen must not open the formula sheet
! "$OUT/sim" "k 49" "down 100 120" "move 100 140" "move 100 200" "move 100 220" "lift 100 220" anim frame \
  | grep -q FORMULA && echo "Pull-down only from the top: ok"
# hold on the display, slide one step left (before the 3), type 2: "1+3" -> "1+23"
"$OUT/sim" "k 49" "k 43" "k 51" "down 100 40" timer "move 88 40" "lift 88 40" "k 50" \
  "k 200" state | grep -q "\[1+23 =\] \[24\]" && echo "Cursor: ok"
# SOLVE on x^2 = 2 lists both roots on the solver sheet
"$OUT/sim" "k 131" "k 133" "k 61" "k 50" "k 200" anim frame | grep -q "B 1.414214" \
  && echo "Solver sheet: ok"
# system: x^2+y^2=25, SOLVE, y=x+1, SOLVE -> (-4,-3) and (3,4); tap the second
"$OUT/sim" "k 131" "k 133" "k 43" "k 151" "k 133" "k 61" "k 50" "k 53" "k 200" \
           "k 151" "k 61" "k 131" "k 43" "k 49" "k 200" anim "tap 100 144" anim frame \
  | grep -q "y = 4" && echo "System solve: ok"
# integral of x^2 from 0 to 3 = 9 (keys: x, x², ∫dx, 0, =, 3, =)
"$OUT/sim" "k 131" "k 133" "k 208" "k 48" "k 200" "k 51" "k 200" frame | grep -q "B 9$" \
  && echo "Integral: ok"
# every screen must use ASCII text only (the watch fonts show anything else as a box)
"$OUT/sim" "k 49" "k 42" "k 50" "k 47" "k 51" "k 200" frame "btn down" frame "btn down" frame \
  "k 131" "k 133" "k 208" frame "k 48" "k 200" "k 49" "k 200" "pull 200" "lift 100 230" anim frame \
  >/dev/null 2>"$OUT/ascii.txt" && "$OUT/sim" "btn up" "k 131" "k 133" "k 61" "k 50" "k 200" anim frame \
  "k 202" "k 151" "k 200" frame "k 61" "k 50" "k 200" frame >/dev/null 2>>"$OUT/ascii.txt" && echo "ASCII-only text: ok" \
  || { cat "$OUT/ascii.txt"; exit 1; }
# cursor: type several keys in order; a stray touch update after lifting must not move it;
# sliding to the end turns it off
"$OUT/sim" "k 49" "k 43" "k 51" "down 100 40" timer "move 88 40" "lift 88 40" "move 88 40" "k 50" \
  "move 88 40" "k 53" state | grep -q "STATE \[1+25|3\]" \
  && "$OUT/sim" "k 49" "k 43" "k 51" "down 100 40" timer "move 88 40" "move 130 40" "lift 130 40" state \
  | grep -q "STATE \[1+3\]" \
  && "$OUT/sim" "k 49" "k 43" "k 51" "down 100 40" timer "move 88 40" "lift 88 40" "tap 100 40" state \
  | grep -q "STATE \[1+3\]" && echo "Cursor order and off: ok"
# the TOOLS keypad: swipe up from MORE (MORE is up from f(x))
T='"swipe 170 140 30 140" "down 100 200" "move 100 150" "move 100 80" "lift 100 80" "down 100 200" "move 100 150" "move 100 80" "lift 100 80"'
eval "\"$OUT/sim\" \"k 49\" \"k 50\" \"k 46\" \"k 53\" \"k 200\" $T \"key 3 0\" \"tap 100 40\" \"tap 100 40\" frame" | grep -q "B 12d 30m 0s" \
  && eval "\"$OUT/sim\" \"k 51\" \"k 44\" \"k 52\" $T \"key 2 0\" state" | grep -q "\[r = 5\] \[deg = 53.13010235\]" \
  && eval "\"$OUT/sim\" \"k 49\" \"k 50\" $T \"key 3 0\" \"tap 100 72\" \"tap 100 40\" state" | grep -q "\[in > cm\] \[30.48\]" \
  && eval "\"$OUT/sim\" \"k 51\" $T \"key 2 3\" \"tap 100 40\" \"btn back\" \"k 202\" \"k 53\" \"key 2 3\" \"tap 100 40\" \"tap 100 200\" state" \
     | grep -q "\[mean =\] \[4\]" \
  && eval "\"$OUT/sim\" \"k 49\" \"k 50\" \"k 51\" \"k 52\" \"k 200\" $T \"key 0 1\" frame" | grep -q "B 1.2340e3" \
  && eval "\"$OUT/sim\" \"k 55\" $T \"key 0 1\" \"key 0 1\" \"key 0 1\" \"key 0 1\" frame" | grep -q "B 7$" \
  && eval "\"$OUT/sim\" \"k 51\" \"k 49\" $T \"key 3 2\" \"tap 100 72\" frame" | grep -q "B 0b11111" \
  && eval "\"$OUT/sim\" \"k 55\" \"k 206\" $T \"key 2 2\" \"tap 100 40\" \"k 200\" \"k 174\" \"k 42\" \"k 50\" \"k 200\" state" \
     | grep -q "\[A\*2 =\] \[14\]" \
  && eval "\"$OUT/sim\" $T \"key 3 1\" \"btn down\" \"btn down\" \"btn select\" \"btn down\" \"btn select\" state" \
     | grep -q "\[6.6743E-11\]" \
  && eval "\"$OUT/sim\" $T \"key 3 1\" \"tap 100 40\" \"btn back\" \"tap 100 104\" \"tap 100 40\" state" \
     | grep -q "1.602176634E-19" \
  && echo "Tools keypad: ok"
# complex: i on TOOLS; sqrt(-4) is 2i; hold i for the list (its last row: the answer as r at angle)
eval "\"$OUT/sim\" \"k 51\" \"k 43\" \"k 52\" $T \"key 1 3\" \"k 200\" state" | grep -q "\[3+4i =\] \[3+4i\]" \
  && "$OUT/sim" "k 148" "k 45" "k 52" "k 200" state | grep -q "\[2i\]" \
  && eval "\"$OUT/sim\" \"k 51\" \"k 43\" \"k 52\" $T \"key 1 3\" \"k 200\" \"down 175 125\" timer \"lift 175 125\" \"tap 100 200\" frame" \
     | grep -q "B 5 at 53.1301 deg" \
  && echo "Complex numbers: ok"
# the brackets round the top of a fraction aren't drawn: sliding to the end of
# the top and typing goes inside them, not out after them as (1+2)*2
# (and / follows the order of operations: 1+2 then / puts only the 2 on top)
"$OUT/sim" "k 40" "k 49" "k 43" "k 50" "k 41" "k 47" "k 52" "down 170 30" timer "move 140 30" "lift 140 30" "k 50" state \
  | grep -q "STATE \[(1+22|)/4\]" \
  && "$OUT/sim" "k 49" "k 43" "k 50" "k 47" "k 51" "k 200" state | grep -q "\[1+2/3 =\] \[1.666666667\]" \
  && echo "Cursor in a fraction: ok"
# three equations in x, y, z: x and y on the small line, z on the big one
"$OUT/sim" "k 131" "k 43" "k 151" "k 43" "k 190" "k 61" "k 54" "k 200" "k 131" "k 45" "k 151" "k 43" "k 190" "k 61" "k 50" "k 200" "k 50" "k 131" "k 43" "k 151" "k 45" "k 190" "k 61" "k 49" "k 200" state | grep -q "\[x = 1, y = 2\] \[z = 3\]" && echo "Three equations: ok"
# Stat's last row opens Probability: normcdf typed; prime factors of the answer
D=""; for i in $(seq 13); do D="$D \"btn down\""; done
eval "\"$OUT/sim\" $T \"key 2 3\" $D \"btn select\" \"btn down\" \"btn down\" \"btn select\" \"k 49\" \"k 200\" state" \
  | grep -q "\[normcdf(1 =\] \[0.8413447461\]" \
  && eval "\"$OUT/sim\" \"k 51\" \"k 54\" \"k 48\" \"k 200\" $T \"key 2 3\" $D \"btn select\" \"btn down\" \"btn down\" \"btn down\" \"btn down\" \"btn down\" \"btn down\" \"btn select\" frame" \
  | grep -q "2^3 \* 3^2 \* 5" && echo "Probability: ok"
# graphing something with z says so instead of plotting it
"$OUT/sim" "k 131" "k 43" "k 190" "btn up" "btn up" "key 2 2" frame | grep -q "Graph: z isn't defined" && echo "Graph with z: ok"
# a formula too tall for the small line (a fraction in a power) becomes one line of text
"$OUT/sim" "k 40" "k 49" "k 43" "k 49" "k 47" "k 49" "k 128" "k 49" "k 50" "k 41" "k 94" "k 49" "k 128" "k 49" "k 50" "k 200" frame \
  | grep -q "B (1+1/1E12)^1E12 =" && echo "Tall formula on one line: ok"
# Down is AC; = types the equation sign, then solves
"$OUT/sim" "k 49" "btn down" state | grep -q "STATE \[\]" \
  && "$OUT/sim" "k 50" "k 131" "k 200" state | grep -q "STATE \[2x=\]" && echo "Down = AC, smart =: ok"
# history: three answers, hold Up, tap the second (the integral) -> it comes back as the answer
"$OUT/sim" "k 49" "k 43" "k 50" "k 200" "k 131" "k 133" "k 208" "k 48" "k 200" "k 51" "k 200" \
  "k 50" "k 42" "k 51" "k 200" "long up" "tap 100 80" state | grep -q "STATE \[\] \[int 0 to 3 =\] \[9\]" \
  && echo "History: ok"
# settings from the phone: Down = next key (9) and Select = press it -> highlight moves 7 -> 8, types 8;
# Up held = DEL; Graph on draws the graph; ClearHistory empties the list
"$OUT/sim" "cfg BtnDown 9" "cfg BtnSelect 10" "btn down" "btn select" state | grep -q "STATE \[8\]" \
  && "$OUT/sim" "cfg BtnUpHold 4" "k 49" "k 50" "long up" state | grep -q "STATE \[1\]" \
  && "$OUT/sim" "cfg Graph 1" "k 131" "k 133" "k 61" "k 50" "k 200" anim frame | grep -q "line .* ffaa00 2" \
  && "$OUT/sim" "k 49" "k 200" "cfg ClearHistory 1" "long up" frame | grep -q "No answers yet" \
  && echo "Phone settings: ok"
# symbolic: d/dx with "at x =" left empty gives the formula (radians); factor; expand
"$OUT/sim" "k 204" "k 131" "k 133" "k 138" "k 131" "k 41" "k 209" "k 200" state \
  | grep -q "STATE \[2x\*sin(x)+x^2cos(x)\]" \
  && "$OUT/sim" "k 204" "k 131" "k 138" "k 131" "k 41" "k 208" "k 200" state | grep -q "\[sin(x)-x\*cos(x) + C\]" \
  && "$OUT/sim" "k 131" "k 133" "k 45" "k 131" "k 45" "k 54" "k 210" state | grep -q "STATE \[(x+2)(x-3)\]" \
  && "$OUT/sim" "k 40" "k 131" "k 43" "k 49" "k 41" "k 94" "k 51" "k 211" state | grep -q "STATE \[x^3+3x^2+3x+1\]" \
  && echo "Symbolic: ok"
# graph: x^2-3 from the Graph key opens the graph; x^2+y^2=25 is drawn as a curve
"$OUT/sim" "k 131" "k 133" "k 45" "k 51" "btn up" "btn up" "key 2 2" frame | grep -q "GRAPH" \
  && "$OUT/sim" "k 131" "k 133" "k 43" "k 151" "k 133" "k 61" "k 50" "k 53" "btn up" "btn up" "key 2 2" frame \
     | grep -c "line .* ffaa00 2" | awk '{exit !($1 > 40)}' \
  && ! "$OUT/sim" "k 131" "k 133" "btn up" "btn up" "key 2 2" "btn back" frame | grep -q "GRAPH" \
  && echo "Graph: ok"
# more functions: 5 nCr 2 = 10, gcd(12,18) = 6
"$OUT/sim" "k 53" "k 167" "k 50" "k 200" state | grep -q "\[10\]" \
  && "$OUT/sim" "k 165" "k 49" "k 50" "k 44" "k 49" "k 56" "k 200" state | grep -q "\[6\]" \
  && echo "More functions: ok"
# from the d/dx prompt, Factor factors the function; x^2+y^2=25 then = = keeps asking (no error),
# and Graph graphs the first equation as a curve
"$OUT/sim" "k 131" "k 133" "k 45" "k 131" "k 45" "k 54" "k 209" "k 210" state | grep -q "STATE \[(x+2)(x-3)\]" \
  && "$OUT/sim" "k 131" "k 133" "k 43" "k 151" "k 133" "k 61" "k 50" "k 53" "k 200" "k 200" state | grep -q "2nd equation?" \
  && "$OUT/sim" "k 131" "k 133" "k 43" "k 151" "k 133" "k 61" "k 50" "k 53" "k 200" "btn up" "btn up" "key 2 2" frame \
     | grep -c "line .* ffaa00 2" | awk '{exit !($1 > 40)}' \
  && echo "Prompts and graph from them: ok"
echo "Preview: $OUT/sim <steps> frame | python3 render.py out.png 2"
