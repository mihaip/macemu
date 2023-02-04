#!/bin/bash
set -euo pipefail

mkdir -p /emlibs
cd /tmp

wget https://gmplib.org/download/gmp/gmp-6.2.1.tar.lz
tar xf gmp-6.2.1.tar.lz
cd gmp-6.2.1
emconfigure ./configure \
    HOST_CC=/usr/bin/cc \
    --disable-assembly \
    --with-readline=no \
    --enable-cxx \
    --host none \
    --prefix=/emlibs
make -j6
make install
cd ..

wget https://www.mpfr.org/mpfr-4.1.1/mpfr-4.1.1.tar.xz
tar xf mpfr-4.1.1.tar.xz
cd mpfr-4.1.1
emconfigure ./configure \
    HOST_CC=/usr/bin/cc \
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
