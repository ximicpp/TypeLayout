using System.Text.Json.Nodes;
using LayoutObserver.ClrMd;
using LayoutObserver.Core;

if (args.Length != 1) throw new ArgumentException("Supply the ObjectHost DLL path.");
string host = Path.GetFullPath(args[0]);
var options = new CaptureOptions(host, RunId: "object-checks");
JsonObject snapshot = await ObjectCapture.CaptureAsync(options);
SnapshotValidator.Validate(snapshot);
var observations = snapshot["observations"]!.AsArray().OfType<JsonObject>().ToDictionary(o => o["id"]!.GetValue<string>());
int pointer = snapshot["build"]!["target"]!["pointerBits"]!.GetValue<int>() / 8;
Require(Number("empty-object", "runtimeReportedObjectBytes") == pointer * 3, "Empty object minimum.");
Require(observations["private-derived"]["members"]!.AsArray().Count == 4, "Inherited private field coverage.");
Require(MemberOffset("boxed-plain", "tag") == pointer * 8, "Boxed value data starts after method table.");
Require(MemberOffset("boxed-plain", "count") == (pointer + 4) * 8, "Boxed count origin.");
Require(Number("byte-array-9", "runtimeReportedObjectBytes") - Number("byte-array-1", "runtimeReportedObjectBytes") == 8, "Byte array length slope.");
Require(Number("string-5", "runtimeReportedObjectBytes") - Number("string-1", "runtimeReportedObjectBytes") == 8, "String length slope.");
Require(Number("int-array-3", "arrayStrideBytes") == 4, "Array stride.");
Require(Number("reference-array-2", "arrayStrideBytes") == pointer, "Reference array slot stride.");
Require(observations["string-2"]["instanceShape"]!["length"]!.GetValue<int>() == 2, "String length metadata.");
if (pointer == 8)
{
    Require(Number("byte-array-1", "runtimeReportedObjectBytes") == 25, "ClrMD variable object size is not an 8-byte rounded allocation extent.");
    Require(Number("string-2", "runtimeReportedObjectBytes") == 26, "ClrMD string size includes terminator but not allocation rounding.");
}
foreach (JsonObject observation in observations.Values.Where(o => o["context"]!["kind"]!.GetValue<string>() is "heap-object" or "boxed-value"))
{
    Require(observation["origin"]!["extentStartBit"]!.GetValue<int>() == -pointer * 8, "Object-reference origin must include negative ObjHeader.");
    Require(observation["origin"]!["conversionEvidence"] is JsonObject, "Origin conversion needs evidence.");
    Require(observation["metrics"]!["alignmentBytes"]!["state"]!.GetValue<string>() == "unknown", "No fabricated object alignment.");
}

await Expect("host-not-found", options with { HostPath = host + ".missing" });
await Expect("dac-not-found", options with { DacPath = host + ".missing-dac" });
await Expect("snapshot-or-dac-failed", options with { DacPath = host });
await Expect("host-factory-failed", options with { FailFactory = true });
await Expect("host-timeout", options with { DelayReadyMilliseconds = 1000, TimeoutMilliseconds = 50 });
await Expect("runtime-mismatch", options with { RequiredRuntimeVersion = "0.0.0" });
await Expect("configuration-mismatch", options with { Configuration = "NotAConfiguration" });
// A second fresh process proves that IDs, not stale addresses from the prior GC/host, are the registry keys.
JsonObject repeat = await ObjectCapture.CaptureAsync(options with { RunId = "object-checks-repeat" });
SnapshotValidator.Validate(repeat);
JsonObject manifest = JsonIO.Parse("""
{"schemaVersion":"0.1","mode":"regression","policy":"value-fields-v1","scope":"object",
"cases":[{"id":"empty","left":"empty-object","right":"empty-object"},
{"id":"private","left":"private-derived","right":"private-derived"},
{"id":"boxed","left":"boxed-reference","right":"boxed-reference"},
{"id":"string","left":"string-2","right":"string-2"}]}
""");
JsonObject comparison = LayoutComparer.Compare(snapshot, repeat, manifest);
Require(comparison["exitCode"]!.GetValue<int>() == 0, "Repeated frozen snapshots disagree: " + comparison.ToJsonString());
Console.WriteLine("PASS: CoreCLR frozen snapshot, origins, private inheritance, boxed/reference values, arrays/strings, seven failure paths, repeat capture");

long Number(string id, string metric) => observations[id]["metrics"]![metric]!["value"]!.GetValue<long>();
long MemberOffset(string id, string member) => observations[id]["members"]!.AsArray().Single(m => m!["id"]!.GetValue<string>() == member)!["offsetBits"]!["value"]!.GetValue<long>();
static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException("FAIL: " + message); }
static async Task Expect(string code, CaptureOptions options)
{
    try { await ObjectCapture.CaptureAsync(options); throw new InvalidOperationException("Expected failure: " + code); }
    catch (CaptureException exception) when (exception.Code == code) { }
}
