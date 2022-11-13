#!/bin/bash
set -euo pipefail

em_flags=""
em_ldflags=""

em_ldflags+=" -s INITIAL_MEMORY=268435456"
em_ldflags+=" -s FORCE_FILESYSTEM=1"
em_ldflags+=" -s WASM=1"

em_defines=""
if [[ -z "${macemujs_conf_debug:-}" ]]; then
  em_flags+=" -O3 -gsource-map"
else
  echo "Debug build"
  em_flags+=" -O0 -gsource-map"
  em_ldflags+=" -s ASSERTIONS=2 "
  em_ldflags+=" -s DEMANGLE_SUPPORT=1"
  em_defines+=" -DDEBUG"
fi

export CC="emcc"
export CXX="em++"
export AR="emar"
export EMSCRIPTEN=1
export DEFINES=$em_defines
export CFLAGS="$em_flags -g"
export CPPFLAGS="$em_flags -g"
export LDFLAGS="$em_flags $em_ldflags"

./autogen.sh \
  --without-esd \
  --without-gtk \
  --disable-sdl-video \
  --disable-sdl-audio \
  --disable-fbdev-dga \
  --disable-xf86-vidmode \
  --disable-xf86-dga \
  --disable-jit-compiler \
  --disable-asm-opts \
  --disable-vosf \
  --enable-uae_cpu_2021 \
  --enable-emscripten \
  --build="`uname -m`-unknown-linux-gnu" \
  --cache-file="/tmp/basiliskii.config.cache.emscripten${macemujs_conf_debug:-}"

cat ./em_config.h >> ./config.h

# use 'mostlyclean' because 'clean' will delete the generated CPU files
# `make mostlyclean` is required when switching between native and web builds, to remove the native .o files
make mostlyclean
