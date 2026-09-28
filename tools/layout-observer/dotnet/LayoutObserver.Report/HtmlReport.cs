using System.Globalization;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace LayoutObserver.Report;

/// <summary>Renders supplied observation facts without invoking collectors or trusting embedded markup.</summary>
public static class HtmlReport
{
    private static readonly JsonSerializerOptions PrettyJson = new() { WriteIndented = true, MaxDepth = 256 };
    private static readonly JsonSerializerOptions CompactJson = new() { MaxDepth = 256 };

    public static string Render(JsonObject left, JsonObject right, JsonObject comparison)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);
        ArgumentNullException.ThrowIfNull(comparison);

        var cases = Objects(comparison["cases"]).ToList();
        var leftIndex = new SnapshotIndex(left);
        var rightIndex = new SnapshotIndex(right);
        var scriptHash = Convert.ToBase64String(SHA256.HashData(Encoding.UTF8.GetBytes(Script)));
        var html = new StringBuilder(32_768);
        html.Append("<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">")
            .Append("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">")
            .Append("<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; style-src 'unsafe-inline'; script-src 'sha256-")
            .Append(scriptHash).Append("'; base-uri 'none'; form-action 'none'\">")
            .Append("<meta name=\"color-scheme\" content=\"light\">")
            .Append("<title>Layout Compare · 内存布局报告</title><style>").Append(Styles).Append("</style></head><body>")
            .Append("<header class=\"page-header\"><p class=\"eyebrow\">离线内存布局报告</p>")
            .Append("<h1>Layout Compare</h1><p>布局事实、差异与证据。结果仅适用于报告中的比较范围与规则。</p></header>");
        RenderSummary(html, comparison);
        RenderEnvironment(html, left, right, comparison);
        RenderControls(html, SelectedObservations(cases, leftIndex, "left")
            .Concat(SelectedObservations(cases, rightIndex, "right")));
        html.Append("<main aria-label=\"按比较案例配对的布局对照\">");
        for (var index = 0; index < cases.Count; index++)
            RenderCase(html, cases[index], index, leftIndex, rightIndex);
        if (cases.Count == 0) html.Append("<p class=\"panel unknown-note\">未请求比较案例；不展示未参与比较的布局。</p>");
        html.Append("</main><section class=\"panel comparison-details\"><h2>完整比较结果</h2>");
        JsonDetails(html, "比较事实与诊断", comparison, open: true);
        html.Append("</section><footer>比例条以位偏移表示半开区间；1 byte = 8 bits。筛选不会重新计算 padding。布局一致不等于调用 ABI 或字节复制兼容。</footer>")
            .Append("<script>").Append(Script).Append("</script></body></html>");
        return html.ToString();
    }

    private static void RenderSummary(StringBuilder html, JsonObject comparison)
    {
        var cases = Objects(comparison["cases"]).ToList();
        var verdicts = cases.Select(c => Text(c["verdict"])).Distinct(StringComparer.Ordinal).ToList();
        html.Append("<section class=\"panel result-panel\" aria-label=\"比较结论\"><div>")
            .Append("<span class=\"result-label\">各案例结论</span>");
        foreach (var verdict in verdicts)
            html.Append("<strong class=\"verdict ").Append(VerdictClass(verdict)).Append("\">")
                .Append(E(verdict)).Append("</strong> ");
        if (verdicts.Count == 0) html.Append("<strong class=\"verdict unknown\">没有比较案例</strong>");
        html.Append("</div><dl class=\"summary-list\"><dt>Policy</dt><dd>").Append(E(Text(comparison["policy"])))
            .Append("</dd><dt>Scope</dt><dd>").Append(E(Text(comparison["scope"])))
            .Append("</dd><dt>Exit code</dt><dd>").Append(E(Text(comparison["exitCode"])))
            .Append("</dd></dl><p class=\"scope-note\">same 仅表示必需属性在选定 scope / policy 下完整且一致；未测属性仍显示为 unknown。</p>");
        if (cases.Count > 0)
        {
            html.Append("<div class=\"case-results\"><table><thead><tr><th>案例</th><th>左 / 右 observation</th><th>Verdict</th><th>Coverage</th><th>差异 / 未知</th></tr></thead><tbody>");
            foreach (var item in cases)
            {
                html.Append("<tr><td>").Append(E(Text(item["id"]))).Append("</td><td>")
                    .Append(E(Text(item["left"]))).Append(" / ").Append(E(Text(item["right"])))
                    .Append("</td><td class=\"").Append(VerdictClass(Text(item["verdict"]))).Append("\">")
                    .Append(E(Text(item["verdict"]))).Append("</td><td>").Append(E(Text(item["coverage"])))
                    .Append("</td><td>").Append(item["differences"] is JsonArray differences ? differences.Count : 0)
                    .Append(" / ").Append(item["unknowns"] is JsonArray unknowns ? unknowns.Count : 0).Append("</td></tr>");
            }
            html.Append("</tbody></table></div>");
        }
        html.Append("</section>");
    }

    private static void RenderControls(StringBuilder html, IEnumerable<JsonObject> observations)
    {
        var groups = observations
            .SelectMany(o => EffectiveOverlapGroups(o).Values)
            .Where(g => g.Length > 0).Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal);
        html.Append("<section class=\"panel controls\" aria-label=\"显示筛选\"><label>字段筛选")
            .Append("<input id=\"field-filter\" type=\"search\" placeholder=\"按字段名、类型或角色筛选\" autocomplete=\"off\"></label>")
            .Append("<label>重叠层<select id=\"overlap-filter\"><option value=\"all\">全部层</option>")
            .Append("<option value=\"overlapping\">仅重叠字段</option><option value=\"ordinary\">非重叠字段</option>");
        foreach (var group in groups)
            html.Append("<option value=\"group:").Append(E(group)).Append("\">").Append(E(group)).Append("</option>");
        html.Append("</select></label><button id=\"expand-all\" type=\"button\">展开嵌套</button>")
            .Append("<button id=\"reset-filter\" type=\"button\">重置筛选</button>")
            .Append("<span id=\"filter-status\" role=\"status\" aria-live=\"polite\">显示全部字段</span>")
            .Append("<div class=\"legend\"><span class=\"legend-item field-key\">字段</span>")
            .Append("<span class=\"legend-item runtime-key\">运行时区域</span><span class=\"legend-item padding-key\">已证明的 padding</span>")
            .Append("<span class=\"legend-item unknown-key\">未知 / 未分类</span></div></section>");
    }

    private sealed class SnapshotIndex(JsonObject snapshot)
    {
        public JsonObject Snapshot { get; } = snapshot;
        public IReadOnlyDictionary<string, JsonObject> Observations { get; } = UniqueIndex(Objects(snapshot["observations"]));
        public IReadOnlyDictionary<string, JsonObject> Types { get; } = UniqueIndex(Objects(snapshot["typeDescriptors"]));

        private static Dictionary<string, JsonObject> UniqueIndex(IEnumerable<JsonObject> entries) => entries
            .GroupBy(o => Text(o["id"], ""), StringComparer.Ordinal)
            .Where(g => g.Key.Length > 0 && g.Count() == 1)
            .ToDictionary(g => g.Key, g => g.Single(), StringComparer.Ordinal);
    }

    private static IEnumerable<JsonObject> SelectedObservations(IEnumerable<JsonObject> cases, SnapshotIndex index, string side)
    {
        var visited = new HashSet<string>(StringComparer.Ordinal);
        foreach (var item in cases)
            foreach (var observation in Visit(Text(item[side], ""), 0)) yield return observation;

        IEnumerable<JsonObject> Visit(string id, int depth)
        {
            if (depth > 24 || !visited.Add(id) || !index.Observations.TryGetValue(id, out var observation)) yield break;
            yield return observation;
            foreach (var member in Members(observation))
                foreach (var child in Visit(Text(member["childObservationId"], ""), depth + 1)) yield return child;
        }
    }

    private static void RenderEnvironment(StringBuilder html, JsonObject left, JsonObject right, JsonObject comparison)
    {
        var leftValues = BuildLeaves(left["build"]);
        var rightValues = BuildLeaves(right["build"]);
        var paths = leftValues.Keys.Union(rightValues.Keys, StringComparer.Ordinal).Order(StringComparer.Ordinal).ToList();
        var changedPaths = paths.Where(path => !leftValues.TryGetValue(path, out var l) || !rightValues.TryGetValue(path, out var r) || !JsonNode.DeepEquals(l, r)).ToList();
        html.Append("<section class=\"panel environment\" aria-label=\"构建环境对照\"><h2>构建环境</h2><p class=\"environment-note\">")
            .Append(E(Text(comparison["context"]?["interpretation"], "环境变化用于解释比较背景，不会改变下面各案例的布局结论。")))
            .Append("</p><p>环境变化：").Append(changedPaths.Count).Append(" 项；完整属性可展开原始构建信息查看。</p>")
            .Append("<div class=\"table-scroll\"><table class=\"environment-table\"><thead><tr><th>环境属性</th><th>左侧构建<br>")
            .Append(E(Text(left["snapshotId"]))).Append("</th><th>右侧构建<br>").Append(E(Text(right["snapshotId"])))
            .Append("</th></tr></thead><tbody>");
        if (changedPaths.Count == 0) html.Append("<tr><td colspan=\"3\">已记录的构建属性一致。</td></tr>");
        foreach (var path in changedPaths)
        {
            var leftExists = leftValues.TryGetValue(path, out var l);
            var rightExists = rightValues.TryGetValue(path, out var r);
            html.Append("<tr").Append(leftExists != rightExists || !JsonNode.DeepEquals(l, r) ? " class=\"environment-change\"" : "")
                .Append(" data-context-path=\"").Append(E(path)).Append("\"><th scope=\"row\">").Append(E(path))
                .Append("</th><td>").Append(E(leftExists ? Text(l, "null") : "未提供")).Append("</td><td>")
                .Append(E(rightExists ? Text(r, "null") : "未提供")).Append("</td></tr>");
        }
        html.Append("</tbody></table></div><div class=\"build-grid provenance-grid\">");
        Provenance(left, "左侧构建"); Provenance(right, "右侧构建");
        html.Append("</div></section>");

        void Provenance(JsonObject snapshot, string heading)
        {
            html.Append("<div>");
            JsonDetails(html, heading + " · 原始构建信息", snapshot["build"]);
            JsonDetails(html, heading + " · 采集器与能力", snapshot["producer"]);
            JsonDetails(html, heading + " · 采集诊断与限制", new JsonObject
            {
                ["diagnostics"] = snapshot["diagnostics"]?.DeepClone(),
                ["limitations"] = snapshot["limitations"]?.DeepClone()
            });
            html.Append("</div>");
        }
    }

    private static Dictionary<string, JsonNode?> BuildLeaves(JsonNode? build)
    {
        var result = new Dictionary<string, JsonNode?>(StringComparer.Ordinal);
        Visit(build, "build", 0);
        return result;
        void Visit(JsonNode? node, string path, int depth)
        {
            if (node is JsonObject obj && obj.Count > 0 && depth < 32)
                foreach (var property in obj) Visit(property.Value, path + "." + property.Key, depth + 1);
            else result[path] = node;
        }
    }

    private static void RenderCase(StringBuilder html, JsonObject item, int index, SnapshotIndex left, SnapshotIndex right)
    {
        html.Append("<section class=\"panel comparison-case\" id=\"case-").Append(index).Append("\" data-case-id=\"")
            .Append(E(Text(item["id"]))).Append("\"><header class=\"case-heading\"><h2>").Append(E(Text(item["id"])))
            .Append("</h2><strong class=\"verdict ").Append(VerdictClass(Text(item["verdict"]))).Append("\">")
            .Append(E(Text(item["verdict"]))).Append("</strong><span class=\"case-coverage\">Coverage: ")
            .Append(E(Text(item["coverage"]))).Append("</span></header><div class=\"build-grid\">");
        Side(left, "left", "左侧构建"); Side(right, "right", "右侧构建");
        html.Append("</div>");
        RenderCaseDifferences(html, item);
        html.Append("</section>");

        void Side(SnapshotIndex source, string side, string heading)
        {
            var observationId = Text(item[side], "");
            html.Append("<article class=\"observation case-side\" data-side=\"").Append(side)
                .Append("\" data-observation-id=\"").Append(E(observationId)).Append("\"><div class=\"build-heading\">")
                .Append("<span class=\"eyebrow\">").Append(heading).Append("</span><p>")
                .Append(E(Text(source.Snapshot["snapshotId"]))).Append("<br><code>").Append(E(observationId)).Append("</code></p></div>");
            if (source.Observations.TryGetValue(observationId, out var observation))
                RenderObservation(html, observation, source.Observations, source.Types, new HashSet<string>(StringComparer.Ordinal), 0);
            else html.Append("<p class=\"unknown-note\">请求的 observation 不存在或 ID 不唯一：").Append(E(observationId)).Append("</p>");
            html.Append("</article>");
        }
    }

    private static void RenderCaseDifferences(StringBuilder html, JsonObject item)
    {
        html.Append("<section class=\"case-differences\" aria-label=\"本案例的差异与未知事实\"><h3>差异与未知事实</h3>");
        var entries = Objects(item["differences"]).Select(entry => (State: "差异", Entry: entry))
            .Concat(Objects(item["unknowns"]).Select(entry => (State: "未知", Entry: entry))).ToList();
        if (entries.Count == 0) html.Append("<p class=\"muted\">没有已知差异或缺失的必需事实；请结合案例结论与诊断阅读。</p>");
        else
        {
            html.Append("<div class=\"table-scroll\"><table class=\"difference-table\"><thead><tr><th>状态 / 属性</th><th>左侧</th><th>右侧</th><th>说明</th></tr></thead><tbody>");
            foreach (var (state, entry) in entries)
                html.Append("<tr><th scope=\"row\">").Append(state).Append(" · ").Append(E(Text(entry["kind"])))
                    .Append("<br><code>").Append(E(Text(entry["path"]))).Append("</code></th><td><pre>")
                    .Append(E(Text(entry["left"], "未提供"))).Append("</pre></td><td><pre>")
                    .Append(E(Text(entry["right"], "未提供"))).Append("</pre></td><td>").Append(E(Text(entry["message"])))
                    .Append("</td></tr>");
            html.Append("</tbody></table></div>");
        }
        JsonDetails(html, "本案例的诊断", item["diagnostics"]);
        html.Append("</section>");
    }

    private static void RenderObservation(StringBuilder html, JsonObject observation,
        IReadOnlyDictionary<string, JsonObject> byId, IReadOnlyDictionary<string, JsonObject> types,
        HashSet<string> ancestors, int depth)
    {
        var id = Text(observation["id"], "");
        var name = Text(observation["displayName"], Text(observation["typeId"], id));
        html.Append("<h3>").Append(E(name)).Append("</h3><div class=\"observation-tags\"><span>")
            .Append(E(Text(observation["view"]))).Append("</span><span>")
            .Append(E(Text(observation["context"]?["kind"]))).Append("</span><span>origin: ")
            .Append(E(Text(observation["origin"]?["kind"]))).Append("</span><span>status: ")
            .Append(E(Text(observation["status"]))).Append("</span></div>");
        if (depth > 24 || (id.Length > 0 && !ancestors.Add(id)))
        {
            html.Append("<p class=\"unknown-note\">嵌套关系存在环或超过报告展开限制；请检查协议诊断。</p>");
            return;
        }
        RenderMetrics(html, observation["metrics"] as JsonObject);
        RenderMap(html, observation, types);
        var members = Members(observation).ToList();
        var overlapGroups = EffectiveOverlapGroups(observation);
        if (members.Count > 0)
        {
            html.Append("<div class=\"member-list\">");
            foreach (var member in members)
            {
                var memberName = Name(member);
                html.Append("<details class=\"member-details\"");
                FieldAttributes(html, member, types, overlapGroups[member]);
                html.Append("><summary>").Append(E(memberName)).Append(" <span class=\"muted\">")
                    .Append(E(Text(member["role"], "field"))).Append(" · ")
                    .Append(E(Text(member["typeRef"]))).Append("</span></summary>");
                JsonDetails(html, "字段事实与证据", member, open: true);
                var childId = Text(member["childObservationId"], "");
                if (childId.Length > 0)
                {
                    html.Append("<details class=\"nested\"><summary>内嵌布局：").Append(E(memberName)).Append("</summary>");
                    if (byId.TryGetValue(childId, out var child))
                        RenderObservation(html, child, byId, types, ancestors, depth + 1);
                    else
                        html.Append("<p class=\"unknown-note\">child observation 不存在或 ID 不唯一：").Append(E(childId)).Append("</p>");
                    html.Append("</details>");
                }
                html.Append("</details>");
            }
            html.Append("</div>");
        }
        JsonDetails(html, "覆盖率、原点与限制", new JsonObject
        {
            ["coverage"] = observation["coverage"]?.DeepClone(),
            ["origin"] = observation["origin"]?.DeepClone(),
            ["context"] = observation["context"]?.DeepClone(),
            ["metrics"] = observation["metrics"]?.DeepClone(),
            ["runtimeRegions"] = observation["runtimeRegions"]?.DeepClone(),
            ["instanceShape"] = observation["instanceShape"]?.DeepClone(),
            ["limitations"] = observation["limitations"]?.DeepClone()
        });
        if (types.TryGetValue(Text(observation["typeId"], ""), out var descriptor))
            JsonDetails(html, "类型表示与证据", descriptor);
        if (id.Length > 0) ancestors.Remove(id);
    }

    private static void RenderMetrics(StringBuilder html, JsonObject? metrics)
    {
        html.Append("<dl class=\"metrics\">");
        if (metrics is null || metrics.Count == 0)
            html.Append("<dt>尺寸</dt><dd class=\"unknown\">unknown · 未提供尺寸事实</dd>");
        else
        {
            foreach (var (key, value) in metrics)
            {
                html.Append("<dt>").Append(E(key)).Append("</dt><dd");
                if (!TryNumber(value, out _)) html.Append(" class=\"unknown\"");
                html.Append('>').Append(E(FactText(value))).Append("</dd>");
            }
        }
        html.Append("</dl>");
    }

    private static void RenderMap(StringBuilder html, JsonObject observation,
        IReadOnlyDictionary<string, JsonObject> types)
    {
        var members = Members(observation).ToList();
        var overlapGroups = EffectiveOverlapGroups(observation);
        var regions = new List<Region>();
        var allMemberRangesKnown = true;
        var intrinsicKnown = false;
        types.TryGetValue(Text(observation["typeId"], ""), out var observedType);
        var observedKind = Text(observedType?["kind"], "");
        if (observedKind is "scalar" or "enum" or "reference")
        {
            var intrinsic = ReadIntrinsicValueRange(observation, types);
            intrinsicKnown = intrinsic.HasValue;
            allMemberRangesKnown = intrinsicKnown;
            regions.Add(new Region(observedKind == "reference" ? "引用槽" : "值表示",
                intrinsicKnown ? "field" : "unknown", intrinsic, null));
        }
        else if (observedKind == "opaque" || observedKind.Length == 0
            || (observedKind == "array" && members.Count == 0))
            allMemberRangesKnown = false;
        foreach (var member in members)
        {
            var ranges = ReadRanges(member["occupiedRanges"]);
            if (ranges is null)
            {
                allMemberRangesKnown = false;
                regions.Add(new Region(Name(member), "unknown", null, member));
            }
            else if (ranges.Count == 0)
                regions.Add(new Region(Name(member) + " · 空占用范围", "field", null, member, Empty: true));
            else
                regions.AddRange(ranges.Select(range => new Region(Name(member), "field", range, member)));
        }
        var runtimeRangesKnown = true;
        foreach (var region in Objects(observation["runtimeRegions"]))
        {
            var ranges = ReadRanges(region["ranges"]);
            if (ranges is null)
            {
                runtimeRangesKnown = false;
                regions.Add(new Region(Text(region["role"], "运行时区域"), "unknown", null, null));
            }
            else
                regions.AddRange(ranges.Select(range => new Region(Text(region["role"], "运行时区域"), "runtime", range, null)));
        }

        var extent = ReadExtent(observation);
        var coverage = observation["coverage"] as JsonObject;
        var complete = extent is not null && allMemberRangesKnown && runtimeRangesKnown
            && IsComplete(coverage?["fieldEnumeration"], allowNotApplicable: intrinsicKnown)
            && IsComplete(coverage?["extent"])
            && IsComplete(coverage?["occupiedRanges"])
            && IsComplete(coverage?["hiddenRegions"], allowNotApplicable: true);
        if (extent is { } knownExtent)
        {
            var gaps = Complement(knownExtent, regions.Where(r => r.Range.HasValue).Select(r => r.Range!.Value));
            regions.AddRange(gaps.Select(range => new Region(complete ? "padding" : "未分类区域", complete ? "padding" : "unknown", range, null)));
        }

        var actualRanges = regions.Where(r => r.Range.HasValue).Select(r => r.Range!.Value).ToList();
        var start = Math.Min(extent?.Start ?? 0, actualRanges.Count == 0 ? 0 : actualRanges.Min(r => r.Start));
        var end = Math.Max(extent?.End ?? 0, actualRanges.Count == 0 ? 0 : actualRanges.Max(r => r.End));
        html.Append("<section class=\"byte-map\" aria-label=\"按位偏移的布局区域\"><div class=\"map-title\"><strong>布局区域</strong><span>");
        if (end > start)
            html.Append(E(Position(start))).Append(" → ").Append(E(Position(end)));
        else html.Append("范围未知");
        html.Append("</span></div>");
        if (extent is null)
            html.Append("<p class=\"unknown-note\">extent 未知；比例只覆盖已知区域，不能推断总大小或 padding。</p>");
        else if (!complete)
            html.Append("<p class=\"unknown-note\">覆盖不完整；未分类区域可能包含字段或运行时数据，不能认定为 padding。</p>");
        if (regions.Count == 0)
            html.Append("<p class=\"unknown-note\">没有可显示的区域事实。</p>");
        foreach (var region in regions)
        {
            html.Append("<div class=\"region-row ").Append(region.Kind).Append('"');
            if (region.Member is not null) FieldAttributes(html, region.Member, types, overlapGroups[region.Member]);
            html.Append("><span class=\"region-label\">").Append(E(region.Label)).Append("</span><div class=\"region-track\">");
            if (region.Range is { } range && end > start)
            {
                var left = (((decimal)range.Start - start) / ((decimal)end - start)) * 100;
                var width = (((decimal)range.End - range.Start) / ((decimal)end - start)) * 100;
                html.Append("<span class=\"region-bar\" style=\"left:").Append(Number(left))
                    .Append("%;width:").Append(Number(width)).Append("%\" title=\"")
                    .Append(E(region.Label + ": [" + Position(range.Start) + ", " + Position(range.End) + ")"))
                    .Append("\"></span>");
            }
            else html.Append(region.Empty
                ? "<span class=\"empty-position\">已知空范围</span>"
                : "<span class=\"unknown-position\">位置 / 范围未知</span>");
            html.Append("</div><span class=\"region-range\">")
                .Append(region.Range is { } shown ? E($"[{Position(shown.Start)}, {Position(shown.End)})") : region.Empty ? "∅" : "unknown")
                .Append("</span></div>");
        }
        html.Append("</section>");
    }

    private static Range? ReadExtent(JsonObject observation)
    {
        if (!TryNumber(observation["origin"]?["extentStartBit"], out var start)) return null;
        var metrics = observation["metrics"] as JsonObject;
        var context = Text(observation["context"]?["kind"], "");
        var objectContext = context is "heap-object" or "boxed-value";
        if (objectContext && (observation["origin"]?["conversionEvidence"] is null
            || !IsComplete(observation["coverage"]?["extent"]))) return null;
        var keys = objectContext
            ? new[] { "runtimeReportedObjectBytes" }
            : context == "complete-value" ? new[] { "valueSizeBytes", "standaloneSizeBytes" }
            : new[] { "valueSizeBytes" };
        foreach (var key in keys)
            if (TryNumber(metrics?[key], out var bytes) && bytes >= 0)
            {
                try { return new Range(start, checked(start + checked(bytes * 8))); }
                catch (OverflowException) { return null; }
            }
        return null;
    }

    private static Range? ReadIntrinsicValueRange(JsonObject observation,
        IReadOnlyDictionary<string, JsonObject> types)
    {
        if (Text(observation["context"]?["kind"], "") is not ("complete-value" or "embedded-value" or "array-element")
            || Text(observation["origin"]?["kind"], "") != "value-start") return null;
        var id = Text(observation["typeId"], "");
        var seen = new HashSet<string>(StringComparer.Ordinal);
        while (seen.Add(id) && types.TryGetValue(id, out var descriptor))
        {
            var kind = Text(descriptor["kind"], "");
            if (kind == "enum") { id = Text(descriptor["enumUnderlyingTypeRef"], ""); continue; }
            if (kind is "scalar" or "reference"
                && TryNumber(descriptor["representation"]?["widthBits"], out var width) && width >= 0)
                return new Range(0, width);
            return null;
        }
        return null;
    }

    private static List<Range>? ReadRanges(JsonNode? node)
    {
        if (node is not JsonObject fact || Text(fact["state"], "") != "known") return null;
        node = fact["value"];
        if (node is not JsonArray array) return null;
        var result = new List<Range>();
        foreach (var item in array)
        {
            if (item is not JsonObject range || !TryNumber(range["startBit"], out var start)
                || !TryNumber(range["lengthBits"], out var length) || length < 0) return null;
            try { result.Add(new Range(start, checked(start + length))); }
            catch (OverflowException) { return null; }
        }
        return result;
    }

    private static IEnumerable<Range> Complement(Range extent, IEnumerable<Range> ranges)
    {
        var cursor = extent.Start;
        foreach (var range in ranges.OrderBy(r => r.Start).ThenBy(r => r.End))
        {
            var start = Math.Clamp(range.Start, extent.Start, extent.End);
            var end = Math.Clamp(range.End, extent.Start, extent.End);
            if (start > cursor) yield return new Range(cursor, start);
            cursor = Math.Max(cursor, end);
        }
        if (cursor < extent.End) yield return new Range(cursor, extent.End);
    }

    private static bool IsComplete(JsonNode? node, bool allowNotApplicable = false)
    {
        var state = Text(node, "");
        return state == "complete" || (allowNotApplicable && state == "not-applicable");
    }

    private static Dictionary<JsonObject, string> EffectiveOverlapGroups(JsonObject observation)
    {
        var members = Members(observation).ToList();
        var ranges = members.Select(m => ReadRanges(m["occupiedRanges"])).ToList();
        var parents = Enumerable.Range(0, members.Count).ToArray();
        var overlaps = new bool[members.Count];
        int Find(int index)
        {
            while (parents[index] != index) index = parents[index];
            return index;
        }
        for (var a = 0; a < members.Count; a++)
        {
            if (ranges[a] is not { } first) continue;
            for (var b = a + 1; b < members.Count; b++)
            {
                if (ranges[b] is not { } second || !first.Any(x => x.End > x.Start && second.Any(y =>
                    y.End > y.Start && x.Start < y.End && y.Start < x.End))) continue;
                overlaps[a] = overlaps[b] = true;
                parents[Find(b)] = Find(a);
            }
        }
        var result = new Dictionary<JsonObject, string>(ReferenceEqualityComparer.Instance);
        for (var index = 0; index < members.Count; index++)
        {
            var declared = Text(members[index]["overlapGroup"], "");
            result[members[index]] = declared.Length > 0 ? declared : overlaps[index]
                ? "range-overlap:" + Text(observation["id"], "") + ":" + Find(index).ToString(CultureInfo.InvariantCulture) : "";
        }
        return result;
    }

    private static void FieldAttributes(StringBuilder html, JsonObject member,
        IReadOnlyDictionary<string, JsonObject> types, string overlapGroup)
    {
        var typeId = Text(member["typeRef"], "");
        var typeDisplay = types.TryGetValue(typeId, out var type) ? Text(type["displayName"], typeId) : typeId;
        var search = string.Join(' ', Name(member), typeId, typeDisplay, Text(member["role"], ""));
        html.Append(" data-field=\"true\" data-search=\"").Append(E(search)).Append("\" data-overlap=\"")
            .Append(E(overlapGroup)).Append('"');
    }

    private static bool TryNumber(JsonNode? node, out long number)
    {
        if (node is JsonObject fact)
        {
            if (Text(fact["state"], "") != "known") { number = 0; return false; }
            node = fact["value"];
        }
        return long.TryParse(node?.ToJsonString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out number);
    }

    private static string FactText(JsonNode? node)
    {
        if (node is not JsonObject fact) return Text(node);
        return Text(fact["state"], "unknown") switch
        {
            "known" => Text(fact["value"]),
            "not-applicable" => "not-applicable",
            _ => "unknown · " + Text(fact["reason"], "未提供原因")
        };
    }

    private static void JsonDetails(StringBuilder html, string title, JsonNode? node, bool open = false)
    {
        html.Append("<details class=\"facts\"").Append(open ? " open" : "").Append("><summary>")
            .Append(E(title)).Append("</summary><pre>").Append(E(node?.ToJsonString(PrettyJson) ?? "unknown"))
            .Append("</pre></details>");
    }

    private static string E(string value) => WebUtility.HtmlEncode(value);
    private static string Text(JsonNode? node, string fallback = "unknown") => node is null ? fallback
        : node is JsonValue value && value.TryGetValue<string>(out var text) ? text : node.ToJsonString(CompactJson);
    private static string Name(JsonObject member) => Text(member["displayName"], Text(member["name"], Text(member["id"])));
    private static IEnumerable<JsonObject> Objects(JsonNode? node) => node is JsonArray array ? array.OfType<JsonObject>() : [];
    private static IEnumerable<JsonObject> Observations(JsonObject snapshot) => Objects(snapshot["observations"]);
    private static IEnumerable<JsonObject> Members(JsonObject observation) => Objects(observation["members"]);
    private static string Number(decimal value) => value.ToString("0.######", CultureInfo.InvariantCulture);
    private static string Position(long bits) => bits % 8 == 0 ? Number(bits / 8) + " B" : Number(bits) + " bit";
    private static string VerdictClass(string verdict) => verdict switch
    {
        "same" => "same", "different" => "different", "incomplete" => "incomplete",
        "not-comparable" => "not-comparable", _ => "unknown"
    };

    private readonly record struct Range(long Start, long End);
    private sealed record Region(string Label, string Kind, Range? Range, JsonObject? Member, bool Empty = false);

    private const string Styles = """
        :root{font-family:Inter,ui-sans-serif,system-ui,-apple-system,"Segoe UI",sans-serif;color:#20303d;background:#eef3f6;line-height:1.5;font-size:14px}
        .table-scroll{overflow-x:auto}.environment-table,.difference-table{width:100%;table-layout:fixed;border-collapse:collapse;font-size:12px}.environment-table th,.environment-table td,.difference-table th,.difference-table td{text-align:left;vertical-align:top;padding:9px 10px;border-bottom:1px solid #dde6ec;overflow-wrap:anywhere}.environment-table thead,.difference-table thead{color:#586f7d;background:#edf3f7}.environment-table th:first-child{width:28%}.environment-change{background:#fff4dc}.environment-note{font-size:12px;color:#617683}.provenance-grid{margin-top:16px}.case-heading{display:flex;align-items:center;flex-wrap:wrap;gap:12px;margin-bottom:20px}.case-heading h2{margin:0;flex:1 1 200px}.case-heading .verdict{font-size:18px;margin:0}.case-coverage{font-size:12px;color:#617683}.case-side{min-width:0;border:1px solid #dce5eb;border-radius:8px;padding:16px}.case-side .build-heading{font-size:12px;overflow-wrap:anywhere}.case-differences{margin-top:20px;border-top:1px solid #dce5eb;padding-top:16px}.case-differences h3{font-size:15px}.difference-table pre{margin:0}.comparison-case{break-inside:auto}
        .case-results{grid-column:1/-1;overflow-x:auto}.case-results table{width:100%;border-collapse:collapse;font-size:12px}.case-results th,.case-results td{text-align:left;padding:7px 10px;border-bottom:1px solid #e2e9ee;overflow-wrap:anywhere}.case-results th{color:#647988;font-weight:500}.case-results td:first-child{font-weight:600}
        *{box-sizing:border-box}body{margin:0 auto;max-width:1800px;padding:36px 28px}h1,h2,h3,p{margin-top:0}h1{font-size:34px;letter-spacing:-.03em;margin-bottom:8px}h2{font-size:21px;overflow-wrap:anywhere}h3{font-size:17px;margin-bottom:9px;overflow-wrap:anywhere}button,input,select{font:inherit}button{cursor:pointer}input,select,button{border:1px solid #b9c8d2;border-radius:7px;background:#fff;padding:8px 10px;color:#20303d}input:focus,select:focus,button:focus,summary:focus{outline:2px solid #357f98;outline-offset:3px}input{min-width:260px}pre{white-space:pre-wrap;overflow-wrap:anywhere;margin:9px 0;font-family:ui-monospace,Consolas,monospace;font-size:12px;color:#4c5a65}summary{cursor:pointer;overflow-wrap:anywhere}details>summary{padding:6px 0}.eyebrow{font-size:11px;letter-spacing:.14em;font-weight:700;color:#55717f}.page-header{margin-bottom:24px}.page-header p{color:#627382}.panel{background:#fff;border:1px solid #d8e2e9;border-radius:12px;padding:20px;margin-bottom:16px;box-shadow:0 2px 3px #20303d04}.result-panel{border-left:5px solid #527b89;display:grid;grid-template-columns:220px 1fr;gap:12px 24px}.result-label{display:block;font-size:12px;color:#647988}.verdict{display:inline-block;padding:3px 10px;border-radius:6px;margin-top:7px;font-size:24px;font-weight:650}.same{background:#def2e6;color:#246746}.different{background:#fce6de;color:#a6482b}.incomplete,.not-comparable,.unknown{color:#8a652b}.summary-list,.metrics{display:grid;grid-template-columns:minmax(90px,max-content) 1fr;gap:5px 14px;margin:0}.summary-list dt,.metrics dt{color:#667984}.summary-list dd,.metrics dd{margin:0;overflow-wrap:anywhere}.scope-note{grid-column:1/-1;font-size:12px;color:#617683;margin:0}.controls{display:flex;flex-wrap:wrap;align-items:flex-end;gap:12px}.controls label{display:flex;flex-direction:column;gap:4px;font-size:12px;color:#5d727f}.controls button{background:#f6f9fb}.legend{flex-basis:100%;display:flex;flex-wrap:wrap;gap:16px;font-size:11px;color:#566c7a}.legend-item:before{content:"";display:inline-block;width:12px;height:9px;margin-right:5px;border:1px solid #486071}.field-key:before{background:#7bb3c3}.runtime-key:before{background:#b7a8cd}.padding-key:before{background:#d7e1e6}.unknown-key:before{background:repeating-linear-gradient(135deg,#fff 0 3px,#e9cc8f 3px 5px)}#filter-status{color:#657b88;font-size:12px;padding-bottom:7px}.build-grid{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:22px;align-items:start}.build-heading{min-height:62px;padding:0 5px}.build-heading h2{margin-bottom:12px}.environment{background:#f9fbfc}.observation-tags{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:14px}.observation-tags span{background:#eef3f6;color:#5c7181;padding:2px 6px;border-radius:4px;font-size:11px;overflow-wrap:anywhere}.metrics{font-size:12px;margin-bottom:18px}.byte-map{border-top:1px solid #e2e9ee;border-bottom:1px solid #e2e9ee;padding:12px 0;margin-bottom:12px}.map-title{display:flex;justify-content:space-between;gap:8px;margin-bottom:12px;font-size:12px}.map-title span{color:#647c89}.region-row{display:grid;grid-template-columns:minmax(65px,1fr) minmax(90px,2fr) minmax(75px,1fr);gap:8px;align-items:center;min-height:29px;font-size:11px}.region-label{overflow-wrap:anywhere}.region-track{position:relative;height:18px;background:linear-gradient(90deg,#eff3f6 1px,transparent 1px);background-size:12.5% 100%;border-left:1px solid #afc4ce;border-right:1px solid #afc4ce}.region-bar{display:block;position:absolute;height:16px;top:1px;min-width:1px;border:1px solid #568fa2;background:#8dbccc;border-radius:2px}.runtime .region-bar{background:#bdb0d1;border-color:#8b75a8}.padding .region-bar{background:#dbe4e8;border-color:#bccbd2}.unknown .region-bar{background:repeating-linear-gradient(135deg,#fff4da 0 4px,#e8ca8c 4px 6px);border-color:#bd9245}.region-range{color:#71848f;text-align:right;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}.unknown-note{font-size:12px;color:#8a652b;background:#fff8e9;border-radius:5px;padding:7px 9px}.unknown-position{font-size:10px;color:#947447;white-space:nowrap}.member-details{border-bottom:1px solid #e6edf1;font-size:12px}.member-details>summary{font-weight:600}.muted{font-weight:400;color:#70828f}.nested{margin:8px 0 10px 10px;border-left:3px solid #a7c8d4;padding-left:12px}.nested h3{font-size:14px}.facts{font-size:12px;color:#537181}.facts>summary{font-weight:600}.facts[open]{margin-bottom:9px}.comparison-details{margin-top:8px}footer{color:#738692;font-size:12px;padding:12px 3px}body [hidden]{display:none!important}@media(max-width:900px){body{padding:20px 12px}.build-grid{grid-template-columns:1fr}.result-panel{grid-template-columns:1fr}.panel{padding:15px}input{min-width:0;width:100%}.controls label{flex:1 1 200px}.region-row{grid-template-columns:minmax(65px,1fr) minmax(110px,2fr) minmax(75px,1fr)}}@media print{body{background:#fff;padding:0;max-width:none}.controls{display:none}.panel{box-shadow:none;break-inside:avoid}.build-grid{gap:12px}.region-bar{print-color-adjust:exact}.build-heading{min-height:0}pre{font-size:10px}}
        """;

    private const string Script = """
        (() => {
          'use strict';
          const search = document.getElementById('field-filter');
          const overlap = document.getElementById('overlap-filter');
          const status = document.getElementById('filter-status');
          const fields = Array.from(document.querySelectorAll('[data-field]'));
          function filter() {
            const query = search.value.trim().toLocaleLowerCase();
            const layer = overlap.value;
            const matches = [];
            let visible = 0;
            for (const row of fields) {
              const group = row.dataset.overlap || '';
              const matchesLayer = layer === 'all' || (layer === 'overlapping' && group !== '') ||
                (layer === 'ordinary' && group === '') || (layer.startsWith('group:') && layer.slice(6) === group);
              row.hidden = !((row.dataset.search || '').toLocaleLowerCase().includes(query) && matchesLayer);
              if (!row.hidden) matches.push(row);
              if (!row.hidden && row.classList.contains('member-details')) visible++;
            }
            // A matching descendant must remain reachable through its enclosing field details.
            if (query || layer !== 'all') {
              for (const row of matches) {
                for (let ancestor = row.parentElement; ancestor; ancestor = ancestor.parentElement) {
                  if (ancestor.hasAttribute('data-field')) ancestor.hidden = false;
                  if (ancestor.tagName === 'DETAILS') ancestor.open = true;
                }
              }
            }
            status.textContent = query || layer !== 'all' ? '当前显示 ' + visible + ' 个字段条目（含嵌套视图）' : '显示全部字段';
          }
          search.addEventListener('input', filter);
          overlap.addEventListener('change', filter);
          document.getElementById('reset-filter').addEventListener('click', () => { search.value = ''; overlap.value = 'all'; filter(); });
          document.getElementById('expand-all').addEventListener('click', () => {
            for (const details of document.querySelectorAll('details.member-details, details.nested')) details.open = true;
          });
        })();
        """;
}
