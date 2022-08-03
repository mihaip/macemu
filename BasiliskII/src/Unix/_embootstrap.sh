#!/bin/bash
set -euo pipefail

./autogen.sh \
  --without-esd \
  --without-gtk \
  --disable-fbdev-dga \
  --disable-xf86-vidmode \
  --disable-xf86-dga \
  --disable-jit-compiler \
  --enable-addressing="banks" \
  --disable-vosf \
  --cache-file=/tmp/config.cache.native

make clean
make -j6 cpuemu.cpp
