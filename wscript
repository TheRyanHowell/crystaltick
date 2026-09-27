import os.path

top = '.'
out = 'build'


def options(ctx):
    ctx.load('pebble_sdk')


def configure(ctx):
    ctx.load('pebble_sdk')


# Extra warnings on top of the SDK's own -Wall -Wextra -Werror baseline
# (see pebble_sdk_gcc.py). None of these are excluded from -Werror by the
# SDK, so any hit here is a hard build failure, same as the SDK's own set.
EXTRA_CFLAGS = [
    '-Wshadow',
    '-Wpointer-arith',
    '-Wcast-align',
    '-Wwrite-strings',
    '-Wformat=2',
    '-Wredundant-decls',
]
# -Wundef is deliberately omitted: the SDK's own vendored xsffi.h (Moddable
# XS bridge header, pulled in transitively by pebble.h) uses "#if mxWindows"
# without a prior #define, which -Wundef flags even though it's harmless
# vendor code we don't control.


def build(ctx):
    ctx.load('pebble_sdk')
    build_worker = os.path.exists('worker_src')
    binaries = []
    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.env.append_value('CFLAGS', EXTRA_CFLAGS)
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_build(source=ctx.path.ant_glob('src/c/**/*.c'),
                       target=app_elf, bin_type='app')
        if build_worker:
            worker_elf = '{}/pebble-worker.elf'.format(ctx.env.BUILD_DIR)
            binaries.append({'platform': platform, 'app_elf': app_elf,
                              'worker_elf': worker_elf})
            ctx.pbl_build(source=ctx.path.ant_glob('worker_src/c/**/*.c'),
                           target=worker_elf, bin_type='worker')
        else:
            binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.env = cached_env
    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                    js=ctx.path.ant_glob(['src/pkjs/**/*.js',
                                           'src/pkjs/**/*.json']),
                    js_entry_file='src/pkjs/index.js')
