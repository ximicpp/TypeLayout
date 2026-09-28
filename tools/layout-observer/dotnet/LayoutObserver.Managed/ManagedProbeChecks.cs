using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using LayoutObserver.Managed.Fixtures;

namespace LayoutObserver.Managed;

/// <summary>Fixture assertions use independently specified layouts and interop APIs for blittable values.</summary>
public static class ManagedProbeChecks
{
    public static void Run()
    {
        var results = GeneratedProbeRegistry.Capture().ToDictionary(result => result.TypeId);
        Check(results["sample"], 12, ("Tag", 0), ("Count", 4), ("Code", 8));
        Check(results["packed"], 7, ("Tag", 0), ("Count", 1), ("Code", 5));
        Check(results["reordered"], 8, ("Count", 0), ("Code", 4), ("Tag", 6));
        Check(results["nested"], 16, ("Payload", 0), ("Extra", 12));
        Check(results["array"], 12, ("Values", 0));
        Check(results["enum"], 2, ("Value", 0));
        Check(results["explicit-record"], 8, ("Whole", 0), ("Low", 0), ("Tail", 4));
        Check(results["boolean-record"], 2, ("Flag", 0), ("Tag", 1));
        Check(results["empty-record"], 1);
        Check(results["enum-scalar"], 2);
        Check(results["int-scalar"], 4);
        Require(results["enum-scalar"].Type?.Kind == "enum", "Enum scalar descriptor.");
        Require(Marshal.SizeOf<FixedRecord>() == results["sample"].SizeBytes, "Blittable Marshal size differs.");
        Require(Marshal.OffsetOf<FixedRecord>(nameof(FixedRecord.Count)).ToInt64() == results["sample"].Fields.Single(f => f.Name == "Count").OffsetBytes, "Blittable Marshal field offset differs.");
        Require(Marshal.SizeOf<BooleanRecord>() == 8, "Runtime marshalling bool fixture changed.");
        Require(results["private-record"].IsComplete && results["private-record"].Fields.Count == 2, "Partial private fields were omitted.");
        Require(results["generic-int-record"].IsComplete && results["generic-double-record"].IsComplete, "Closed generic partial probe is incomplete.");
        Require(results["generic-int-record"].Fields.Single(f => f.Name == "_value").SizeBytes == 4, "Generic int field size.");
        Require(results["generic-double-record"].Fields.Single(f => f.Name == "_value").SizeBytes == 8, "Generic double field size.");
        Require(!results["inaccessible-private-record"].IsComplete && results["inaccessible-private-record"].Fields.Count == 1, "Inaccessible field was lost or invented.");
        Require(results["unsupported-class"].Limitation == "static-probe-requires-value-type", "Class reference was mistaken for object data.");
        ProbeType inline = results["array"].Fields.Single().Type;
        Require(inline.Kind == "array" && inline.FixedCount == 3 && inline.Element?.Id == "i32", "Inline array descriptor is not physical i32[3].");
        foreach (ProbeResult result in results.Values.Where(r => r.Limitation is null))
            Require(result.ArrayStrideBytes == result.SizeBytes, "Array stride differs from value size: " + result.TypeId);

        // Both references remain managed interior byrefs into one object while a compacting GC runs.
        ReferenceRecord[] instances = new ReferenceRecord[2];
        instances[0].Target = new object();
        ref byte origin = ref Unsafe.As<ReferenceRecord, byte>(ref instances[0]);
        ref byte target = ref Unsafe.As<object?, byte>(ref instances[0].Target);
        long before = (long)Unsafe.ByteOffset(ref origin, ref target);
        GC.Collect(2, GCCollectionMode.Forced, blocking: true, compacting: true);
        long after = (long)Unsafe.ByteOffset(ref origin, ref target);
        Require(before == after && before == results["reference-record"].Fields.Single(f => f.Name == "Target").OffsetBytes, "Managed byref offsets changed across GC.");
        GC.KeepAlive(instances);

        var snapshot = ManagedCapture.Capture("checks");
        var observations = snapshot["observations"]!.AsArray();
        var nested = observations.Single(node => node!["id"]!.GetValue<string>() == "nested/payload")!;
        Require(nested["context"]!["hostObservationId"]!.GetValue<string>() == "nested", "Embedded context linkage missing.");
        var arrayElement = observations.Single(node => node!["id"]!.GetValue<string>() == "array/values/2")!;
        Require(arrayElement["context"]!["kind"]!.GetValue<string>() == "array-element" &&
            arrayElement["context"]!["elementIndex"]!.GetValue<int>() == 2 &&
            arrayElement["metrics"]!["arrayStrideBytes"]!["value"]!.GetValue<long>() == 4, "Inline array element context/stride.");
        Require(observations.All(node => node!["metrics"]!["alignmentBytes"]!["state"]!.GetValue<string>() == "unknown"), "Managed alignment was fabricated.");
    }

    private static void Check(ProbeResult result, int size, params (string Name, long Offset)[] fields)
    {
        Require(result.IsComplete, result.TypeId + " has incomplete field coverage.");
        Require(result.SizeBytes == size && result.ArrayStrideBytes == size, result.TypeId + " size/stride.");
        Require(result.Fields.Count == fields.Length, result.TypeId + " field count.");
        foreach (var field in fields) Require(result.Fields.Single(f => f.Name == field.Name).OffsetBytes == field.Offset, result.TypeId + "." + field.Name + " offset.");
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException("FAIL: " + message);
    }
}
