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
        if (profiles.Keys.Distinct(StringComparer.OrdinalIgnoreCase).Count() != profiles.Count)
            throw new ProtocolException("Profile path IDs must be unique ignoring case.");
        var comparisons = Arr(manifest["comparisons"], "comparisons");
        if (comparisons.Count == 0) throw new ProtocolException("No comparisons requested.");
        foreach (var profile in profiles.Values) ValidateProfile(profile);
        foreach (var item in comparisons.OfType<JsonObject>())
        {
            Keys(item, "id left right compareManifest", "comparison");
            if (!profiles.ContainsKey(item.S("left")) || !profiles.ContainsKey(item.S("right"))) throw new ProtocolException("Unknown comparison profile.");
            SafeId(item.S("id")); LayoutComparer.ValidateManifest(JsonIO.Read(Resolve(basePath, item.S("compareManifest"))));
        }
        if (comparisons.OfType<JsonObject>().Select(c => c.S("id")).Distinct(StringComparer.OrdinalIgnoreCase).Count() != comparisons.Count) throw new ProtocolException("Duplicate or invalid comparison path IDs (case-insensitive).");
        Directory.CreateDirectory(output);
        var snapshots = new Dictionary<string, JsonObject>(); var results = new JsonArray(); var exitCode = 0;
        var runId = Guid.NewGuid().ToString("N");
        var defaultSourceRoot = !manifest.ContainsKey("sourceRoot") ? basePath : Resolve(basePath, manifest.S("sourceRoot"));
        var profileSources = new Dictionary<string, string>(StringComparer.Ordinal);
        var sources = new Dictionary<string, SourceStamp>(PathComparer);
        var sourceErrors = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var profile in profiles.Values)
        {
            try
            {
                var requestedRoot = profile.ContainsKey("sourceRoot") ? Resolve(basePath, profile.S("sourceRoot")) : defaultSourceRoot;
                var root = Path.GetFullPath((await Git(requestedRoot, "rev-parse", "--show-toplevel")).Trim());
                if (!sources.ContainsKey(root)) sources.Add(root, await SourceIdentity(root));
                profileSources.Add(profile.S("id"), root);
            }
            catch (Exception ex) when (ex is IOException or ProtocolException or InvalidOperationException or System.ComponentModel.Win32Exception or UnauthorizedAccessException or ArgumentException)
            {
                sourceErrors.Add(profile.S("id"), ex.Message);
            }
        }
        async Task CheckSources()
        {
            foreach (var (root, original) in sources)
                if (await SourceIdentity(root) != original)
                    throw new ProtocolException("Source tree changed during this run; start a fresh run against stable inputs.");
        }
        foreach (var profile in profiles.Values)
        {
            var id = profile.S("id"); var directory = Path.Combine(output, id); Directory.CreateDirectory(directory);
            try
            {
                if (sourceErrors.TryGetValue(id, out var sourceError)) throw new ProtocolException("Source registration failed: " + sourceError);
                await CheckSources();
                var source = sources[profileSources[id]];
                if (profile["buildSteps"] is JsonArray steps)
                {
                    for (var i = 0; i < steps.Count; i++) await Execute(Obj(steps[i], "build step"), basePath, directory, "build-" + i, []);
                }
                await CheckSources();
                var artifact = Resolve(basePath, profile.S("artifact"));
                var artifactDigest = await ArtifactDigest(artifact);
                var snapshotPath = Path.Combine(directory, "snapshot.json");
                await Execute(profile, basePath, directory, "capture", [profile.S("outputArgument"), snapshotPath]);
                if (await ArtifactDigest(artifact) != artifactDigest)
                    throw new ProtocolException("Declared artifact changed during capture; its identity is not stable.");
                await CheckSources();
                if (!File.Exists(snapshotPath)) throw new ProtocolException("Collector produced no snapshot.");
                var snapshot = JsonIO.Read(snapshotPath); SnapshotValidator.Validate(snapshot);
                var actual = Obj(snapshot["build"], "build"); var actualTarget = Obj(actual["target"], "target");
                var requested = Obj(profile["target"], "target");
                if (actualTarget.S("architecture") != requested.S("architecture") || actualTarget.S("os") != requested.S("os") || actual.S("configuration") != profile.S("configuration"))
                    throw new ProtocolException("Actual target/configuration does not match requested profile.");
                var capabilities = Arr(snapshot["producer"]!["capabilities"], "capabilities").Select(n => Str(n, "capability")).ToHashSet();
                foreach (var capability in Arr(profile["requiredCapabilities"], "requiredCapabilities"))
                    if (!capabilities.Contains(Str(capability, "capability"))) throw new ProtocolException("Collector lacks required capability " + capability);
                if (actual.S("artifactDigest") is var claimedDigest && claimedDigest != "unknown" && NormalizeArtifactClaim(claimedDigest) != artifactDigest)
                    throw new ProtocolException("Collector artifactDigest does not match the declared stable artifact.");
                // Preserve all collector claims before enriching this checkout observation. A prebuilt
                // artifact hash alone is not evidence that the current source produced that artifact.
                actual["collectorProvenance"] = actual.DeepClone();
                actual["captureProvenance"] = new JsonObject
                {
                    ["sourceBinding"] = profile.ContainsKey("sourceRoot") ? "profile" : "run-default",
                    ["sourceDigestScope"] = "git-head-index-worktree-recursive-submodules-v1",
                    ["artifactVerifiedStable"] = true,
                    ["sourceRelation"] = profile["buildSteps"] is JsonArray { Count: > 0 } ? "built-in-run" : "unverified"
                };
                actual["artifactDigest"] = artifactDigest;
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
        // A later profile may modify a source used by an earlier successful profile. Never
        // compare those earlier snapshots after a run-wide stability failure.
        try { await CheckSources(); }
        catch (Exception ex) when (ex is IOException or ProtocolException or InvalidOperationException or System.ComponentModel.Win32Exception or UnauthorizedAccessException or ArgumentException)
        {
            exitCode = 3; snapshots.Clear();
            foreach (var result in results.OfType<JsonObject>().Where(r => r.S("status") == "captured"))
            {
                result["status"] = "error"; result["message"] = "Run-wide source validation failed: " + ex.Message;
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
        try { await CheckSources(); }
        catch (Exception ex) when (ex is IOException or ProtocolException or InvalidOperationException or System.ComponentModel.Win32Exception or UnauthorizedAccessException or ArgumentException)
        {
            exitCode = 3;
            foreach (var result in results.Concat(comparisonResults).OfType<JsonObject>().Where(r => r.S("status") is "captured" or "compared"))
            {
                result["status"] = "error"; result["message"] = "Final source validation failed: " + ex.Message;
            }
        }
        var summary = new JsonObject { ["schemaVersion"] = "0.1", ["runId"] = runId, ["profiles"] = results, ["comparisons"] = comparisonResults, ["exitCode"] = exitCode };
        JsonIO.Write(Path.Combine(output, "run.json"), summary); return summary;
    }

    private static void ValidateProfile(JsonObject profile)
    {
        Keys(profile, "id executable arguments workingDirectory configuration target outputArgument requiredCapabilities timeoutSeconds buildSteps artifact sourceRoot", "profile");
        SafeId(profile.S("id")); profile.S("configuration"); profile.S("outputArgument"); profile.S("artifact");
        if (profile.ContainsKey("sourceRoot")) profile.S("sourceRoot");
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
        if (System.Text.RegularExpressions.Regex.IsMatch(id, "^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$", System.Text.RegularExpressions.RegexOptions.IgnoreCase | System.Text.RegularExpressions.RegexOptions.CultureInvariant))
            throw new ProtocolException("Profile/comparison path IDs cannot use reserved Windows device names.");
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

    private static readonly StringComparer PathComparer = OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
    private sealed record SourceStamp(string Revision, bool Dirty, string Digest);

    private static async Task<string> ArtifactDigest(string path)
    {
        if (!File.Exists(path)) throw new ProtocolException("Declared artifact does not exist.");
        await using var stream = File.OpenRead(path);
        return "sha256:" + Convert.ToHexStringLower(await SHA256.HashDataAsync(stream));
    }

    private static string NormalizeArtifactClaim(string claim)
    {
        // Older collectors emitted bare SHA-256 hex. Preserve that original claim in
        // collectorProvenance, but compare only canonical, strictly validated digests.
        var match = System.Text.RegularExpressions.Regex.Match(claim, @"\A(?:sha256:)?([0-9a-f]{64})\z",
            System.Text.RegularExpressions.RegexOptions.IgnoreCase | System.Text.RegularExpressions.RegexOptions.CultureInvariant);
        if (!match.Success) throw new ProtocolException("Collector artifactDigest has an unsupported format; expected 64 hexadecimal SHA-256 digits, optionally prefixed with sha256:.");
        return "sha256:" + match.Groups[1].Value.ToLowerInvariant();
    }

    private static async Task<string> Git(string root, params string[] args)
    {
        var info = new ProcessStartInfo("git")
        {
            WorkingDirectory = root, RedirectStandardOutput = true, RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8,
            UseShellExecute = false, CreateNoWindow = true
        };
        // Explicit sourceRoot must not accidentally inherit another checkout/index from a caller.
        foreach (var variable in new[] { "GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_COMMON_DIR", "GIT_PREFIX" }) info.Environment.Remove(variable);
        info.Environment["GIT_OPTIONAL_LOCKS"] = "0";
        foreach (var arg in args) info.ArgumentList.Add(arg);
        Process started;
        try { started = Process.Start(info) ?? throw new ProtocolException("Cannot run git for source identity."); }
        catch (System.ComponentModel.Win32Exception ex) { throw new ProtocolException("Cannot run git for source identity: " + ex.Message); }
        using var process = started;
        using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(30));
        var text = process.StandardOutput.ReadToEndAsync(deadline.Token); var error = process.StandardError.ReadToEndAsync(deadline.Token);
        try { await Task.WhenAll(process.WaitForExitAsync(deadline.Token), text, error).WaitAsync(deadline.Token); }
        catch (OperationCanceledException) when (deadline.IsCancellationRequested)
        {
            if (!process.HasExited) process.Kill(entireProcessTree: true);
            throw new ProtocolException("Git source identity timed out after 30 seconds.");
        }
        if (process.ExitCode != 0) throw new ProtocolException("Source identity: " + await error);
        return await text;
    }

    private static Task<SourceStamp> SourceIdentity(string root) => SourceIdentity(root, new HashSet<string>(PathComparer), 0);

    private static async Task<SourceStamp> SourceIdentity(string root, HashSet<string> ancestors, int depth)
    {
        root = Path.GetFullPath(root);
        if (depth > 32 || !ancestors.Add(root)) throw new ProtocolException("Source submodule graph is cyclic or exceeds 32 levels.");
        var repoRoot = Path.GetFullPath((await Git(root, "rev-parse", "--show-toplevel")).Trim());
        if (!PathComparer.Equals(root.TrimEnd(Path.DirectorySeparatorChar), repoRoot.TrimEnd(Path.DirectorySeparatorChar)))
            throw new ProtocolException("A source submodule is missing or not initialized as its own checkout.");
        var revision = (await Git(root, "rev-parse", "HEAD")).Trim();
        var dirty = !string.IsNullOrEmpty(await Git(root, "status", "--porcelain", "--untracked-files=normal", "--ignore-submodules=none"));
        var indexEntries = (await Git(root, "ls-files", "--stage", "-z")).Split('\0', StringSplitOptions.RemoveEmptyEntries).Order(StringComparer.Ordinal).ToArray();
        var gitlinks = new HashSet<string>(StringComparer.Ordinal);
        var paths = new HashSet<string>(StringComparer.Ordinal);
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        void Append(string value) => hash.AppendData(Encoding.UTF8.GetBytes(value + "\0"));
        Append("git-head-index-worktree-recursive-submodules-v1"); Append(revision);
        foreach (var entry in indexEntries)
        {
            var tab = entry.IndexOf('\t');
            if (tab < 0) throw new ProtocolException("Invalid Git index entry while reading source identity.");
            Append("index"); Append(entry);
            var path = entry[(tab + 1)..]; paths.Add(path);
            if (entry.StartsWith("160000 ", StringComparison.Ordinal)) gitlinks.Add(path);
        }
        foreach (var path in (await Git(root, "ls-files", "--others", "--exclude-standard", "-z")).Split('\0', StringSplitOptions.RemoveEmptyEntries)) paths.Add(path);
        foreach (var path in paths.Order(StringComparer.Ordinal))
        {
            Append("worktree"); Append(path);
            var full = Path.GetFullPath(path, repoRoot);
            var link = new FileInfo(full).LinkTarget;
            if (!gitlinks.Contains(path) && link is not null) { Append("symlink"); Append(link); continue; }
            if (gitlinks.Contains(path) || Directory.Exists(full))
            {
                if (!Directory.Exists(full)) throw new ProtocolException("Source submodule is missing: " + path);
                var child = await SourceIdentity(full, ancestors, depth + 1);
                Append("repository"); Append(child.Revision); Append(child.Digest); Append(child.Dirty ? "dirty" : "clean");
                dirty |= child.Dirty;
                continue;
            }
            if (!File.Exists(full)) { Append("deleted"); continue; }
            Append("file");
            if (!OperatingSystem.IsWindows()) Append(((int)File.GetUnixFileMode(full)).ToString(System.Globalization.CultureInfo.InvariantCulture));
            await using var stream = File.OpenRead(full);
            hash.AppendData(await SHA256.HashDataAsync(stream));
        }
        ancestors.Remove(root);
        return new SourceStamp(revision, dirty, "sha256:" + Convert.ToHexStringLower(hash.GetHashAndReset()));
    }
}
