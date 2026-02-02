## 任务清单

### 1. Union 成员签名修复 ✅
- [x] 1.1 修改 `type_signature.hpp` 中的 union 分支，添加成员反射
- [x] 1.2 更新 `test_all_types.cpp` 中的 union 签名断言

### 2. 位域序列化模型同步 ✅  
- [x] 2.1 确认 `serialization_check.hpp` 已移除位域阻止（已在 analyze-remaining-limitations 中完成）
- [x] 2.2 更新 `test_all_types.cpp` 中的位域序列化测试断言

### 3. 待跟踪项目（低优先级）
- [ ] 3.1 `[[no_unique_address]]` 属性检测（依赖 P2996 `is_nua()`）
- [ ] 3.2 考虑在签名中标识 EBO（当前 size 已隐式反映）
