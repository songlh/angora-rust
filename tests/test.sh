#!/bin/sh
set -eux

BUILD_TYPE="debug"
num_jobs=1
sync_afl=""
LOG_TYPE=angora
MODE="pin"
MODE="llvm"


if [ ! -z ${RELEASE+x} ]; then
    BUILD_TYPE="release"
fi

if [ ! -z ${LLVM_MODE+x} ]; then
    MODE="llvm"
fi

if [ ! -z ${PIN_MODE+x} ]; then
    MODE="pin"
fi


envs="RUST_BACKTRACE=1 RUST_LOG=${LOG_TYPE}"
fuzzer="../target/${BUILD_TYPE}/fuzzer"


if [ "$#" -ne 1 ] || ! [ -d "$1" ]; then
    echo "Usage: $0 DIRECTORY" >&2
    exit 1
fi

name=$1

echo "Compile..."

target=${name}/${name}

rm -f ${target}.fast ${target}.cmp ${target}.taint 


bin_dir=../bin/
ANGORA_USE_ASAN=1 USE_FAST=1 ${bin_dir}/angora-clang ${target}.c -lz -o ${target}.fast
