using System.Runtime.InteropServices;
using System.Text.Json.Nodes;
using LayoutObserver.Managed;

[assembly: ObserveLayout(typeof(LayoutObserver.ManagedChecks.ConsumerPrivate), "consumer-private")]
[assembly: ObserveLayout(typeof(short), "consumer-i16")]

namespace LayoutObserver.ManagedChecks;

// This type belongs to the consumer assembly, not the collector assembly.
[StructLayout(LayoutKind.Sequential, Pack = 1)]
internal partial struct ConsumerPrivate
{
    private readonly byte tag;
    private readonly int count;

    public ConsumerPrivate(byte tag, int count) { this.tag = tag; this.count = count; }
    public int Sum() => tag + count;
}

internal static class ConsumerChecks
{
    public static JsonObject Run()
    {
        IReadOnlyList<ProbeResult> results = GeneratedProbeRegistry.Capture();
        if (!results.Select(result => result.TypeId).Order().SequenceEqual(new[] { "consumer-i16", "consumer-private" }))
            throw new InvalidOperationException("Consumer registry must contain exactly its own registrations.");
        ProbeResult record = results.Single(result => result.TypeId == "consumer-private");
        if (!record.IsComplete || record.SizeBytes != 5 || record.ArrayStrideBytes != 5 ||
            record.Fields.Single(field => field.Name == "tag").OffsetBytes != 0 ||
            record.Fields.Single(field => field.Name == "count").OffsetBytes != 1)
            throw new InvalidOperationException("Consumer partial private/readonly fields were not measured.");
        JsonObject snapshot = ManagedCapture.CaptureRegistered(results, GeneratedProbeRegistry.CompilerVersion,
            GeneratedProbeRegistry.BuildConfiguration, "consumer-check", GeneratedProbeRegistry.BuildConfiguration);
        if (snapshot["observations"]!.AsArray().Count != 2 ||
            snapshot["observations"]!.AsArray().Any(observation => observation!["id"]!.GetValue<string>() == "sample") ||
            snapshot["build"]!["compiler"]!["version"]!.GetValue<string>() != GeneratedProbeRegistry.CompilerVersion ||
            snapshot["build"]!["configuration"]!.GetValue<string>() != GeneratedProbeRegistry.BuildConfiguration)
            throw new InvalidOperationException("Consumer capture mixed built-in fixtures or the wrong compilation identity.");
        try
        {
            _ = ManagedCapture.CaptureRegistered(results, GeneratedProbeRegistry.CompilerVersion,
                GeneratedProbeRegistry.BuildConfiguration, requestedConfiguration: "not-the-compiled-configuration");
            throw new InvalidOperationException("Consumer capture accepted a false configuration label.");
        }
        catch (ArgumentException) { }
        Console.WriteLine("PASS: external consumer registry, private readonly fields, own observations and compilation identity");
        return snapshot;
    }
}
