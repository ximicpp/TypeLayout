# Native P2996 collector

这是独立 CMake 项目。它直接遍历反射信息生成协议 0.1 observation，不解析旧签名，也不改变原库的 byte-copy admission。

在支持当前反射能力的 Linux x64 Bloomberg Clang 工具链环境中，从仓库根目录运行：

```sh
cmake -S tools/layout-observer/native -B build-observer-native-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build-observer-native-debug --parallel
ctest --test-dir build-observer-native-debug --output-on-failure
./build-observer-native-debug/typelayout-native \
  --configuration Debug --run-id native-debug --output native-debug.json
```

Release 使用另一个构建目录并设置 `-DCMAKE_BUILD_TYPE=Release`。`--configuration` 必须与实际二进制一致；它不能重新标注构建类型。省略 `--output` 或使用 `--output -` 时只向 stdout 写 JSON，诊断写 stderr。配置、文件写入等错误退出 3；某个不支持的类型仍保留在成功采集的快照里，后续比较决定该请求能否通过。

`--output` 通过操作系统独占创建文件；路径已存在时退出 3，原文件内容保持不变。每次采集需使用新的输出路径。

CMake 在 configure 时编译最小能力程序，验证字段偏移、位域宽度和 `no_unique_address` 属性反射。普通 Clang 或其他编译器不能仅凭版本号被视为已支持。首个实际验证环境是 Linux x64、Bloomberg Clang `21.0.0git`，commit `9ffb96e3ce362289008e14ad2a79a249f58aa90a`、libc++ `210000`。这不是对所有 P2996 工具链版本的支持承诺。

## 观测范围

- 共享案例：sample、packed、reordered、nested、array、enum。字段名称与跨语言 manifest 一致。
- 原生案例：普通/空基类、普通空成员、`no_unique_address`、union、位域、opaque、非平凡类型、多态类型、虚继承、指针、bool、char16。
- 嵌套类型保留 child observation 和双向 host 关系；数组保留元素类型、数量、每个元素的位置和步长。
- ordinary 空成员用真实大小；可能重叠的成员与基类占位未知，保留独立类型大小。多态隐藏区域未知，不画成 padding。
- 位域保留实际 bit width。当前小端位编号由独立赋值实验校准；其他字节序的位域范围保留未知。
- opaque 的内部覆盖未知；虚继承输出稳定的 unsupported 原因并继续其他案例。
- 无论 admission 是否允许复制，都能记录已支持对象的几何事实；不输出“可复制/互传”的新结论。

`observer.hpp` 的 `Collector::collect<T>(id)` 可注册其他闭合类型；本 CLI 明确运行 `fixtures.hpp` 中的受控案例。`opaque_registration<T>` 是本观察器的独立注册点，不借用旧库的 relocation 保证。私有字段通过 unchecked 反射枚举，并不访问字段值。

## 验证和证据

`native-layout-oracle` 不包含反射或 exporter，通过标准布局类型的 `sizeof/alignof/offsetof`、实际数组步长和一个没有 padding 的位域字节独立验证。`test_snapshot.py` 将实际 JSON 与 oracle 输出对照，并覆盖树关系、重叠未知、opaque、unsupported、JSON 转义、配置不匹配及错误退出码。重复输出到相同路径时，测试验证退出 3 且原文件 SHA-256 不变。

快照记录编译器实际版本、构建配置、编译选项、配置时的 Git revision/dirty、libc++ 身份和实际 glibc 版本。配置后源码变化应重新 configure；源码和产物摘要初始明确为 unknown，由调度器计算补全后才可接纳为可复现 baseline。构建记录不能单凭 `--run-id` 或 `Debug` 标签充当内容身份。

若现有编译器需要比宿主更新的 glibc，应先解决工具链运行环境。可使用匹配发行版的隔离容器/运行时，并通过 `CMAKE_EXE_LINKER_FLAGS` 指定匹配 loader 和 rpath；不要替换系统 glibc、选择旧编译器或跳过 capability probe。局部运行时与编译器二进制不应提交入库。
