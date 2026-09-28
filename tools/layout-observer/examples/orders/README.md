# 业务类型的跨语言、跨构建比较

这里的 Order 定义属于两个独立 consumer 项目。C++ 使用 `kind/id/units`，C# 使用 `Status/OrderId/Quantity`；[project.json](project.json) 将它们显式映射为相同的逻辑字段，并让每种语言的多个构建复用映射。映射引用快照中的 member ID（本例 C# 为小写 `status/orderid/quantity`），不以 displayName 代替身份。采集器没有注册内置 fixture。

在已配置真实 P2996 工具链的 Linux x64 上，从 `tools/layout-observer` 运行：

```sh
python3 examples/orders/run.py --compiler clang++ --output artifacts/orders-001
```

需要本目录 `global.json` 指定的 .NET SDK。局部工具链可通过 `--dotnet`、`--cmake`、`--linker-flags` 指定。输出目录必须全新；源码在运行中必须保持稳定。WSL 需要 Git 可正常解析的 Linux checkout，不依赖全局 `GIT_DIR` 把其他源码根重定向到同一仓库。

脚本真实构建并执行五个变体：C++ Debug/Release、C# Debug/Release，以及 C++ Release Pack1。四个正常比较应为 same/complete；Pack4→Pack1 应为 different/complete（12→7 字节，字段偏移相应变化）。脚本还编译一个不含反射采集器的 `sizeof/offsetof` oracle，并逐项核对原生结果。这个验收脚本验证全部预期后退出 0；内部比较命令因包含已确认 packing 差异退出 1。

`artifacts/orders-001/report/index.html` 是离线入口；目录中还保留全部快照、逐对报告、展开映射和可重放项目。将整个 report 目录复制到另一台有 CLI 的机器，然后运行：

```sh
dotnet run --project dotnet/LayoutObserver.Cli -c Release -- project \
  --manifest artifacts/orders-001/report/project.json --out-dir artifacts/orders-replay
```

重放不需要编译器或原始业务项目；本例预期退出 1。

接入自己的代码时，替换类型和注册，修改 project 的 observation/member 映射。原生 CMake 参数 `TYPELAYOUT_OBSERVER_REGISTRATION_HEADER` 指定定义 `register_layouts(observer::Collector&)` 的业务头文件；源码反射能力探针仍必须通过。自定义注册不运行内置 fixture 的 CTest，业务项目应提供自己的独立 oracle/验收，本例已提供。C# 项目引用静态采集器和 source generator，使用 `CaptureRegistered` 输出自己的注册结果。

已由其他构建流水线获得快照时，直接运行 `project` 即可。不同仓库的采集过程使用 run manifest 的每 profile `sourceRoot`；比较项目不要求它们位于同一仓库，也不会猜测源码与产物关系。
