# Change: 修复 CI 中的 LD_LIBRARY_PATH 配置

## Why
当前 GitHub Actions CI 的测试步骤会失败，因为 P2996 工具链的 `libc++.so.1` 位于非标准路径 `/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu/`，CTest 运行测试二进制时找不到这个共享库。

本地构建已经验证了这个问题和解决方案（通过 `-e LD_LIBRARY_PATH=...` 设置环境变量后所有测试通过）。

## What Changes
- 在 CI 工作流的容器环境中添加 `LD_LIBRARY_PATH` 环境变量
- 影响的 jobs: `build-and-test`, `static-analysis`, `header-only-check`

## Impact
- Affected specs: `ci`
- Affected code: `.github/workflows/ci.yml`
- 预期结果: GitHub Actions CI 测试能够正常运行并通过
