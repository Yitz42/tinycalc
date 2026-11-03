#
# Standard Pebble SDK build script: C watchapp plus the phone-side settings page (src/pkjs).
#
import os.path
import glob
import subprocess
import sys

top = '.'
out = 'build'


def options(ctx):
    ctx.load('pebble_sdk')


def configure(ctx):
    ctx.load('pebble_sdk')


# Overlays: code kept in resources and loaded onto the heap when used, so it
# doesn't count against the 64 KB app limit: math (root finder, exact answers,
# x/y systems, integrals, derivatives), cas (symbolic math), graph, and fmt
# (the typesetter, loaded at start and kept). Rebuilt with the SDK's compiler when its sources
# change; see tools/build_overlay.py.
OVERLAYS = {
    'resources/ovl/math.ovl': ['src/ovl/ovl_math.c', 'src/ovl/calcx.c', 'src/ovl/sheet.c', 'src/ovl/exact.c', 'src/ovl/ovl_rt.c',
                               'src/c/calc.c', 'src/c/mmath.c'],
    'resources/ovl/cas.ovl': ['src/ovl/ovl_cas.c', 'src/ovl/cas.c', 'src/ovl/ovl_rt.c',
                              'src/c/calc.c', 'src/c/mmath.c'],
    'resources/ovl/graph.ovl': ['src/ovl/ovl_graph.c', 'src/ovl/graph.c', 'src/ovl/ovl_rt.c',
                                'src/c/calc.c', 'src/c/mmath.c'],
    'resources/ovl/fmt.ovl': ['src/ovl/ovl_fmt.c', 'src/ovl/mathfmt.c', 'src/ovl/ovl_rt.c',
                              'src/c/calc.c', 'src/c/mmath.c'],
    'resources/ovl/tools.ovl': ['src/ovl/ovl_tools.c', 'src/ovl/tools.c', 'src/ovl/ovl_rt.c',
                                'src/c/calc.c', 'src/c/mmath.c'],
    'resources/ovl/cplx.ovl': ['src/ovl/ovl_cplx.c', 'src/ovl/cplx.c', 'src/ovl/ovl_rt.c',
                               'src/c/calc.c', 'src/c/mmath.c'],
    'resources/ovl/calcu.ovl': ['src/ovl/ovl_calcu.c', 'src/ovl/calcx.c', 'src/ovl/ovl_rt.c',
                                'src/c/calc.c', 'src/c/mmath.c'],
}


def build_overlays(ctx):
    top = ctx.path.abspath()
    deps = glob.glob(os.path.join(top, 'src/c/*.h')) + glob.glob(os.path.join(top, 'src/ovl/*.h')) + [os.path.join(top, 'src/ovl/ovl.ld'),
                                                         os.path.join(top, 'tools/build_overlay.py')]
    for out, srcs in OVERLAYS.items():
        out_abs = os.path.join(top, out)
        files = [os.path.join(top, s) for s in srcs] + deps
        if os.path.exists(out_abs) and os.path.getmtime(out_abs) >= max(os.path.getmtime(f) for f in files):
            continue
        print('Building overlay %s' % out)
        subprocess.check_call([sys.executable, os.path.join(top, 'tools/build_overlay.py'), out_abs] +
                              [os.path.join(top, s) for s in srcs])


def build(ctx):
    ctx.load('pebble_sdk')
    build_overlays(ctx)

    binaries = []
    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_build(source=ctx.path.ant_glob('src/c/**/*.c'), target=app_elf, bin_type='app')
        binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.env = cached_env

    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js', 'src/pkjs/**/*.json']),
                   js_entry_file='src/pkjs/index.js')
