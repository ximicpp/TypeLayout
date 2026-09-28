using System.Text.Json.Nodes;
using LayoutObserver.Core;

namespace LayoutObserver.SignatureChecks;

// Hand-specified storage: a one-byte tag at byte 0 and a signed int32 count at byte 4,
// total extent 8 bytes. These fixtures do not call any normalizer to construct expectations.
internal sealed record Input(JsonObject Snapshot, JsonObject Manifest)
{
    public Input Copy() => new((JsonObject)Snapshot.DeepClone(), (JsonObject)Manifest.DeepClone());
    public JsonObject Root => Snapshot["observations"]![0]!.AsObject();
    public JsonArray Fields => Manifest["cases"]![0]!["fields"]!.AsArray();
}

internal static class Fixtures
{
    public static JsonObject Evidence(string method = "independent-storage-spec") => new()
    {
        ["kind"] = "fixture", ["method"] = method, ["version"] = "conformance-1", ["inputs"] = new JsonArray()
    };
    public static JsonObject Known(JsonNode value) => new() { ["state"] = "known", ["value"] = value, ["evidence"] = Evidence() };
    public static JsonObject Unknown(string reason = "deliberately-unmeasured") => new() { ["state"] = "unknown", ["reason"] = reason };
    public static JsonObject NA() => new() { ["state"] = "not-applicable", ["reason"] = "not-applicable-to-this-storage" };
    public static JsonObject Range(long start, long length) => new() { ["startBit"] = start, ["lengthBits"] = length };
    public static JsonObject Scalar(string id, int bits, string sign = "signed") => new()
    {
        ["id"] = id, ["displayName"] = "local scalar " + id, ["kind"] = "scalar",
        ["representation"] = new JsonObject
        {
            ["widthBits"] = Known(bits), ["category"] = Known("integer"), ["signedness"] = Known(sign),
            ["encoding"] = Known("binary-integer"), ["floatingFormat"] = NA()
        }
    };
    public static JsonObject Member(string id, string type, long offset, long width, int order = 0) => new()
    {
        ["id"] = id, ["displayName"] = "local member " + id, ["declarationOrder"] = order, ["role"] = "field", ["typeRef"] = type,
        ["offsetBits"] = Known(offset), ["bitWidth"] = Known(width), ["declaredTypeSizeBits"] = Known(width),
        ["occupiedRanges"] = Known(new JsonArray(Range(offset, width)))
    };
    public static JsonObject Select(string logical, string member, JsonArray? children = null)
    {
        var result = new JsonObject { ["id"] = logical, ["member"] = member };
        if (children is not null) result["children"] = children;
        return result;
    }

    public static Input Packet(string prefix = "cpp", string view = "native")
    {
        var id = prefix + "/packet";
        var record = prefix + "::Packet";
        var u8 = prefix + "::u8"; var i32 = prefix + "::i32";
        var tag = prefix + "::tag"; var count = prefix + "::count";
        var root = new JsonObject
        {
            ["id"] = id, ["typeId"] = record, ["displayName"] = "Packet " + prefix, ["status"] = "ok", ["view"] = view,
            ["context"] = new JsonObject { ["kind"] = "complete-value" },
            ["origin"] = new JsonObject { ["kind"] = "value-start", ["extentStartBit"] = 0 },
            ["metrics"] = new JsonObject { ["valueSizeBytes"] = Known(8), ["alignmentBytes"] = Known(4), ["arrayStrideBytes"] = Known(8) },
            ["members"] = new JsonArray(Member(tag, u8, 0, 8), Member(count, i32, 32, 32, 1)),
            ["runtimeRegions"] = new JsonArray(), ["limitations"] = new JsonArray(),
            ["coverage"] = new JsonObject { ["fieldEnumeration"] = "complete", ["extent"] = "complete", ["occupiedRanges"] = "complete", ["hiddenRegions"] = "not-applicable" }
        };
        var snapshot = new JsonObject
        {
            ["schemaVersion"] = "0.1", ["snapshotId"] = prefix + "-capture",
            ["producer"] = new JsonObject { ["id"] = "independent-conformance-fixture", ["version"] = "1", ["capabilities"] = new JsonArray("static-test-storage") },
            ["build"] = new JsonObject
            {
                ["buildId"] = prefix + "-build", ["runId"] = prefix + "-run", ["configuration"] = "fixture",
                ["sourceRevision"] = "unknown", ["sourceDirty"] = null, ["sourceDigest"] = "unknown", ["artifactDigest"] = "unknown",
                ["compiler"] = new JsonObject { ["name"] = "test specification", ["version"] = "1" },
                ["runtime"] = new JsonObject { ["name"] = "none", ["version"] = "none" }, ["languages"] = new JsonArray(prefix),
                ["target"] = new JsonObject { ["os"] = "fixture", ["architecture"] = "x64", ["abi"] = "fixture", ["pointerBits"] = 64, ["bitsPerByte"] = 8, ["endian"] = "little" },
                ["flags"] = new JsonArray(), ["dependencies"] = new JsonObject()
            },
            ["typeDescriptors"] = new JsonArray(new JsonObject { ["id"] = record, ["displayName"] = "Local record " + prefix, ["kind"] = "record" }, Scalar(u8, 8, "unsigned"), Scalar(i32, 32)),
            ["observations"] = new JsonArray(root), ["diagnostics"] = new JsonArray(), ["limitations"] = new JsonArray()
        };
        return new(snapshot, new JsonObject
        {
            ["schemaVersion"] = "0.1", ["scope"] = "value", ["policy"] = "value-fields-v1",
            ["cases"] = new JsonArray(new JsonObject { ["id"] = "wire", ["observation"] = id, ["fields"] = new JsonArray(Select("tag", tag), Select("count", count)) })
        });
    }

    public static Input Nested(string prefix = "cpp", string view = "native")
    {
        var input = Packet(prefix, view); var root = input.Root; var child = (JsonObject)root.DeepClone();
        var childId = prefix + "/payload"; var payloadId = prefix + "::payload"; var tailId = prefix + "::tail";
        child["id"] = childId;
        child["context"] = new JsonObject { ["kind"] = "embedded-value", ["hostObservationId"] = root["id"]!.DeepClone(), ["hostMemberId"] = payloadId };
        var payload = Member(payloadId, root["typeId"]!.GetValue<string>(), 0, 64); payload["childObservationId"] = childId;
        root["typeId"] = prefix + "::Outer";
        root["metrics"]!["valueSizeBytes"] = Known(16); root["metrics"]!["arrayStrideBytes"] = Known(16);
        root["members"] = new JsonArray(payload, Member(tailId, prefix + "::i64", 64, 64, 1));
        input.Snapshot["typeDescriptors"]!.AsArray().Add(new JsonObject { ["id"] = prefix + "::Outer", ["displayName"] = "Outer", ["kind"] = "record" });
        input.Snapshot["typeDescriptors"]!.AsArray().Add(Scalar(prefix + "::i64", 64));
        input.Snapshot["observations"]!.AsArray().Add(child);
        var nestedSelectors = (JsonArray)input.Fields.DeepClone();
        input.Manifest["cases"]![0]!["fields"] = new JsonArray(Select("payload", payloadId, nestedSelectors), Select("tail", tailId));
        return input;
    }

    public static Input Array(bool runtimeCount = false, bool withChildren = true)
    {
        var input = Packet(); var root = input.Root;
        input.Snapshot["typeDescriptors"]!.AsArray().Add(new JsonObject
        {
            ["id"] = "array-type", ["displayName"] = "I32 array", ["kind"] = "array", ["elementTypeRef"] = "cpp::i32",
            ["fixedCount"] = runtimeCount ? Unknown() : Known(2)
        });
        root["typeId"] = "array-type"; root["members"] = new JsonArray(Member("0", "cpp::i32", 0, 32), Member("1", "cpp::i32", 32, 32, 1));
        root["instanceShape"] = new JsonObject { ["length"] = 2, ["dimensions"] = new JsonArray(2) };
        root["metrics"]!["arrayStrideBytes"] = Known(4);
        input.Manifest["cases"]![0]!["fields"] = new JsonArray(Select("first", "0"), Select("second", "1"));
        if (withChildren)
            for (var i = 0; i < 2; i++)
            {
                var childId = "array-element-" + i; var child = (JsonObject)root.DeepClone();
                child["id"] = childId; child["typeId"] = "cpp::i32"; child["members"] = new JsonArray(); child.Remove("instanceShape");
                child["context"] = new JsonObject { ["kind"] = "array-element", ["hostObservationId"] = root["id"]!.DeepClone(), ["hostMemberId"] = i.ToString(), ["elementIndex"] = i };
                child["metrics"]!["valueSizeBytes"] = Known(4);
                root["members"]![i]!["childObservationId"] = childId;
                input.Snapshot["observations"]!.AsArray().Add(child);
            }
        return input;
    }

    public static Input Object()
    {
        var input = Packet(view: "managed"); var root = input.Root;
        root["context"] = new JsonObject { ["kind"] = "heap-object" };
        root["origin"] = new JsonObject { ["kind"] = "object-reference", ["extentStartBit"] = -64, ["conversionEvidence"] = Evidence("independent-object-coordinate-calibration") };
        root["metrics"] = new JsonObject { ["runtimeReportedObjectBytes"] = Known(24), ["alignmentBytes"] = Unknown() };
        foreach (var member in root["members"]!.AsArray().OfType<JsonObject>()) Move(member, member["offsetBits"]!["value"]!.GetValue<long>() + 64);
        root["runtimeRegions"] = new JsonArray(
            new JsonObject { ["role"] = "object-header", ["ranges"] = Known(new JsonArray(Range(-64, 64))) },
            new JsonObject { ["role"] = "method-table", ["ranges"] = Known(new JsonArray(Range(0, 64))) });
        root["coverage"]!["hiddenRegions"] = "complete";
        input.Manifest["scope"] = "object";
        return input;
    }

    public static Input EmbeddedArray()
    {
        var input = Array(); var root = input.Root; var child = (JsonObject)root.DeepClone();
        const string childId = "embedded-array";
        child["id"] = childId;
        child["context"] = new JsonObject { ["kind"] = "embedded-value", ["hostObservationId"] = root["id"]!.DeepClone(), ["hostMemberId"] = "array" };
        foreach (var element in input.Snapshot["observations"]!.AsArray().OfType<JsonObject>().Skip(1)) element["context"]!["hostObservationId"] = childId;
        var member = Member("array", "array-type", 0, 64); member["childObservationId"] = childId;
        root["typeId"] = "array-container"; root["members"] = new JsonArray(member); root.Remove("instanceShape");
        input.Snapshot["typeDescriptors"]!.AsArray().Add(new JsonObject { ["id"] = "array-container", ["displayName"] = "Array container", ["kind"] = "record" });
        input.Snapshot["observations"]!.AsArray().Add(child);
        var children = (JsonArray)input.Fields.DeepClone();
        input.Manifest["cases"]![0]!["fields"] = new JsonArray(Select("payload", "array", children));
        return input;
    }

    public static Input DeepGraph(int levels)
    {
        var input = Packet(); var leaf = (JsonObject)input.Root.DeepClone(); var current = input.Root;
        input.Manifest["cases"]![0]!.AsObject().Remove("fields");
        for (var depth = 0; depth < levels; depth++)
        {
            var child = (JsonObject)leaf.DeepClone(); var childId = "deep-child-" + depth;
            child["id"] = childId;
            child["context"] = new JsonObject { ["kind"] = "embedded-value", ["hostObservationId"] = current["id"]!.DeepClone(), ["hostMemberId"] = "payload" };
            var typeId = "deep-record-" + depth; current["typeId"] = typeId;
            input.Snapshot["typeDescriptors"]!.AsArray().Add(new JsonObject { ["id"] = typeId, ["displayName"] = "Layer " + depth, ["kind"] = "record" });
            var childTypeId = depth + 1 < levels ? "deep-record-" + (depth + 1) : "cpp::Packet";
            var payload = Member("payload", childTypeId, 0, 64); payload["childObservationId"] = childId;
            current["members"] = new JsonArray(payload);
            input.Snapshot["observations"]!.AsArray().Add(child); current = child;
        }
        return input;
    }

    public static void Move(JsonObject member, long bitOffset)
    {
        var width = Nodes.Number(member["bitWidth"]!["value"], "width");
        member["offsetBits"] = Known(bitOffset); member["occupiedRanges"] = Known(new JsonArray(Range(bitOffset, width)));
    }

    public static JsonObject PairManifest(Input left, Input right, string mode = "representation")
    {
        var lc = left.Manifest["cases"]!.AsArray(); var rc = right.Manifest["cases"]!.AsArray();
        var cases = new JsonArray();
        foreach (var l in lc.OfType<JsonObject>())
        {
            var r = rc.OfType<JsonObject>().Single(c => c["id"]!.GetValue<string>() == l["id"]!.GetValue<string>());
            var pair = new JsonObject { ["id"] = l["id"]!.DeepClone(), ["left"] = l["observation"]!.DeepClone(), ["right"] = r["observation"]!.DeepClone() };
            if (l["fields"] is JsonArray lf && r["fields"] is JsonArray rf) pair["fields"] = Join(lf, rf);
            cases.Add(pair);
        }
        return new JsonObject { ["schemaVersion"] = "0.1", ["mode"] = mode, ["scope"] = left.Manifest["scope"]!.DeepClone(), ["policy"] = left.Manifest["policy"]!.DeepClone(), ["cases"] = cases };
    }
    private static JsonArray Join(JsonArray left, JsonArray right)
    {
        var result = new JsonArray();
        foreach (var l in left.OfType<JsonObject>())
        {
            var r = right.OfType<JsonObject>().Single(f => f["id"]!.GetValue<string>() == l["id"]!.GetValue<string>());
            var pair = new JsonObject { ["id"] = l["id"]!.DeepClone(), ["left"] = l["member"]!.DeepClone(), ["right"] = r["member"]!.DeepClone() };
            if (l["children"] is JsonArray lf && r["children"] is JsonArray rf) pair["children"] = Join(lf, rf);
            result.Add(pair);
        }
        return result;
    }
}
