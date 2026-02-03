# Change: 添加插件/DLL 接口验证示例

## Why

当前示例覆盖了 4 个核心边界场景中的 3 个（共享内存、网络协议、文件格式），缺少**模块边界**（插件/DLL）场景。

插件系统是 C++ 中最常见且最容易出现 ABI 兼容性问题的场景之一：
- 主程序和插件可能使用不同编译器版本编译
- 结构体定义可能在版本迭代中发生变化
- 错误通常表现为难以调试的崩溃或静默数据损坏

TypeLayout 可以在插件加载时进行运行时验证，立即检测布局不匹配。

## What Changes

- 新增 `example/plugin_interface.cpp` - 完整的插件接口验证示例
- 更新 `example/CMakeLists.txt` - 添加新示例的构建目标
- 更新文档导航 - 在示例列表中添加新场景

## Impact

- Affected specs: `documentation` (示例代码文档需求)
- Affected code: `example/` 目录
- 无破坏性变更
