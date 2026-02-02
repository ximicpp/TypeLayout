## 任务清单

### 1. 配置 CTest
- [x] 1.1 在根 CMakeLists.txt 添加 `include(CTest)` 或 `enable_testing()`
- [x] 1.2 在 test/CMakeLists.txt 为每个测试目标添加 `add_test()`
- [x] 1.3 配置测试超时和标签

### 2. 更新 CI
- [x] 2.1 在 GitHub Actions 中添加 `ctest --output-on-failure` 步骤 (已存在)
- [x] 2.2 确保编译时测试 (static_assert) 通过构建验证

### 3. 验证
- [ ] 3.1 本地运行 `ctest --output-on-failure` 验证
- [ ] 3.2 CI 构建验证