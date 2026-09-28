using System.Net;
using System.Text.Json.Nodes;
using LayoutObserver.Report;

var checks = new (string Name, Action Run)[]
{
    ("case verdict, scope and evidence are visible", Summary),
    ("untrusted names cannot create script or markup", Escaping),
    ("complete ranges produce only proved padding", Padding),
    ("hidden regions prevent padding inference", HiddenRegion),
    ("unknown occupied range stays unknown", UnknownRange),
    ("overlapping ranges are unioned", Overlap),
    ("known range overlap does not require a group tag", DerivedOverlap),
    ("bit-field ranges retain their bit units", BitFields),
    ("known empty occupied ranges are not unknown", EmptyRange),
    ("scalar and enum observations are data, not padding", ScalarObservation),
    ("opaque observations do not become padding", OpaqueObservation),
    ("unexpanded array observations do not become padding", UnexpandedArray),
    ("embedded observations remain expandable", Nested),
    ("inline cycles produce a bounded diagnostic", Cycle),
    ("calibrated negative object origin is preserved", ObjectOrigin),
    ("uncalibrated object size is not an extent", UncalibratedObject),
    ("64-bit positions are not rounded", LargeOffset),
    ("overflowing extent stays unknown", Overflow),
    ("rendering does not mutate snapshots", Immutability)
};
var failures = 0;
foreach (var check in checks)
{
    try { check.Run(); Console.WriteLine("PASS " + check.Name); }
    catch (Exception error) { failures++; Console.Error.WriteLine("FAIL " + check.Name + ": " + error.Message); }
}
if (args.Length == 2 && args[0] == "--write-preview")
{
    var left = Snapshot("native-debug");
    var right = Snapshot("managed-release");
    right["observations"]![0]!["metrics"]!["alignmentBytes"] = Unknown("managed-alignment-not-reported");
    right["build"]!["configuration"] = "Release";
    right["build"]!["runtime"] = new JsonObject { ["name"] = "CoreCLR", ["version"] = "fixture" };
    right["observations"]![0]!["view"] = "managed";
    File.WriteAllText(args[1], HtmlReport.Render(left, right, Comparison()), System.Text.Encoding.UTF8);
    Console.WriteLine("Preview: " + Path.GetFullPath(args[1]));
}
return failures == 0 ? 0 : 1;

static void Summary()
{
    var comparison = Comparison();
    comparison["cases"]![0]!["verdict"] = "different";
    comparison["cases"]![0]!["coverage"] = "partial";
    var html = Render(Snapshot(), comparison);
    Contains(html, "class=\"verdict different\">different");
    Contains(html, "value-fields-v1");
    Contains(html, "<td>partial</td>");
    Contains(html, "fixture-scalar-layout");
    Contains(html, WebUtility.HtmlEncode("unknown · managed-alignment-not-reported"));
    Contains(html, "左侧构建");
    Contains(html, "右侧构建");
}

static void Escaping()
{
    const string payload = "</script><img src=x onerror=alert(1)>\" onclick=\"alert(2)";
    var snapshot = Snapshot(payload);
    var observation = First(snapshot);
    observation["displayName"] = payload;
    observation["members"]![0]!["displayName"] = payload;
    observation["members"]![0]!["overlapGroup"] = payload;
    snapshot["build"]!["compiler"]!["name"] = payload;
    var comparison = Comparison();
    comparison["cases"]![0]!["id"] = payload;
    var html = Render(snapshot, comparison);
    Equal(Count(html, "<script>"), 1, "Exactly one static script");
    Equal(Count(html, "</script>"), 1, "Exactly one static script close");
    Absent(html, "<img");
    Absent(html, "\" onclick=\"");
    Contains(html, WebUtility.HtmlEncode(payload));
    Absent(html, "<script src=");
    Absent(html, "<link ");
    Absent(html, "innerHTML");
    Contains(html, "Content-Security-Policy");
    Contains(html, "default-src 'none'");
    Contains(html, "script-src 'sha256-");
    Absent(html, "script-src 'unsafe-inline'");
}

static void Padding()
{
    var html = Render(Snapshot());
    Equal(Count(html, "class=\"region-row padding\""), 4, "Two padding gaps per side");
    Contains(html, "[1 B, 4 B)");
    Contains(html, "[10 B, 12 B)");
}

static void HiddenRegion()
{
    var snapshot = Snapshot();
    First(snapshot)["coverage"]!["hiddenRegions"] = "unknown";
    var html = Render(snapshot);
    Absent(html, "class=\"region-row padding\"");
    Contains(html, "class=\"region-row unknown\"");
    Contains(html, "未分类区域");
}

static void UnknownRange()
{
    var snapshot = Snapshot();
    First(snapshot)["members"]![1]!["occupiedRanges"] = Unknown("collector-cannot-prove-occupied-ranges");
    var html = Render(snapshot);
    Absent(html, "class=\"region-row padding\"");
    Contains(html, "位置 / 范围未知");
    Contains(html, "collector-cannot-prove-occupied-ranges");
}

static void Overlap()
{
    var snapshot = Snapshot();
    var observation = First(snapshot);
    observation["metrics"]!["valueSizeBytes"] = Known(8);
    observation["members"] = new JsonArray(Member("wide", 0, 64, "overlay"), Member("narrow", 0, 32, "overlay"));
    var html = Render(snapshot);
    Absent(html, "class=\"region-row padding\"");
    Contains(html, "value=\"group:overlay\"");
    Contains(html, "data-overlap=\"overlay\"");
    Contains(html, "width:100%");
    Contains(html, "width:50%");
}

static void Nested()
{
    var snapshot = Snapshot();
    var parent = First(snapshot);
    var child = (JsonObject)parent.DeepClone();
    child["id"] = "sample.payload";
    child["displayName"] = "嵌入 Sample";
    child["context"] = new JsonObject { ["kind"] = "embedded-value", ["hostObservationId"] = "sample", ["hostMemberId"] = "payload" };
    parent["metrics"]!["valueSizeBytes"] = Known(16);
    var payload = Member("payload", 0, 96);
    payload["childObservationId"] = "sample.payload";
    payload["typeRef"] = "sample-type";
    parent["members"] = new JsonArray(payload, Member("extra", 96, 32));
    snapshot["observations"]!.AsArray().Add(child);
    var html = Render(snapshot);
    Contains(html, "<details class=\"nested\"><summary>内嵌布局：payload</summary>");
    Contains(html, "嵌入 Sample");
    Contains(html, "details.member-details, details.nested");
}

static void DerivedOverlap()
{
    var snapshot = Snapshot();
    var observation = First(snapshot);
    observation["metrics"]!["valueSizeBytes"] = Known(8);
    observation["members"] = new JsonArray(Member("wide", 0, 64), Member("narrow", 0, 32), Member("empty", 16, 0));
    var html = Render(snapshot);
    Contains(html, "value=\"group:range-overlap:sample:0\"");
    Contains(html, "data-search=\"empty i32 i32 field\" data-overlap=\"\"");
    Absent(html, "class=\"region-row padding\"");
}

static void BitFields()
{
    var snapshot = Snapshot();
    var observation = First(snapshot);
    observation["metrics"]!["valueSizeBytes"] = Known(1);
    observation["members"] = new JsonArray(Member("low", 0, 3), Member("high", 3, 5));
    var html = Render(snapshot);
    Contains(html, "[0 B, 3 bit)");
    Contains(html, "[3 bit, 1 B)");
    Absent(html, "class=\"region-row padding\"");
    Absent(html, "value=\"group:range-overlap:");
}

static void EmptyRange()
{
    var snapshot = Snapshot();
    First(snapshot)["members"]![0]!["occupiedRanges"] = Known(new JsonArray());
    var html = Render(snapshot);
    Contains(html, "<span class=\"empty-position\">已知空范围</span>");
    Contains(html, "<span class=\"region-range\">∅</span>");
}

static void ScalarObservation()
{
    foreach (var kind in new[] { "scalar", "enum" })
    {
        var snapshot = Snapshot();
        var observation = First(snapshot);
        observation["typeId"] = kind == "scalar" ? "i32" : "test-enum";
        observation["members"] = new JsonArray();
        observation["metrics"]!["valueSizeBytes"] = Known(4);
        if (kind == "enum") snapshot["typeDescriptors"]!.AsArray().Add(new JsonObject
        {
            ["id"] = "test-enum", ["displayName"] = "TestEnum", ["kind"] = "enum", ["enumUnderlyingTypeRef"] = "i32"
        });
        var html = Render(snapshot);
        Contains(html, "值表示");
        Contains(html, "[0 B, 4 B)");
        Absent(html, "class=\"region-row padding\"");
    }
}

static void OpaqueObservation()
{
    var snapshot = Snapshot();
    First(snapshot)["members"] = new JsonArray();
    snapshot["typeDescriptors"]![0]!["kind"] = "opaque";
    snapshot["typeDescriptors"]![0]!["opaqueTag"] = "opaque-sample";
    var html = Render(snapshot);
    Absent(html, "class=\"region-row padding\"");
    Contains(html, "未分类区域");
}

static void UnexpandedArray()
{
    var snapshot = Snapshot();
    First(snapshot)["members"] = new JsonArray();
    First(snapshot)["typeId"] = "i32-array";
    snapshot["typeDescriptors"]!.AsArray().Add(new JsonObject
    {
        ["id"] = "i32-array", ["displayName"] = "int[3]", ["kind"] = "array",
        ["elementTypeRef"] = "i32", ["fixedCount"] = Known(3)
    });
    // Even a caller claiming complete coverage cannot make an unexpanded array's bytes into padding.
    var html = Render(snapshot);
    Absent(html, "class=\"region-row padding\"");
    Contains(html, "未分类区域");
    Contains(html, "[0 B, 12 B)");
}

static void Cycle()
{
    var snapshot = Snapshot();
    First(snapshot)["members"]![0]!["childObservationId"] = "sample";
    var html = Render(snapshot);
    Contains(html, "嵌套关系存在环");
    if (html.Length > 100_000) throw new Exception("Cycle rendering was not bounded");
}

static void ObjectOrigin()
{
    var snapshot = ObjectSnapshot(calibrated: true);
    var html = Render(snapshot);
    Contains(html, "-8 B → 16 B");
    Contains(html, "[12 B, 16 B)");
    Contains(html, "class=\"region-row runtime\"");
    Contains(html, "class=\"region-row padding\"");
}

static void UncalibratedObject()
{
    var html = Render(ObjectSnapshot(calibrated: false));
    Absent(html, "class=\"region-row padding\"");
    Contains(html, "extent 未知");
}

static void LargeOffset()
{
    const long offset = 9_007_199_254_740_993;
    var snapshot = Snapshot();
    var observation = First(snapshot);
    observation["metrics"]!["valueSizeBytes"] = Unknown("fixture-has-no-extent");
    observation["members"] = new JsonArray(Member("large-offset", offset, 1));
    var html = Render(snapshot);
    Contains(html, "9007199254740993 bit");
    Contains(html, "9007199254740994 bit");
}

static void Overflow()
{
    var snapshot = Snapshot();
    First(snapshot)["metrics"]!["valueSizeBytes"] = Known(long.MaxValue);
    First(snapshot)["metrics"]!["standaloneSizeBytes"] = Unknown("no-fallback");
    var html = Render(snapshot);
    Contains(html, "extent 未知");
    Absent(html, "class=\"region-row padding\"");
}

static void Immutability()
{
    var snapshot = Snapshot();
    var comparison = Comparison();
    var originalSnapshot = snapshot.ToJsonString();
    var originalComparison = comparison.ToJsonString();
    _ = Render(snapshot, comparison);
    if (snapshot.ToJsonString() != originalSnapshot || comparison.ToJsonString() != originalComparison)
        throw new Exception("Renderer mutated its input JSON");
}

static JsonObject ObjectSnapshot(bool calibrated)
{
    var snapshot = Snapshot();
    var observation = First(snapshot);
    observation["view"] = "managed";
    observation["context"] = new JsonObject { ["kind"] = "heap-object" };
    observation["origin"] = new JsonObject { ["kind"] = "object-reference", ["extentStartBit"] = -64 };
    if (calibrated) observation["origin"]!["conversionEvidence"] = Evidence();
    observation["metrics"] = new JsonObject { ["runtimeReportedObjectBytes"] = Known(24) };
    observation["members"] = new JsonArray(Member("count", 64, 32));
    observation["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "runtime-header", ["ranges"] = Ranges(-64, 128) });
    observation["coverage"]!["hiddenRegions"] = "complete";
    return snapshot;
}

static JsonObject Snapshot(string id = "fixture") => new()
{
    ["schemaVersion"] = "0.1", ["snapshotId"] = id,
    ["producer"] = new JsonObject { ["id"] = "report-fixture", ["version"] = "0.1", ["capabilities"] = new JsonArray("known-value-fields") },
    ["build"] = new JsonObject
    {
        ["buildId"] = "fixture-build", ["runId"] = "fixture-run", ["configuration"] = "Debug",
        ["sourceRevision"] = "fixture", ["sourceDirty"] = false, ["sourceDigest"] = "fixture", ["artifactDigest"] = "fixture",
        ["compiler"] = new JsonObject { ["name"] = "fixture-compiler", ["version"] = "fixture" },
        ["runtime"] = new JsonObject { ["name"] = "none", ["version"] = "none" },
        ["target"] = new JsonObject { ["os"] = "fixture-os", ["architecture"] = "x64", ["abi"] = "fixture-abi", ["pointerBits"] = 64, ["bitsPerByte"] = 8, ["endian"] = "little" },
        ["flags"] = new JsonArray(), ["dependencies"] = new JsonObject()
    },
    ["typeDescriptors"] = new JsonArray(
        new JsonObject { ["id"] = "sample-type", ["kind"] = "record", ["displayName"] = "Sample" },
        Scalar("u8", 8, "unsigned"), Scalar("i32", 32, "signed"), Scalar("i16", 16, "signed")),
    ["observations"] = new JsonArray(new JsonObject
    {
        ["id"] = "sample", ["typeId"] = "sample-type", ["displayName"] = "Sample", ["status"] = "ok", ["view"] = "native",
        ["context"] = new JsonObject { ["kind"] = "complete-value" },
        ["origin"] = new JsonObject { ["kind"] = "value-start", ["extentStartBit"] = 0 },
        ["metrics"] = new JsonObject { ["valueSizeBytes"] = Known(12), ["alignmentBytes"] = Unknown("managed-alignment-not-reported") },
        ["members"] = new JsonArray(Member("tag", 0, 8, type: "u8"), Member("count", 32, 32), Member("code", 64, 16, type: "i16")),
        ["runtimeRegions"] = new JsonArray(),
        ["coverage"] = new JsonObject { ["fieldEnumeration"] = "complete", ["extent"] = "complete", ["occupiedRanges"] = "complete", ["hiddenRegions"] = "not-applicable" },
        ["limitations"] = new JsonArray()
    }),
    ["diagnostics"] = new JsonArray(), ["limitations"] = new JsonArray()
};

static JsonObject Scalar(string id, int width, string signedness) => new()
{
    ["id"] = id, ["displayName"] = id, ["kind"] = "scalar",
    ["representation"] = new JsonObject
    {
        ["widthBits"] = Known(width), ["category"] = Known("integer"), ["signedness"] = Known(signedness),
        ["encoding"] = Known("binary-integer"),
        ["floatingFormat"] = new JsonObject { ["state"] = "not-applicable", ["reason"] = "integer" }
    }
};

static JsonObject Member(string id, long start, long length, string? overlap = null, string type = "i32")
{
    var member = new JsonObject
    {
        ["id"] = id, ["displayName"] = id, ["declarationOrder"] = 0, ["role"] = "field", ["typeRef"] = type,
        ["offsetBits"] = Known(start), ["bitWidth"] = Known(length), ["declaredTypeSizeBits"] = Known(length),
        ["occupiedRanges"] = Ranges(start, length)
    };
    if (overlap is not null) member["overlapGroup"] = overlap;
    return member;
}

static JsonObject Comparison() => new()
{
    ["schemaVersion"] = "0.1", ["mode"] = "representation", ["scope"] = "value", ["policy"] = "value-fields-v1",
    ["leftSnapshotId"] = "fixture", ["rightSnapshotId"] = "fixture", ["exitCode"] = 0,
    ["cases"] = new JsonArray(new JsonObject
    {
        ["id"] = "sample", ["left"] = "sample", ["right"] = "sample", ["verdict"] = "same", ["coverage"] = "complete",
        ["differences"] = new JsonArray(), ["unknowns"] = new JsonArray(), ["diagnostics"] = new JsonArray()
    })
};

static JsonObject Evidence() => new() { ["kind"] = "fixture", ["method"] = "fixture-scalar-layout", ["version"] = "0.1", ["inputs"] = new JsonArray() };
static JsonObject Known(JsonNode value) => new() { ["state"] = "known", ["value"] = value, ["evidence"] = Evidence() };
static JsonObject Unknown(string reason) => new() { ["state"] = "unknown", ["reason"] = reason };
static JsonObject Ranges(long start, long length) => Known(new JsonArray(new JsonObject { ["startBit"] = start, ["lengthBits"] = length }));
static JsonObject First(JsonObject snapshot) => snapshot["observations"]![0]!.AsObject();
static string Render(JsonObject snapshot, JsonObject? comparison = null) => HtmlReport.Render(snapshot, snapshot, comparison ?? Comparison());
static int Count(string text, string expected) => text.Split(expected, StringSplitOptions.None).Length - 1;
static void Equal(int actual, int expected, string label) { if (actual != expected) throw new Exception($"{label}: expected {expected}, got {actual}"); }
static void Contains(string text, string expected) { if (!text.Contains(expected, StringComparison.Ordinal)) throw new Exception("Missing: " + expected); }
static void Absent(string text, string unexpected) { if (text.Contains(unexpected, StringComparison.Ordinal)) throw new Exception("Unexpected: " + unexpected); }
