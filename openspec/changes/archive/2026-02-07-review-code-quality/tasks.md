## 1. P0 修复
- [x] 1.1 修复 `signature_detail.hpp:21` 注释乱码
- [x] 1.2 将 `<experimental/meta>` include 和宏从 `fwd.hpp` 移到 `signature_detail.hpp`

## 2. P1 改进
- [x] 2.1 提取 `from_number` 为自由函数 `to_fixed_string`
- [x] 2.2 删除 `number_buffer_size` 常量（不再需要）
- [x] 2.3 更新所有 `FixedString<number_buffer_size>::from_number(x)` 调用为 `to_fixed_string(x)`
- [x] 2.4 在测试文件中添加 `consteval contains` 辅助函数并简化搜索代码

## 3. P2 可选
- [x] 3.1 简化 `typelayout.hpp` 入口文件的 include

## 4. 验证
- [x] 4.1 Docker 构建测试通过
