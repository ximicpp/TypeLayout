using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text.Json.Nodes;
using Microsoft.Diagnostics.Runtime;

namespace LayoutObserver.ClrMd;

public static class ObjectCapture
{
    public const string ClrMdPackageVersion = "4.1.745802";

    public static async Task<JsonObject> CaptureAsync(CaptureOptions options, CancellationToken cancellationToken = default)
    {
        string host = Path.GetFullPath(options.HostPath);
        if (!File.Exists(host)) throw new CaptureException("host-not-found", "Object host artifact does not exist: " + host);
        if (options.DacPath is not null && !File.Exists(options.DacPath)) throw new CaptureException("dac-not-found", "Requested DAC does not exist.");
        if (options.TimeoutMilliseconds <= 0) throw new CaptureException("invalid-timeout", "Timeout must be positive.");
        string dotnet = options.DotnetPath ?? FindDotnet();
        var start = new ProcessStartInfo(dotnet) { RedirectStandardInput = true, RedirectStandardOutput = true, RedirectStandardError = true, UseShellExecute = false, CreateNoWindow = true };
        start.ArgumentList.Add(host);
        if (options.FailFactory) start.ArgumentList.Add("--fail-factory");
        if (options.DelayReadyMilliseconds > 0) { start.ArgumentList.Add("--delay-ready-ms"); start.ArgumentList.Add(options.DelayReadyMilliseconds.ToString()); }
        using var process = new Process { StartInfo = start };
        if (!process.Start()) throw new CaptureException("host-start-failed", "Could not start the controlled object host.");
        Task<string> errors = process.StandardError.ReadToEndAsync(cancellationToken);
        try
        {
            using var deadline = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            deadline.CancelAfter(options.TimeoutMilliseconds);
            string? ready;
            try { ready = await process.StandardOutput.ReadLineAsync(deadline.Token); }
            catch (OperationCanceledException error) { throw new CaptureException("host-timeout", "Object host did not reach the capture barrier in time.", error); }
            if (ready is null || !ready.StartsWith("LAYOUT_READY_V1 ", StringComparison.Ordinal))
            {
                if (!process.HasExited) process.Kill(entireProcessTree: true);
                await process.WaitForExitAsync(cancellationToken);
                throw new CaptureException("host-factory-failed", "Object host failed before readiness: " + await errors);
            }
            JsonObject calibration = JsonNode.Parse(ready[16..])!.AsObject();
            if (options.Configuration is not null && calibration["configuration"]!.GetValue<string>() != options.Configuration)
                throw new CaptureException("configuration-mismatch", "Host configuration differs from the requested profile.");
            // Never inspect the moving live heap or reuse an address reported by the child.
            using DataTarget snapshot = DataTarget.CreateSnapshotAndAttach(process.Id);
            if (snapshot.ClrVersions.Length != 1) throw new CaptureException("runtime-count", "Exactly one CoreCLR runtime is required.");
            ClrInfo info = snapshot.ClrVersions[0];
            if (info.Version.Major != 10) throw new CaptureException("runtime-not-calibrated", "Object origin adapter is calibrated for CoreCLR 10 only.");
            string actualRuntime = calibration["runtimeVersion"]!.GetValue<string>();
            if (options.RequiredRuntimeVersion is not null && options.RequiredRuntimeVersion != actualRuntime)
                throw new CaptureException("runtime-mismatch", "Target runtime does not match the required version.");
            using ClrRuntime runtime = options.DacPath is null ? info.CreateRuntime() : info.CreateRuntime(options.DacPath);
            if (!runtime.Heap.CanWalkHeap) throw new CaptureException("heap-not-walkable", "The frozen heap cannot be walked consistently.");
            var cases = new Dictionary<string, ClrObject>(StringComparer.Ordinal);
            foreach (ClrObject candidate in runtime.Heap.EnumerateObjects())
            {
                if (candidate.Type?.Name != "LayoutObserver.ObjectHost.CaptureCase") continue;
                string? id = candidate.ReadStringField("Id");
                ClrObject value = candidate.ReadObjectField("Value");
                if (string.IsNullOrWhiteSpace(id) || !value.IsValid) throw new CaptureException("invalid-case-wrapper", "Snapshot contains an invalid registered case.");
                if (!cases.TryAdd(id, value)) throw new CaptureException("duplicate-case-id", "Snapshot contains duplicate logical IDs: " + id);
            }
            if (cases.Count != calibration["caseCount"]!.GetValue<int>()) throw new CaptureException("missing-cases", "Not all strong-root cases were found in the snapshot.");
            int pointerBytes = snapshot.DataReader.PointerSize;
            if (pointerBytes != calibration["pointerBytes"]!.GetValue<int>()) throw new CaptureException("architecture-mismatch", "Target pointer width does not match its readiness handshake.");
            if (snapshot.DataReader.Architecture.ToString().ToLowerInvariant() != calibration["architecture"]!.GetValue<string>())
                throw new CaptureException("architecture-mismatch", "Snapshot architecture does not match the host handshake.");
            Calibrate(cases, calibration, pointerBytes);
            return new ObjectSnapshotWriter(actualRuntime, pointerBytes, host, calibration).Write(cases, options.RunId ?? Guid.NewGuid().ToString("N"));
        }
        catch (CaptureException) { throw; }
        catch (Exception error) { throw new CaptureException("snapshot-or-dac-failed", error.Message, error); }
        finally
        {
            if (!process.HasExited)
            {
                try { await process.StandardInput.WriteLineAsync("RELEASE"); await process.StandardInput.FlushAsync(); } catch (IOException) { }
                using var cleanup = new CancellationTokenSource(TimeSpan.FromSeconds(5));
                try { await process.WaitForExitAsync(cleanup.Token); }
                catch (OperationCanceledException) { process.Kill(entireProcessTree: true); await process.WaitForExitAsync(CancellationToken.None); }
            }
        }
    }

    private static void Calibrate(Dictionary<string, ClrObject> cases, JsonObject calibration, int pointerBytes)
    {
        if (cases["empty-object"].Size != (ulong)(pointerBytes * 3)) throw new CaptureException("calibration-failed", "CoreCLR minimum object size differs from the calibrated contract.");
        ClrObject plain = cases["boxed-plain"];
        foreach (var expected in new[] { ("Tag", 0), ("Count", 4), ("Code", 8) })
        {
            ClrInstanceField field = RequireField(plain.Type!, expected.Item1);
            if (field.GetAddress(plain.Address) - plain.Address != (ulong)(pointerBytes + expected.Item2))
                throw new CaptureException("calibration-failed", "Boxed field origin differs from the independent blittable fixture.");
        }
        ClrObject reference = cases["boxed-reference"];
        string[] names = ["Tag", "Target", "Count"];
        for (int i = 0; i < names.Length; i++)
            if ((long)(RequireField(reference.Type!, names[i]).GetAddress(reference.Address) - reference.Address) - pointerBytes != calibration["referenceOffsets"]![i]!.GetValue<long>())
                throw new CaptureException("calibration-failed", "Reference-containing value offset differs from the host's typed byref probe.");
        ClrObject derived = cases["private-derived"];
        string[] inherited = ["_tag", "_count", "_code", "_target"];
        long anchor = (long)RequireField(derived.Type!, inherited[0]).GetAddress(derived.Address);
        for (int i = 0; i < inherited.Length; i++)
            if ((long)RequireField(derived.Type!, inherited[i]).GetAddress(derived.Address) - anchor != calibration["derivedDistances"]![i]!.GetValue<long>())
                throw new CaptureException("calibration-failed", "Private inherited field distance differs from the host's typed byrefs.");
        if (cases["byte-array-9"].Size - cases["byte-array-1"].Size != 8 || cases["int-array-3"].Size - cases["int-array-1"].Size != 8 ||
            cases["string-5"].Size - cases["string-1"].Size != 8)
            throw new CaptureException("calibration-failed", "Variable-length object size slope is inconsistent.");
    }

    private static ClrInstanceField RequireField(ClrType type, string name) => type.GetFieldByName(name)
        ?? throw new CaptureException("required-field-unavailable", $"Required field {type.Name}.{name} is unavailable; private/inherited coverage is incomplete.");

    private static string FindDotnet()
    {
        string? configured = Environment.GetEnvironmentVariable("DOTNET_HOST_PATH");
        if (!string.IsNullOrEmpty(configured) && File.Exists(configured)) return configured;
        DirectoryInfo version = new(RuntimeEnvironment.GetRuntimeDirectory().TrimEnd(Path.DirectorySeparatorChar));
        string path = Path.Combine(version.Parent!.Parent!.Parent!.FullName, OperatingSystem.IsWindows() ? "dotnet.exe" : "dotnet");
        if (!File.Exists(path)) throw new CaptureException("dotnet-not-found", "Pass --dotnet with the matching runtime host.");
        return path;
    }
}
