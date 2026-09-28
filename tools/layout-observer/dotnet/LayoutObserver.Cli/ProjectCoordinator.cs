using System.Net;
using System.Text;
using System.Text.Json.Nodes;
using LayoutObserver.Core;
using LayoutObserver.Report;

internal static class ProjectCoordinator
{
    public static JsonObject Run(string manifestPath, string outputDirectory)
    {
        var manifest = JsonIO.Read(manifestPath);
        var plan = ComparisonProject.Compile(manifest);
        var baseDirectory = Path.GetDirectoryName(Path.GetFullPath(manifestPath))!;
        var output = Path.GetFullPath(outputDirectory);
        if (Directory.Exists(output) || File.Exists(output)) throw new ProtocolException("Output directory must be new: " + output);
        Directory.CreateDirectory(output);
        JsonIO.Write(Path.Combine(output, "project.source.json"), manifest);
        var portable = (JsonObject)manifest.DeepClone();
        var snapshots = new Dictionary<string, JsonObject>(StringComparer.Ordinal);
        var variants = new JsonArray(); var comparisons = new JsonArray(); var exitCode = 0;
        foreach (var variant in plan.Variants)
        {
            var result = new JsonObject { ["id"] = variant.Id };
            variants.Add(result);
            try
            {
                var snapshot = JsonIO.Read(Path.GetFullPath(variant.SnapshotPath, baseDirectory));
                SnapshotValidator.Validate(snapshot);
                var relative = "snapshots/" + variant.Id + ".json";
                JsonIO.Write(Path.Combine(output, relative), snapshot);
                result["status"] = "ok"; result["snapshotId"] = snapshot["snapshotId"]!.DeepClone(); result["snapshot"] = relative;
                var entry = portable["variants"]!.AsArray().Single(v => v!["id"]!.GetValue<string>() == variant.Id)!;
                entry["snapshot"] = relative;
                snapshots.Add(variant.Id, snapshot);
            }
            catch (Exception ex) when (IsInputError(ex))
            {
                result["status"] = "error"; result["error"] = ex.Message; exitCode = 3;
            }
        }
        foreach (var pair in plan.Comparisons)
        {
            var result = new JsonObject { ["id"] = pair.Id, ["left"] = pair.Left, ["right"] = pair.Right };
            comparisons.Add(result);
            try
            {
                if (!snapshots.TryGetValue(pair.Left, out var left) || !snapshots.TryGetValue(pair.Right, out var right))
                    throw new ProtocolException("A required snapshot failed to load; this comparison was not performed.");
                var diff = LayoutComparer.Compare(left, right, pair.Manifest);
                var prefix = "comparisons/" + pair.Id;
                JsonIO.Write(Path.Combine(output, prefix + ".manifest.json"), pair.Manifest);
                JsonIO.Write(Path.Combine(output, prefix + ".diff.json"), diff);
                WriteText(Path.Combine(output, prefix + ".html"), HtmlReport.Render(left, right, diff));
                var code = diff["exitCode"]!.GetValue<int>(); exitCode = Math.Max(exitCode, code);
                result["status"] = "ok"; result["exitCode"] = code; result["diff"] = prefix + ".diff.json"; result["html"] = prefix + ".html";
            }
            catch (Exception ex) when (IsInputError(ex))
            {
                result["status"] = "error"; result["error"] = ex.Message; exitCode = 3;
            }
        }
        // A successful import is replayable from its bundle, independent of the original checkout.
        if (snapshots.Count == plan.Variants.Count) JsonIO.Write(Path.Combine(output, "project.json"), portable);
        var summary = new JsonObject { ["schemaVersion"] = "0.1", ["projectId"] = plan.Id, ["exitCode"] = exitCode, ["variants"] = variants, ["comparisons"] = comparisons };
        JsonIO.Write(Path.Combine(output, "result.json"), summary);
        WriteText(Path.Combine(output, "index.html"), Index(summary));
        return summary;
    }

    private static bool IsInputError(Exception ex) => ex is ProtocolException or IOException or UnauthorizedAccessException or ArgumentException or InvalidOperationException or System.Text.Json.JsonException;

    private static void WriteText(string path, string text)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        using var writer = new StreamWriter(new FileStream(path, FileMode.CreateNew, FileAccess.Write), new UTF8Encoding(false));
        writer.Write(text);
    }

    private static string Index(JsonObject summary)
    {
        static string E(JsonNode? value) => WebUtility.HtmlEncode(value?.ToString() ?? "—");
        var html = new StringBuilder("<!doctype html><html lang=\"en\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'\"><title>Layout Compare project</title><style>body{font:16px system-ui;max-width:1100px;margin:3em auto;padding:0 1em;color:#182634;background:#f5f7fa}table{border-collapse:collapse;width:100%;background:white}td,th{padding:.8em;border:1px solid #ccd5df;text-align:left}code{white-space:pre-wrap}a{color:#164b8c}</style><h1>Layout Compare · ");
        html.Append(E(summary["projectId"])).Append("</h1><p>Exit code: ").Append(E(summary["exitCode"])).Append(". 0 = complete same, 1 = complete different, 2 = inconclusive, 3 = error.</p><p>Each selected pair has an explicit mapping and scope. Build changes describe experimental conditions; they do not establish causation or ABI compatibility.</p><table><tr><th>Comparison</th><th>Variants</th><th>Result</th><th>Artifacts</th></tr>");
        foreach (var row in summary["comparisons"]!.AsArray())
        {
            html.Append("<tr><td>").Append(E(row!["id"])).Append("</td><td>").Append(E(row["left"])).Append(" → ").Append(E(row["right"])).Append("</td><td>");
            if (row["status"]!.GetValue<string>() == "ok")
                html.Append("exit ").Append(E(row["exitCode"])).Append("</td><td><a href=\"").Append(E(row["html"])).Append("\">Report</a> · <a href=\"").Append(E(row["diff"])).Append("\">JSON</a>");
            else html.Append("error</td><td>").Append(E(row["error"]));
            html.Append("</td></tr>");
        }
        html.Append("</table><h2>Required inputs</h2><ul>");
        foreach (var row in summary["variants"]!.AsArray()) html.Append("<li>").Append(E(row!["id"])).Append(": ").Append(E(row["status"])).Append(" ").Append(E(row["error"] ?? row["snapshotId"])).Append("</li>");
        return html.Append("</ul><p><a href=\"result.json\">Machine-readable result</a></p></html>").ToString();
    }
}
