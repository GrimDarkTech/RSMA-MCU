#!/usr/bin/env sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
llvm_source="$repo/external/llvm-project/llvm"
llvm_build="$repo/external/llvm-project/build"
install_prefix="${1:-$repo/toolchains/llvm}"

if [ ! -f "$llvm_source/CMakeLists.txt" ]; then
    echo "LLVM submodule is not initialized. Run: git submodule update --init --recursive" >&2
    exit 1
fi

cmake -S "$llvm_source" -B "$llvm_build" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS=clang \
    -DLLVM_TARGETS_TO_BUILD=Native \
    -DLLVM_ENABLE_TERMINFO=OFF \
    -DLLVM_ENABLE_ZLIB=OFF \
    -DLLVM_ENABLE_LIBXML2=OFF \
    -DCMAKE_INSTALL_PREFIX="$install_prefix"

cmake --build "$llvm_build" --target install

llvm_commit=$(git -C "$repo/external/llvm-project" rev-parse HEAD)
mkdir -p "$install_prefix"
manifest="$install_prefix/rsma-llvm-manifest.txt"
{
    printf 'llvmCommit=%s\n' "$llvm_commit"
    printf 'configuration=Release\n'
    printf 'generator=Ninja\n'
    printf 'installPrefix=%s\n' "$install_prefix"
} > "$manifest"

echo "LLVM/Clang installed to $install_prefix"
