#!/usr/bin/env bash
set -e

# Usage: build.sh {rel|dbg|dbg_asan|clean} [native] [install]

if [ -z "$1" ]; then
  echo 'build.sh {rel|dbg|dbg_asan|clean} [native] [install]'
  exit 1
fi

NATIVE_FLAG=""
if [ "$2" = 'native' ] || [ "$3" = 'native' ]; then
  NATIVE_FLAG="-DDRAMSYS_NATIVE_INTEGRATION=ON"
fi

case "$1" in
  rel)
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${SIPU_SDK_PATH} ${NATIVE_FLAG}
    cmake --build build --parallel
    ;;
  dbg)
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=${SIPU_SDK_PATH} ${NATIVE_FLAG}
    cmake --build build --parallel
    ;;
  dbg_asan)
    cmake -S . -B build -DCMAKE_BUILD_TYPE=DebugWithASan -DCMAKE_INSTALL_PREFIX=${SIPU_SDK_PATH} ${NATIVE_FLAG}
    cmake --build build --parallel
    ;;
  clean)
    rm -rf build
    exit 0
    ;;
  *)
    echo 'build.sh {rel|dbg|dbg_asan|clean} [native] [install]'
    exit 1
    ;;
esac

if [ "$2" = 'install' ] || [ "$3" = 'install' ]; then
  cmake --install build
fi
