## 1. 移除 Layer 2 相关规范

- [x] 1.1 移除 "Two-Layer Signature Architecture" 中的 Layer 2 场景
- [x] 1.2 移除 "Platform-Relative Serialization Compatibility" 要求
- [x] 1.3 移除 "Serialization Compatibility Check" 要求
- [x] 1.4 移除 "Serialization Signature Generation" 要求
- [x] 1.5 移除 "Platform Constraints (Required)" 要求
- [x] 1.6 移除 "Predefined Platform Sets" 要求
- [x] 1.7 移除 "is_serializable Trait" 要求
- [x] 1.8 移除 "Serialization Blocker Diagnostic" 要求

## 2. 更新保留的规范

- [x] 2.1 重命名 "Two-Layer Architecture" 为 "Layout Signature Architecture"
- [x] 2.2 更新 Purpose 描述，移除序列化相关内容
- [x] 2.3 确保剩余规范仍然完整有效

## 3. 验证

- [x] 3.1 运行 openspec validate 确保规范格式正确
- [x] 3.2 确认规范与当前实现一致
