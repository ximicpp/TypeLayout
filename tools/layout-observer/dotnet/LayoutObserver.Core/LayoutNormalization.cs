using System.Globalization;
using System.Text;
using System.Text.Json.Nodes;
using static LayoutObserver.Core.Nodes;

namespace LayoutObserver.Core;

// The policy projection is shared by snapshot comparison and standalone signatures.
internal static class LayoutNormalization
{
    internal static readonly string[] ScalarFacts = ["widthBits", "category", "signedness", "encoding", "floatingFormat"];
    internal static readonly string[] CoverageKeys = ["fieldEnumeration", "extent", "occupiedRanges", "hiddenRegions"];
    internal readonly record struct Binding(string Kind, string Id)
    {
        internal JsonObject Json() => new() { ["kind"] = Kind, ["id"] = Id };
        internal string Token => Canonical(Json());
    }
    internal sealed record SelectedMember(Binding Key, JsonObject? Member, JsonObject? Mapping);

    internal static IEnumerable<(string Name, string Kind)> Metrics(string scope, string policy, int depth, bool objectExtent)
    {
        yield return (objectExtent ? "runtimeReportedObjectBytes" : "valueSizeBytes", "size");
        if (policy == "value-alignment-v1") yield return ("alignmentBytes", "alignment");
        if (scope == "array" && depth == 0) yield return ("arrayStrideBytes", "stride");
    }

    internal static List<SelectedMember> Members(JsonObject observation, JsonArray? maps, string selector)
    {
        var members = Index(Arr(observation["members"], "members"), "members");
        var used = new HashSet<string>(StringComparer.Ordinal);
        var result = new List<SelectedMember>();
        if (maps is not null)
            foreach (var map in maps.OfType<JsonObject>())
            {
                var source = map.S(selector);
                if (!used.Add(source)) throw new ProtocolException("A source field is mapped more than once.");
                result.Add(new(new("explicit", map.S("id")), members.GetValueOrDefault(source), map));
            }
        result.AddRange(members.Where(x => !used.Contains(x.Key)).Select(x => new SelectedMember(new("identity", x.Key), x.Value, null)));
        return result.OrderBy(x => x.Key.Kind, StringComparer.Ordinal).ThenBy(x => x.Key.Id, StringComparer.Ordinal).ToList();
    }

    internal static JsonObject Fact(JsonNode? fact)
    {
        var result = new JsonObject { ["state"] = fact?["state"]?.GetValue<string>() ?? "unknown" };
        if (IsKnown(fact)) result["value"] = Value(fact)!.DeepClone();
        return result;
    }
    internal static JsonObject Known(JsonNode value) => new() { ["state"] = "known", ["value"] = value };
    internal static JsonObject Unknown() => new() { ["state"] = "unknown" };

    internal static JsonObject Type(string id, Dictionary<string, JsonObject> types, bool observedArray = false, int depth = 0)
    {
        if (depth > 64) throw new ProtocolException("Type nesting exceeds 64.");
        var type = types[id]; var kind = type.S("kind");
        var result = new JsonObject { ["kind"] = kind };
        if (kind is "scalar" or "reference")
        {
            var repr = new JsonObject();
            foreach (var key in kind == "scalar" ? ScalarFacts : ["widthBits"]) repr[key] = Fact(type["representation"]![key]);
            result["representation"] = repr;
        }
        if (kind == "reference") result["referenceKind"] = type.S("referenceKind");
        if (kind == "enum") result["underlying"] = Type(type.S("enumUnderlyingTypeRef"), types, false, depth + 1);
        if (kind == "array")
        {
            if (!observedArray) result["fixedCount"] = Fact(type["fixedCount"]);
            result["element"] = Type(type.S("elementTypeRef"), types, observedArray, depth + 1);
        }
        return result;
    }

    internal static JsonObject ArrayShape(JsonObject observation, JsonObject type)
    {
        var count = observation["instanceShape"] is JsonObject shape ? Number(shape["length"], "length") : Numeric(type["fixedCount"]);
        return new JsonObject
        {
            ["count"] = count is null ? Unknown() : Known(JsonValue.Create(count.Value)!),
            ["dimensions"] = count is null ? Unknown() : Known(observation["instanceShape"]?["dimensions"]?.DeepClone() ?? new JsonArray(JsonValue.Create(count.Value)))
        };
    }

    internal static JsonArray Ranges(JsonNode? fact)
    {
        try { return RangeUnion(fact); }
        catch (OverflowException) { throw new ProtocolException("Canonical range union length exceeds the Int64 protocol bound."); }
    }
    private static JsonArray RangeUnion(JsonNode? fact)
    {
        var result = new JsonArray(); long? start = null; long end = 0;
        foreach (var range in Arr(Value(fact), "ranges").OfType<JsonObject>().OrderBy(r => Number(r["startBit"], "startBit")))
        {
            var next = Number(range["startBit"], "startBit"); var length = Number(range["lengthBits"], "lengthBits");
            if (length == 0) continue;
            var nextEnd = checked(next + length);
            if (start is null) { start = next; end = nextEnd; }
            else if (next <= end) end = Math.Max(end, nextEnd);
            else { result.Add(new JsonObject { ["startBit"] = start.Value, ["lengthBits"] = checked(end - start.Value) }); start = next; end = nextEnd; }
        }
        if (start is not null) result.Add(new JsonObject { ["startBit"] = start.Value, ["lengthBits"] = checked(end - start.Value) });
        return result;
    }

    internal static JsonObject EraseArrayCounts(JsonObject type)
    {
        var result = (JsonObject)type.DeepClone();
        if (result.S("kind") == "array")
        {
            result.Remove("fixedCount"); result["element"] = EraseArrayCounts(Obj(result["element"], "element"));
        }
        return result;
    }

    internal static Dictionary<string, JsonNode> Regions(JsonObject observation)
    {
        var result = new Dictionary<string, JsonNode>(StringComparer.Ordinal);
        foreach (var group in Arr(observation["runtimeRegions"], "regions").OfType<JsonObject>().GroupBy(x => x.S("role")))
        {
            // Unavailable state is canonical regardless of input fragment ordering.
            var unavailable = group.Where(x => !IsKnown(x["ranges"])).OrderBy(x => x["ranges"]!["state"]!.GetValue<string>(), StringComparer.Ordinal).FirstOrDefault();
            if (unavailable is not null) result.Add(group.Key, unavailable["ranges"]!.DeepClone());
            else
            {
                var fact = Obj(group.First()["ranges"]!.DeepClone(), "ranges");
                fact["value"] = new JsonArray(group.SelectMany(x => Arr(Value(x["ranges"]), "ranges")).Select(x => x!.DeepClone()).ToArray());
                result.Add(group.Key, fact);
            }
        }
        return result;
    }

    // layout-signature-v1 bytes: ordinal UTF-16 keys, ASCII JSON escaping, exact Int64.
    // This deliberately is not JCS (which uses IEEE754 numbers).
    internal static string Canonical(JsonNode? node)
    {
        var text = new StringBuilder(); Write(node, text, 0); return text.ToString();
    }
    private static void Write(JsonNode? node, StringBuilder text, int depth)
    {
        if (depth > 512) throw new ProtocolException("Canonical JSON nesting exceeds 512.");
        if (node is null) { text.Append("null"); return; }
        if (node is JsonObject obj)
        {
            text.Append('{'); var first = true;
            foreach (var p in obj.OrderBy(x => x.Key, StringComparer.Ordinal))
            {
                if (!first) text.Append(','); first = false; String(p.Key, text); text.Append(':'); Write(p.Value, text, depth + 1);
            }
            text.Append('}'); return;
        }
        if (node is JsonArray array)
        {
            text.Append('['); for (var i = 0; i < array.Count; i++) { if (i > 0) text.Append(','); Write(array[i], text, depth + 1); } text.Append(']'); return;
        }
        var value = (JsonValue)node;
        if (value.TryGetValue<string>(out var s)) { String(s, text); return; }
        if (value.TryGetValue<bool>(out var b)) { text.Append(b ? "true" : "false"); return; }
        text.Append(Number(value, "canonical number").ToString(CultureInfo.InvariantCulture));
    }
    private static void String(string value, StringBuilder text)
    {
        text.Append('"');
        for (var i = 0; i < value.Length; i++)
        {
            var c = value[i];
            if (char.IsHighSurrogate(c)) { if (i + 1 == value.Length || !char.IsLowSurrogate(value[i + 1])) throw new ProtocolException("Unpaired Unicode surrogate."); }
            else if (char.IsLowSurrogate(c) && (i == 0 || !char.IsHighSurrogate(value[i - 1]))) throw new ProtocolException("Unpaired Unicode surrogate.");
            if (c == '"') text.Append("\\\""); else if (c == '\\') text.Append("\\\\");
            else if (c < 0x20 || c > 0x7e) text.Append("\\u").Append(((int)c).ToString("x4", CultureInfo.InvariantCulture));
            else text.Append(c);
        }
        text.Append('"');
    }
}
