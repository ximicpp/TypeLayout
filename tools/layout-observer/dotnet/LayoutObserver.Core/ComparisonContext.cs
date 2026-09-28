using System.Text.Json.Nodes;

namespace LayoutObserver.Core;

/// <summary>Experimental conditions and provenance, separate from layout verdicts.</summary>
public static class ComparisonContext
{
    public static JsonObject Create(JsonObject left, JsonObject right)
    {
        var changes = new JsonArray();
        Walk(left["build"], right["build"], "build", changes);
        return new JsonObject
        {
            ["left"] = new JsonObject { ["snapshotId"] = left["snapshotId"]!.DeepClone(), ["build"] = left["build"]!.DeepClone() },
            ["right"] = new JsonObject { ["snapshotId"] = right["snapshotId"]!.DeepClone(), ["build"] = right["build"]!.DeepClone() },
            ["changes"] = changes,
            ["interpretation"] = "Build changes describe conditions and provenance; they neither change the layout verdict nor establish the cause of a layout difference. Missing language or build metadata remains unknown."
        };
    }

    private static void Walk(JsonNode? left, JsonNode? right, string path, JsonArray changes)
    {
        if (JsonNode.DeepEquals(left, right)) return;
        if (left is JsonObject lo && right is JsonObject ro)
        {
            foreach (var key in lo.Select(p => p.Key).Union(ro.Select(p => p.Key), StringComparer.Ordinal).Order(StringComparer.Ordinal))
                Walk(lo[key], ro[key], path + "/" + key.Replace("~", "~0").Replace("/", "~1"), changes);
            return;
        }
        var root = path.Split('/')[1];
        var kind = root is "buildId" or "runId" ? "identity"
            : root is "sourceRevision" or "sourceDirty" or "sourceDigest" or "artifactDigest" or "collectorProvenance" or "captureProvenance" ? "provenance" : "environment";
        changes.Add(new JsonObject { ["path"] = path, ["kind"] = kind, ["left"] = left?.DeepClone(), ["right"] = right?.DeepClone() });
    }
}
