#!/usr/bin/env bash
set -euo pipefail

# Usage: build.sh {rel|dbg|dbg_asan|clean} [native] [install]

if [ $# -lt 1 ]; then
  echo 'build.sh {rel|dbg|dbg_asan|clean} [native] [install]'
  exit 1
fi

command=$1
shift

native_flag=""
install_requested=false

for arg in "$@"; do
  case "$arg" in
    native)
      native_flag="-DDRAMSYS_NATIVE_INTEGRATION=ON"
      ;;
    install)
      install_requested=true
      ;;
    *)
      echo "Unknown option: $arg"
      echo 'build.sh {rel|dbg|dbg_asan|clean} [native] [install]'
      exit 1
      ;;
  esac
done

cmake_args=(-S . -B build)

case "$command" in
  rel)
    cmake_args+=(-DCMAKE_BUILD_TYPE=Release)
    ;;
  dbg)
    cmake_args+=(-DCMAKE_BUILD_TYPE=Debug)
    ;;
  dbg_asan)
    cmake_args+=(-DCMAKE_BUILD_TYPE=DebugWithASan)
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

if [ -n "${SIPU_SDK_PATH:-}" ]; then
  cmake_args+=(-DCMAKE_INSTALL_PREFIX="${SIPU_SDK_PATH}")
fi

if [ -n "$native_flag" ]; then
  cmake_args+=($native_flag)
fi

cmake "${cmake_args[@]}"
cmake --build build --parallel

if [ "$install_requested" = true ]; then
  cmake --install build
fi
