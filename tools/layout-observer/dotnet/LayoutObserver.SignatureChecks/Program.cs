using System.Text.Json.Nodes;
using LayoutObserver.Core;
using LayoutObserver.SignatureChecks;
using static LayoutObserver.SignatureChecks.Fixtures;

var passed = 0; var failed = 0;
void Check(string name, Action test)
{
    try { test(); passed++; Console.WriteLine("PASS " + name); }
    catch (Exception error) { failed++; Console.Error.WriteLine("FAIL " + name + ": " + error.Message); }
}

Check("independently specified protocol fixtures", () =>
{
    foreach (var input in new[] { Packet(), Packet("cs", "managed"), Nested(), Array(), Array(runtimeCount: true), EmbeddedArray(), Object() })
        SnapshotValidator.Validate(input.Snapshot);
});
Check("public hand-written golden signatures and fixed canonical bytes", () =>
{
    var directory = Path.Combine(AppContext.BaseDirectory, "golden");
    var vectors = Directory.GetFiles(directory, "*.signature.json");
    Must(vectors.Length >= 2, "Scalar and record golden vectors are required");
    foreach (var path in vectors)
    {
        var prefix = path[..^".signature.json".Length];
        var input = new Input(JsonIO.Read(prefix + ".snapshot.json"), JsonIO.Read(prefix + ".manifest.json"));
        var expected = JsonIO.Read(path, maxDepth: 512); var actual = Sign(input);
        LayoutSignature.Validate(expected);
        Must(JsonNode.DeepEquals(actual, expected), "Public golden signature changed: " + Path.GetFileName(path));
        var codec = JsonIO.Read(prefix + ".codec.json", maxDepth: 512);
        Must(JsonNode.DeepEquals(codec["input"], Case(actual)["payload"]), "Golden payload differs from canonical byte vector");
        var independentHash = Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.ASCII.GetBytes(codec["canonical"]!.GetValue<string>())));
        Must(independentHash == codec["sha256"]!.GetValue<string>() && independentHash == Digest(Case(actual)), "Fixed canonical bytes/digest mismatch");
        Must(LayoutSignature.Compare(actual, expected)["exitCode"]!.GetValue<int>() == 0, "Golden signature cannot roundtrip through comparison");
    }
});
Check("mapped C++ and managed names represent identical bytes", () => Pair(Packet(), Packet("cs", "managed")));
Check("mapped native language and ABI labels do not define storage equality", () => Pair(Packet(), Packet("rust")));
Check("nested physical IDs disappear after a common logical mapping", () => Pair(Nested(), Nested("cs", "managed")));
Check("member enumeration and declaration order are not physical layout", () =>
{
    var left = Packet(); var right = left.Copy(); Reverse(right.Root["members"]!.AsArray()); Reverse(right.Fields);
    foreach (var member in right.Root["members"]!.AsArray().OfType<JsonObject>()) member["declarationOrder"] = 99;
    Pair(left, right);
});
Check("JSON object key order is not content", () =>
{
    var left = Nested(); var right = new Input(ReverseKeys(left.Snapshot).AsObject(), ReverseKeys(left.Manifest).AsObject()); Pair(left, right);
});
Check("evidence, names, source and compiler metadata are not content", () =>
{
    var left = Packet(); var right = left.Copy();
    right.Snapshot["snapshotId"] = "a different capture";
    right.Snapshot["build"]!["configuration"] = "Release"; right.Snapshot["build"]!["sourceRevision"] = "another revision";
    right.Snapshot["build"]!["compiler"]!["name"] = "unrelated frontend"; right.Snapshot["build"]!["runtime"]!["version"] = "123";
    right.Snapshot["producer"]!["id"] = "another collector"; right.Root["displayName"] = "renamed record";
    foreach (var type in right.Snapshot["typeDescriptors"]!.AsArray().OfType<JsonObject>()) type["displayName"] = "cosmetic type name";
    Walk(right.Snapshot, node =>
    {
        if (node.ContainsKey("evidence")) node["evidence"] = new JsonObject { ["kind"] = "runtime", ["method"] = "alternate-independent-probe", ["version"] = "88", ["inputs"] = new JsonArray("different/source") };
        if (node.ContainsKey("declarationOrder")) node["displayName"] = "cosmetic member name";
    });
    Pair(left, right);
});
Check("unselected types and observations do not enter a case signature", () =>
{
    var left = Packet(); var right = left.Copy(); var extra = Nested("unselected");
    foreach (var node in extra.Snapshot["typeDescriptors"]!.AsArray()) right.Snapshot["typeDescriptors"]!.AsArray().Add(node!.DeepClone());
    foreach (var node in extra.Snapshot["observations"]!.AsArray()) right.Snapshot["observations"]!.AsArray().Add(node!.DeepClone());
    Pair(left, right);
});
Check("case, descriptor and observation arrays can be reordered", () =>
{
    var left = Combine(Packet(), Packet("second")); var right = left.Copy();
    Reverse(right.Manifest["cases"]!.AsArray()); Reverse(right.Snapshot["observations"]!.AsArray()); Reverse(right.Snapshot["typeDescriptors"]!.AsArray());
    Pair(left, right);
});
Check("case labels identify reports without salting their content digests", () =>
{
    var left = Packet(); var right = left.Copy(); right.Manifest["cases"]![0]!["id"] = "a renamed case";
    var ls = Sign(left); var rs = Sign(right);
    Must(JsonNode.DeepEquals(Case(ls)["payload"], Case(rs)["payload"]) && Digest(Case(ls)) == Digest(Case(rs)), "Case label entered physical content");
});
Check("scalar occupancy partitions, sorting and zero-length pieces are immaterial", () =>
{
    var left = Packet(); var right = left.Copy();
    right.Root["members"]![1]!["occupiedRanges"] = Known(new JsonArray(Range(48, 16), Range(0, 0), Range(32, 8), Range(40, 8), Range(40, 8)));
    Pair(left, right);
});
Check("runtime regions normalize per role rather than per collector row", () =>
{
    var left = Packet(); var right = left.Copy();
    left.Root["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "reserved", ["ranges"] = Known(new JsonArray(Range(8, 16))) });
    right.Root["runtimeRegions"] = new JsonArray(new JsonObject { ["role"] = "reserved", ["ranges"] = Known(new JsonArray(Range(16, 8))) }, new JsonObject { ["role"] = "reserved", ["ranges"] = Known(new JsonArray(Range(8, 8))) });
    Pair(left, right);
});
Check("an unrepresentable range union fails with a protocol error", () =>
{
    var input = Packet(); input.Root["metrics"]!["valueSizeBytes"] = Unknown();
    input.Root["runtimeRegions"] = new JsonArray(new JsonObject
    {
        ["role"] = "reserved",
        ["ranges"] = Known(new JsonArray(Range(long.MinValue, long.MaxValue), Range(-1, 2)))
    });
    SnapshotValidator.Validate(input.Snapshot);
    Reject(() => Sign(input));
    Reject(() => LayoutComparer.Compare(input.Snapshot, input.Snapshot, PairManifest(input, input)));
});
Check("offsets beyond 2^53 remain exact and adjacent values differ", () =>
{
    const long start = 9_007_199_254_740_993;
    var left = Packet(); var right = left.Copy();
    foreach (var input in new[] { left, right }) { input.Root["metrics"]!["valueSizeBytes"] = Known((start + 32 + 7) / 8); Move(input.Root["members"]![1]!.AsObject(), start); }
    Move(right.Root["members"]![1]!.AsObject(), start + 1);
    Pair(left, right, "different");
    Must(Case(Sign(left))["payload"]!.ToJsonString().Contains(start.ToString(System.Globalization.CultureInfo.InvariantCulture), StringComparison.Ordinal), "Exact 64-bit offset missing from payload");
});
Check("field offsets are relevant even if size and representation remain equal", () =>
{
    var left = Packet(); var right = left.Copy(); Move(right.Root["members"]![1]!.AsObject(), 24); Pair(left, right, "different");
});
Check("total extent and byte order are each required content", () =>
{
    var left = Packet(); var larger = left.Copy(); larger.Root["metrics"]!["valueSizeBytes"] = Known(12); Pair(left, larger, "different");
    var otherEndian = left.Copy(); otherEndian.Snapshot["build"]!["target"]!["endian"] = "big"; Pair(left, otherEndian, "different");
});
Check("signedness is representation, not a local type-name convention", () =>
{
    var left = Packet(); var right = left.Copy(); right.Snapshot["typeDescriptors"]![2]!["representation"]!["signedness"] = Known("unsigned"); Pair(left, right, "different");
});
Check("floating encoding differs and retains the asymmetric required-fact gap", () =>
{
    var left = Packet(); var right = left.Copy(); var repr = right.Snapshot["typeDescriptors"]![2]!["representation"]!;
    repr["category"] = Known("float"); repr["signedness"] = Known("not-applicable"); repr["encoding"] = Known("ieee754"); repr["floatingFormat"] = Known("binary32");
    Pair(left, right, "different", "partial");
});
Check("reference kinds distinguish native pointers and GC references", () =>
{
    var left = Packet(); var type = left.Snapshot["typeDescriptors"]![2]!.AsObject(); var id = type["id"]!.DeepClone();
    type.Clear(); type["id"] = id; type["displayName"] = "slot"; type["kind"] = "reference"; type["referenceKind"] = "native-pointer";
    type["representation"] = new JsonObject { ["widthBits"] = Known(32) };
    var right = left.Copy(); right.Snapshot["typeDescriptors"]![2]!["referenceKind"] = "GC-reference"; Pair(left, right, "different");
});
Check("a known field removal differs under complete enumeration", () =>
{
    var left = Packet(); var right = left.Copy(); right.Root["members"]!.AsArray().RemoveAt(1); Pair(left, right, "different");
});
Check("partial explicit maps preserve identity fields for regression", () =>
{
    var left = Packet(); left.Fields.RemoveAt(1); var right = left.Copy(); Pair(left, right, mode: "regression");
    Move(right.Root["members"]![1]!.AsObject(), 24); Pair(left, right, "different", mode: "regression");
    Reject(() => LayoutSignature.Compare(Sign(left), Sign(right), "representation"));
});
Check("explicit and identity bindings with equal text do not collide", () =>
{
    var left = Packet(); left.Fields.RemoveAt(1); left.Fields[0]!["id"] = "cpp::count";
    var right = left.Copy(); Move(right.Root["members"]![1]!.AsObject(), 24); Pair(left, right, "different", mode: "regression");
});
Check("selectors absent on both complete sides remain a configuration error", () =>
{
    var input = Packet(); input.Fields.Add(Select("missing", "not-a-member"));
    var signature = Sign(input); Must(Case(signature)["state"]!.GetValue<string>() == "complete", "A proven absence became unknown");
    Reject(() => LayoutSignature.Compare(signature, signature));
});
Check("unknown offset cannot certify sameness even against itself", () =>
{
    var input = Packet(); input.Root["members"]![1]!["offsetBits"] = Unknown(); Partial(input); Pair(input, input.Copy(), "incomplete", "partial");
});
Check("required not-applicable size cannot masquerade as an empty value", () =>
{
    var input = Packet(); input.Root["metrics"]!["valueSizeBytes"] = NA(); Partial(input); Pair(input, input.Copy(), "incomplete", "partial");
});
Check("opaque type identities never establish their internal representation", () =>
{
    var input = Packet(); input.Snapshot["typeDescriptors"]![0]!["kind"] = "opaque"; input.Snapshot["typeDescriptors"]![0]!["opaqueTag"] = "foreign";
    Partial(input); Pair(input, input.Copy(), "incomplete", "partial");
});
Check("missing nested record observations remain incomplete", () =>
{
    var input = Nested(); input.Root["members"]![0]!.AsObject().Remove("childObservationId"); input.Snapshot["observations"]!.AsArray().RemoveAt(1);
    Partial(input); Pair(input, input.Copy(), "incomplete", "partial");
});
Check("partial enumeration cannot prove absence of a requested field", () =>
{
    var left = Packet(); var right = left.Copy(); right.Root["members"]!.AsArray().RemoveAt(1); right.Root["coverage"]!["fieldEnumeration"] = "partial";
    Partial(right); Pair(left, right, "incomplete", "partial");
});
Check("known differences retain a different verdict alongside unknown required facts", () =>
{
    var left = Packet(); var right = left.Copy();
    foreach (var input in new[] { left, right }) { input.Manifest["policy"] = "value-alignment-v1"; input.Root["metrics"]!["alignmentBytes"] = Unknown(); }
    Move(right.Root["members"]![1]!.AsObject(), 24); Partial(left); Partial(right); Pair(left, right, "different", "partial");
});
Check("unknown explanations are diagnostics, not partial payload content", () =>
{
    var left = Packet(); left.Root["metrics"]!["valueSizeBytes"] = Unknown("first explanation"); var right = left.Copy(); right.Root["metrics"]!["valueSizeBytes"] = Unknown("different explanation");
    Partial(left); Partial(right); Must(JsonNode.DeepEquals(Case(Sign(left))["payload"], Case(Sign(right))["payload"]), "Unknown reasons entered content payload");
});
Check("alignment is ignored by fields policy and required by alignment policy", () =>
{
    var left = Packet(); var right = left.Copy(); right.Root["metrics"]!["alignmentBytes"] = Unknown(); Pair(left, right);
    left.Manifest["policy"] = "value-alignment-v1"; right.Manifest["policy"] = "value-alignment-v1"; Partial(right); Pair(left, right, "incomplete", "partial");
    right.Root["metrics"]!["alignmentBytes"] = Known(8); Pair(left, right, "different");
});
Check("stride is ignored in value scope and required in array scope", () =>
{
    var left = Packet(); var right = left.Copy(); right.Root["metrics"]!["arrayStrideBytes"] = Known(16); Pair(left, right);
    left.Manifest["scope"] = "array"; right.Manifest["scope"] = "array"; Pair(left, right, "different");
    right.Root["metrics"]!["arrayStrideBytes"] = Unknown(); Partial(right); Pair(left, right, "incomplete", "partial");
});
Check("observed runtime array cardinality can match a fixed array", () => Pair(Array(), Array(runtimeCount: true)));
Check("one-sided inline observations retain the missing placement prerequisite", () => Pair(Array(withChildren: true), Array(withChildren: false), "incomplete", "partial"));
Check("one-sided array observations retain conditional declaration facts", () =>
{
    var left = EmbeddedArray(); var right = left.Copy();
    right.Root["members"]![0]!.AsObject().Remove("childObservationId");
    while (right.Snapshot["observations"]!.AsArray().Count > 1) right.Snapshot["observations"]!.AsArray().RemoveAt(1);
    Pair(left, right, "incomplete", "partial");
    right.Snapshot["typeDescriptors"]!.AsArray().OfType<JsonObject>().Single(t => t["id"]!.GetValue<string>() == "array-type")["fixedCount"] = Known(3);
    Pair(left, right, "different", "partial");
});
Check("array dimensions carry meaning beyond equal element count", () =>
{
    var left = Array(); var right = left.Copy(); left.Root["instanceShape"]!["dimensions"] = new JsonArray(1, 2); right.Root["instanceShape"]!["dimensions"] = new JsonArray(2, 1); Pair(left, right, "different");
});
Check("calibrated object coordinates support complete content signatures", () => Pair(Object(), Object(), mode: "regression"));
Check("object extent start is content within a shared coordinate system", () =>
{
    var left = Object(); var right = left.Copy(); right.Root["origin"]!["extentStartBit"] = -32;
    right.Root["runtimeRegions"]![0]!["ranges"] = Known(new JsonArray(Range(-32, 32)));
    Pair(left, right, "different", mode: "regression");
});
Check("native-managed view differences are gates, not content salt", () =>
{
    var left = Packet(); var right = Packet("cs", "managed"); Pair(left, right); Pair(left, right, "not-comparable", "unknown", "regression");
});
Check("nested regression view mismatches cannot bypass compatibility", () =>
{
    var left = Nested(); var right = left.Copy(); right.Snapshot["observations"]![1]!["view"] = "managed"; Pair(left, right, "incomplete", "partial", "regression");
});
Check("nested origin mismatches cannot bypass compatibility", () =>
{
    var left = Nested(); var right = left.Copy(); right.Snapshot["observations"]![1]!["origin"]!["kind"] = "instance-data"; Pair(left, right, "incomplete", "partial");
});
Check("object scope cannot accept an uncalibrated origin", () =>
{
    var left = Object(); var right = left.Copy(); right.Root["origin"]!.AsObject().Remove("conversionEvidence"); Pair(left, right, "not-comparable", "unknown", "regression");
});
Check("marshaled view requires the dedicated adapter mode", () =>
{
    var left = Packet(); var right = Packet("marshal", "marshaled"); Profile(right);
    Pair(left, right, "not-comparable", "unknown"); Pair(left, right, mode: "marshaled-layout");
    right.Root["marshallingProfile"]!["mechanism"] = "custom-marshalling"; Pair(left, right, "not-comparable", "unknown", "marshaled-layout");
});
Check("marshaled regression profile is retained as a prerequisite", () =>
{
    var left = Packet(view: "marshaled"); Profile(left); var right = left.Copy(); right.Root["marshallingProfile"]!["id"] = "other-profile";
    Pair(left, right, "not-comparable", "unknown", "regression");
});
Check("Unicode normalization and path punctuation cannot alias logical members", () =>
{
    foreach (var (first, second) in new[] { ("e\u0301", "\u00e9"), ("a/b", "a~1b"), ("a.b", "a/b"), ("0", "[0]"), ("a\0b", "ab"), ("x\"}],\"members\":[{\"id\":\"z", "x") })
    {
        var left = Packet(); var right = left.Copy(); left.Fields[0]!["id"] = first; right.Fields[0]!["id"] = second;
        var ls = Sign(left); var rs = Sign(right);
        Must(!JsonNode.DeepEquals(Case(ls)["payload"], Case(rs)["payload"]), "Logical IDs aliased: " + first + " / " + second);
        Must(Digest(Case(ls)) != Digest(Case(rs)), "Logical ID digest collision");
    }
});
Check("readable path injection cannot flatten a nested structure into a leaf", () =>
{
    var left = Packet(); left.Fields[0]!["id"] = "payload/tag";
    var right = Nested(); var ls = Sign(left); var rs = Sign(right);
    Must(!JsonNode.DeepEquals(Case(ls)["payload"], Case(rs)["payload"]), "A path-looking leaf replaced structural nesting");
});
Check("tampering and missing complete digests are rejected", () =>
{
    var signature = Sign(Packet()); var bad = (JsonObject)signature.DeepClone(); Case(bad)["digest"]!["value"] = new string('0', 64); Reject(() => LayoutSignature.Validate(bad));
    bad = (JsonObject)signature.DeepClone(); Case(bad)["payload"]!["endian"] = "big"; Reject(() => LayoutSignature.Validate(bad));
    bad = (JsonObject)signature.DeepClone(); Case(bad).Remove("digest"); Reject(() => LayoutSignature.Validate(bad));
    bad = (JsonObject)signature.DeepClone(); Case(bad)["state"] = "partial"; Reject(() => LayoutSignature.Validate(bad));
});
Check("signature object-key permutations do not invalidate its digest", () => LayoutSignature.Validate(ReverseKeys(Sign(Nested())).AsObject()));
Check("invalid optional child values cannot hide behind a valid content digest", () =>
{
    var original = Sign(Packet()); var checkEncoder = (JsonObject)original.DeepClone(); RefreshAsciiDigest(checkEncoder);
    Must(Digest(Case(original)) == Digest(Case(checkEncoder)), "Adversarial test encoder differs on this ASCII fixture");
    foreach (var value in new JsonNode?[] { null, JsonValue.Create("ignored"), new JsonArray() })
    {
        var bad = (JsonObject)original.DeepClone();
        Case(bad)["payload"]!["layout"]!["members"]![0]!["value"] = value?.DeepClone(); RefreshAsciiDigest(bad);
        Reject(() => LayoutSignature.Validate(bad));
    }
    var invalidGuard = (JsonObject)original.DeepClone();
    Case(invalidGuard)["prerequisites"]!["members"]![0]!["value"] = null;
    Reject(() => LayoutSignature.Validate(invalidGuard));
});
Check("unknown diagnostics preserve fact paths without fixing English wording or order", () =>
{
    var input = Packet(); input.Root["metrics"]!["valueSizeBytes"] = Unknown(); input.Root["members"]![0]!["offsetBits"] = Unknown();
    var signature = Sign(input); var changed = (JsonObject)signature.DeepClone(); var unknowns = Case(changed)["unknowns"]!.AsArray();
    Must(unknowns.Count >= 2, "Diagnostic permutation fixture needs multiple missing facts");
    Reverse(unknowns); foreach (var item in unknowns.OfType<JsonObject>()) item["message"] = "此必需事实没有观测结果。";
    LayoutSignature.Validate(changed);
    Must(LayoutSignature.Compare(signature, changed)["cases"]![0]!["verdict"]!.GetValue<string>() == "incomplete", "Diagnostic text changed the storage result");
    var invalid = (JsonObject)changed.DeepClone(); Case(invalid)["unknowns"]![0]!["path"] = "a different missing fact"; Reject(() => LayoutSignature.Validate(invalid));
    invalid = (JsonObject)changed.DeepClone(); Case(invalid)["unknowns"]!.AsArray().RemoveAt(0); Reject(() => LayoutSignature.Validate(invalid));
});
Check("malformed source build objects fail with protocol errors", () =>
{
    foreach (var target in new JsonNode?[] { null, JsonValue.Create("wrong"), new JsonArray(), new JsonObject() })
    {
        var invalid = Sign(Packet()); invalid["source"]!["build"]!["target"] = target?.DeepClone(); Reject(() => LayoutSignature.Validate(invalid));
    }
});
Check("invalid modes, case selection and malformed Unicode are rejected", () =>
{
    var signature = Sign(Packet()); Reject(() => LayoutSignature.Compare(signature, signature, "abi-compatible"));
    var invalid = Packet(); invalid.Manifest["cases"] = new JsonArray(); Reject(() => Sign(invalid));
    invalid = Packet(); invalid.Manifest["cases"]![0]!["observation"] = "unobserved"; Reject(() => Sign(invalid));
    invalid = Packet(); invalid.Fields[0]!["id"] = "\ud800"; Reject(() => Sign(invalid));
});
Check("invalid snapshot facts and duplicate selectors are rejected", () =>
{
    var invalid = Packet(); invalid.Root["members"]![0]!["offsetBits"] = Known(0.5); Reject(() => Sign(invalid));
    invalid = Packet(); invalid.Fields.Add(invalid.Fields[0]!.DeepClone()); Reject(() => Sign(invalid));
    invalid = Packet(); invalid.Fields[0]!["children"] = new JsonArray(Select("impossible", "missing")); Reject(() => Sign(invalid));
});
Check("signature generation preserves caller inputs", () =>
{
    var input = Nested(); var snapshot = input.Snapshot.ToJsonString(); var manifest = input.Manifest.ToJsonString(); _ = Sign(input);
    Must(input.Snapshot.ToJsonString() == snapshot && input.Manifest.ToJsonString() == manifest, "Signature mutated source JSON");
});
Check("deep flat observation graphs survive structured signature JSON roundtrip", () =>
{
    var input = DeepGraph(60); SnapshotValidator.Validate(input.Snapshot);
    var signature = Sign(input);
    var json = signature.ToJsonString(new System.Text.Json.JsonSerializerOptions { MaxDepth = 512 });
    var restored = JsonIO.Parse(json, maxDepth: 512); LayoutSignature.Validate(restored);
    var result = LayoutSignature.Compare(signature, restored, "regression");
    Must(result["exitCode"]!.GetValue<int>() == 0, "Deep exported signature cannot be compared after roundtrip");
    Must(Digest(Case(signature)) == Digest(Case(restored)), "Roundtrip altered the deep content digest");
});

Console.WriteLine($"Signature conformance: {passed} passed, {failed} failed");
return failed == 0 ? 0 : 1;

static void Must(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
static JsonObject Sign(Input input)
{
    var signature = LayoutSignature.Generate(input.Snapshot, input.Manifest); LayoutSignature.Validate(signature); return signature;
}
static JsonObject Case(JsonObject signature) => signature["cases"]![0]!.AsObject();
static string Digest(JsonObject item) => item["digest"]!["value"]!.GetValue<string>();
static void Partial(Input input)
{
    var signature = Sign(input); var item = Case(signature);
    Must(item["state"]!.GetValue<string>() == "partial" && !item.ContainsKey("digest") && signature["exitCode"]!.GetValue<int>() == 2, "Incomplete knowledge produced a complete content digest");
}
static void Pair(Input left, Input right, string verdict = "same", string coverage = "complete", string mode = "representation")
{
    var direct = LayoutComparer.Compare(left.Snapshot, right.Snapshot, PairManifest(left, right, mode));
    var ls = Sign(left); var rs = Sign(right); var projected = LayoutSignature.Compare(ls, rs, mode);
    Must(direct["exitCode"]!.GetValue<int>() == projected["exitCode"]!.GetValue<int>(), "Signature/direct exit codes disagree");
    var actual = projected["cases"]!.AsArray().OfType<JsonObject>().ToDictionary(c => c["id"]!.GetValue<string>(), StringComparer.Ordinal);
    var leftCases = ls["cases"]!.AsArray().OfType<JsonObject>().ToDictionary(c => c["id"]!.GetValue<string>(), StringComparer.Ordinal);
    var rightCases = rs["cases"]!.AsArray().OfType<JsonObject>().ToDictionary(c => c["id"]!.GetValue<string>(), StringComparer.Ordinal);
    foreach (var expected in direct["cases"]!.AsArray().OfType<JsonObject>())
    {
        var id = expected["id"]!.GetValue<string>(); var got = actual[id];
        Must(expected["verdict"]!.GetValue<string>() == verdict && expected["coverage"]!.GetValue<string>() == coverage, "Independent storage expectation failed in comparer: " + expected.ToJsonString());
        Must(got["verdict"]!.GetValue<string>() == verdict && got["coverage"]!.GetValue<string>() == coverage, "Signature comparison lost a fact or gate: " + got.ToJsonString());
        if (coverage == "complete" && verdict is "same" or "different")
        {
            var l = leftCases[id]; var r = rightCases[id];
            Must(l["state"]!.GetValue<string>() == "complete" && r["state"]!.GetValue<string>() == "complete", "Fully observed comparable case has a partial signature");
            var equal = JsonNode.DeepEquals(l["payload"], r["payload"]);
            Must(equal == (verdict == "same"), "Compatible complete payload equality is not equivalent to Core same");
            Must((Digest(l) == Digest(r)) == equal, "Complete digest does not agree with payload equality");
            Must(Digest(l).Length == 64 && Digest(l).All(c => c is >= '0' and <= '9' or >= 'a' and <= 'f'), "Digest is not a lowercase SHA-256 hex value");
        }
    }
}
static void Reject(Action action)
{
    try { action(); } catch (ProtocolException) { return; }
    throw new InvalidOperationException("Invalid input was accepted");
}
static void Reverse(JsonArray array)
{
    var nodes = array.Select(n => n?.DeepClone()).Reverse().ToArray(); array.Clear(); foreach (var node in nodes) array.Add(node);
}
static JsonNode ReverseKeys(JsonNode node) => node switch
{
    JsonObject obj => new JsonObject(obj.Reverse().Select(p => new KeyValuePair<string, JsonNode?>(p.Key, p.Value is null ? null : ReverseKeys(p.Value)))),
    JsonArray array => new JsonArray(array.Select(item => item is null ? null : ReverseKeys(item)).ToArray()),
    _ => node.DeepClone()
};
static void RefreshAsciiDigest(JsonObject signature)
{
    // Independent digest oracle for these ASCII-only adversarial mutations. Standard JSON
    // emission is canonical here because this fixture has no characters requiring escapes.
    static JsonNode Sorted(JsonNode node) => node switch
    {
        JsonObject obj => new JsonObject(obj.OrderBy(p => p.Key, StringComparer.Ordinal).Select(p => new KeyValuePair<string, JsonNode?>(p.Key, p.Value is null ? null : Sorted(p.Value)))),
        JsonArray array => new JsonArray(array.Select(item => item is null ? null : Sorted(item)).ToArray()),
        _ => node.DeepClone()
    };
    var canonical = Sorted(Case(signature)["payload"]!).ToJsonString();
    Must(canonical.All(c => c <= 127) && !canonical.Contains('\\'), "Adversarial digest helper only accepts unescaped ASCII");
    Case(signature)["digest"]!["value"] = Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.ASCII.GetBytes(canonical)));
}
static void Walk(JsonNode? node, Action<JsonObject> visitor)
{
    if (node is JsonObject obj) { visitor(obj); foreach (var value in obj.Select(p => p.Value).ToArray()) Walk(value, visitor); }
    else if (node is JsonArray array) foreach (var value in array) Walk(value, visitor);
}
static Input Combine(Input first, Input second)
{
    var result = first.Copy(); var other = second.Copy(); result.Manifest["cases"]![0]!["id"] = "alpha"; other.Manifest["cases"]![0]!["id"] = "beta";
    result.Manifest["cases"]!.AsArray().Add(other.Manifest["cases"]![0]!.DeepClone());
    foreach (var key in new[] { "observations", "typeDescriptors" }) foreach (var node in other.Snapshot[key]!.AsArray()) result.Snapshot[key]!.AsArray().Add(node!.DeepClone());
    return result;
}
static void Profile(Input input)
{
    foreach (var observation in input.Snapshot["observations"]!.AsArray().OfType<JsonObject>())
        observation["marshallingProfile"] = new JsonObject { ["id"] = "runtime-profile", ["mechanism"] = "runtime-marshalling", ["configuration"] = new JsonObject() };
}
