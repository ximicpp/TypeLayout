using System.Text.Json;
using System.Text.Json.Nodes;

namespace LayoutObserver.Core;

public sealed class ProtocolException(string message) : Exception(message);

public static class JsonIO
{
    public static readonly JsonSerializerOptions Options = new() { WriteIndented = true };

    public static JsonObject Read(string path)
    {
        if (new FileInfo(path).Length > 64 * 1024 * 1024)
            throw new ProtocolException("JSON input exceeds 64 MiB limit.");
        return Parse(File.ReadAllText(path));
    }

    public static JsonObject Parse(string text)
    {
        try
        {
            using var document = JsonDocument.Parse(text, new JsonDocumentOptions { MaxDepth = 96 });
            CheckDuplicateKeys(document.RootElement, "$", 0);
            return JsonNode.Parse(text, documentOptions: new JsonDocumentOptions { MaxDepth = 96 }) as JsonObject
                ?? throw new ProtocolException("Root must be an object.");
        }
        catch (JsonException ex) { throw new ProtocolException("Invalid JSON: " + ex.Message); }
    }

    public static void Write(string path, JsonObject value, bool overwrite = false)
    {
        var full = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(full)!);
        using var stream = new FileStream(full, overwrite ? FileMode.Create : FileMode.CreateNew, FileAccess.Write);
        JsonSerializer.Serialize(stream, value, Options);
    }

    private static void CheckDuplicateKeys(JsonElement node, string path, int depth)
    {
        if (depth > 96) throw new ProtocolException("Maximum nesting exceeded.");
        if (node.ValueKind == JsonValueKind.Object)
        {
            var names = new HashSet<string>(StringComparer.Ordinal);
            foreach (var property in node.EnumerateObject())
            {
                if (!names.Add(property.Name)) throw new ProtocolException($"{path}: duplicate property '{property.Name}'.");
                CheckDuplicateKeys(property.Value, path + "." + property.Name, depth + 1);
            }
        }
        else if (node.ValueKind == JsonValueKind.Array)
            foreach (var item in node.EnumerateArray()) CheckDuplicateKeys(item, path + "[]", depth + 1);
    }
}

public static class Nodes
{
    public static JsonObject Obj(JsonNode? node, string path) => node as JsonObject ?? throw new ProtocolException(path + ": expected object.");
    public static JsonArray Arr(JsonNode? node, string path) => node as JsonArray ?? throw new ProtocolException(path + ": expected array.");
    public static string Str(JsonNode? node, string path)
    {
        if (node is JsonValue value && value.TryGetValue<string>(out var text) && !string.IsNullOrWhiteSpace(text)) return text;
        throw new ProtocolException(path + ": expected nonempty string.");
    }
    public static string S(this JsonObject node, string key) => Str(node[key], key);
    public static long Number(JsonNode? node, string path)
    {
        if (node is JsonValue value && value.TryGetValue<long>(out var number)) return number;
        // JsonValue.Create(int) is also used by in-process collectors/tests.
        if (node is JsonValue integer && integer.TryGetValue<int>(out var small)) return small;
        throw new ProtocolException(path + ": expected exact Int64 integer.");
    }
    public static bool IsKnown(JsonNode? fact) => fact is JsonObject obj && obj["state"]?.GetValue<string>() == "known";
    public static JsonNode? Value(JsonNode? fact) => IsKnown(fact) ? fact!["value"] : null;
    public static long? Numeric(JsonNode? fact) => IsKnown(fact) ? Number(fact!["value"], "fact.value") : null;
    public static Dictionary<string, JsonObject> Index(JsonArray array, string name)
    {
        var result = new Dictionary<string, JsonObject>(StringComparer.Ordinal);
        foreach (var entry in array)
        {
            var obj = Obj(entry, name);
            if (!result.TryAdd(obj.S("id"), obj)) throw new ProtocolException(name + ": duplicate ID " + obj.S("id"));
        }
        return result;
    }
    public static void Keys(JsonObject obj, string allowed, string path)
    {
        var set = allowed.Split(' ', StringSplitOptions.RemoveEmptyEntries).ToHashSet(StringComparer.Ordinal);
        foreach (var key in obj.Select(p => p.Key)) if (!set.Contains(key)) throw new ProtocolException(path + ": unknown property '" + key + "'.");
    }
    public static void OneOf(string value, string values, string path)
    {
        if (!values.Split(' ').Contains(value, StringComparer.Ordinal)) throw new ProtocolException(path + ": unsupported value '" + value + "'.");
    }
    public static void Strings(JsonNode? node, string path)
    {
        foreach (var item in Arr(node, path)) Str(item, path + "[]");
    }
}
