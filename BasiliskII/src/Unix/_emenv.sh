use_wasm=""

if [[ -n "${macemujs_conf_wasm:-}" ]]; then
  use_wasm="y"
fi
emflgs=""
emflgs+=" -s TOTAL_MEMORY=536870912"
emflgs+=" -s FORCE_FILESYSTEM=1"
if [ -z $use_wasm ]; then
emflgs+=" -s ASM_JS=1"
emflgs+=" -s WASM=0"
emflgs+=" -D__ELF__"
# emflgs+=" -s BINARYEN_METHOD='asmjs'"
else
emflgs+=" -D__wasm__"
emflgs+=" -s WASM=1"
# emflgs+=" -s BINARYEN_METHOD='native-wasm'"
fi

use_emterpreter=""
if [ -n "$use_emterpreter" ]; then
emflgs+=" -s EMTERPRETIFY=1"
emflgs+=" -s EMTERPRETIFY_ASYNC=1"
emflgs+=" -s ASSERTIONS=1"
# emflgs+=" -s EMTERPRETIFY_WHITELIST=$(node ./_em_whitelist_funcs.js)"
fi


# emflgs+=" --js-library ./src/Unix/js/library_workerthread.js"

defines=""
emenv_debug=""
if [ -z $emenv_debug ]; then
emflgs+=" -O3 -g4"
elif [ -n "$use_emterpreter" ]; then
emflgs+=" -O3 -g3" # sourcemaps not supported in emterpreter
defines+=" -DDEBUG"
else
emflgs+=" -O0 -g4"
# emflgs+=" -s UNALIGNED_MEMORY=1 " # not supported in fastcomp
# emflgs+=" -s SAFE_HEAP=1 "
# emflgs+=" -s SAFE_HEAP_LOG=1 "
emflgs+=" -s ASSERTIONS=2 "
emflgs+=" -s DEMANGLE_SUPPORT=1"
defines+=" -DDEBUG"
fi

if [[ -z "${macemujs_conf_mainthread:-}" ]]; then
  echo "EMSCRIPTEN_SAB=1"
  emflgs+=" -DEMSCRIPTEN_SAB=1"
else
  echo "EMSCRIPTEN_MAINTHREAD=1"
  emflgs+=" -DEMSCRIPTEN_MAINTHREAD=1"
fi

export macemujs_conf_pthreads=""
if [[ -n "$macemujs_conf_pthreads" ]]; then
  echo "USE_PTHREADS=1"
  emflgs+=" -s USE_PTHREADS=1"
  emflgs+=" -s PTHREAD_POOL_SIZE=2"
fi

export EMFLAGS=$emflgs
export DEFINES=$defines

if [[ -z "${macemujs_conf_native:-}" ]]; then
  echo "building for emscripten"
  export CC="emcc"
  export CXX="em++"
  export AR="emar"
  export EMSCRIPTEN=1
  export CFLAGS="-I/opt/X11/include -Iem_config.h $EMFLAGS -g"
  export CPPFLAGS="-I/opt/X11/include $EMFLAGS -g"
  export LDFLAGS="-L/opt/X11/lib $EMFLAGS"
  echo "with flags '$EMFLAGS'"
else
  echo "building for native"
fi
