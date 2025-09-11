#!/bin/bash

set -e

PROJECT_ROOT=$(pwd)

# 如果没有 build 目录，创建该目录
if [ ! -d "$PROJECT_ROOT/build" ]; then
    mkdir "$PROJECT_ROOT/build"
fi

rm -rf "$PROJECT_ROOT/build"/*

cd "$PROJECT_ROOT/build"
cmake ..
make -j$(nproc)

cd "$PROJECT_ROOT"

# 头文件安装
if [ ! -d /usr/include/mymuduo ]; then
    mkdir /usr/include/mymuduo
fi

# 假设头文件都在项目根目录
cp "$PROJECT_ROOT"/*.h /usr/include/mymuduo

# 拷贝 so 库（生成在项目根目录 lib/ 下）
cp "$PROJECT_ROOT/lib/libmymuduo.so" /usr/lib

# 刷新动态库缓存
ldconfig