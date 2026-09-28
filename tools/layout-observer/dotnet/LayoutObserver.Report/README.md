# 离线布局报告

`LayoutObserver.Report` 是只依赖 .NET 基础库的 HTML renderer。输入遵循
[`PROTOCOL.md`](../../contracts/PROTOCOL.md)；调用前由 Core 验证输入，报告不采集进程、不执行构建，也不重新判定比较结论。

```csharp
string html = LayoutObserver.Report.HtmlReport.Render(leftSnapshot, rightSnapshot, comparison);
File.WriteAllText(outputPath, html);
```

三个参数均为 `System.Text.Json.Nodes.JsonObject`。返回值是完整 HTML，包含固定脚本和 CSS，可离线打开。API 不修改输入对象。

- 两侧展示构建信息、采集器能力、observation、各案例的 policy / scope / verdict / coverage。
- 区域比例图使用明确原点和半开位范围；偏移保持 64 位整数，绘制比例使用 decimal。
- 只有 extent、字段枚举、占用范围及隐藏区域覆盖完整，才根据范围并集推导 padding。其余空白标作未分类区域。
- 标量和 enum 的已知表示宽度属于值数据；opaque 或未展开元素的数组不能因 members 为空就被标成 padding。
- 对象的 `runtimeReportedObjectBytes` 需要完整 extent 覆盖及原点转换证据，才能绘制完整对象 extent；否则只展示已知区域。
- 可展开内嵌 observation、筛选字段或重叠层；未提供 overlapGroup 时，仅从已知非空范围推导重叠组。筛选不改变原始占用事实。
- 全部动态文本及属性都经 HTML 编码；原始 JSON 只作为编码后的文本显示，不嵌入脚本。CSP 只允许当前固定脚本的 SHA-256 hash，禁止外部资源。

在仓库根目录运行无外部测试包的检查：

```shell
dotnet run --project tools/layout-observer/dotnet/LayoutObserver.ReportChecks
```

可同时生成人工检查用 fixture 报告：

```shell
dotnet run --project tools/layout-observer/dotnet/LayoutObserver.ReportChecks -- --write-preview tools/layout-observer/dotnet/LayoutObserver.ReportChecks/preview.html
```

检查涵盖恶意 `</script>` / 属性文本、padding 的证据前提、重叠、位域、嵌套环、负对象原点、64 位偏移、整数溢出和输入不可变性。生成的 fixture 报告不是实际采集结果。
