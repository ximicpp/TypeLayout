# Change: 添加 B2 构建支持

## Why

Boost 库需要支持 B2 (Boost.Build) 构建系统。当前项目仅支持 CMake。

## What Changes

- 添加根目录 `Jamfile.v2`
- 添加 `build/Jamfile.v2` 构建配置
- 配置头文件安装路径
- 与 CMake 构建并存

## Impact

- Affected specs: build-system
- Affected code: 构建配置文件
