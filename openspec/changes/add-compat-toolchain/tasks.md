## 1. Bash CLI Tool
- [x] 1.1 Create `tools/typelayout-compat` — main CLI script
  - Subcommands: `check`, `export`, `compare`, `list-platforms`
  - Arguments: `--types`, `--platforms`, `--output`, `--sigs`, `--include`
  - Auto-detect TypeLayout include path
  - Auto-detect struct/class definitions in user header
- [x] 1.2 Create `tools/platforms.conf` — platform registry
  - INI-style format with platform sections
  - Fields: docker_image, mode (docker/local), compiler, flags, env
  - Pre-configured: x86_64-linux-clang, arm64-linux-clang, arm64-macos-clang, x86_64-macos-clang
- [x] 1.3 Create `tools/sig_export_template.cpp.in` — export program template
  - Template variables: @TYPES_HEADER@, @TYPE_REGISTRATIONS@
- [x] 1.4 Create `tools/compat_check_template.cpp.in` — check program template
  - Template variables: @SIG_INCLUDES@, @STATIC_ASSERTS@, @REPORTER_REGISTRATIONS@

## 2. GitHub Actions Workflow
- [x] 2.1 Rewrite `.github/workflows/compat-check.yml`
  - Two-phase pipeline: export → compare
  - Platform matrix from comma-separated input
  - Docker-based Phase 1 using platforms.conf
  - Phase 2: generate checker, compile with g++ C++17, run report

## 3. Spec Update
- [x] 3.1 Update cross-platform-compat spec with toolchain requirements

## 4. Documentation
- [x] 4.1 Create `tools/README.md` — toolchain documentation
- [x] 4.2 Update `example/README.md` — add toolchain usage section

## 5. Testing
- [x] 5.1 Test CLI tool locally — `list-platforms` and `compare` verified
- [ ] 5.2 Test GitHub Actions workflow (requires CI push)