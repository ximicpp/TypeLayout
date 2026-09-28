using System.Text.Json.Nodes;
using static LayoutObserver.Core.Nodes;

namespace LayoutObserver.Core;

public static class LayoutComparer
{
    public static JsonObject Compare(JsonObject left, JsonObject right, JsonObject manifest)
    {
        SnapshotValidator.Validate(left); SnapshotValidator.Validate(right);
        ValidateManifest(manifest);
        var leftObservations = Index(Arr(left["observations"], "observations"), "observations");
        var rightObservations = Index(Arr(right["observations"], "observations"), "observations");
        var leftTypes = Index(Arr(left["typeDescriptors"], "types"), "types");
        var rightTypes = Index(Arr(right["typeDescriptors"], "types"), "types");
        var results = new JsonArray(); var exitCode = 0;
        foreach (var mapping in Index(Arr(manifest["cases"], "cases"), "cases").Values)
        {
            if (!leftObservations.TryGetValue(mapping.S("left"), out var l) || !rightObservations.TryGetValue(mapping.S("right"), out var r))
                throw new ProtocolException(mapping.S("id") + ": requested observation missing from snapshot.");
            var session = new ComparisonSession(leftObservations, rightObservations, leftTypes, rightTypes, manifest.S("mode"), manifest.S("policy"), manifest.S("scope"));
            session.Run(l, r, mapping["fields"] as JsonArray);
            if (!session.NotComparable)
                session.Equal("$target.endian", "representation", left["build"]!["target"]!["endian"], right["build"]!["target"]!["endian"]);
            var verdict = session.NotComparable ? "not-comparable" : session.Differences.Count > 0 ? "different" : session.Unknowns.Count > 0 ? "incomplete" : "same";
            var coverage = session.NotComparable ? "unknown" : session.Unknowns.Count > 0 ? "partial" : "complete";
            var result = new JsonObject
            {
                ["id"] = mapping.S("id"), ["left"] = mapping.S("left"), ["right"] = mapping.S("right"),
                ["verdict"] = verdict, ["coverage"] = coverage,
                ["differences"] = session.Differences, ["unknowns"] = session.Unknowns, ["diagnostics"] = session.Diagnostics
            };
            results.Add(result);
            exitCode = Math.Max(exitCode, coverage != "complete" ? 2 : verdict == "different" ? 1 : 0);
        }
        return new JsonObject
        {
            ["schemaVersion"] = "0.1", ["mode"] = manifest.S("mode"), ["scope"] = manifest.S("scope"), ["policy"] = manifest.S("policy"),
            ["leftSnapshotId"] = left.S("snapshotId"), ["rightSnapshotId"] = right.S("snapshotId"), ["exitCode"] = exitCode, ["cases"] = results
        };
    }

    public static void ValidateManifest(JsonObject manifest)
    {
        Keys(manifest, "schemaVersion mode policy scope cases", "compare manifest");
        if (manifest.S("schemaVersion") != "0.1") throw new ProtocolException("Unsupported compare schemaVersion.");
        OneOf(manifest.S("mode"), "regression representation marshaled-layout", "mode");
        OneOf(manifest.S("policy"), "value-fields-v1 value-alignment-v1", "policy");
        OneOf(manifest.S("scope"), "value array object", "scope");
        var cases = Index(Arr(manifest["cases"], "cases"), "cases");
        if (cases.Count == 0) throw new ProtocolException("No cases requested.");
        foreach (var mapping in cases.Values)
        {
            Keys(mapping, "id left right fields", "case"); mapping.S("left"); mapping.S("right");
            if (mapping.ContainsKey("fields")) ValidateFieldMaps(Arr(mapping["fields"], "fields"));
            else if (manifest.S("mode") != "regression") throw new ProtocolException("Cross-language comparison requires an explicit fields mapping (possibly empty).");
        }
    }

    private static void ValidateFieldMaps(JsonArray fields)
    {
        var left = new HashSet<string>(); var right = new HashSet<string>();
        foreach (var field in Index(fields, "fields").Values)
        {
            Keys(field, "id left right children", "field mapping");
            if (!left.Add(field.S("left")) || !right.Add(field.S("right"))) throw new ProtocolException("A source field is mapped more than once.");
            if (field.ContainsKey("children")) ValidateFieldMaps(Arr(field["children"], "children"));
        }
    }

    private sealed class ComparisonSession(
        Dictionary<string, JsonObject> leftObservations, Dictionary<string, JsonObject> rightObservations,
        Dictionary<string, JsonObject> leftTypes, Dictionary<string, JsonObject> rightTypes, string mode, string policy, string scope)
    {
        public JsonArray Differences { get; } = [];
        public JsonArray Unknowns { get; } = [];
        public JsonArray Diagnostics { get; } = [];
        public bool NotComparable { get; private set; }
        private readonly HashSet<(string, string)> activeTypePairs = [];

        public void Run(JsonObject left, JsonObject right, JsonArray? maps)
        {
            if (!CompatibleViews(left.S("view"), right.S("view")))
            {
                Reject("Selected mode does not permit these representation views."); return;
            }
            var lKind = Obj(left["context"], "context").S("kind"); var rKind = Obj(right["context"], "context").S("kind");
            var lObject = lKind is "heap-object" or "boxed-value"; var rObject = rKind is "heap-object" or "boxed-value";
            if (lObject != rObject || (scope == "object") != lObject)
            {
                Reject("Value and object extents cannot be compared under the selected scope."); return;
            }
            if (mode == "regression" && lKind != rKind) { Reject("Regression requires corresponding observation contexts."); return; }
            if (mode == "regression" && left.S("view") == "marshaled" && !JsonNode.DeepEquals(left["marshallingProfile"], right["marshallingProfile"]))
            {
                Reject("Marshaled regression requires the same explicit marshalling profile."); return;
            }
            var lo = Obj(left["origin"], "origin"); var ro = Obj(right["origin"], "origin");
            if (lo.S("kind") != ro.S("kind") || Number(lo["extentStartBit"], "origin") != Number(ro["extentStartBit"], "origin") || lo.S("kind") == "anchor-field")
            {
                Reject("Origins have no established common coordinate system."); return;
            }
            if (scope == "object" && (lo["conversionEvidence"] is not JsonObject || ro["conversionEvidence"] is not JsonObject))
            {
                Reject("Object extent comparison requires calibrated origin conversion evidence."); return;
            }
            if (mode == "marshaled-layout")
            {
                var marshaled = left.S("view") == "marshaled" ? left : right;
                if (marshaled["marshallingProfile"]?["mechanism"]?.GetValue<string>() != "runtime-marshalling")
                {
                    Reject("No adapter for the requested marshalling profile."); return;
                }
                Diagnostics.Add("Only the recorded runtime-marshalling layout is compared; no general call ABI guarantee.");
            }
            CompareObservation(left, right, "$", maps, 0, scope == "object");
        }

        private bool CompatibleViews(string left, string right) => mode switch
        {
            "regression" => left == right,
            "representation" => (left == "native" && right == "managed") || (left == "managed" && right == "native"),
            "marshaled-layout" => (left == "native" && right == "marshaled") || (left == "marshaled" && right == "native"),
            _ => false
        };

        private void Reject(string reason) { NotComparable = true; Diagnostics.Add(reason); }

        public void Equal(string path, string kind, JsonNode? left, JsonNode? right)
        {
            if (!JsonNode.DeepEquals(left, right)) Differences.Add(Entry(path, kind, left, right, "Known values differ."));
        }

        private void Unknown(string path, string kind, JsonNode? left, JsonNode? right, string message) => Unknowns.Add(Entry(path, kind, left, right, message));

        private static JsonObject Entry(string path, string kind, JsonNode? left, JsonNode? right, string message) => new()
        {
            ["path"] = path, ["kind"] = kind, ["left"] = left?.DeepClone(), ["right"] = right?.DeepClone(), ["message"] = message
        };

        private void Fact(string path, string kind, JsonNode? left, JsonNode? right, bool notApplicableAllowed = false)
        {
            if (IsKnown(left) && IsKnown(right)) Equal(path, kind, Value(left), Value(right));
            else if (notApplicableAllowed && left?["state"]?.GetValue<string>() == "not-applicable" && right?["state"]?.GetValue<string>() == "not-applicable") { }
            else Unknown(path, kind, left, right, "Required fact is unavailable; missing values do not prove equality.");
        }

        private void Coverage(JsonObject left, JsonObject right, string path)
        {
            foreach (var key in new[] { "fieldEnumeration", "extent", "occupiedRanges", "hiddenRegions" })
            {
                var l = left["coverage"]![key]!.GetValue<string>(); var r = right["coverage"]![key]!.GetValue<string>();
                if ((l is not ("complete" or "not-applicable")) || (r is not ("complete" or "not-applicable")))
                    Unknown(path + ".coverage." + key, "coverage", left["coverage"]![key], right["coverage"]![key], "Coverage is insufficient for this policy.");
            }
        }

        private void CompareObservation(JsonObject left, JsonObject right, string path, JsonArray? maps, int depth, bool objectExtent = false)
        {
            if (depth > 64) throw new ProtocolException("Comparison nesting exceeds 64.");
            if (depth > 0)
            {
                var lo = Obj(left["origin"], "origin"); var ro = Obj(right["origin"], "origin");
                if (!CompatibleViews(left.S("view"), right.S("view")) ||
                    left["context"]!["kind"]!.GetValue<string>() != right["context"]!["kind"]!.GetValue<string>() ||
                    lo.S("kind") != ro.S("kind") || lo.S("kind") == "anchor-field" ||
                    Number(lo["extentStartBit"], "origin") != Number(ro["extentStartBit"], "origin"))
                {
                    Unknown(path + ".context", "context", left["origin"], right["origin"], "Nested observations do not share a proven coordinate system and representation context.");
                    return;
                }
                if ((mode == "regression" && left.S("view") == "marshaled" && !JsonNode.DeepEquals(left["marshallingProfile"], right["marshallingProfile"])) ||
                    (mode == "marshaled-layout" && (left.S("view") == "marshaled" ? left : right)["marshallingProfile"]?["mechanism"]?.GetValue<string>() != "runtime-marshalling"))
                {
                    Unknown(path + ".marshallingProfile", "context", left["marshallingProfile"], right["marshallingProfile"], "Nested marshalling profiles have no established compatible adapter.");
                    return;
                }
            }
            if (left.S("status") != "ok" || right.S("status") != "ok")
            {
                Unknown(path + ".status", "coverage", left["status"], right["status"], "A requested observation is unsupported."); return;
            }
            Coverage(left, right, path);
            var sizeKey = objectExtent ? "runtimeReportedObjectBytes" : "valueSizeBytes";
            Fact(path + "." + sizeKey, "size", left["metrics"]![sizeKey], right["metrics"]![sizeKey]);
            if (policy == "value-alignment-v1") Fact(path + ".alignmentBytes", "alignment", left["metrics"]!["alignmentBytes"], right["metrics"]!["alignmentBytes"]);
            if (scope == "array" && depth == 0) Fact(path + ".arrayStrideBytes", "stride", left["metrics"]!["arrayStrideBytes"], right["metrics"]!["arrayStrideBytes"]);
            if (objectExtent)
            {
                Equal(path + ".instanceShape", "size", left["instanceShape"], right["instanceShape"]);
            }
            CompareRuntimeRegions(left, right, path);
            CompareType(left.S("typeId"), right.S("typeId"), path + ".type", 0);
            CompareMembers(left, right, path, maps, depth);
        }

        private void CompareRuntimeRegions(JsonObject left, JsonObject right, string path)
        {
            // Runtime regions have explicit roles; overlapping fields do not overwrite them.
            static Dictionary<string, JsonNode> Regions(JsonObject observation)
            {
                var result = new Dictionary<string, JsonNode>();
                foreach (var group in Arr(observation["runtimeRegions"], "regions").OfType<JsonObject>().GroupBy(x => x.S("role")))
                {
                    var unavailable = group.FirstOrDefault(x => !IsKnown(x["ranges"]));
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
            var l = Regions(left); var r = Regions(right);
            foreach (var role in l.Keys.Union(r.Keys, StringComparer.Ordinal).Order(StringComparer.Ordinal))
            {
                if (!l.ContainsKey(role) || !r.ContainsKey(role))
                {
                    var missing = !l.ContainsKey(role) ? left : right;
                    if (missing["coverage"]!["hiddenRegions"]!.GetValue<string>() is "complete" or "not-applicable")
                        Equal(path + $".runtimeRegions[{role}]", "representation", l.GetValueOrDefault(role), r.GetValueOrDefault(role));
                    else Unknown(path + $".runtimeRegions[{role}]", "coverage", l.GetValueOrDefault(role), r.GetValueOrDefault(role), "Region absence cannot be confirmed without complete hidden-region enumeration.");
                    continue;
                }
                CompareRanges(path + $".runtimeRegions[{role}].ranges", l[role], r[role]);
            }
        }

        private void CompareMembers(JsonObject left, JsonObject right, string path, JsonArray? maps, int depth)
        {
            var lm = Index(Arr(left["members"], "members"), "members"); var rm = Index(Arr(right["members"], "members"), "members");
            var usedL = new HashSet<string>(); var usedR = new HashSet<string>();
            var pairs = new List<(string Logical, JsonObject? L, JsonObject? R, JsonArray? Children)>();
            if (maps is not null)
            {
                foreach (var map in maps.OfType<JsonObject>())
                {
                    var lid = map.S("left"); var rid = map.S("right"); usedL.Add(lid); usedR.Add(rid);
                    lm.TryGetValue(lid, out var l); rm.TryGetValue(rid, out var r);
                    if (l is null && r is null && IsComplete(left) && IsComplete(right)) throw new ProtocolException("Mapping selects absent fields on both sides: " + map.S("id"));
                    pairs.Add((map.S("id"), l, r, map["children"] as JsonArray));
                }
                if (mode != "regression" && (lm.Keys.Except(usedL).Any() || rm.Keys.Except(usedR).Any()))
                    throw new ProtocolException(path + ": explicit mapping does not account for all observed fields.");
            }
            foreach (var id in lm.Keys.Except(usedL).Union(rm.Keys.Except(usedR), StringComparer.Ordinal).Order(StringComparer.Ordinal))
            {
                var l = !usedL.Contains(id) ? lm.GetValueOrDefault(id) : null;
                var r = !usedR.Contains(id) ? rm.GetValueOrDefault(id) : null;
                pairs.Add((id, l, r, null));
            }
            foreach (var (logical, l, r, children) in pairs)
            {
                var p = path + ".members[" + logical + "]";
                if (l is null || r is null)
                {
                    if (IsComplete(l is null ? left : right)) Differences.Add(Entry(p, l is null ? "added" : "removed", l, r, "Field membership changed under complete enumeration."));
                    else Unknown(p, "coverage", l, r, "Field absence cannot be confirmed with incomplete enumeration.");
                    continue;
                }
                Equal(p + ".role", "representation", l["role"], r["role"]);
                Fact(p + ".offsetBits", "offset", l["offsetBits"], r["offsetBits"]);
                Fact(p + ".bitWidth", "size", l["bitWidth"], r["bitWidth"]);
                CompareRanges(p + ".occupiedRanges", l["occupiedRanges"], r["occupiedRanges"]);
                CompareType(l.S("typeRef"), r.S("typeRef"), p + ".type", 0);
                var lt = leftTypes[l.S("typeRef")]; var rt = rightTypes[r.S("typeRef")];
                var lc = l["childObservationId"]?.GetValue<string>(); var rc = r["childObservationId"]?.GetValue<string>();
                if (lc is not null && rc is not null)
                    CompareObservation(leftObservations[lc], rightObservations[rc], p + ".value", children, depth + 1);
                else if (lc is not null || rc is not null || lt.S("kind") == "array" || rt.S("kind") == "array" || ContainsRecord(lt, leftTypes) || ContainsRecord(rt, rightTypes))
                    Unknown(p + ".value", "coverage", l["childObservationId"], r["childObservationId"], "Nested record placement is not fully observed on both sides.");
            }
        }

        private static bool IsComplete(JsonObject observation) => observation["coverage"]!["fieldEnumeration"]?.GetValue<string>() == "complete";

        private static bool ContainsRecord(JsonObject type, Dictionary<string, JsonObject> types)
        {
            while (type.S("kind") == "array") type = types[type.S("elementTypeRef")];
            return type.S("kind") is "record" or "union";
        }

        private void CompareRanges(string path, JsonNode? left, JsonNode? right)
        {
            if (!IsKnown(left) || !IsKnown(right)) { Fact(path, "overlap", left, right); return; }
            static JsonArray Canonical(JsonNode? fact)
            {
                var result = new JsonArray();
                long? start = null; long end = 0;
                foreach (var range in Arr(Value(fact), "ranges").OfType<JsonObject>().OrderBy(r => Number(r["startBit"], "startBit")))
                {
                    var nextStart = Number(range["startBit"], "startBit"); var length = Number(range["lengthBits"], "lengthBits");
                    if (length == 0) continue;
                    var nextEnd = checked(nextStart + length);
                    if (start is null) { start = nextStart; end = nextEnd; }
                    else if (nextStart <= end) end = Math.Max(end, nextEnd);
                    else { result.Add(new JsonObject { ["startBit"] = start.Value, ["lengthBits"] = checked(end - start.Value) }); start = nextStart; end = nextEnd; }
                }
                if (start is not null) result.Add(new JsonObject { ["startBit"] = start.Value, ["lengthBits"] = checked(end - start.Value) });
                return result;
            }
            Equal(path, "overlap", Canonical(left), Canonical(right));
        }

        private void CompareType(string leftId, string rightId, string path, int depth)
        {
            if (depth > 64) throw new ProtocolException("Type nesting exceeds 64.");
            if (!activeTypePairs.Add((leftId, rightId))) return;
            try
            {
                var left = leftTypes[leftId]; var right = rightTypes[rightId];
                var lk = left.S("kind"); var rk = right.S("kind");
                if (lk == "opaque" || rk == "opaque") { Unknown(path, "representation", left, right, "Opaque internals cannot prove representation equality."); return; }
                Equal(path + ".kind", "representation", left["kind"], right["kind"]);
                if (lk != rk) return;
                switch (lk)
                {
                    case "scalar":
                        foreach (var key in new[] { "widthBits", "category", "signedness", "encoding" }) Fact(path + "." + key, "representation", left["representation"]![key], right["representation"]![key]);
                        var lc = Value(left["representation"]!["category"])?.GetValue<string>();
                        var rc = Value(right["representation"]!["category"])?.GetValue<string>();
                        Fact(path + ".floatingFormat", "representation", left["representation"]!["floatingFormat"], right["representation"]!["floatingFormat"], lc is not null && rc is not null && lc != "float" && rc != "float");
                        break;
                    case "reference":
                        Equal(path + ".referenceKind", "representation", left["referenceKind"], right["referenceKind"]);
                        Fact(path + ".widthBits", "representation", left["representation"]!["widthBits"], right["representation"]!["widthBits"]);
                        break;
                    case "enum": CompareType(left.S("enumUnderlyingTypeRef"), right.S("enumUnderlyingTypeRef"), path + ".underlying", depth + 1); break;
                    case "array":
                        Fact(path + ".fixedCount", "representation", left["fixedCount"], right["fixedCount"]);
                        CompareType(left.S("elementTypeRef"), right.S("elementTypeRef"), path + ".element", depth + 1);
                        break;
                    // Records/unions are compared through contextual member observations.
                }
            }
            finally { activeTypePairs.Remove((leftId, rightId)); }
        }
    }
}
