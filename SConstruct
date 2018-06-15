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

env.Append(CFLAGS= '-Wall -std=gnu11 -Wno-unknown-pragmas',
           CXXFLAGS='-Wall -std=gnu++11 -Wno-narrowing '
                    '-Wno-unknown-pragmas')
if werror:
    env.Append(CCFLAGS='-Werror')

if debug:
    env.Append(CCFLAGS='-DCOMPILE_TESTS')

if profile or debug:
    env.Append(CCFLAGS='-g')
    if target_os != 'js':
        env.Append(CCFLAGS='-Og')

if not debug:
     env.Append(CCFLAGS='-O3 -DNDEBUG', LINKFLAGS='-O3')
     if target_os != 'js':
         env.Append(CCFLAGS='-flto', LINKFLAGS='-flto')

sources = (glob.glob('src/*.c*') + glob.glob('src/algos/*.c') +
           glob.glob('src/projections/*.c') + glob.glob('src/modules/*.c') +
           glob.glob('src/utils/*.c'))
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

if not emscripten:
    sources += glob.glob('ext_src/imgui/*.cpp')
    env.Append(CPPPATH=['ext_src/imgui'])

if target_os == 'posix':
    env.Append(LIBS=['curl', 'GL', 'm', 'z'])
    env.ParseConfig('pkg-config --libs glfw3')

sources = ['build/%s' % x for x in sources]

if target_os == 'js':
    assert(os.environ['EMSCRIPTEN_TOOL_PATH'])
    env.Tool('emscripten', toolpath=[os.environ['EMSCRIPTEN_TOOL_PATH']])

    # XXX: when I use it I cannot access the exported functions, why?
    if 0:
        env.Append(CCFLAGS=['--closure', '1'],
                   LINKFLAGS=['--closure', '1'])

    # Clang does not like overrided initializers.
    env.Append(CCFLAGS=['-Wno-initializer-overrides'])

    # All the emscripten runtime functions we use.
    # Needed since emscripten 1.37.
    extra_exported = [
        'GL',
        'Pointer_stringify',
        'addFunction',
        'cwrap',
        'getValue',
        'removeFunction',
        'setValue',
    ]
    extra_exported = ','.join("'%s'" % x for x in extra_exported)

    flags = [
             '-s', 'MODULARIZE=1', '-s', 'EXPORT_NAME=StelWebEngine',
             '-s', 'TOTAL_MEMORY=268435456', # 256M
             '--pre-js', 'src/js/pre.js',
             '--pre-js', 'src/js/obj.js',
             '--pre-js', 'src/js/canvas.js',
             '--llvm-lto', '3',
             '-s', 'STRICT=1',
             '-s', 'RESERVED_FUNCTION_POINTERS=5',
             '-Os',
             '-s', 'USE_WEBGL2=1',
             '-s', 'NO_EXIT_RUNTIME=1',
             '-s', '"EXPORTED_FUNCTIONS=[]"',
             '-s', '"EXTRA_EXPORTED_RUNTIME_METHODS=[%s]"' % extra_exported,
            ]

    if profile or debug:
        flags += [
            '--profiling',
            '-s', 'ASM_JS=2', # Removes 'use asm'.
        ]

    if debug:
        flags += ['-s', 'SAFE_HEAP=1', '-s', 'ASSERTIONS=1']

    env.Append(CCFLAGS=['-DNO_ARGP', '-DGLES2 1'] + flags)
    env.Append(LINKFLAGS=flags)
    env.Append(LIBS=['GL'])

    prog = env.Program(target='stellarium-web-engine.js', source=sources)
    env.Depends(prog, glob.glob('src/*.js'))
    env.Depends(prog, glob.glob('src/js/*.js'))

    # Copy js files in the html example after build.
    env.Depends('stellarium-web-engine.wasm', prog)
    env.Command('html/static/js/stellarium-web-engine.js',
                'stellarium-web-engine.js', 'cp $SOURCE $TARGET')
    env.Command('html/static/js/stellarium-web-engine.wasm',
                'stellarium-web-engine.wasm',
                'cp $SOURCE $TARGET')

env.Program(target='stellarium-web-engine', source=sources)

# Ugly hack to run makeasset before each compilation
from subprocess import call
call('./tools/makeassets.py')
