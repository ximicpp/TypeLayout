## 1. Spec Updates
- [x] 1.1 MODIFIED: cross-platform-compat spec — Runtime Compatibility Report verdict 描述
- [x] 1.2 MODIFIED: signature spec — Cross-platform correctness guarantee 增加 C1/C2 ZST 条件

## 2. Code Changes
- [x] 2.1 修改 `compat_check.hpp` print_report(): 区分 serialization_free vs layout_compatible 计数
- [x] 2.2 修改 summary 输出格式：分别显示 Serialization-free (C1+C2) 和 Layout-compatible (C1) 数量

## 3. Validation
- [x] 3.1 Run `openspec validate update-compat-with-zst-model --strict --no-interactive`