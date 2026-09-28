# MSVC PE/PDB/DIA collector

本后端读取实际 PE 与完整私有 PDB；不重编译成 Clang 布局，也不使用仓库里的模拟 MSVC 签名。采集时不执行输入的 PE 文件。

在安装 Visual Studio C++ 工具和 DIA SDK 的 Windows 主机，从仓库根目录运行：

```powershell
cmake -S tools/layout-observer/native/msvc -B build-observer-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-observer-msvc --config Debug --parallel
ctest --test-dir build-observer-msvc -C Debug --output-on-failure
cmake --build build-observer-msvc --config Release --parallel
ctest --test-dir build-observer-msvc -C Release --output-on-failure

build-observer-msvc/Release/typelayout-msvc.exe `
  --pe build-observer-msvc/Release/msvc-layout-fixture.exe `
  --pdb build-observer-msvc/Release/msvc-layout-fixture.pdb `
  --configuration Release --run-id msvc-release --output msvc-release.json
```

Win32 使用独立目录与 `-A Win32`。CMake 从选中的 VS 实例定位 DIA SDK，也可明确传入 `-DDIA_SDK_ROOT=...`。采集器旁边复制对应位宽的 SDK `msdia140.dll`，直接调用 DLL class factory，无需全局 COM 注册。`--dia-dll` 可选择明确路径。

`--output` 通过 Windows `CREATE_NEW` 独占创建文件；路径已存在时退出 3，原文件内容保持不变。每次采集需使用新的输出路径。

默认注册六个共享类型及 private、base/derived、bitfields、union、polymorphic、virtual、MSVC空子对象重叠案例。自己的实际产物可使用重复的 `--type id=Namespace::Type` 替换默认列表；类型找不到或有歧义时退出 3，不能静默省略请求。采集器保留 unsupported 虚继承 observation；采集退出 0 只说明快照写出成功，不等于比较通过。

## 身份与事实

PE 解析器读取 `IMAGE_DEBUG_DIRECTORY` 的 RSDS GUID 和 age，检查结构边界，再调用 `IDiaDataSource::loadAndValidateDataFromPdb`。加载后再次核对 PDB global GUID/age。缺失、错误、stripped PDB，冲突/损坏的 PE 调试目录均失败。不会仅凭相同文件名判定匹配。

字段来自 `SymTagData / DataIsMember`，包括 private 字段，静态字段不算入实例。基类用 `SymTagBaseClass`，位域分别使用 byte offset、bit position 和以 bit 为单位的 length。数组保留元素类型、count 和子 observation；enum保留实际underlying type。普通类型尺寸来自DIA type length，alignment明确unknown。DIA没有可靠的no_unique_address属性，因此空类成员占位保留unknown；非union的类成员独立尺寸与兄弟成员相交时，也保留子对象extent未知。基类占位、虚继承、未证明的多态隐藏区域不伪造；非空vtable shape才作为隐藏多态状态的依据，单独的`S_OK`并不证明存在虚表。

fixture用 `/Zi /DEBUG:FULL` 生成实际符号，Release仍保留优化。fixture的命名PE数据export记录实际配置和`_MSC_FULL_VER`；`--configuration`检查这个记录，不能重新标注产物。外部产物没有该记录时，configuration为unknown，不能通过传参伪造；编译器线索来自PDB compiland details，实际`cmd`记录存在时原样保留其中选项和宏。未嵌入的编译配置、源码状态、source/artifact digest仍需构建调度器补全，才接受为可复现baseline；缺失源码状态用 `sourceDirty: null` 表示未知。

## 验证

`msvc-layout-fixture.exe` 同时是独立oracle：普通类型用 `sizeof/alignof/offsetof`；private标准布局类型在成员函数中取offsetof；非标准布局派生类型用实际实例成员地址与合法base转换测相对位置；位域通过填满一个字节的具体值交叉验证。它不调用DIA或collector。

`test_dia.py`比较实际DIA快照和oracle，覆盖private/base/bitfields、树关系、默认unknown alignment，并故意输入另一个真实PDB、不存在PDB、截断PE、不存在的类型及错误配置，确认失败且stdout不产生伪成功快照。重复输出到相同路径时，测试验证退出 3 且原文件 SHA-256 不变。Core validator和shared comparison另负责统一协议及跨语言端到端检查。

核对依据（2026-09-28）：[匹配PDB身份](https://learn.microsoft.com/en-us/visualstudio/debugger/debug-interface-access/idiadatasource-loadandvalidatedatafrompdb?view=visualstudio)、[DIA length单位](https://learn.microsoft.com/en-us/visualstudio/debugger/debug-interface-access/idiasymbol-get-length?view=visualstudio)、[位域bit position](https://learn.microsoft.com/en-us/visualstudio/debugger/debug-interface-access/idiasymbol-get-bitposition?view=visualstudio)。
