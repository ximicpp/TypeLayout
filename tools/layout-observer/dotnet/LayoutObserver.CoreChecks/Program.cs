using System.Text.Json.Nodes;
using LayoutObserver.Core;

var count = 0;
void Check(string name, Action test)
{
    try { test(); Console.WriteLine("PASS " + name); count++; }
    catch (Exception ex) { Console.Error.WriteLine("FAIL " + name + ": " + ex); Environment.Exit(1); }
}
void Assert(bool condition, string reason) { if (!condition) throw new Exception(reason); }
void Reject(Action action) { try { action(); } catch (ProtocolException) { return; } throw new Exception("Invalid contract accepted."); }
JsonObject Copy(JsonObject value) => (JsonObject)value.DeepClone();
JsonObject Known(JsonNode value) => new() { ["state"] = "known", ["value"] = value, ["evidence"] = new JsonObject { ["kind"] = "fixture", ["method"] = "independent-test-data", ["version"] = "1", ["inputs"] = new JsonArray() } };
JsonObject Unknown() => new() { ["state"] = "unknown", ["reason"] = "not-observed" };
JsonObject NA() => new() { ["state"] = "not-applicable", ["reason"] = "not-a-float" };
JsonObject Range(long start, long length) => new() { ["startBit"] = start, ["lengthBits"] = length };
JsonObject Member(string id, long offset, long width, string type, int order) => new()
{
    ["id"] = id, ["displayName"] = id, ["declarationOrder"] = order, ["role"] = "field", ["typeRef"] = type,
    ["offsetBits"] = Known(JsonValue.Create(offset)!), ["bitWidth"] = Known(JsonValue.Create(width)!), ["declaredTypeSizeBits"] = Known(JsonValue.Create(width)!),
    ["occupiedRanges"] = Known(new JsonArray(Range(offset, width)))
};
JsonObject Scalar(string id, int width, string sign) => new()
{
    ["id"] = id, ["displayName"] = id, ["kind"] = "scalar", ["representation"] = new JsonObject
    {
        ["widthBits"] = Known(JsonValue.Create(width)!), ["category"] = Known(JsonValue.Create("integer")!),
        ["signedness"] = Known(JsonValue.Create(sign)!), ["encoding"] = Known(JsonValue.Create("binary-integer")!), ["floatingFormat"] = NA()
    }
};
JsonObject Fixture() => new()
{
    ["schemaVersion"] = "0.1", ["snapshotId"] = "fixture",
    ["producer"] = new JsonObject { ["id"] = "test", ["version"] = "1", ["capabilities"] = new JsonArray("values") },
    ["build"] = new JsonObject
    {
        ["buildId"] = "test", ["runId"] = "test", ["configuration"] = "Test", ["sourceRevision"] = "fixture", ["sourceDirty"] = false,
        ["sourceDigest"] = "fixture", ["artifactDigest"] = "fixture", ["compiler"] = new JsonObject { ["name"] = "fixture", ["version"] = "1" },
        ["runtime"] = new JsonObject { ["name"] = "none", ["version"] = "none" },
        ["target"] = new JsonObject { ["os"] = "test", ["architecture"] = "x64", ["abi"] = "test", ["pointerBits"] = 64, ["bitsPerByte"] = 8, ["endian"] = "little" },
        ["flags"] = new JsonArray(), ["dependencies"] = new JsonObject()
    },
    ["typeDescriptors"] = new JsonArray(Scalar("u8", 8, "unsigned"), Scalar("i32", 32, "signed"), new JsonObject { ["id"] = "record", ["kind"] = "record", ["displayName"] = "Sample" }),
    ["observations"] = new JsonArray(new JsonObject
    {
        ["id"] = "sample", ["typeId"] = "record", ["displayName"] = "Sample", ["status"] = "ok", ["limitations"] = new JsonArray(), ["view"] = "native",
        ["context"] = new JsonObject { ["kind"] = "complete-value" }, ["origin"] = new JsonObject { ["kind"] = "value-start", ["extentStartBit"] = 0 },
        ["metrics"] = new JsonObject { ["valueSizeBytes"] = Known(JsonValue.Create(8)!), ["alignmentBytes"] = Known(JsonValue.Create(4)!), ["arrayStrideBytes"] = Known(JsonValue.Create(8)!) },
        ["members"] = new JsonArray(Member("tag", 0, 8, "u8", 0), Member("count", 32, 32, "i32", 1)), ["runtimeRegions"] = new JsonArray(),
        ["coverage"] = new JsonObject { ["fieldEnumeration"] = "complete", ["extent"] = "complete", ["occupiedRanges"] = "complete", ["hiddenRegions"] = "not-applicable" }
    }), ["diagnostics"] = new JsonArray(), ["limitations"] = new JsonArray()
};
JsonObject Manifest(string mode = "regression", string policy = "value-fields-v1")
{
    var mapping = new JsonObject { ["id"] = "case", ["left"] = "sample", ["right"] = "sample" };
    if (mode != "regression") mapping["fields"] = new JsonArray(new JsonObject { ["id"] = "tag", ["left"] = "tag", ["right"] = "tag" }, new JsonObject { ["id"] = "count", ["left"] = "count", ["right"] = "count" });
    return new JsonObject { ["schemaVersion"] = "0.1", ["mode"] = mode, ["policy"] = policy, ["scope"] = "value", ["cases"] = new JsonArray(mapping) };
}
JsonObject O(JsonObject s) => (JsonObject)s["observations"]![0]!;
int Code(JsonObject result) => result["exitCode"]!.GetValue<int>();
string Verdict(JsonObject result) => result["cases"]![0]!["verdict"]!.GetValue<string>();
Check("identical independent snapshot", () => Assert(Code(LayoutComparer.Compare(Fixture(), Fixture(), Manifest())) == 0, "not same"));
Check("different compiler/build IDs are not layout differences", () => { var r = Fixture(); r["build"]!["runId"] = "another"; r["build"]!["compiler"]!["version"] = "2"; Assert(Code(LayoutComparer.Compare(Fixture(), r, Manifest())) == 0, "environment leaked into equality"); });
Check("cross-language explicit mapping", () => { var r = Fixture(); O(r)["view"] = "managed"; Assert(Code(LayoutComparer.Compare(Fixture(), r, Manifest("representation"))) == 0, "native/managed not same"); });
Check("known size change", () => { var r = Fixture(); O(r)["metrics"]!["valueSizeBytes"]!["value"] = 16; Assert(Code(LayoutComparer.Compare(Fixture(), r, Manifest())) == 1, "size ignored"); });
Check("packing offset change", () => { var r = Fixture(); var member = O(r)["members"]![1]!; member["offsetBits"]!["value"] = 8; member["occupiedRanges"]!["value"] = new JsonArray(Range(8, 32)); Assert(Code(LayoutComparer.Compare(Fixture(), r, Manifest())) == 1, "offset ignored"); });
Check("unknown alignment is optional by default", () => { var r = Fixture(); O(r)["metrics"]!["alignmentBytes"] = Unknown(); Assert(Code(LayoutComparer.Compare(Fixture(), r, Manifest())) == 0, "optional alignment blocked"); });
Check("strict alignment requires evidence", () => { var r = Fixture(); O(r)["metrics"]!["alignmentBytes"] = Unknown(); Assert(Code(LayoutComparer.Compare(Fixture(), r, Manifest(policy: "value-alignment-v1"))) == 2, "unknown considered equal"); });
Check("two unknowns do not equal", () => { var l = Fixture(); O(l)["members"]![0]!["offsetBits"] = Unknown(); Assert(Verdict(LayoutComparer.Compare(l, Copy(l), Manifest())) == "incomplete", "unknowns equal"); });
Check("difference preserved alongside unknown", () => { var r = Fixture(); O(r)["metrics"]!["valueSizeBytes"]!["value"] = 16; O(r)["members"]![0]!["offsetBits"] = Unknown(); var result = LayoutComparer.Compare(Fixture(), r, Manifest()); Assert(Verdict(result) == "different" && Code(result) == 2, "difference or coverage lost"); });
Check("complete field removal is a difference", () => { var r = Fixture(); ((JsonArray)O(r)["members"]!).RemoveAt(1); Assert(Code(LayoutComparer.Compare(Fixture(), r, Manifest())) == 1, "removal ignored"); });
Check("partial missing field not a confirmed removal", () => { var r = Fixture(); ((JsonArray)O(r)["members"]!).RemoveAt(1); O(r)["coverage"]!["fieldEnumeration"] = "partial"; Assert(Verdict(LayoutComparer.Compare(Fixture(), r, Manifest())) == "incomplete", "partial read called removal"); });
Check("view mismatch is not-comparable", () => { var r = Fixture(); O(r)["view"] = "managed"; Assert(Verdict(LayoutComparer.Compare(Fixture(), r, Manifest())) == "not-comparable", "view ignored"); });
Check("origin mismatch is not-comparable", () => { var r = Fixture(); O(r)["origin"]!["kind"] = "instance-data"; Assert(Verdict(LayoutComparer.Compare(Fixture(), r, Manifest())) == "not-comparable", "origin ignored"); });
Check("opaque is never fully equal", () => { var l = Fixture(); ((JsonArray)l["typeDescriptors"]!).Add(new JsonObject { ["id"] = "opaque", ["kind"] = "opaque", ["displayName"] = "Secret", ["opaqueTag"] = "secret" }); O(l)["members"]![1]!["typeRef"] = "opaque"; Assert(Code(LayoutComparer.Compare(l, Copy(l), Manifest())) == 2, "opaque equal"); });
Check("signedness is representation", () => { var r = Fixture(); r["typeDescriptors"]![1]!["representation"]!["signedness"]!["value"] = "unsigned"; Assert(Code(LayoutComparer.Compare(Fixture(), r, Manifest())) == 1, "signedness ignored"); });
Check("unsupported case remains inconclusive", () => { var r = Fixture(); O(r)["status"] = "unsupported"; Assert(Code(LayoutComparer.Compare(Fixture(), r, Manifest())) == 2, "unsupported passed"); });
Check("strict JSON duplicate keys", () => Reject(() => JsonIO.Parse("{\"a\":1,\"a\":2}")));
Check("unknown schema version", () => { var s = Fixture(); s["schemaVersion"] = "future"; Reject(() => SnapshotValidator.Validate(s)); });
Check("duplicate observation IDs", () => { var s = Fixture(); ((JsonArray)s["observations"]!).Add(O(s).DeepClone()); Reject(() => SnapshotValidator.Validate(s)); });
Check("duplicate member IDs", () => { var s = Fixture(); O(s)["members"]![1]!["id"] = "tag"; Reject(() => SnapshotValidator.Validate(s)); });
Check("dangling type reference", () => { var s = Fixture(); O(s)["members"]![0]!["typeRef"] = "missing"; Reject(() => SnapshotValidator.Validate(s)); });
Check("unknown with invented zero", () => { var s = Fixture(); O(s)["metrics"]!["alignmentBytes"] = new JsonObject { ["state"] = "unknown", ["reason"] = "missing", ["value"] = 0 }; Reject(() => SnapshotValidator.Validate(s)); });
Check("range outside extent", () => { var s = Fixture(); O(s)["members"]![0]!["occupiedRanges"]!["value"] = new JsonArray(Range(64, 8)); Reject(() => SnapshotValidator.Validate(s)); });
Check("range arithmetic overflow", () => { var s = Fixture(); O(s)["members"]![0]!["occupiedRanges"]!["value"] = new JsonArray(Range(long.MaxValue, 8)); Reject(() => SnapshotValidator.Validate(s)); });
Check("unmapped cross-language fields", () => { var r = Fixture(); O(r)["view"] = "managed"; var m = Manifest("representation"); ((JsonArray)m["cases"]![0]!["fields"]!).RemoveAt(1); Reject(() => LayoutComparer.Compare(Fixture(), r, m)); });
Check("duplicate source mapping", () => { var m = Manifest("representation"); m["cases"]![0]!["fields"]![1]!["left"] = "tag"; Reject(() => LayoutComparer.ValidateManifest(m)); });
Check("empty case list cannot pass", () => { var m = Manifest(); m["cases"] = new JsonArray(); Reject(() => LayoutComparer.Compare(Fixture(), Fixture(), m)); });
Check("invalid inline relation", () => { var s = Fixture(); O(s)["members"]![0]!["childObservationId"] = "sample"; Reject(() => SnapshotValidator.Validate(s)); });
Check("strict reparse retains facts", () => { var s = JsonIO.Parse(Fixture().ToJsonString()); Assert(Code(LayoutComparer.Compare(s, s, Manifest())) == 0, "serialize drift"); });
Check("aggregate cannot bypass coverage with not-applicable", () => { var s = Fixture(); O(s)["members"] = new JsonArray(); O(s)["coverage"]!["fieldEnumeration"] = "not-applicable"; Reject(() => SnapshotValidator.Validate(s)); });
Check("enum self cycle is invalid", () => { var s = Fixture(); ((JsonArray)s["typeDescriptors"]!).Add(new JsonObject { ["id"] = "cycle", ["kind"] = "enum", ["displayName"] = "Cycle", ["enumUnderlyingTypeRef"] = "cycle" }); Reject(() => SnapshotValidator.Validate(s)); });
Check("array type cycle is invalid", () => { var s = Fixture(); ((JsonArray)s["typeDescriptors"]!).Add(new JsonObject { ["id"] = "cycle", ["kind"] = "array", ["displayName"] = "Cycle", ["elementTypeRef"] = "cycle", ["fixedCount"] = Known(JsonValue.Create(1)!) }); Reject(() => SnapshotValidator.Validate(s)); });
Check("array of unobserved records cannot compare same", () => { var s = Fixture(); ((JsonArray)s["typeDescriptors"]!).Add(new JsonObject { ["id"] = "records", ["kind"] = "array", ["displayName"] = "Records", ["elementTypeRef"] = "record", ["fixedCount"] = Known(JsonValue.Create(1)!) }); O(s)["members"]![1]!["typeRef"] = "records"; Assert(Code(LayoutComparer.Compare(s, Copy(s), Manifest())) == 2, "unobserved array element representation passed"); });
JsonObject NestedFixture()
{
    var s = Fixture(); var child = Copy(O(s)); child["id"] = "child";
    child["context"] = new JsonObject { ["kind"] = "embedded-value", ["hostObservationId"] = "sample", ["hostMemberId"] = "payload" };
    ((JsonArray)s["typeDescriptors"]!).Add(new JsonObject { ["id"] = "outer", ["kind"] = "record", ["displayName"] = "Outer" });
    O(s)["typeId"] = "outer"; var field = Member("payload", 0, 64, "record", 0); field["childObservationId"] = "child";
    O(s)["members"] = new JsonArray(field); ((JsonArray)s["observations"]!).Add(child); return s;
}
Check("valid nested placements", () => { var s = NestedFixture(); Assert(Code(LayoutComparer.Compare(s, Copy(s), Manifest())) == 0, "valid nested mismatch"); });
Check("nested origin participates in compatibility", () => { var l = NestedFixture(); var r = Copy(l); r["observations"]![1]!["origin"]!["kind"] = "instance-data"; Assert(Code(LayoutComparer.Compare(l, r, Manifest())) == 2, "nested origin bypass"); });
Check("nested view participates in compatibility", () => { var l = NestedFixture(); var r = Copy(l); r["observations"]![1]!["view"] = "managed"; Assert(Code(LayoutComparer.Compare(l, r, Manifest())) == 2, "nested view bypass"); });
Check("equivalent occupied range partitions", () => { var r = Fixture(); O(r)["members"]![1]!["occupiedRanges"]!["value"] = new JsonArray(Range(32, 16), Range(48, 16)); Assert(Code(LayoutComparer.Compare(Fixture(), r, Manifest())) == 0, "partition changed occupancy"); });
Check("native reserved region changes are compared", () =>
{
    var l = Fixture(); var r = Fixture();
    O(l)["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "abi-reserved", ["ranges"] = Known(new JsonArray(Range(8, 8))) });
    O(r)["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "abi-reserved", ["ranges"] = Known(new JsonArray(Range(16, 8))) });
    Assert(Code(LayoutComparer.Compare(l, r, Manifest())) == 1, "native hidden region ignored");
});
Check("unknown native reserved region remains incomplete", () =>
{
    var s = Fixture(); O(s)["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "abi-reserved", ["ranges"] = Unknown() });
    Assert(Code(LayoutComparer.Compare(s, Copy(s), Manifest())) == 2, "unknown hidden region equal");
});
Check("null origin calibration is invalid", () => { var s = Fixture(); O(s)["origin"]!["conversionEvidence"] = null; Reject(() => SnapshotValidator.Validate(s)); });
Check("incomplete origin calibration is invalid", () => { var s = Fixture(); O(s)["origin"]!["conversionEvidence"] = new JsonObject { ["kind"] = "runtime" }; Reject(() => SnapshotValidator.Validate(s)); });
Check("object extent requires calibration", () =>
{
    var s = Fixture(); O(s)["context"]!["kind"] = "heap-object"; O(s)["metrics"]!["runtimeReportedObjectBytes"] = Known(JsonValue.Create(8)!);
    var m = Manifest(); m["scope"] = "object";
    Assert(Verdict(LayoutComparer.Compare(s, Copy(s), m)) == "not-comparable", "uncalibrated object equal");
});
Check("unknown checkout state is explicit", () => { var s = Fixture(); s["build"]!["sourceDirty"] = null; SnapshotValidator.Validate(s); });
Check("missing checkout state is invalid", () => { var s = Fixture(); s["build"]!.AsObject().Remove("sourceDirty"); Reject(() => SnapshotValidator.Validate(s)); });
Check("array complete enumeration cannot hide every element", () =>
{
    var s = Fixture(); ((JsonArray)s["typeDescriptors"]!).Add(new JsonObject { ["id"] = "a", ["displayName"] = "int[2]", ["kind"] = "array", ["elementTypeRef"] = "i32", ["fixedCount"] = Known(JsonValue.Create(2)!) });
    O(s)["typeId"] = "a"; O(s)["members"] = new JsonArray(); Reject(() => SnapshotValidator.Validate(s));
});
Check("nested marshalling profile cannot bypass adapter", () =>
{
    var l = NestedFixture();
    foreach (var o in l["observations"]!.AsArray().OfType<JsonObject>())
    {
        o["view"] = "marshaled"; o["marshallingProfile"] = new JsonObject { ["id"] = "profile", ["mechanism"] = "runtime-marshalling", ["configuration"] = new JsonObject() };
    }
    var r = Copy(l); r["observations"]![1]!["marshallingProfile"]!["mechanism"] = "custom-marshalling";
    Assert(Code(LayoutComparer.Compare(l, r, Manifest())) == 2, "nested profile ignored");
});
Check("runtime role range partition is not representation", () =>
{
    var l = Fixture(); var r = Fixture();
    O(l)["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "reserved", ["ranges"] = Known(new JsonArray(Range(8, 16))) });
    O(r)["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "reserved", ["ranges"] = Known(new JsonArray(Range(8, 8))) }, new JsonObject { ["role"] = "reserved", ["ranges"] = Known(new JsonArray(Range(16, 8))) });
    Assert(Code(LayoutComparer.Compare(l, r, Manifest())) == 0, "region partition changed representation");
});
Check("float format cannot claim not-applicable", () =>
{
    var s = Fixture(); s["typeDescriptors"]![1]!["representation"]!["category"]!["value"] = "float";
    Reject(() => SnapshotValidator.Validate(s));
});
Check("unknown float format is incomplete", () =>
{
    var s = Fixture(); var repr = s["typeDescriptors"]![1]!["representation"]!;
    repr["category"]!["value"] = "float"; repr["signedness"]!["value"] = "not-applicable"; repr["encoding"]!["value"] = "ieee754"; repr["floatingFormat"] = Unknown();
    Assert(Code(LayoutComparer.Compare(s, Copy(s), Manifest())) == 2, "unknown floating format considered equal");
});
Check("unrecognized known scalar category is invalid", () => { var s = Fixture(); s["typeDescriptors"]![1]!["representation"]!["category"]!["value"] = "typo"; Reject(() => SnapshotValidator.Validate(s)); });
Check("malformed optional field map is not ignored", () => { var m = Manifest(); m["cases"]![0]!["fields"] = "bad-map"; Reject(() => LayoutComparer.ValidateManifest(m)); });
Check("malformed optional instance shape is invalid", () => { var s = Fixture(); O(s)["instanceShape"] = "bad-shape"; Reject(() => SnapshotValidator.Validate(s)); });
Check("C++ bool-based enum is an integral representation", () =>
{
    var s = Fixture(); s["typeDescriptors"]![0]!["representation"]!["category"]!["value"] = "boolean";
    ((JsonArray)s["typeDescriptors"]!).Add(new JsonObject { ["id"] = "bool-enum", ["displayName"] = "enum:bool", ["kind"] = "enum", ["enumUnderlyingTypeRef"] = "u8" });
    O(s)["members"]![0]!["typeRef"] = "bool-enum";
    Assert(Code(LayoutComparer.Compare(s, Copy(s), Manifest())) == 0, "valid C++ enum rejected");
});
Check("runtime regions stay inside the declared extent", () =>
{
    var s = Fixture(); O(s)["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "reserved", ["ranges"] = Known(new JsonArray(Range(64, 8))) });
    Reject(() => SnapshotValidator.Validate(s));
});
Console.WriteLine($"PASS: {count} core contract checks");
