## 1. P0: 文档同步
- [x] 1.1 重写 project.md — 删除所有已移除功能引用（hash、concepts、宏、序列化层）
- [x] 1.2 重写 project.md 签名格式段落 — 分别展示 Layout 和 Definition 签名的实际格式
- [x] 1.3 修正 project.md 核心保证措辞 — `⟺` 改为 `⟹`（签名相同→布局相同）

## 2. P1: 实现修复
- [x] 2.1 修复 union Layout 签名中嵌套 struct 的不当展平
- [x] 2.2 添加 union 展平修复的测试用例

## 3. P2: 清理
- [x] 3.1 评估并清理 refactor-clean-code-style 残留 change