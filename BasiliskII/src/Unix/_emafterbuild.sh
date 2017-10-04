#!/bin/bash

export macemujs_conf_worker=$macemujs_conf_worker
source ./_emenv.sh

echo "converting bitcode with flags '$EMFLAGS'"

cp ./BasiliskII ./BasiliskII.bc
emcc \
  ./BasiliskII.bc \
  -o ./BasiliskII.js \
  $EMFLAGS 
  # --preload-file MacOS753 \
  # --preload-file DCImage.img \
  # --preload-file Quadra-650.rom \
  # --preload-file prefs \
