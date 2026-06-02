#!/bin/bash

export ARCH=arm64
export SUBARCH=arm64

# HOME path
export HOME=/root

export PATH="$HOME/android-clang/clang-r547379/bin:$PATH"

# Compiler environment
export CROSS_COMPILE=aarch64-linux-gnu-
export CROSS_COMPILE_ARM32=arm-linux-gnueabi-
export KBUILD_BUILD_USER=Isaiah
export KBUILD_BUILD_HOST=Server

echo
echo "Setting defconfig"
echo

make ARCH=arm64 CC=clang LD=ld.lld AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip neptune_defconfig

echo
echo "Compiling kernel"
echo

make ARCH=arm64 CC=clang LD=ld.lld AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip \
  KSU_NEXT_MANAGER_SIZE=0x306 \
  KSU_NEXT_MANAGER_HASH=363a3e793b47249815300a94f14c443994b828d06896672fcb582ddd2ed8dad7 \
  -j$(nproc --all) || exit 1
