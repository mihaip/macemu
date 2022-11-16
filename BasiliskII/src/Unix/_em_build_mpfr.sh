#!/bin/bash
set -euo pipefail

mkdir -p /emlibs
cd /tmp

wget https://gmplib.org/download/gmp/gmp-6.2.1.tar.lz
tar xf gmp-6.2.1.tar.lz
cd gmp-6.2.1
emconfigure ./configure \
    --disable-assembly \
    --with-readline=no \
    --enable-cxx \
    --host none \
    --prefix=/emlibs
make -j6
make install
cd ..

wget https://www.mpfr.org/mpfr-current/mpfr-4.1.0.tar.xz
tar xf mpfr-4.1.0.tar.xz
cd mpfr-4.1.0
emconfigure ./configure \
    --enable-assert=none \
    --disable-thread-safe \
    --disable-float128 \
    --disable-decimal-float \
    --host none \
    --with-gmp=/emlibs \
    --prefix=/emlibs
make -j6
make install
cd ..
