using System.Text.Json.Nodes;
using LayoutObserver.Core;
using LayoutObserver.Managed;

SnapshotValidator.Validate(LayoutObserver.ManagedChecks.ConsumerChecks.Run());
ManagedProbeChecks.Run();
JsonObject snapshot = ManagedCapture.Capture("protocol-check");
SnapshotValidator.Validate(snapshot);
if (args.Length > 0 && args[0] == "--architecture-matrix")
{
    if (args.Length != 5) throw new ArgumentException("--architecture-matrix requires x64 CoreCLR, x86 CoreCLR, x64 NativeAOT, x86 NativeAOT snapshots.");
    JsonObject[] matrix = args.Skip(1).Select(JsonIO.Read).ToArray();
    for (int i = 0; i < matrix.Length; i++)
    {
        SnapshotValidator.Validate(matrix[i]);
        string architecture = i % 2 == 0 ? "x64" : "x86";
        int bits = i % 2 == 0 ? 64 : 32;
        string runtime = i < 2 ? "CoreCLR" : "NativeAOT";
        Require(matrix[i]["build"]!["target"]!["architecture"]!.GetValue<string>() == architecture &&
            matrix[i]["build"]!["target"]!["pointerBits"]!.GetValue<int>() == bits &&
            matrix[i]["build"]!["runtime"]!["name"]!.GetValue<string>() == runtime,
            "Matrix cell does not describe the requested actual execution: " + args[i + 1]);
    }
    string[] ids = ["sample", "packed", "reordered", "nested", "array", "enum", "private-record", "generic-int-record", "generic-double-record", "reference-record", "boolean-record", "explicit-record"];
    var cases = new JsonArray();
    foreach (string id in ids) cases.Add((JsonNode)new JsonObject { ["id"] = id, ["left"] = id, ["right"] = id });
    var matrixManifest = new JsonObject { ["schemaVersion"] = "0.1", ["mode"] = "regression", ["policy"] = "value-fields-v1", ["scope"] = "value", ["cases"] = cases };
    foreach (int i in new[] { 0, 1 })
    {
        JsonObject comparison = LayoutComparer.Compare(matrix[i], matrix[i + 2], matrixManifest);
        Require(comparison["exitCode"]!.GetValue<int>() is 0 or 1, "JIT/AOT cell has incomplete required facts: " + comparison.ToJsonString());
    }
    matrixManifest["cases"] = JsonNode.Parse("""[{"id":"reference-width","left":"reference-record","right":"reference-record"}]""");
    JsonObject architectureDifference = LayoutComparer.Compare(matrix[0], matrix[1], matrixManifest);
    Require(architectureDifference["exitCode"]!.GetValue<int>() == 1 &&
        architectureDifference["cases"]![0]!["verdict"]!.GetValue<string>() == "different", "Actual x64/x86 reference representation difference was not detected.");
    Console.WriteLine("PASS: actual Windows x64/x86 CoreCLR/NativeAOT identities, complete cross-runtime comparisons, expected reference-width difference");
}
else foreach (string file in args) SnapshotValidator.Validate(JsonIO.Read(file));

JsonObject manifest = JsonIO.Parse("""
{"schemaVersion":"0.1","mode":"regression","policy":"value-fields-v1","scope":"value",
 "cases":[{"id":"sample","left":"sample","right":"sample"},
          {"id":"nested","left":"nested","right":"nested"},
          {"id":"array","left":"array","right":"array"},
          {"id":"enum","left":"enum","right":"enum"},
          {"id":"private","left":"private-record","right":"private-record"},
          {"id":"generic","left":"generic-int-record","right":"generic-int-record"}]}
""");
Require(LayoutComparer.Compare(snapshot, snapshot, manifest)["exitCode"]!.GetValue<int>() == 0,
    "Complete managed fixtures must compare equal under value-fields policy.");
manifest["policy"] = "value-alignment-v1";
Require(LayoutComparer.Compare(snapshot, snapshot, manifest)["exitCode"]!.GetValue<int>() == 2,
    "Unknown alignment must remain incomplete under the strict policy.");
manifest["policy"] = "value-fields-v1";
manifest["cases"] = JsonNode.Parse("""[{"id":"private","left":"inaccessible-private-record","right":"inaccessible-private-record"}]""");
Require(LayoutComparer.Compare(snapshot, snapshot, manifest)["exitCode"]!.GetValue<int>() == 2,
    "Unobserved private field must not be reported equal.");
Console.WriteLine("PASS: managed probes, protocol, nested/inline-array comparisons, unknown coverage");

static void Require(bool condition, string message)
{
    if (!condition) throw new InvalidOperationException("FAIL: " + message);
}
