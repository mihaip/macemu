#!/bin/bash

export macemujs_conf_worker=$macemujs_conf_worker
source ./_emenv.sh

CFLAGS="-I/opt/X11/include -Iem_config.h $EMFLAGS -g"
CPPFLAGS="-I/opt/X11/include $EMFLAGS -g"
LDFLAGS="-L/opt/X11/lib"
DEFINES="-DDEBUG"

if [[ -z "$macemujs_conf_native" ]]; then
  echo "building for emscripten"
  # TODO: not use EMSCRIPTEN var
  export CC="$EMSCRIPTEN/emcc"
  export CXX="$EMSCRIPTEN/em++"
  export AR="$EMSCRIPTEN/emar"
  export EMSCRIPTEN=1
else
  echo "building for native"
fi

# env CFLAGS=$CFLAGS CPPFLAGS=$CPPFLAGS LDFLAGS=$LDFLAGS
./autogen.sh \
  --without-esd \
  --without-gtk \
  --disable-fbdev-dga \
  --disable-xf86-vidmode \
  --disable-xf86-dga \
  --disable-jit-compiler \
  --enable-addressing="banks" \
  --enable-sdl-video \
  --disable-vosf \
  # --enable-sdl-audio \

if [[ -z "$macemujs_conf_native" ]]; then
  cat ./em_config.h >> ./config.h
else
  {
    echo "#define USE_CPU_EMUL_SERVICES 1"
    echo "#undef AQUA"
    echo "#undef HAVE_FRAMEWORK_COREFOUNDATION"
    echo "#undef HAVE_MACH_EXCEPTIONS"
    echo "#undef HAVE_LIBPOSIX4"
    echo "#undef HAVE_LIBRT"
  } >> ./config.h
fi