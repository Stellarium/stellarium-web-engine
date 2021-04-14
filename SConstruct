import glob
import os
import sys

vars = Variables()
vars.AddVariables(
    EnumVariable('mode', 'Build mode', 'debug',
        allowed_values=('debug', 'release', 'profile')),
    BoolVariable('es6', 'Create ES6 js module', False),
    BoolVariable('werror', 'Warnings as error', True),
)

VariantDir('build/src', 'src', duplicate=0)
VariantDir('build/ext_src', 'ext_src', duplicate=0)
env = Environment(variables=vars)

env.Append(CFLAGS= '-Wall -std=gnu11 -Wno-unknown-pragmas -D_GNU_SOURCE '
                   '-Wno-missing-braces',
           CXXFLAGS='-Wall -std=gnu++11 -Wno-narrowing '
                    '-Wno-unknown-pragmas -Wno-unused-function')

if env['werror']:
    env.Append(CCFLAGS='-Werror')

if env['mode'] == 'debug':
    env.Append(CCFLAGS=['-O0', '-DCOMPILE_TESTS'])

if env['mode'] in ['profile', 'debug']:
    env.Append(CCFLAGS='-g', LINKFLAGS='-g')

if env['mode'] != 'debug':
    env.Append(CCFLAGS='-DNDEBUG')

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

sources += glob.glob('ext_src/md4c/*.c')
env.Append(CPPPATH=['ext_src/md4c'])
env.Append(CFLAGS=['-DMD4C_USE_UTF8'])

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

for fname in ['alpha_processing', 'dec', 'filters', 'lossless', 'rescaler',
        'upsampling', 'yuv']:
    sources += ('ext_src/webp/src/dsp/' + fname + '.c', )

env.Append(CPPPATH=['ext_src/webp'])
env.Append(CPPPATH=['ext_src/webp/src'])

sources = ['build/%s' % x for x in sources]

if not env.GetOption('clean'):
    assert(os.environ['EMSCRIPTEN_TOOL_PATH'])
    # EMSCRIPTEN_ROOT need to be set, but current emscripten version doesn't
    # provide it. Here we set it automatically from EMSCRIPTEN_TOOL_PATH:
    os.environ['EMSCRIPTEN_ROOT'] = os.path.join(
        os.environ['EMSCRIPTEN_TOOL_PATH'], '../../../../../')
    env.Tool('emscripten', toolpath=[os.environ['EMSCRIPTEN_TOOL_PATH']])

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
         '-s', 'ALLOW_TABLE_GROWTH=1',
         '--pre-js', 'src/js/pre.js',
         '--pre-js', 'src/js/obj.js',
         '--pre-js', 'src/js/geojson.js',
         '--pre-js', 'src/js/canvas.js',
         # '-s', 'STRICT=1', # Note: to put back once we switch to emsdk 2
         '-s', 'RESERVED_FUNCTION_POINTERS=10',
         '-O3',
         '-s', 'USE_WEBGL2=1',
         '-s', 'NO_EXIT_RUNTIME=1',
         '-s', '"EXPORTED_FUNCTIONS=[]"',
         '-s', '"EXTRA_EXPORTED_RUNTIME_METHODS=[%s]"' % extra_exported,
         '-s', 'FILESYSTEM=0'
        ]

#if env['mode'] not in ['profile', 'debug']:
#    flags += ['--closure', '1']

if env['mode'] in ['profile', 'debug']:
    flags += [
        '--profiling',
        '-s', 'ASM_JS=2', # Removes 'use asm'.
    ]

if env['mode'] == 'debug':
    flags += ['-s', 'SAFE_HEAP=1', '-s', 'ASSERTIONS=1',
              '-s', 'WARN_UNALIGNED=1']

if env['es6']:
    flags += ['-s', 'EXPORT_ES6=1', '-s', 'USE_ES6_IMPORT_META=0']

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
