using System.Diagnostics;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;
using LayoutObserver.Core;
using LayoutObserver.Report;
using static LayoutObserver.Core.Nodes;

internal static class RunCoordinator
{
    public static async Task<JsonObject> RunAsync(string manifestPath, string outputDirectory)
    {
        var manifest = JsonIO.Read(manifestPath);
        Keys(manifest, "schemaVersion sourceRoot profiles comparisons", "run manifest");
        if (manifest.S("schemaVersion") != "0.1") throw new ProtocolException("Unsupported run schemaVersion.");
        var basePath = Path.GetDirectoryName(Path.GetFullPath(manifestPath))!;
        var output = Path.GetFullPath(outputDirectory);
        if (Directory.Exists(output) || File.Exists(output)) throw new ProtocolException("Run output directory already exists; use a new directory.");
        var profiles = Index(Arr(manifest["profiles"], "profiles"), "profiles");
        if (profiles.Count == 0) throw new ProtocolException("No profiles requested.");
        var comparisons = Arr(manifest["comparisons"], "comparisons");
        if (comparisons.Count == 0) throw new ProtocolException("No comparisons requested.");
        foreach (var profile in profiles.Values) ValidateProfile(profile);
        foreach (var item in comparisons.OfType<JsonObject>())
        {
            Keys(item, "id left right compareManifest", "comparison");
            if (!profiles.ContainsKey(item.S("left")) || !profiles.ContainsKey(item.S("right"))) throw new ProtocolException("Unknown comparison profile.");
            SafeId(item.S("id")); LayoutComparer.ValidateManifest(JsonIO.Read(Resolve(basePath, item.S("compareManifest"))));
        }
        if (comparisons.OfType<JsonObject>().Select(c => c.S("id")).Distinct().Count() != comparisons.Count) throw new ProtocolException("Duplicate or invalid comparison IDs.");
        Directory.CreateDirectory(output);
        var snapshots = new Dictionary<string, JsonObject>(); var results = new JsonArray(); var exitCode = 0;
        var runId = Guid.NewGuid().ToString("N");
        var sourceRoot = !manifest.ContainsKey("sourceRoot") ? basePath : Resolve(basePath, manifest.S("sourceRoot"));
        var source = await SourceIdentity(sourceRoot);
        foreach (var profile in profiles.Values)
        {
            var id = profile.S("id"); var directory = Path.Combine(output, id); Directory.CreateDirectory(directory);
            try
            {
                var before = await SourceIdentity(sourceRoot);
                if (before != source) throw new ProtocolException("Source tree changed during this run; start a fresh run against stable inputs.");
                if (profile["buildSteps"] is JsonArray steps)
                {
                    for (var i = 0; i < steps.Count; i++) await Execute(Obj(steps[i], "build step"), basePath, directory, "build-" + i, []);
                }
                var snapshotPath = Path.Combine(directory, "snapshot.json");
                await Execute(profile, basePath, directory, "capture", [profile.S("outputArgument"), snapshotPath]);
                if (!File.Exists(snapshotPath)) throw new ProtocolException("Collector produced no snapshot.");
                var snapshot = JsonIO.Read(snapshotPath); SnapshotValidator.Validate(snapshot);
                var actual = Obj(snapshot["build"], "build"); var actualTarget = Obj(actual["target"], "target");
                var requested = Obj(profile["target"], "target");
                if (actualTarget.S("architecture") != requested.S("architecture") || actualTarget.S("os") != requested.S("os") || actual.S("configuration") != profile.S("configuration"))
                    throw new ProtocolException("Actual target/configuration does not match requested profile.");
                var capabilities = Arr(snapshot["producer"]!["capabilities"], "capabilities").Select(n => Str(n, "capability")).ToHashSet();
                foreach (var capability in Arr(profile["requiredCapabilities"], "requiredCapabilities"))
                    if (!capabilities.Contains(Str(capability, "capability"))) throw new ProtocolException("Collector lacks required capability " + capability);
                var artifact = Resolve(basePath, profile.S("artifact"));
                if (!File.Exists(artifact)) throw new ProtocolException("Declared artifact does not exist.");
                await using (var artifactStream = File.OpenRead(artifact))
                    actual["artifactDigest"] = "sha256:" + Convert.ToHexStringLower(await SHA256.HashDataAsync(artifactStream));
                var after = await SourceIdentity(sourceRoot);
                if (after != source) throw new ProtocolException("Source tree changed while building/capturing; provenance is not stable.");
                // Keep the collector's original source claims separate from the orchestrator's observed checkout.
                actual["compiler"]!["collectorSourceRevision"] = actual["sourceRevision"]!.DeepClone();
                actual["compiler"]!["sourceIdentityScope"] = "git-worktree-at-run; artifact/source relationship requires recorded buildSteps";
                actual["sourceRevision"] = source.Revision; actual["sourceDirty"] = source.Dirty; actual["sourceDigest"] = source.Digest;
                actual["buildId"] = runId + "/" + id; actual["runId"] = runId; actual["requestedProfile"] = profile.DeepClone();
                SnapshotValidator.Validate(snapshot);
                JsonIO.Write(Path.Combine(directory, "snapshot.enriched.json"), snapshot);
                snapshots.Add(id, snapshot);
                results.Add(new JsonObject { ["id"] = id, ["status"] = "captured", ["snapshot"] = id + "/snapshot.enriched.json" });
            }
            catch (Exception ex) when (ex is IOException or ProtocolException or InvalidOperationException or System.ComponentModel.Win32Exception or UnauthorizedAccessException or ArgumentException or System.Text.Json.JsonException)
            {
                exitCode = 3; results.Add(new JsonObject { ["id"] = id, ["status"] = "error", ["message"] = ex.Message });
            }
        }
        var comparisonResults = new JsonArray();
        foreach (var item in comparisons.OfType<JsonObject>())
        {
            var id = item.S("id");
            if (!snapshots.TryGetValue(item.S("left"), out var left) || !snapshots.TryGetValue(item.S("right"), out var right))
            {
                exitCode = 3; comparisonResults.Add(new JsonObject { ["id"] = id, ["status"] = "error", ["message"] = "Required profile did not complete." }); continue;
            }
            try
            {
                var compared = LayoutComparer.Compare(left, right, JsonIO.Read(Resolve(basePath, item.S("compareManifest"))));
                JsonIO.Write(Path.Combine(output, id + ".diff.json"), compared);
                await using var html = new StreamWriter(new FileStream(Path.Combine(output, id + ".html"), FileMode.CreateNew, FileAccess.Write), new UTF8Encoding(false));
                await html.WriteAsync(HtmlReport.Render(left, right, compared));
                var comparisonExit = compared["exitCode"]!.GetValue<int>(); exitCode = Math.Max(exitCode, comparisonExit);
                comparisonResults.Add(new JsonObject { ["id"] = id, ["status"] = "compared", ["exitCode"] = comparisonExit });
            }
            catch (Exception ex) when (ex is IOException or ProtocolException or InvalidOperationException or UnauthorizedAccessException or ArgumentException or System.Text.Json.JsonException)
            {
                exitCode = 3; comparisonResults.Add(new JsonObject { ["id"] = id, ["status"] = "error", ["message"] = ex.Message });
            }
        }
        var summary = new JsonObject { ["schemaVersion"] = "0.1", ["runId"] = runId, ["profiles"] = results, ["comparisons"] = comparisonResults, ["exitCode"] = exitCode };
        JsonIO.Write(Path.Combine(output, "run.json"), summary); return summary;
    }

    private static void ValidateProfile(JsonObject profile)
    {
        Keys(profile, "id executable arguments workingDirectory configuration target outputArgument requiredCapabilities timeoutSeconds buildSteps artifact", "profile");
        SafeId(profile.S("id")); profile.S("configuration"); profile.S("outputArgument"); profile.S("artifact");
        var target = Obj(profile["target"], "target"); Keys(target, "os architecture", "target"); target.S("os"); target.S("architecture");
        Strings(profile["requiredCapabilities"], "requiredCapabilities"); ValidateCommand(profile);
        if (profile.ContainsKey("buildSteps"))
            foreach (var step in Arr(profile["buildSteps"], "buildSteps"))
            {
                var command = Obj(step, "build step"); Keys(command, "executable arguments workingDirectory timeoutSeconds", "build step"); ValidateCommand(command);
            }
    }

    private static void ValidateCommand(JsonObject command)
    {
        command.S("executable"); command.S("workingDirectory"); Strings(command["arguments"], "arguments");
        var timeout = Number(command["timeoutSeconds"], "timeoutSeconds"); if (timeout <= 0 || timeout > 3600) throw new ProtocolException("Command timeout must be 1..3600 seconds.");
    }

    private static void SafeId(string id)
    {
        if (id.Length > 80 || !System.Text.RegularExpressions.Regex.IsMatch(id, "^[a-zA-Z0-9][a-zA-Z0-9_-]*$")) throw new ProtocolException("Invalid profile/comparison path ID.");
    }
    private static string Resolve(string directory, string path) => Path.GetFullPath(path, directory);

    private static async Task Execute(JsonObject command, string basePath, string logs, string prefix, string[] extra)
    {
        var executable = command.S("executable");
        if (executable.Contains('/') || executable.Contains('\\')) executable = Resolve(basePath, executable);
        var info = new ProcessStartInfo(executable) { WorkingDirectory = Resolve(basePath, command.S("workingDirectory")), UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true, CreateNoWindow = true };
        foreach (var argument in Arr(command["arguments"], "arguments")) info.ArgumentList.Add(Str(argument, "argument"));
        foreach (var argument in extra) info.ArgumentList.Add(argument);
        var exit = await BoundedProcess.ExecuteAsync(info, checked((int)Number(command["timeoutSeconds"], "timeoutSeconds")),
            Path.Combine(logs, prefix + ".stdout.log"), Path.Combine(logs, prefix + ".stderr.log"));
        if (exit != 0) throw new ProtocolException(prefix + " exited " + exit + "; see logs.");
    }

    private static async Task<(string Revision, bool Dirty, string Digest)> SourceIdentity(string root)
    {
        async Task<string> Git(params string[] args)
        {
            var info = new ProcessStartInfo("git") { WorkingDirectory = root, RedirectStandardOutput = true, RedirectStandardError = true, UseShellExecute = false, CreateNoWindow = true };
            foreach (var arg in args) info.ArgumentList.Add(arg);
            using var process = Process.Start(info) ?? throw new ProtocolException("Cannot run git for source identity.");
            var text = process.StandardOutput.ReadToEndAsync(); var error = process.StandardError.ReadToEndAsync();
            await process.WaitForExitAsync(); if (process.ExitCode != 0) throw new ProtocolException("Source identity: " + await error);
            return await text;
        }
        var revision = (await Git("rev-parse", "HEAD")).Trim();
        var repoRoot = (await Git("rev-parse", "--show-toplevel")).Trim();
        root = repoRoot;
        var dirty = !string.IsNullOrEmpty(await Git("status", "--porcelain", "--untracked-files=normal"));
        var paths = (await Git("ls-files", "--cached", "--others", "--exclude-standard", "--full-name", "-z")).Split('\0', StringSplitOptions.RemoveEmptyEntries).Distinct().Order(StringComparer.Ordinal);
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        foreach (var path in paths)
        {
            hash.AppendData(Encoding.UTF8.GetBytes(path + "\0"));
            var full = Path.GetFullPath(path, repoRoot);
            if (!File.Exists(full)) { hash.AppendData(Encoding.UTF8.GetBytes("deleted\0")); continue; }
            hash.AppendData(SHA256.HashData(await File.ReadAllBytesAsync(full)));
        }
        return (revision, dirty, "sha256:" + Convert.ToHexStringLower(hash.GetHashAndReset()));
    }
}
