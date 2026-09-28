using System.Text;
using LayoutObserver.Core;
using LayoutObserver.Report;

try
{
    if (args.Length == 1 && args[0] == "internal-child") return await BoundedProcess.RunChildAsync();
    if (args.Length == 0 || args[0] is "--help" or "help")
    {
        Console.WriteLine("Layout Compare 0.1\nvalidate --input SNAPSHOT\ncompare --left SNAPSHOT --right SNAPSHOT --manifest MAPPING [--out DIFF] [--html REPORT]\nproject --manifest PROJECT --out-dir NEW-DIRECTORY\nrun --manifest RUN-MANIFEST --out-dir NEW-DIRECTORY\nOutputs are never overwritten. Exit codes: 0 same, 1 different, 2 incomplete, 3 error.");
        return 0;
    }
    var options = new Dictionary<string, string>(StringComparer.Ordinal);
    for (var i = 1; i < args.Length; i += 2)
    {
        if (!args[i].StartsWith("--", StringComparison.Ordinal) || i + 1 == args.Length || !options.TryAdd(args[i], args[i + 1]))
            throw new ProtocolException("Options must be unique --name value pairs.");
    }
    string Required(string name) => options.TryGetValue(name, out var value) ? value : throw new ProtocolException("Missing " + name);
    void Allow(params string[] names)
    {
        foreach (var key in options.Keys) if (!names.Contains(key, StringComparer.Ordinal)) throw new ProtocolException("Unknown option " + key);
    }
    switch (args[0])
    {
        case "validate":
            Allow("--input");
            var snapshot = JsonIO.Read(Required("--input")); SnapshotValidator.Validate(snapshot);
            Console.WriteLine("PASS snapshot " + snapshot["snapshotId"] + " (" + snapshot["observations"]!.AsArray().Count + " observations)");
            return 0;
        case "compare":
            Allow("--left", "--right", "--manifest", "--out", "--html");
            var left = JsonIO.Read(Required("--left")); var right = JsonIO.Read(Required("--right"));
            var comparison = LayoutComparer.Compare(left, right, JsonIO.Read(Required("--manifest")));
            if (options.TryGetValue("--out", out var resultPath)) JsonIO.Write(resultPath, comparison);
            if (options.TryGetValue("--html", out var htmlPath))
            {
                Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(htmlPath))!);
                using var html = new StreamWriter(new FileStream(htmlPath, FileMode.CreateNew, FileAccess.Write), new UTF8Encoding(false));
                html.Write(HtmlReport.Render(left, right, comparison));
            }
            PrintSummary(comparison);
            return comparison["exitCode"]!.GetValue<int>();
        case "project":
            Allow("--manifest", "--out-dir");
            var project = ProjectCoordinator.Run(Required("--manifest"), Required("--out-dir"));
            Console.WriteLine(project.ToJsonString(JsonIO.Options));
            return project["exitCode"]!.GetValue<int>();
        case "run":
            Allow("--manifest", "--out-dir");
            var run = await RunCoordinator.RunAsync(Required("--manifest"), Required("--out-dir"));
            Console.WriteLine(run.ToJsonString(JsonIO.Options));
            return run["exitCode"]!.GetValue<int>();
        default: throw new ProtocolException("Unknown command " + args[0]);
    }
}
catch (Exception ex) when (ex is ProtocolException or IOException or UnauthorizedAccessException or ArgumentException or InvalidOperationException or System.Text.Json.JsonException)
{
    Console.Error.WriteLine("ERROR: " + ex.Message);
    return 3;
}

static void PrintSummary(System.Text.Json.Nodes.JsonObject comparison)
{
    Console.WriteLine($"{comparison["mode"]} / {comparison["scope"]} / {comparison["policy"]}");
    foreach (var result in comparison["cases"]!.AsArray())
        Console.WriteLine($"{result!["id"]}: {result["verdict"]}; coverage={result["coverage"]}; differences={result["differences"]!.AsArray().Count}; unknowns={result["unknowns"]!.AsArray().Count}");
}
