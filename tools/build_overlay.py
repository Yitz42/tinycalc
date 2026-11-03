#!/usr/bin/env python3
"""Build an overlay blob: feature code that lives in the app's resources and
is loaded onto the heap when used (see src/c/ovl_api.h and src/c/ovl.c).

    python3 tools/build_overlay.py resources/ovl/math.ovl src/ovl/ovl_math.c src/ovl/calcx.c ...

It compiles the source for the watch (ARMv7-M Thumb, soft float, like the
app), links it at address 0 with its own copy of the soft-float helpers,
and writes: header, image, then the offsets of every absolute address in the
image (the loader adds the load address to each).

The compiler is the one in the Pebble SDK; set ARM_GCC_BIN to use another.
"""
import os, re, struct, subprocess, sys, tempfile, glob

MAGIC = 0x314C564F
# Relocations that need nothing at load time (PC-relative inside the image).
PC_RELATIVE = {"R_ARM_THM_CALL", "R_ARM_THM_JUMP24", "R_ARM_THM_JUMP11", "R_ARM_THM_JUMP8",
               "R_ARM_THM_JUMP19", "R_ARM_REL32", "R_ARM_PREL31", "R_ARM_THM_PC8",
               "R_ARM_THM_PC12", "R_ARM_THM_ALU_PREL_11_0", "R_ARM_NONE", "R_ARM_V4BX"}
CFLAGS = ["-mcpu=cortex-m3", "-mthumb", "-mfloat-abi=soft", "-Os", "-std=c99",
          "-ffreestanding", "-fno-common", "-ffunction-sections", "-fdata-sections",
          "-fno-pic", "-mno-unaligned-access", "-fno-builtin", "-fno-tree-loop-distribute-patterns",
          "-Wall", "-Wno-unused-function", "-Werror"]


def toolchain():
    d = os.environ.get("ARM_GCC_BIN")
    if d:
        return d
    sdk = os.path.expanduser("~/Library/Application Support/Pebble SDK/SDKs")
    for c in sorted(glob.glob(os.path.join(sdk, "*", "toolchain", "arm-none-eabi", "bin")), reverse=True):
        if os.path.exists(os.path.join(c, "arm-none-eabi-gcc")):
            return c
    sys.exit("Can't find arm-none-eabi-gcc: set ARM_GCC_BIN to its folder")


def run(*cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.exit("%s\n%s%s" % (" ".join(cmd), r.stdout, r.stderr))
    return r.stdout


def main():
    out, srcs = sys.argv[1], sys.argv[2:]
    src = srcs[0]
    tc = toolchain()
    gcc = os.path.join(tc, "arm-none-eabi-gcc")
    here = os.path.dirname(os.path.abspath(src))
    with tempfile.TemporaryDirectory() as tmp:
        objs, elf = [], os.path.join(tmp, "o.elf")
        for i, s_ in enumerate(srcs):
            objs.append(os.path.join(tmp, "o%d.o" % i))
            run(gcc, *CFLAGS, "-c", s_, "-o", objs[-1])
        run(gcc, *CFLAGS, "-nostdlib", "-nostartfiles", "-T", os.path.join(here, "ovl.ld"),
            "-Wl,--emit-relocs", "-Wl,--gc-sections", *objs, "-lgcc", "-o", elf)
        image = os.path.join(tmp, "image.bin")
        run(os.path.join(tc, "arm-none-eabi-objcopy"), "-O", "binary", "-j", ".image", elf, image)
        data = open(image, "rb").read()
        syms = run(os.path.join(tc, "arm-none-eabi-nm"), elf)
        entry = int(re.search(r"^([0-9a-f]+) T ovl_main$", syms, re.M).group(1), 16)
        rels = []
        cur = None
        for line in run(os.path.join(tc, "arm-none-eabi-readelf"), "-rW", elf).splitlines():
            m = re.match(r"Relocation section '\.rel\.(\S+)'", line)
            if m:
                cur = m.group(1)
                continue
            m = re.match(r"^([0-9a-f]{8})\s+[0-9a-f]+\s+(R_ARM_\w+)", line)
            if not m or cur is None:
                continue
            off, kind = int(m.group(1), 16), m.group(2)
            if kind == "R_ARM_ABS32":
                rels.append(off)
            elif kind not in PC_RELATIVE:
                sys.exit("unsupported relocation %s at 0x%x in %s" % (kind, off, src))
    size = (len(data) + 3) & ~3
    data += b"\0" * (size - len(data))
    rels = sorted(set(rels))
    blob = struct.pack("<4I", MAGIC, size, entry, len(rels)) + data + b"".join(struct.pack("<I", r) for r in rels)
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    open(out, "wb").write(blob)
    print("%s: %d bytes of code and data, %d address fix-ups, entry 0x%x" % (out, size, len(rels), entry))


if __name__ == "__main__":
    main()
