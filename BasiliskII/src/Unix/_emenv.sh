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

# emflgs+=" --js-library ./src/Unix/js/library_workerthread.js"

emenv_debug="y"
if [ -z $emenv_debug ]; then
emflgs+=" -O3 -g4"
else
emflgs+=" -O0 -g4"

# emflgs+=" -s UNALIGNED_MEMORY=1 " # not supported in fastcomp
# emflgs+=" -s SAFE_HEAP=1 "
# emflgs+=" -s SAFE_HEAP_LOG=1 "
emflgs+=" -s ASSERTIONS=2 " 
emflgs+=" -s DEMANGLE_SUPPORT=1"
fi

if [[ -n "${macemujs_conf_worker:-}" ]]; then
  echo "DEMSCRIPTEN_SAB_WORKER=1"
  emflgs+=" -s DEMSCRIPTEN_SAB_WORKER=1"
fi

export macemujs_conf_pthreads=""
if [[ -n "$macemujs_conf_pthreads" ]]; then
  echo "USE_PTHREADS=1"
  emflgs+=" -s USE_PTHREADS=1"
  emflgs+=" -s PTHREAD_POOL_SIZE=2"
fi

export EMFLAGS=$emflgs


export DEFINES="-DDEBUG"

if [[ -z "${macemujs_conf_native:-}" ]]; then
  echo "building for emscripten" 
  export CC="emcc"
  export CXX="em++"
  export AR="emar"
  export EMSCRIPTEN=1
  export CFLAGS="-I/opt/X11/include -Iem_config.h $EMFLAGS -g"
  export CPPFLAGS="-I/opt/X11/include $EMFLAGS -g"
  export LDFLAGS="-L/opt/X11/lib"
  echo "with flags '$EMFLAGS'"
else
  echo "building for native"
fi