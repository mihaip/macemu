#!/bin/bash
set -euo pipefail

export macemujs_conf_worker=${macemujs_conf_worker:-}
source ./_emenv.sh

PLATFORM_FLAGS=""
if [[ -n "${macemujs_conf_native:-}" ]]; then
PLATFORM_FLAGS+=" --enable-sdl-audio"
fi

# env CFLAGS=$CFLAGS CPPFLAGS=$CPPFLAGS LDFLAGS=$LDFLAGS
# CFLAGS="-g -fsanitize=address" CPPFLAGS="-g -fsanitize=address" LDFLAGS="-g -fsanitize=address" \
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
  $PLATFORM_FLAGS

if [[ -z "${macemujs_conf_native:-}" ]]; then
  cat ./em_config.h >> ./config.h
else
  {
    echo "#define USE_CPU_EMUL_SERVICES 1"
    # echo "#undef __MACH__"
    # echo "#undef __APPLE__"
    echo "#undef AQUA"
    echo "#undef HAVE_FRAMEWORK_COREFOUNDATION"
    echo "#undef HAVE_MACH_EXCEPTIONS"
    echo "#undef HAVE_LIBPOSIX4"
    echo "#undef HAVE_LIBRT"
    # echo "#undef HAVE_MACH_VM"
    # echo "#undef HAVE_MMAP_VM"
    # echo "#undef EMSCRIPTEN"
  } >> ./config.h
fi