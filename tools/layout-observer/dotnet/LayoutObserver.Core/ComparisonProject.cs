using System.Text.Json.Nodes;
using System.Text.RegularExpressions;
using static LayoutObserver.Core.Nodes;

namespace LayoutObserver.Core;

public sealed record ProjectVariant(string Id, string SnapshotPath, string MappingId);
public sealed record ProjectComparison(string Id, string Left, string Right, JsonObject Manifest);
public sealed record ProjectPlan(string Id, IReadOnlyList<ProjectVariant> Variants, IReadOnlyList<ProjectComparison> Comparisons);

/// <summary>Compile reusable logical mappings and explicitly selected variant pairs. No IO or collector dependencies.</summary>
public static partial class ComparisonProject
{
    [GeneratedRegex("^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$", RegexOptions.CultureInvariant)]
    private static partial Regex SafeId();

    public static ProjectPlan Compile(JsonObject project)
    {
        Keys(project, "project", "schemaVersion", "projectId", "variants", "mappings", "comparisons");
        if (Str(project["schemaVersion"], "schemaVersion") != "0.1") throw new ProtocolException("Unsupported project schemaVersion.");
        var projectId = Str(project["projectId"], "projectId");
        var mappings = Index(Nonempty(project["mappings"], "mappings"), "mappings");
        foreach (var (id, mapping) in mappings)
        {
            Keys(mapping, "mapping", "id", "cases");
            var cases = Index(Nonempty(mapping["cases"], "mapping.cases"), "mapping.cases");
            foreach (var (_, item) in cases)
            {
                Keys(item, "mapping.case", "id", "observation", "fields");
                Str(item["observation"], "observation");
                ValidateFields(Arr(item["fields"], "fields"), 0);
            }
        }
        var variants = new List<ProjectVariant>();
        foreach (var (id, variant) in Index(Nonempty(project["variants"], "variants"), "variants"))
        {
            CheckId(id);
            Keys(variant, "variant", "id", "snapshot", "mapping");
            var mapping = Str(variant["mapping"], "variant.mapping");
            if (!mappings.ContainsKey(mapping)) throw new ProtocolException("Unknown mapping " + mapping);
            variants.Add(new(id, Str(variant["snapshot"], "variant.snapshot"), mapping));
        }
        // IDs become portable bundle filenames, including on case-insensitive filesystems.
        if (variants.Select(v => v.Id).Distinct(StringComparer.OrdinalIgnoreCase).Count() != variants.Count)
            throw new ProtocolException("Variant IDs must be unique ignoring case.");
        var byId = variants.ToDictionary(v => v.Id, StringComparer.Ordinal);
        var comparisons = new List<ProjectComparison>();
        foreach (var (id, pair) in Index(Nonempty(project["comparisons"], "comparisons"), "comparisons"))
        {
            CheckId(id);
            Keys(pair, "comparison", "id", "left", "right", "mode", "scope", "policy", "cases");
            var left = Str(pair["left"], "comparison.left"); var right = Str(pair["right"], "comparison.right");
            if (!byId.ContainsKey(left) || !byId.ContainsKey(right)) throw new ProtocolException("Comparison " + id + " references an unknown variant.");
            var lc = Index(Arr(mappings[byId[left].MappingId]["cases"], "cases"), "cases");
            var rc = Index(Arr(mappings[byId[right].MappingId]["cases"], "cases"), "cases");
            var selected = new HashSet<string>(StringComparer.Ordinal); var cases = new JsonArray();
            foreach (var selectedNode in Nonempty(pair["cases"], "comparison.cases"))
            {
                var logical = Str(selectedNode, "case ID");
                if (!selected.Add(logical)) throw new ProtocolException("Duplicate selected case " + logical);
                if (!lc.TryGetValue(logical, out var lm) || !rc.TryGetValue(logical, out var rm))
                    throw new ProtocolException("Case " + logical + " needs an explicit mapping on both variants.");
                cases.Add(new JsonObject { ["id"] = logical, ["left"] = lm["observation"]!.DeepClone(), ["right"] = rm["observation"]!.DeepClone(),
                    ["fields"] = JoinFields(Arr(lm["fields"], "fields"), Arr(rm["fields"], "fields"), logical) });
            }
            var manifest = new JsonObject { ["schemaVersion"] = "0.1", ["mode"] = pair["mode"]?.DeepClone(), ["scope"] = pair["scope"]?.DeepClone(), ["policy"] = pair["policy"]?.DeepClone(), ["cases"] = cases };
            LayoutComparer.ValidateManifest(manifest);
            comparisons.Add(new(id, left, right, manifest));
        }
        if (comparisons.Select(c => c.Id).Distinct(StringComparer.OrdinalIgnoreCase).Count() != comparisons.Count)
            throw new ProtocolException("Comparison IDs must be unique ignoring case.");
        return new(projectId, variants, comparisons);
    }

    private static JsonArray Nonempty(JsonNode? node, string path)
    {
        var result = Arr(node, path);
        if (result.Count == 0) throw new ProtocolException(path + " must not be empty.");
        return result;
    }

    private static void Keys(JsonObject obj, string path, params string[] keys) => Nodes.Keys(obj, string.Join(' ', keys), path);

    private static void CheckId(string id)
    {
        if (!SafeId().IsMatch(id) || Regex.IsMatch(id, "^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant))
            throw new ProtocolException("ID must be a portable filename component: " + id);
    }

    private static void ValidateFields(JsonArray fields, int depth)
    {
        if (depth > 64) throw new ProtocolException("Logical mapping exceeds 64 nested levels.");
        var members = new HashSet<string>(StringComparer.Ordinal);
        foreach (var (_, field) in Index(fields, "fields"))
        {
            Keys(field, "logical field", "id", "member", "children");
            if (!members.Add(Str(field["member"], "member"))) throw new ProtocolException("A member cannot map to two logical fields.");
            if (field.ContainsKey("children")) ValidateFields(Arr(field["children"], "children"), depth + 1);
        }
    }

    private static JsonArray JoinFields(JsonArray left, JsonArray right, string path)
    {
        var li = Index(left, path); var ri = Index(right, path);
        if (!li.Keys.ToHashSet(StringComparer.Ordinal).SetEquals(ri.Keys))
            throw new ProtocolException(path + ": logical field IDs must match; keep explicit mappings for added/removed physical fields.");
        var result = new JsonArray();
        foreach (var (id, lf) in li)
        {
            var rf = ri[id];
            var field = new JsonObject { ["id"] = id, ["left"] = lf["member"]!.DeepClone(), ["right"] = rf["member"]!.DeepClone() };
            if (lf.ContainsKey("children") != rf.ContainsKey("children")) throw new ProtocolException(path + "/" + id + ": children must be mapped on both sides.");
            if (lf.ContainsKey("children")) field["children"] = JoinFields(Arr(lf["children"], "children"), Arr(rf["children"], "children"), path + "/" + id);
            result.Add(field);
        }
        return result;
    }
}
