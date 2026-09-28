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
JsonObject CompareWithSignatureParity(JsonObject left, JsonObject right, JsonObject manifest)
{
    // Preserve the original check's oracle and input-error behavior. Every successful comparison
    // must also be expressible through the public single-side signature protocol.
    var expected = LayoutComparer.Compare(left, right, manifest);
    var leftSignature = LayoutSignature.Generate(left, Project("left"));
    var rightSignature = LayoutSignature.Generate(right, Project("right"));
    LayoutSignature.Validate(leftSignature); LayoutSignature.Validate(rightSignature);
    var actual = LayoutSignature.Compare(leftSignature, rightSignature, manifest["mode"]!.GetValue<string>());
    Assert(Code(actual) == Code(expected), "Signature/direct comparison exit codes disagree.");
    var actualCases = actual["cases"]!.AsArray().OfType<JsonObject>().ToDictionary(c => c["id"]!.GetValue<string>(), StringComparer.Ordinal);
    Assert(actualCases.Count == expected["cases"]!.AsArray().Count, "Signature comparison lost a requested case.");
    foreach (var item in expected["cases"]!.AsArray().OfType<JsonObject>())
    {
        Assert(actualCases.TryGetValue(item["id"]!.GetValue<string>(), out var counterpart), "Signature comparison lost a logical case ID.");
        Assert(JsonNode.DeepEquals(item["verdict"], counterpart!["verdict"]) && JsonNode.DeepEquals(item["coverage"], counterpart["coverage"]),
            "Signature/direct case result mismatch: expected " + item.ToJsonString() + "; actual " + counterpart.ToJsonString());
    }
    return expected;

    JsonObject Project(string side)
    {
        var cases = new JsonArray();
        foreach (var item in manifest["cases"]!.AsArray().OfType<JsonObject>())
        {
            var selected = new JsonObject { ["id"] = item["id"]!.DeepClone(), ["observation"] = item[side]!.DeepClone() };
            if (item["fields"] is JsonArray fields) selected["fields"] = Fields(fields, side);
            cases.Add(selected);
        }
        return new JsonObject { ["schemaVersion"] = "0.1", ["scope"] = manifest["scope"]!.DeepClone(), ["policy"] = manifest["policy"]!.DeepClone(), ["cases"] = cases };
    }
    static JsonArray Fields(JsonArray fields, string side)
    {
        var result = new JsonArray();
        foreach (var field in fields.OfType<JsonObject>())
        {
            var selected = new JsonObject { ["id"] = field["id"]!.DeepClone(), ["member"] = field[side]!.DeepClone() };
            if (field["children"] is JsonArray children) selected["children"] = Fields(children, side);
            result.Add(selected);
        }
        return result;
    }
}
Check("identical independent snapshot", () => Assert(Code(CompareWithSignatureParity(Fixture(), Fixture(), Manifest())) == 0, "not same"));
Check("different compiler/build IDs are not layout differences", () => { var r = Fixture(); r["build"]!["runId"] = "another"; r["build"]!["compiler"]!["version"] = "2"; Assert(Code(CompareWithSignatureParity(Fixture(), r, Manifest())) == 0, "environment leaked into equality"); });
Check("cross-language explicit mapping", () => { var r = Fixture(); O(r)["view"] = "managed"; Assert(Code(CompareWithSignatureParity(Fixture(), r, Manifest("representation"))) == 0, "native/managed not same"); });
Check("known size change", () => { var r = Fixture(); O(r)["metrics"]!["valueSizeBytes"]!["value"] = 16; Assert(Code(CompareWithSignatureParity(Fixture(), r, Manifest())) == 1, "size ignored"); });
Check("packing offset change", () => { var r = Fixture(); var member = O(r)["members"]![1]!; member["offsetBits"]!["value"] = 8; member["occupiedRanges"]!["value"] = new JsonArray(Range(8, 32)); Assert(Code(CompareWithSignatureParity(Fixture(), r, Manifest())) == 1, "offset ignored"); });
Check("unknown alignment is optional by default", () => { var r = Fixture(); O(r)["metrics"]!["alignmentBytes"] = Unknown(); Assert(Code(CompareWithSignatureParity(Fixture(), r, Manifest())) == 0, "optional alignment blocked"); });
Check("strict alignment requires evidence", () => { var r = Fixture(); O(r)["metrics"]!["alignmentBytes"] = Unknown(); Assert(Code(CompareWithSignatureParity(Fixture(), r, Manifest(policy: "value-alignment-v1"))) == 2, "unknown considered equal"); });
Check("two unknowns do not equal", () => { var l = Fixture(); O(l)["members"]![0]!["offsetBits"] = Unknown(); Assert(Verdict(CompareWithSignatureParity(l, Copy(l), Manifest())) == "incomplete", "unknowns equal"); });
Check("difference preserved alongside unknown", () => { var r = Fixture(); O(r)["metrics"]!["valueSizeBytes"]!["value"] = 16; O(r)["members"]![0]!["offsetBits"] = Unknown(); var result = CompareWithSignatureParity(Fixture(), r, Manifest()); Assert(Verdict(result) == "different" && Code(result) == 2, "difference or coverage lost"); });
Check("complete field removal is a difference", () => { var r = Fixture(); ((JsonArray)O(r)["members"]!).RemoveAt(1); Assert(Code(CompareWithSignatureParity(Fixture(), r, Manifest())) == 1, "removal ignored"); });
Check("partial missing field not a confirmed removal", () => { var r = Fixture(); ((JsonArray)O(r)["members"]!).RemoveAt(1); O(r)["coverage"]!["fieldEnumeration"] = "partial"; Assert(Verdict(CompareWithSignatureParity(Fixture(), r, Manifest())) == "incomplete", "partial read called removal"); });
Check("view mismatch is not-comparable", () => { var r = Fixture(); O(r)["view"] = "managed"; Assert(Verdict(CompareWithSignatureParity(Fixture(), r, Manifest())) == "not-comparable", "view ignored"); });
Check("origin mismatch is not-comparable", () => { var r = Fixture(); O(r)["origin"]!["kind"] = "instance-data"; Assert(Verdict(CompareWithSignatureParity(Fixture(), r, Manifest())) == "not-comparable", "origin ignored"); });
Check("opaque is never fully equal", () => { var l = Fixture(); ((JsonArray)l["typeDescriptors"]!).Add(new JsonObject { ["id"] = "opaque", ["kind"] = "opaque", ["displayName"] = "Secret", ["opaqueTag"] = "secret" }); O(l)["members"]![1]!["typeRef"] = "opaque"; Assert(Code(CompareWithSignatureParity(l, Copy(l), Manifest())) == 2, "opaque equal"); });
Check("signedness is representation", () => { var r = Fixture(); r["typeDescriptors"]![1]!["representation"]!["signedness"]!["value"] = "unsigned"; Assert(Code(CompareWithSignatureParity(Fixture(), r, Manifest())) == 1, "signedness ignored"); });
Check("unsupported case remains inconclusive", () => { var r = Fixture(); O(r)["status"] = "unsupported"; Assert(Code(CompareWithSignatureParity(Fixture(), r, Manifest())) == 2, "unsupported passed"); });
Check("strict JSON duplicate keys", () => Reject(() => JsonIO.Parse("{\"a\":1,\"a\":2}")));
Check("unknown schema version", () => { var s = Fixture(); s["schemaVersion"] = "future"; Reject(() => SnapshotValidator.Validate(s)); });
Check("duplicate observation IDs", () => { var s = Fixture(); ((JsonArray)s["observations"]!).Add(O(s).DeepClone()); Reject(() => SnapshotValidator.Validate(s)); });
Check("duplicate member IDs", () => { var s = Fixture(); O(s)["members"]![1]!["id"] = "tag"; Reject(() => SnapshotValidator.Validate(s)); });
Check("dangling type reference", () => { var s = Fixture(); O(s)["members"]![0]!["typeRef"] = "missing"; Reject(() => SnapshotValidator.Validate(s)); });
Check("unknown with invented zero", () => { var s = Fixture(); O(s)["metrics"]!["alignmentBytes"] = new JsonObject { ["state"] = "unknown", ["reason"] = "missing", ["value"] = 0 }; Reject(() => SnapshotValidator.Validate(s)); });
Check("range outside extent", () => { var s = Fixture(); O(s)["members"]![0]!["occupiedRanges"]!["value"] = new JsonArray(Range(64, 8)); Reject(() => SnapshotValidator.Validate(s)); });
Check("range arithmetic overflow", () => { var s = Fixture(); O(s)["members"]![0]!["occupiedRanges"]!["value"] = new JsonArray(Range(long.MaxValue, 8)); Reject(() => SnapshotValidator.Validate(s)); });
Check("unmapped cross-language fields", () => { var r = Fixture(); O(r)["view"] = "managed"; var m = Manifest("representation"); ((JsonArray)m["cases"]![0]!["fields"]!).RemoveAt(1); Reject(() => CompareWithSignatureParity(Fixture(), r, m)); });
Check("duplicate source mapping", () => { var m = Manifest("representation"); m["cases"]![0]!["fields"]![1]!["left"] = "tag"; Reject(() => LayoutComparer.ValidateManifest(m)); });
Check("empty case list cannot pass", () => { var m = Manifest(); m["cases"] = new JsonArray(); Reject(() => CompareWithSignatureParity(Fixture(), Fixture(), m)); });
Check("invalid inline relation", () => { var s = Fixture(); O(s)["members"]![0]!["childObservationId"] = "sample"; Reject(() => SnapshotValidator.Validate(s)); });
Check("strict reparse retains facts", () => { var s = JsonIO.Parse(Fixture().ToJsonString()); Assert(Code(CompareWithSignatureParity(s, s, Manifest())) == 0, "serialize drift"); });
Check("aggregate cannot bypass coverage with not-applicable", () => { var s = Fixture(); O(s)["members"] = new JsonArray(); O(s)["coverage"]!["fieldEnumeration"] = "not-applicable"; Reject(() => SnapshotValidator.Validate(s)); });
Check("enum self cycle is invalid", () => { var s = Fixture(); ((JsonArray)s["typeDescriptors"]!).Add(new JsonObject { ["id"] = "cycle", ["kind"] = "enum", ["displayName"] = "Cycle", ["enumUnderlyingTypeRef"] = "cycle" }); Reject(() => SnapshotValidator.Validate(s)); });
Check("array type cycle is invalid", () => { var s = Fixture(); ((JsonArray)s["typeDescriptors"]!).Add(new JsonObject { ["id"] = "cycle", ["kind"] = "array", ["displayName"] = "Cycle", ["elementTypeRef"] = "cycle", ["fixedCount"] = Known(JsonValue.Create(1)!) }); Reject(() => SnapshotValidator.Validate(s)); });
Check("array of unobserved records cannot compare same", () => { var s = Fixture(); ((JsonArray)s["typeDescriptors"]!).Add(new JsonObject { ["id"] = "records", ["kind"] = "array", ["displayName"] = "Records", ["elementTypeRef"] = "record", ["fixedCount"] = Known(JsonValue.Create(1)!) }); O(s)["members"]![1]!["typeRef"] = "records"; Assert(Code(CompareWithSignatureParity(s, Copy(s), Manifest())) == 2, "unobserved array element representation passed"); });
JsonObject NestedFixture()
{
    var s = Fixture(); var child = Copy(O(s)); child["id"] = "child";
    child["context"] = new JsonObject { ["kind"] = "embedded-value", ["hostObservationId"] = "sample", ["hostMemberId"] = "payload" };
    ((JsonArray)s["typeDescriptors"]!).Add(new JsonObject { ["id"] = "outer", ["kind"] = "record", ["displayName"] = "Outer" });
    O(s)["typeId"] = "outer"; var field = Member("payload", 0, 64, "record", 0); field["childObservationId"] = "child";
    O(s)["members"] = new JsonArray(field); ((JsonArray)s["observations"]!).Add(child); return s;
}
Check("valid nested placements", () => { var s = NestedFixture(); Assert(Code(CompareWithSignatureParity(s, Copy(s), Manifest())) == 0, "valid nested mismatch"); });
Check("nested origin participates in compatibility", () => { var l = NestedFixture(); var r = Copy(l); r["observations"]![1]!["origin"]!["kind"] = "instance-data"; Assert(Code(CompareWithSignatureParity(l, r, Manifest())) == 2, "nested origin bypass"); });
Check("nested view participates in compatibility", () => { var l = NestedFixture(); var r = Copy(l); r["observations"]![1]!["view"] = "managed"; Assert(Code(CompareWithSignatureParity(l, r, Manifest())) == 2, "nested view bypass"); });
Check("equivalent occupied range partitions", () => { var r = Fixture(); O(r)["members"]![1]!["occupiedRanges"]!["value"] = new JsonArray(Range(32, 16), Range(48, 16)); Assert(Code(CompareWithSignatureParity(Fixture(), r, Manifest())) == 0, "partition changed occupancy"); });
Check("native reserved region changes are compared", () =>
{
    var l = Fixture(); var r = Fixture();
    O(l)["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "abi-reserved", ["ranges"] = Known(new JsonArray(Range(8, 8))) });
    O(r)["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "abi-reserved", ["ranges"] = Known(new JsonArray(Range(16, 8))) });
    Assert(Code(CompareWithSignatureParity(l, r, Manifest())) == 1, "native hidden region ignored");
});
Check("unknown native reserved region remains incomplete", () =>
{
    var s = Fixture(); O(s)["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "abi-reserved", ["ranges"] = Unknown() });
    Assert(Code(CompareWithSignatureParity(s, Copy(s), Manifest())) == 2, "unknown hidden region equal");
});
Check("null origin calibration is invalid", () => { var s = Fixture(); O(s)["origin"]!["conversionEvidence"] = null; Reject(() => SnapshotValidator.Validate(s)); });
Check("incomplete origin calibration is invalid", () => { var s = Fixture(); O(s)["origin"]!["conversionEvidence"] = new JsonObject { ["kind"] = "runtime" }; Reject(() => SnapshotValidator.Validate(s)); });
Check("object extent requires calibration", () =>
{
    var s = Fixture(); O(s)["context"]!["kind"] = "heap-object"; O(s)["metrics"]!["runtimeReportedObjectBytes"] = Known(JsonValue.Create(8)!);
    var m = Manifest(); m["scope"] = "object";
    Assert(Verdict(CompareWithSignatureParity(s, Copy(s), m)) == "not-comparable", "uncalibrated object equal");
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
    Assert(Code(CompareWithSignatureParity(l, r, Manifest())) == 2, "nested profile ignored");
});
Check("runtime role range partition is not representation", () =>
{
    var l = Fixture(); var r = Fixture();
    O(l)["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "reserved", ["ranges"] = Known(new JsonArray(Range(8, 16))) });
    O(r)["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "reserved", ["ranges"] = Known(new JsonArray(Range(8, 8))) }, new JsonObject { ["role"] = "reserved", ["ranges"] = Known(new JsonArray(Range(16, 8))) });
    Assert(Code(CompareWithSignatureParity(l, r, Manifest())) == 0, "region partition changed representation");
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
    Assert(Code(CompareWithSignatureParity(s, Copy(s), Manifest())) == 2, "unknown floating format considered equal");
});
Check("unrecognized known scalar category is invalid", () => { var s = Fixture(); s["typeDescriptors"]![1]!["representation"]!["category"]!["value"] = "typo"; Reject(() => SnapshotValidator.Validate(s)); });
Check("malformed optional field map is not ignored", () => { var m = Manifest(); m["cases"]![0]!["fields"] = "bad-map"; Reject(() => LayoutComparer.ValidateManifest(m)); });
Check("malformed optional instance shape is invalid", () => { var s = Fixture(); O(s)["instanceShape"] = "bad-shape"; Reject(() => SnapshotValidator.Validate(s)); });
Check("C++ bool-based enum is an integral representation", () =>
{
    var s = Fixture(); s["typeDescriptors"]![0]!["representation"]!["category"]!["value"] = "boolean";
    ((JsonArray)s["typeDescriptors"]!).Add(new JsonObject { ["id"] = "bool-enum", ["displayName"] = "enum:bool", ["kind"] = "enum", ["enumUnderlyingTypeRef"] = "u8" });
    O(s)["members"]![0]!["typeRef"] = "bool-enum";
    Assert(Code(CompareWithSignatureParity(s, Copy(s), Manifest())) == 0, "valid C++ enum rejected");
});
Check("runtime regions stay inside the declared extent", () =>
{
    var s = Fixture(); O(s)["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "reserved", ["ranges"] = Known(new JsonArray(Range(64, 8))) });
    Reject(() => SnapshotValidator.Validate(s));
});

JsonObject ArrayFixture(bool runtimeLength = true, bool heapObject = false)
{
    var s = Fixture();
    ((JsonArray)s["typeDescriptors"]!).Add(new JsonObject { ["id"] = "array", ["kind"] = "array", ["displayName"] = "IntArray", ["elementTypeRef"] = "i32", ["fixedCount"] = runtimeLength ? Unknown() : Known(JsonValue.Create(2)!) });
    var o = O(s); o["typeId"] = "array";
    o["members"] = new JsonArray(Member("0", 0, 32, "i32", 0), Member("1", 32, 32, "i32", 1));
    o["instanceShape"] = new JsonObject { ["length"] = 2, ["dimensions"] = new JsonArray(2) };
    o["metrics"]!["arrayStrideBytes"] = Known(JsonValue.Create(4)!);
    if (heapObject)
    {
        o["view"] = "managed"; o["context"]!["kind"] = "heap-object";
        o["metrics"]!["runtimeReportedObjectBytes"] = Known(JsonValue.Create(8)!);
        o["origin"]!["conversionEvidence"] = Known(JsonValue.Create(0)!)["evidence"]!.DeepClone();
    }
    return s;
}
JsonObject ArrayElementFixture()
{
    var s = ArrayFixture(false);
    for (var i = 0; i < 2; i++)
    {
        var id = "element-" + i; O(s)["members"]![i]!["childObservationId"] = id;
        var child = Copy(O(s)); child["id"] = id; child["typeId"] = "i32";
        child["context"] = new JsonObject { ["kind"] = "array-element", ["hostObservationId"] = "sample", ["hostMemberId"] = i.ToString(), ["elementIndex"] = i };
        child.Remove("instanceShape"); child["members"] = new JsonArray(); child["metrics"]!["valueSizeBytes"] = Known(JsonValue.Create(4)!);
        s["observations"]!.AsArray().Add(child);
    }
    return s;
}
Check("mapped native representations support different language frontends", () =>
{
    var l = Fixture(); var r = Fixture(); l["build"]!["languages"] = new JsonArray("c++"); r["build"]!["languages"] = new JsonArray("rust");
    Assert(Code(CompareWithSignatureParity(l, r, Manifest("representation"))) == 0, "native/native mapping rejected");
});
Check("mapped managed representations share the same comparison path", () =>
{
    var l = Fixture(); O(l)["view"] = "managed";
    Assert(Code(CompareWithSignatureParity(l, Copy(l), Manifest("representation"))) == 0, "managed/managed mapping rejected");
});
Check("representation mode does not bypass marshalling profiles", () =>
{
    var r = Fixture(); O(r)["view"] = "marshaled"; O(r)["marshallingProfile"] = new JsonObject { ["id"] = "profile", ["mechanism"] = "runtime-marshalling", ["configuration"] = new JsonObject() };
    Assert(Verdict(CompareWithSignatureParity(Fixture(), r, Manifest("representation"))) == "not-comparable", "marshaled view bypassed dedicated adapter");
});
Check("fully observed runtime arrays are comparable instances", () =>
{
    var s = ArrayFixture(heapObject: true); var m = Manifest(); m["scope"] = "object";
    Assert(Code(CompareWithSignatureParity(s, Copy(s), m)) == 0, "runtime type count obscured observed instance count");
});
Check("array declaration cardinality is not substituted for instance evidence", () =>
{
    var l = ArrayFixture(false); O(l).Remove("instanceShape"); var r = ArrayFixture();
    var result = CompareWithSignatureParity(l, r, Manifest());
    Assert(Code(result) == 0 && result["cases"]![0]!["diagnostics"]!.AsArray().Count > 0, "instance/declaration comparison lost its scope distinction");
});
Check("runtime array length changes are observed", () =>
{
    var l = ArrayFixture(); var r = Copy(l); O(r)["instanceShape"] = new JsonObject { ["length"] = 1, ["dimensions"] = new JsonArray(1) }; O(r)["members"]!.AsArray().RemoveAt(1);
    Assert(Code(CompareWithSignatureParity(l, r, Manifest())) == 1, "runtime instance count change ignored");
});
Check("array unknown length without instance evidence remains incomplete", () =>
{
    var s = ArrayFixture(); O(s).Remove("instanceShape"); O(s)["coverage"]!["fieldEnumeration"] = "partial";
    Assert(Code(CompareWithSignatureParity(s, Copy(s), Manifest())) == 2, "unobserved cardinality passed");
});
Check("array placement scope requires the root stride", () =>
{
    var l = Fixture(); var r = Copy(l); O(r)["metrics"]!["arrayStrideBytes"] = Known(JsonValue.Create(16)!); var m = Manifest(); m["scope"] = "array";
    Assert(Code(CompareWithSignatureParity(l, r, m)) == 1, "array placement scope ignored root stride");
});
Check("array rank dimensions participate outside object scope", () =>
{
    var l = ArrayFixture(); var r = Copy(l); O(l)["instanceShape"]!["dimensions"] = new JsonArray(1, 2); O(r)["instanceShape"]!["dimensions"] = new JsonArray(2, 1);
    Assert(Code(CompareWithSignatureParity(l, r, Manifest())) == 1, "array dimensions ignored");
});
Check("fixed array count must agree with instance count", () => { var s = ArrayFixture(false); O(s)["instanceShape"] = new JsonObject { ["length"] = 3, ["dimensions"] = new JsonArray(3) }; Reject(() => SnapshotValidator.Validate(s)); });
Check("instance length must match dimensions product", () => { var s = ArrayFixture(); O(s)["instanceShape"]!["dimensions"] = new JsonArray(1, 3); Reject(() => SnapshotValidator.Validate(s)); });
Check("instance dimensions product cannot overflow", () => { var s = ArrayFixture(); O(s)["instanceShape"]!["dimensions"] = new JsonArray(long.MaxValue, 2L); Reject(() => SnapshotValidator.Validate(s)); });
Check("plain value cannot claim unused instance shape", () => { var s = Fixture(); O(s)["instanceShape"] = new JsonObject { ["length"] = 2, ["dimensions"] = new JsonArray(2) }; Reject(() => SnapshotValidator.Validate(s)); });
Check("non-inline child object is rejected", () =>
{
    var s = NestedFixture(); s["observations"]![1]!["context"]!["kind"] = "heap-object"; s["observations"]![1]!["metrics"]!["runtimeReportedObjectBytes"] = Known(JsonValue.Create(8)!);
    Reject(() => SnapshotValidator.Validate(s));
});
Check("inline observation requires host relationship", () => { var s = Fixture(); O(s)["context"]!["kind"] = "embedded-value"; Reject(() => SnapshotValidator.Validate(s)); });
Check("valid array element placements pass", () => SnapshotValidator.Validate(ArrayElementFixture()));
Check("array element indices cannot repeat", () => { var s = ArrayElementFixture(); s["observations"]![2]!["context"]!["elementIndex"] = 0; Reject(() => SnapshotValidator.Validate(s)); });
Check("array element index is bounded by observed count", () => { var s = ArrayElementFixture(); s["observations"]![2]!["context"]!["elementIndex"] = 2; Reject(() => SnapshotValidator.Validate(s)); });
Check("known scalar occupancy must agree with its offset and width", () =>
{
    var s = Fixture(); O(s)["members"]![0]!["occupiedRanges"] = Known(new JsonArray(Range(32, 8))); Reject(() => SnapshotValidator.Validate(s));
});
Check("known scalar occupancy cannot omit interior bits", () =>
{
    var s = Fixture(); O(s)["members"]![1]!["occupiedRanges"] = Known(new JsonArray(Range(32, 8), Range(48, 16))); Reject(() => SnapshotValidator.Validate(s));
});
Check("calibrated object extent start is a difference within a shared origin", () =>
{
    var l = ArrayFixture(heapObject: true); var r = Copy(l);
    foreach (var o in new[] { O(l), O(r) }) { o["origin"]!["kind"] = "object-reference"; o["metrics"]!["runtimeReportedObjectBytes"] = Known(JsonValue.Create(16)!); }
    O(l)["origin"]!["extentStartBit"] = -32; O(r)["origin"]!["extentStartBit"] = -64;
    var m = Manifest(); m["scope"] = "object";
    Assert(Code(CompareWithSignatureParity(l, r, m)) == 1, "extent origin coordinate confused with region start");
});
Check("language identities cannot silently repeat", () => { var s = Fixture(); s["build"]!["languages"] = new JsonArray("c++", "c++"); Reject(() => SnapshotValidator.Validate(s)); });
Check("provenance requires all observation claims", () => { var s = Fixture(); s["build"]!["captureProvenance"] = new JsonObject { ["sourceBinding"] = "profile" }; Reject(() => SnapshotValidator.Validate(s)); });


Check("nested selectors on leaf fields cannot be silently ignored", () =>
{
    foreach (var kind in new[] { "scalar", "enum", "reference" })
    {
        var s = Fixture(); var type = s["typeDescriptors"]![0]!.AsObject();
        if (kind == "enum") { type.Clear(); type["id"] = "u8"; type["displayName"] = "Enum"; type["kind"] = "enum"; type["enumUnderlyingTypeRef"] = "i32"; }
        if (kind == "reference") { type.Clear(); type["id"] = "u8"; type["displayName"] = "Reference"; type["kind"] = "reference"; type["referenceKind"] = "native-pointer"; type["representation"] = new JsonObject { ["widthBits"] = Known(JsonValue.Create(8)!) }; }
        var m = Manifest("representation"); m["cases"]![0]!["fields"]![0]!["children"] = new JsonArray(new JsonObject { ["id"] = "required", ["left"] = "missing", ["right"] = "missing" });
        Reject(() => CompareWithSignatureParity(s, Copy(s), m));
    }
});
Check("unobserved record mapping remains incomplete with an explicit diagnostic", () =>
{
    var s = NestedFixture(); O(s)["members"]![0]!.AsObject().Remove("childObservationId"); s["observations"]!.AsArray().RemoveAt(1);
    var m = Manifest("representation"); m["cases"]![0]!["fields"] = new JsonArray(new JsonObject { ["id"] = "payload", ["left"] = "payload", ["right"] = "payload", ["children"] = new JsonArray(new JsonObject { ["id"] = "tag", ["left"] = "tag", ["right"] = "tag" }) });
    var result = CompareWithSignatureParity(s, Copy(s), m);
    Assert(Code(result) == 2 && result["cases"]![0]!["diagnostics"]!.AsArray().Any(d => d!.GetValue<string>().Contains("mapping was not evaluated")), "missing record observations hid an unevaluated selector");
});
Check("selector absent on both complete sides is a configuration error", () =>
{
    var m = Manifest("representation"); m["cases"]![0]!["fields"]!.AsArray().Add(new JsonObject { ["id"] = "absent", ["left"] = "absent", ["right"] = "absent" });
    Reject(() => CompareWithSignatureParity(Fixture(), Fixture(), m));
});
Check("unresolved selector does not invent a change from incomplete enumeration", () =>
{
    var l = Fixture(); var r = Fixture(); O(r)["coverage"]!["fieldEnumeration"] = "partial";
    var m = Manifest("representation"); m["cases"]![0]!["fields"]!.AsArray().Add(new JsonObject { ["id"] = "absent", ["left"] = "absent", ["right"] = "absent" });
    var result = CompareWithSignatureParity(l, r, m);
    Assert(Verdict(result) == "incomplete" && result["cases"]![0]!["differences"]!.AsArray().Count == 0, "no observed field was misreported as an addition/removal");
});

Console.WriteLine($"PASS: {count} core contract checks");
