import glob
import os
import sys

# CCFLAGS   : C and C++
# CFLAGS    : only C
# CXXFLAGS  : only C++

target_os = str(Platform())

debug = int(ARGUMENTS.get('debug', 1))
profile = int(ARGUMENTS.get('profile', 0))
emscripten = int(ARGUMENTS.get('emscripten', 0))
werror = int(ARGUMENTS.get("werror", 1))
analyze = int(ARGUMENTS.get("analyze", 0))
es6 = int(ARGUMENTS.get("es6", 0))
remotery = int(ARGUMENTS.get('remotery', 0))

if emscripten: target_os = 'js'

VariantDir('build/src', 'src', duplicate=0)
VariantDir('build/ext_src', 'ext_src', duplicate=0)
env = Environment()

if analyze:
    # Make sure clang static analyzer has a chance to override de compiler
    # and set CCC settings
    env["CC"] = os.getenv("CC") or env["CC"]
    env["CXX"] = os.getenv("CXX") or env["CXX"]
    env["ENV"].update(x for x in os.environ.items() if x[0].startswith("CCC_"))

if debug and target_os == 'posix':
    env.Append(CCFLAGS=['-fsanitize=address', '-fsanitize=undefined'],
               LIBS=['asan', 'ubsan'])

env.Append(CFLAGS= '-Wall -std=gnu11 -Wno-unknown-pragmas -D_GNU_SOURCE',
           CXXFLAGS='-Wall -std=gnu++11 -Wno-narrowing '
                    '-Wno-unknown-pragmas -Wno-unused-function')

# Attempt to fix a bug with imgui compilation.
# XXX: should update imgui version instead.
if target_os != 'js':
    env.Append(CXXFLAGS='-Wno-class-memaccess')

if werror:
    env.Append(CCFLAGS='-Werror')

if debug:
    env.Append(CCFLAGS=['-O0', '-DCOMPILE_TESTS'])

if profile or debug:
    env.Append(CCFLAGS='-g', LINKFLAGS='-g')

if not debug:
    env.Append(CCFLAGS='-DNDEBUG')
    if target_os != 'js':
         env.Append(CCFLAGS='-Ofast', LINKFLAGS='-Ofast')
    if target_os != 'js' and not profile:
        env.Append(CCFLAGS='-flto', LINKFLAGS='-flto')

sources = (glob.glob('src/*.c*') + glob.glob('src/algos/*.c') +
           glob.glob('src/projections/*.c') + glob.glob('src/modules/*.c') +
           glob.glob('src/utils/*.c') + glob.glob('src/private/*.c'))
env.Append(CPPPATH=['src'])

env.Append(CCFLAGS='-include config.h')

sources += glob.glob('ext_src/erfa/*.c')
env.Append(CPPPATH=['ext_src/erfa'])

sources += glob.glob('ext_src/json/*.c')
env.Append(CPPPATH=['ext_src/json'])

env.Append(CPPPATH=['ext_src/uthash'])
env.Append(CPPPATH=['ext_src/stb'])

sources += glob.glob('ext_src/zlib/*.c')
env.Append(CPPPATH=['ext_src/zlib'])
env.Append(CFLAGS=['-DHAVE_UNISTD_H'])

sources += glob.glob('ext_src/inih/*.c')
env.Append(CPPPATH=['ext_src/inih'])

sources += glob.glob('ext_src/nanovg/*.c')
env.Append(CPPPATH=['ext_src/nanovg'])

if remotery:
    env.Append(CPPPATH=['ext_src/remotery'])
    env.Append(CFLAGS=['-DRMT_ENABLED=1'])

# Add webp
sources += (
    'ext_src/webp/src/dec/alpha_dec.c',
    'ext_src/webp/src/dec/buffer_dec.c',
    'ext_src/webp/src/dec/frame_dec.c',
    'ext_src/webp/src/dec/idec_dec.c',
    'ext_src/webp/src/dec/io_dec.c',
    'ext_src/webp/src/dec/quant_dec.c',
    'ext_src/webp/src/dec/tree_dec.c',
    'ext_src/webp/src/dec/vp8_dec.c',
    'ext_src/webp/src/dec/vp8l_dec.c',
    'ext_src/webp/src/dec/webp_dec.c',
    'ext_src/webp/src/utils/bit_reader_utils.c',
    'ext_src/webp/src/utils/color_cache_utils.c',
    'ext_src/webp/src/utils/filters_utils.c',
    'ext_src/webp/src/utils/huffman_utils.c',
    'ext_src/webp/src/utils/quant_levels_dec_utils.c',
    'ext_src/webp/src/utils/random_utils.c',
    'ext_src/webp/src/utils/rescaler_utils.c',
    'ext_src/webp/src/utils/thread_utils.c',
    'ext_src/webp/src/utils/utils.c',
    'ext_src/webp/src/dsp/cpu.c',
    'ext_src/webp/src/dsp/dec_clip_tables.c')

# SIMD specific WepP sources
simd = []
if target_os == 'posix':
    simd += ['sse2', 'sse41']
    env.Append(CCFLAGS='-DWEBP_USE_THREAD')

# Emscripten supports SSE1 SSE2 and SSE3
# https://kripken.github.io/emscripten-site/docs/porting/simd.html
# But webp specifically disables SSE2 in emscripten because of a clang issue
# (see dsp.h and README.webp_js)
# We can force SSE2 optims enabling the below flag, should we?
if 0 and target_os == 'js':
    simd += ['sse2']
    env.Append(CCFLAGS='-DWEBP_USE_SSE2')
    # We also need to add this emscripten compilation flag '-msse2'

for fname in ['alpha_processing', 'dec', 'filters', 'lossless', 'rescaler',
        'upsampling', 'yuv']:
    sources += ('ext_src/webp/src/dsp/' + fname + '.c', )
    for s in simd:
        f = 'ext_src/webp/src/dsp/' + fname + '_' + s + '.c'
        if os.path.isfile(f):
            sources += (f, )

env.Append(CPPPATH=['ext_src/webp'])
env.Append(CPPPATH=['ext_src/webp/src'])

if not emscripten:
    sources += glob.glob('ext_src/imgui/*.cpp')
    env.Append(CPPPATH=['ext_src/imgui'])

if target_os == 'posix':
    env.Append(CCFLAGS='-DHAVE_PTHREAD')
    env.Append(LIBS=['curl', 'GL', 'm', 'z', 'pthread'])
    env.ParseConfig('pkg-config --libs glfw3')

sources = ['build/%s' % x for x in sources]

if target_os == 'js':
    assert(os.environ['EMSCRIPTEN_TOOL_PATH'])
    # EMSCRIPTEN_ROOT need to be set, but current emscripten version doesn't
    # provide it. Here we set it automatically from EMSCRIPTEN_TOOL_PATH:
    os.environ['EMSCRIPTEN_ROOT'] = os.path.join(
        os.environ['EMSCRIPTEN_TOOL_PATH'], '../../../../../')
    # os.environ['EMSCRIPTEN_ROOT'] = '/home/guillaume/emsdk/fastcomp/emscripten'
    env.Tool('emscripten', toolpath=[os.environ['EMSCRIPTEN_TOOL_PATH']])

    # XXX: when I use it I cannot access the exported functions, why?
    if 0:
        env.Append(CCFLAGS=['--closure', '1'],
                   LINKFLAGS=['--closure', '1'])

    # Clang does not like overrided initializers.
    env.Append(CCFLAGS=['-Wno-initializer-overrides'])
    env.Append(CCFLAGS='-DNO_LIBCURL')

    # All the emscripten runtime functions we use.
    # Needed since emscripten 1.37.
    extra_exported = [
        'ALLOC_NORMAL',
        'GL',
        'UTF8ToString',
        '_free',
        '_malloc',
        'addFunction',
        'allocate',
        'ccall',
        'cwrap',
        'getValue',
        'intArrayFromString',
        'lengthBytesUTF8',
        'removeFunction',
        'setValue',
        'stringToUTF8',
        'writeAsciiToMemory',
        'writeArrayToMemory',
    ]
    extra_exported = ','.join("'%s'" % x for x in extra_exported)

    flags = [
             '-s', 'MODULARIZE=1', '-s', 'EXPORT_NAME=StelWebEngine',
             '-s', 'ALLOW_MEMORY_GROWTH=1',
             '--pre-js', 'src/js/pre.js',
             '--pre-js', 'src/js/obj.js',
             '--pre-js', 'src/js/geojson.js',
             '--pre-js', 'src/js/canvas.js',
             '-s', 'STRICT=1',
             '-s', 'RESERVED_FUNCTION_POINTERS=5',
             '-O3',
             '-s', 'USE_WEBGL2=1',
             '-s', 'NO_EXIT_RUNTIME=1',
             '-s', '"EXPORTED_FUNCTIONS=[]"',
             '-s', '"EXTRA_EXPORTED_RUNTIME_METHODS=[%s]"' % extra_exported,
             '-s', 'FILESYSTEM=0'
            ]

    if not profile and not debug:
        flags += ['--llvm-lto', '3']

    if profile or debug:
        flags += [
            '--profiling',
            '-s', 'ASM_JS=2', # Removes 'use asm'.
        ]

    if debug:
        flags += ['-s', 'SAFE_HEAP=1', '-s', 'ASSERTIONS=1',
                  '-s', 'WARN_UNALIGNED=1']

    if es6:
        flags += ['-s', 'EXPORT_ES6=1']

    env.Append(CCFLAGS=['-DNO_ARGP', '-DGLES2 1'] + flags)
    env.Append(LINKFLAGS=flags)
    env.Append(LIBS=['GL'])

    prog = env.Program(target='build/stellarium-web-engine.js', source=sources)
    env.Depends(prog, glob.glob('src/*.js'))
    env.Depends(prog, glob.glob('src/js/*.js'))

    # Copy js files in the html example after build.
    env.Depends('build/stellarium-web-engine.wasm', prog)

env.Program(target='build/stellarium-web-engine', source=sources)

# Ugly hack to run makeasset before each compilation
from subprocess import call
call('./tools/make-assets.py')
