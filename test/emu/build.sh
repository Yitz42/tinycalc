#!/bin/sh
# Builds the fuzzer for the PC and for qemu-arm. ARM_GCC_BIN: the ARM gcc's bin folder.
set -e
cd "$(dirname "$0")"
S=../../src
OUT=${OUT:-/tmp/solver-calc-emu}
mkdir -p "$OUT"
gcc -std=c99 -D_DEFAULT_SOURCE -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -ffp-contract=off \
  -I$S/c -o "$OUT/fuzz-pc" fuzz.c $S/c/calc.c $S/ovl/cplx.c $S/c/mmath.c $S/ovl/calcx.c $S/ovl/exact.c $S/ovl/cas.c \
  $S/ovl/graph.c $S/ovl/mathfmt.c -lm
X=${ARM_GCC_BIN:?set ARM_GCC_BIN}
for o in math cas graph fmt tools cplx calcu; do
  (cd ../../resources/ovl && "$X/arm-none-eabi-objcopy" -I binary -O elf32-littlearm -B arm $o.ovl "$OUT/$o.o")
done
"$X/arm-none-eabi-gcc" -mcpu=cortex-m3 -mthumb -mfloat-abi=soft -Os -std=c99 -ffreestanding -fno-builtin \
  -fno-tree-loop-distribute-patterns -ffp-contract=off -DEMU $EXTRA -I. -I$S/c -nostdlib -nostartfiles -static -Wl,-e,_start \
  -Wl,--no-warn-rwx-segments -o "$OUT/fuzz-arm" fuzz.c emu_rt.c $S/c/calc.c $S/c/mmath.c $S/c/ovl.c $S/c/ovl_calls.c \
  "$OUT/math.o" "$OUT/cas.o" "$OUT/graph.o" "$OUT/fmt.o" "$OUT/tools.o" "$OUT/cplx.o" "$OUT/calcu.o" -lgcc
echo built
