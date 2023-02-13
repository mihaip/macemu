#!/bin/bash
set -euo pipefail

./autogen.sh \
  --without-esd \
  --without-gtk \
  --disable-fbdev-dga \
  --disable-xf86-vidmode \
  --disable-xf86-dga \
  --disable-jit-compiler \
  --disable-asm-opts \
  --disable-vosf \
  --enable-uae_cpu_2021 \
  --build="`uname -m`-unknown-linux-gnu" \
  --cache-file=/tmp/config.cache.native

make clean
# Make sure that CPU generator for other architectures are cleaned up.
rm -rf obj/*
mkdir -p obj
make -j8 cpuemu.cpp
