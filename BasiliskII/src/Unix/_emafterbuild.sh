#!/bin/bash

export macemujs_conf_worker=$macemujs_conf_worker
export macemujs_conf_mainthread=$macemujs_conf_mainthread
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

# instrument malloc and free
# sed -i '' -E 's/function _free\(\$0\) {$/var _prevmalloc = _malloc;_malloc = function _wrapmalloc($0){$0 = $0|0;var $1=_prevmalloc($0);memAllocAdd($1);return ($1|0);};function _free(\$0) {memAllocRemove(\$0);/' ./BasiliskII.js




if [[ -n "$macemujs_conf_mainthread" ]]; then
  cp -a ./BasiliskII.* ./mainthread/
fi
