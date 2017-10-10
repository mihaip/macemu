emflgs=""
emflgs+=" -s TOTAL_MEMORY=536870912"
emflgs+=" -s ASM_JS=1"

emenv_debug=""
if [ -z $emenv_debug ]; then
emflgs+=" -O3 -g4"
else
emflgs+=" -O0 -g4"
# emflgs+=" -s SAFE_HEAP=1 -s"
emflgs+=" -s ASSERTIONS=2"
emflgs+=" -s DEMANGLE_SUPPORT=1"
fi

if [[ -n "$macemujs_conf_worker" ]]; then
  echo "BUILD_AS_WORKER=1"
  emflgs+=" -s BUILD_AS_WORKER=1"
fi

export macemujs_conf_pthreads=""
if [[ -n "$macemujs_conf_pthreads" ]]; then
  echo "USE_PTHREADS=1"
  emflgs+=" -s USE_PTHREADS=1"
  emflgs+=" -s PTHREAD_POOL_SIZE=2"
fi

export EMFLAGS=$emflgs