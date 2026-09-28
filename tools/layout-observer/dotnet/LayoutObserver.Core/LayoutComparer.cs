using System.Text.Json.Nodes;
using static LayoutObserver.Core.Nodes;

namespace LayoutObserver.Core;

public static class LayoutComparer
{
    public static JsonObject Compare(JsonObject left, JsonObject right, JsonObject manifest)
    {
        SnapshotValidator.Validate(left); SnapshotValidator.Validate(right);
        return CompareValidated(left, right, manifest);
    }

    // Signature projections have already been validated, and intentionally omit source-only facts.
    internal static JsonObject CompareValidated(JsonObject left, JsonObject right, JsonObject manifest)
    {
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
            ["leftSnapshotId"] = left.S("snapshotId"), ["rightSnapshotId"] = right.S("snapshotId"), ["exitCode"] = exitCode, ["cases"] = results,
            ["context"] = ComparisonContext.Create(left, right)
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
            if (lo.S("kind") != ro.S("kind") || lo.S("kind") == "anchor-field")
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
            "representation" => (left is "native" or "managed") && (right is "native" or "managed"),
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
            foreach (var key in LayoutNormalization.CoverageKeys)
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
                    lo.S("kind") != ro.S("kind") || lo.S("kind") == "anchor-field")
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
            Equal(path + ".extentStartBit", "offset", left["origin"]!["extentStartBit"], right["origin"]!["extentStartBit"]);
            foreach (var (key, kind) in LayoutNormalization.Metrics(scope, policy, depth, objectExtent))
                Fact(path + "." + key, kind, left["metrics"]![key], right["metrics"]![key]);
            var observedArrays = leftTypes[left.S("typeId")].S("kind") == "array" && rightTypes[right.S("typeId")].S("kind") == "array";
            if (observedArrays) CompareArrayShape(left, right, path);
            else if (objectExtent)
            {
                Equal(path + ".instanceShape", "size", left["instanceShape"], right["instanceShape"]);
            }
            CompareRuntimeRegions(left, right, path);
            CompareType(left.S("typeId"), right.S("typeId"), path + ".type", 0, observedArrays);
            CompareMembers(left, right, path, maps, depth);
        }

        private void CompareArrayShape(JsonObject left, JsonObject right, string path)
        {
            var lt = leftTypes[left.S("typeId")]; var rt = rightTypes[right.S("typeId")];
            var ls = LayoutNormalization.ArrayShape(left, lt); var rs = LayoutNormalization.ArrayShape(right, rt);
            var lc = Numeric(ls["count"]); var rc = Numeric(rs["count"]);
            if (lc is not null && rc is not null)
            {
                Equal(path + ".elementCount", "size", JsonValue.Create(lc.Value), JsonValue.Create(rc.Value));
                var ld = Value(ls["dimensions"]); var rd = Value(rs["dimensions"]);
                Equal(path + ".dimensions", "size", ld, rd);
            }
            else Unknown(path + ".elementCount", "size", left["instanceShape"], right["instanceShape"], "Array cardinality requires an observed instance length or a known declared fixed count.");
            if (!IsKnown(lt["fixedCount"]) || !IsKnown(rt["fixedCount"]))
                Diagnostics.Add(path + ": array comparison uses observed instance cardinality; it does not assert equivalent fixed-length type declarations.");
        }

        private void CompareRuntimeRegions(JsonObject left, JsonObject right, string path)
        {
            var l = LayoutNormalization.Regions(left); var r = LayoutNormalization.Regions(right);
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
            var lSelections = LayoutNormalization.Members(left, maps, "left").ToDictionary(x => x.Key);
            var rSelections = LayoutNormalization.Members(right, maps, "right").ToDictionary(x => x.Key);
            if (maps is not null && mode != "regression" && (lSelections.Keys.Any(x => x.Kind == "identity") || rSelections.Keys.Any(x => x.Kind == "identity")))
                throw new ProtocolException(path + ": explicit mapping does not account for all observed fields.");
            var pairs = new List<(string Logical, JsonObject? L, JsonObject? R, JsonArray? Children)>();
            foreach (var key in lSelections.Keys.Union(rSelections.Keys).OrderBy(x => x.Kind, StringComparer.Ordinal).ThenBy(x => x.Id, StringComparer.Ordinal))
            {
                var l = lSelections.GetValueOrDefault(key); var r = rSelections.GetValueOrDefault(key);
                if (key.Kind == "explicit" && l?.Member is null && r?.Member is null && IsComplete(left) && IsComplete(right))
                    throw new ProtocolException("Mapping selects absent fields on both sides: " + key.Id);
                pairs.Add((key.Id, l?.Member, r?.Member, (l?.Mapping ?? r?.Mapping)?["children"] as JsonArray));
            }
            foreach (var (logical, l, r, children) in pairs)
            {
                var p = path + ".members[" + logical + "]";
                if (l is null && r is null)
                {
                    Unknown(p, "coverage", null, null, "The requested field mapping could not be resolved on either side; incomplete enumeration does not establish a field change.");
                    continue;
                }
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
                var lt = leftTypes[l.S("typeRef")]; var rt = rightTypes[r.S("typeRef")];
                var lc = l["childObservationId"]?.GetValue<string>(); var rc = r["childObservationId"]?.GetValue<string>();
                if (children is { Count: > 0 } &&
                    ((lc is null && lt.S("kind") is "scalar" or "enum" or "reference") ||
                     (rc is null && rt.S("kind") is "scalar" or "enum" or "reference")))
                    throw new ProtocolException(p + ": children mapping selects fields inside a leaf value with no child observation.");
                CompareType(l.S("typeRef"), r.S("typeRef"), p + ".type", 0, lc is not null && rc is not null);
                if (lc is not null && rc is not null)
                    CompareObservation(leftObservations[lc], rightObservations[rc], p + ".value", children, depth + 1);
                else if (lc is not null || rc is not null || lt.S("kind") == "array" || rt.S("kind") == "array" || ContainsRecord(lt, leftTypes) || ContainsRecord(rt, rightTypes))
                {
                    Unknown(p + ".value", "coverage", l["childObservationId"], r["childObservationId"], "Nested record placement is not fully observed on both sides.");
                    if (children is { Count: > 0 }) Diagnostics.Add(p + ": requested children mapping was not evaluated because a nested observation is unavailable.");
                }
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
            Equal(path, "overlap", LayoutNormalization.Ranges(left), LayoutNormalization.Ranges(right));
        }

        private void CompareType(string leftId, string rightId, string path, int depth, bool observedArray = false)
            => CompareTypeShape(LayoutNormalization.Type(leftId, leftTypes, observedArray), LayoutNormalization.Type(rightId, rightTypes, observedArray), path, depth);

        private void CompareTypeShape(JsonObject left, JsonObject right, string path, int depth)
        {
            if (depth > 64) throw new ProtocolException("Type nesting exceeds 64.");
            var lk = left.S("kind"); var rk = right.S("kind");
            if (lk == "opaque" || rk == "opaque") { Unknown(path, "representation", left, right, "Opaque internals cannot prove representation equality."); return; }
            Equal(path + ".kind", "representation", left["kind"], right["kind"]);
            if (lk != rk) return;
            switch (lk)
            {
                case "scalar":
                    foreach (var key in LayoutNormalization.ScalarFacts)
                    {
                        var lc = Value(left["representation"]!["category"])?.GetValue<string>();
                        var rc = Value(right["representation"]!["category"])?.GetValue<string>();
                        Fact(path + "." + key, "representation", left["representation"]![key], right["representation"]![key], key == "floatingFormat" && lc is not null && rc is not null && lc != "float" && rc != "float");
                    }
                    break;
                case "reference":
                    Equal(path + ".referenceKind", "representation", left["referenceKind"], right["referenceKind"]);
                    Fact(path + ".widthBits", "representation", left["representation"]!["widthBits"], right["representation"]!["widthBits"]);
                    break;
                case "enum": CompareTypeShape(Obj(left["underlying"], "underlying"), Obj(right["underlying"], "underlying"), path + ".underlying", depth + 1); break;
                case "array":
                    if (left.ContainsKey("fixedCount") || right.ContainsKey("fixedCount")) Fact(path + ".fixedCount", "representation", left["fixedCount"], right["fixedCount"]);
                    CompareTypeShape(Obj(left["element"], "element"), Obj(right["element"], "element"), path + ".element", depth + 1); break;
            }
        }
    }
}
