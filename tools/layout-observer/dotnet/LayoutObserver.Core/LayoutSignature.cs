using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using static LayoutObserver.Core.Nodes;
using static LayoutObserver.Core.LayoutNormalization;

namespace LayoutObserver.Core;

/// <summary>A one-sided, versioned policy projection. A digest is evidence of content identity,
/// never a substitute for the comparison prerequisites or collection completeness.</summary>
public static class LayoutSignature
{
    public const string Format = "layout-signature-v1";

    public static JsonObject Generate(JsonObject snapshot, JsonObject manifest)
    {
        SnapshotValidator.Validate(snapshot); ValidateManifest(manifest);
        var observations = Index(Arr(snapshot["observations"], "observations"), "observations");
        var types = Index(Arr(snapshot["typeDescriptors"], "types"), "types");
        var cases = new JsonArray();
        var result = new JsonObject
        {
            ["schemaVersion"] = "0.1", ["signatureFormat"] = Format, ["scope"] = manifest.S("scope"), ["policy"] = manifest.S("policy"),
            ["source"] = new JsonObject { ["snapshotId"] = snapshot.S("snapshotId"), ["build"] = snapshot["build"]!.DeepClone() }, ["cases"] = cases
        };
        foreach (var request in Index(Arr(manifest["cases"], "cases"), "cases").Values.OrderBy(x => x.S("id"), StringComparer.Ordinal))
        {
            if (!observations.TryGetValue(request.S("observation"), out var observation)) throw new ProtocolException("Requested signature observation is missing: " + request.S("observation"));
            var (layout, prerequisites) = Project(observation, request["fields"] as JsonArray, observations, types, manifest.S("scope"), manifest.S("policy"), 0);
            var payload = new JsonObject
            {
                ["signatureFormat"] = Format, ["scope"] = manifest.S("scope"), ["policy"] = manifest.S("policy"),
                ["endian"] = snapshot["build"]!["target"]!["endian"]!.DeepClone(), ["layout"] = layout
            };
            var entry = new JsonObject { ["id"] = request.S("id"), ["payload"] = payload, ["prerequisites"] = prerequisites };
            var projection = Restore(result, entry);
            var unknowns = Eligibility(projection, layout, prerequisites, manifest.S("scope"), manifest.S("policy"));
            entry["unknowns"] = unknowns; entry["state"] = unknowns.Count == 0 ? "complete" : "partial";
            if (unknowns.Count == 0) entry["digest"] = Digest(payload);
            cases.Add(entry);
        }
        result["exitCode"] = cases.OfType<JsonObject>().Any(c => c.S("state") == "partial") ? 2 : 0;
        if (JsonSerializer.SerializeToUtf8Bytes(result, JsonIO.Options).Length > 64 * 1024 * 1024)
            throw new ProtocolException("Generated signature exceeds the 64 MiB input/output limit; select fewer cases.");
        return result;
    }

    public static void ValidateManifest(JsonObject manifest)
    {
        Keys(manifest, "schemaVersion scope policy cases", "signature manifest");
        if (manifest.S("schemaVersion") != "0.1") throw new ProtocolException("Unsupported signature manifest version.");
        OneOf(manifest.S("scope"), "value array object", "scope"); OneOf(manifest.S("policy"), "value-fields-v1 value-alignment-v1", "policy");
        var cases = Index(Arr(manifest["cases"], "cases"), "cases");
        if (cases.Count == 0) throw new ProtocolException("No signature cases requested.");
        foreach (var entry in cases.Values)
        {
            Keys(entry, "id observation fields", "signature case"); entry.S("observation");
            if (entry.ContainsKey("fields")) ValidateFields(Arr(entry["fields"], "fields"), 0);
        }
    }

    private static void ValidateFields(JsonArray fields, int depth)
    {
        if (depth > 64) throw new ProtocolException("Signature field mapping nesting exceeds 64.");
        var selected = new HashSet<string>(StringComparer.Ordinal);
        foreach (var field in Index(fields, "fields").Values)
        {
            Keys(field, "id member children", "signature field");
            if (!selected.Add(field.S("member"))) throw new ProtocolException("A source field is mapped more than once.");
            if (field.ContainsKey("children")) ValidateFields(Arr(field["children"], "children"), depth + 1);
        }
    }

    private static (JsonObject Layout, JsonObject Prerequisites) Project(JsonObject observation, JsonArray? fields,
        Dictionary<string, JsonObject> observations, Dictionary<string, JsonObject> types, string scope, string policy, int depth)
    {
        if (depth > 64) throw new ProtocolException("Signature layout nesting exceeds 64.");
        var context = Obj(observation["context"], "context"); var origin = Obj(observation["origin"], "origin");
        var objectExtent = depth == 0 && context.S("kind") is "heap-object" or "boxed-value";
        var type = types[observation.S("typeId")];
        var metrics = new JsonObject();
        foreach (var (key, _) in Metrics(scope, policy, depth, objectExtent)) metrics[key] = Fact(observation["metrics"]![key]);
        var members = new JsonArray(); var guards = new JsonArray();
        var layout = new JsonObject
        {
            ["type"] = Type(observation.S("typeId"), types, type.S("kind") == "array"), ["extentStartBit"] = origin["extentStartBit"]!.DeepClone(),
            ["metrics"] = metrics, ["members"] = members, ["runtimeRegions"] = new JsonArray()
        };
        var prerequisites = new JsonObject
        {
            ["view"] = observation.S("view"), ["contextKind"] = context.S("kind"), ["originKind"] = origin.S("kind"),
            ["calibrated"] = origin["conversionEvidence"] is JsonObject, ["status"] = observation.S("status"),
            ["coverage"] = observation["coverage"]!.DeepClone(), ["explicitFields"] = fields is not null,
            ["unmappedFields"] = new JsonArray(), ["members"] = guards
        };
        if (context.ContainsKey("elementIndex")) prerequisites["elementIndex"] = context["elementIndex"]!.DeepClone();
        if (observation.ContainsKey("marshallingProfile")) prerequisites["marshallingProfile"] = observation["marshallingProfile"]!.DeepClone();
        if (type.S("kind") == "array") { layout["arrayShape"] = ArrayShape(observation, type); prerequisites["declaredType"] = Type(observation.S("typeId"), types); }
        else if (objectExtent && observation.ContainsKey("instanceShape")) layout["instanceShape"] = observation["instanceShape"]!.DeepClone();
        foreach (var (role, ranges) in Regions(observation).OrderBy(x => x.Key, StringComparer.Ordinal))
        {
            var fact = Fact(ranges); if (IsKnown(fact)) fact["value"] = Ranges(fact);
            ((JsonArray)layout["runtimeRegions"]!).Add(new JsonObject { ["role"] = role, ["ranges"] = fact });
        }
        foreach (var selected in Members(observation, fields, "member"))
        {
            var member = selected.Member; var children = selected.Mapping?["children"] as JsonArray;
            var entry = new JsonObject { ["key"] = selected.Key.Json(), ["presence"] = member is not null ? "present" : observation["coverage"]!["fieldEnumeration"]!.GetValue<string>() == "complete" ? "absent" : "unknown" };
            var guard = new JsonObject { ["key"] = selected.Key.Json(), ["childrenRequested"] = children is { Count: > 0 } };
            members.Add(entry); guards.Add(guard);
            if (fields is not null && selected.Key.Kind == "identity") ((JsonArray)prerequisites["unmappedFields"]!).Add(selected.Key.Id);
            if (member is null) continue;
            var childId = member["childObservationId"]?.GetValue<string>(); var memberType = types[member.S("typeRef")];
            if (children is { Count: > 0 } && childId is null && memberType.S("kind") is "scalar" or "enum" or "reference")
                throw new ProtocolException("Children mapping selects a leaf without an observation: " + selected.Key.Id);
            entry["role"] = member.S("role"); entry["offsetBits"] = Fact(member["offsetBits"]); entry["bitWidth"] = Fact(member["bitWidth"]);
            var occupied = Fact(member["occupiedRanges"]); if (IsKnown(occupied)) occupied["value"] = Ranges(occupied); entry["occupiedRanges"] = occupied;
            entry["type"] = Type(member.S("typeRef"), types, childId is not null);
            if (memberType.S("kind") == "array") guard["declaredType"] = Type(member.S("typeRef"), types);
            if (childId is not null)
            {
                var (value, valueGuards) = Project(observations[childId], children, observations, types, scope, policy, depth + 1);
                entry["value"] = value; guard["value"] = valueGuards;
            }
        }
        return (layout, prerequisites);
    }

    public static void Validate(JsonObject signature)
    {
        Keys(signature, "schemaVersion signatureFormat scope policy source cases exitCode", "signature");
        if (signature.S("schemaVersion") != "0.1" || signature.S("signatureFormat") != Format) throw new ProtocolException("Unsupported signature format.");
        OneOf(signature.S("scope"), "value array object", "scope"); OneOf(signature.S("policy"), "value-fields-v1 value-alignment-v1", "policy");
        var source = Obj(signature["source"], "source"); Keys(source, "snapshotId build", "source"); source.S("snapshotId"); SnapshotValidator.ValidateBuild(Obj(source["build"], "build"));
        var cases = Index(Arr(signature["cases"], "cases"), "cases"); if (cases.Count == 0) throw new ProtocolException("Signature has no cases.");
        var partial = false;
        foreach (var entry in cases.Values)
        {
            Keys(entry, "id state payload prerequisites unknowns digest", "signature case");
            OneOf(entry.S("state"), "complete partial", "case.state");
            var payload = Obj(entry["payload"], "payload"); Keys(payload, "signatureFormat scope policy endian layout", "payload");
            if (payload.S("signatureFormat") != Format || payload.S("scope") != signature.S("scope") || payload.S("policy") != signature.S("policy")) throw new ProtocolException("Signature payload profile differs from its envelope.");
            if (payload.S("endian") != source["build"]!["target"]!["endian"]?.GetValue<string>()) throw new ProtocolException("Payload endian differs from its source target.");
            OneOf(payload.S("endian"), "little big", "endian");
            var layout = Obj(payload["layout"], "layout"); var prerequisites = Obj(entry["prerequisites"], "prerequisites");
            ValidateTree(layout, prerequisites, signature.S("scope"), signature.S("policy"), 0);
            _ = Canonical(payload); // Also rejects unsupported numeric forms and invalid Unicode.
            var projection = Restore(signature, entry); SnapshotValidator.ValidateProjection(projection, "root");
            var unknowns = Eligibility(projection, layout, prerequisites, signature.S("scope"), signature.S("policy"));
            static string DiagnosticKey(JsonNode? node)
            {
                var diagnostic = Obj(node, "unknown"); Keys(diagnostic, "path kind message", "unknown"); diagnostic.S("message");
                return Canonical(new JsonObject { ["path"] = diagnostic.S("path"), ["kind"] = diagnostic.S("kind") });
            }
            if (!Arr(entry["unknowns"], "unknowns").Select(DiagnosticKey).Order(StringComparer.Ordinal).SequenceEqual(unknowns.Select(DiagnosticKey).Order(StringComparer.Ordinal)))
                throw new ProtocolException("Signature unknown paths/kinds are not the derived eligibility result.");
            var complete = unknowns.Count == 0;
            if ((entry.S("state") == "complete") != complete) throw new ProtocolException("Signature completeness is not supported by its facts and prerequisites.");
            if (complete)
            {
                if (!JsonNode.DeepEquals(entry["digest"], Digest(payload))) throw new ProtocolException("Signature digest does not match canonical payload.");
            }
            else if (entry.ContainsKey("digest")) throw new ProtocolException("Partial signatures must not carry a comparable digest.");
            partial |= !complete;
        }
        if (Number(signature["exitCode"], "exitCode") != (partial ? 2 : 0)) throw new ProtocolException("Signature exitCode disagrees with completeness.");
    }

    public static JsonObject Compare(JsonObject left, JsonObject right, string mode = "representation")
    {
        Validate(left); Validate(right); OneOf(mode, "regression representation marshaled-layout", "mode");
        if (left.S("scope") != right.S("scope") || left.S("policy") != right.S("policy")) throw new ProtocolException("Signature scope/policy profiles must match.");
        var lc = Index(Arr(left["cases"], "cases"), "cases"); var rc = Index(Arr(right["cases"], "cases"), "cases");
        if (!lc.Keys.ToHashSet(StringComparer.Ordinal).SetEquals(rc.Keys)) throw new ProtocolException("Signature logical case sets must match.");
        var results = new JsonArray(); var exitCode = 0;
        foreach (var id in lc.Keys.Order(StringComparer.Ordinal))
        {
            var l = lc[id]; var r = rc[id]; var ls = Restore(left, l); var rs = Restore(right, r);
            var maps = PairMaps(Obj(l["payload"]!["layout"], "layout"), Obj(l["prerequisites"], "prerequisites"), Obj(r["payload"]!["layout"], "layout"), Obj(r["prerequisites"], "prerequisites"), false);
            var manifest = Manifest(id, left.S("scope"), left.S("policy"), mode, maps);
            var comparison = LayoutComparer.CompareValidated(ls, rs, manifest);
            results.Add(comparison["cases"]![0]!.DeepClone()); exitCode = Math.Max(exitCode, checked((int)Number(comparison["exitCode"], "exitCode")));
        }
        return new JsonObject
        {
            ["schemaVersion"] = "0.1", ["mode"] = mode, ["scope"] = left.S("scope"), ["policy"] = left.S("policy"),
            ["leftSnapshotId"] = left["source"]!["snapshotId"]!.DeepClone(), ["rightSnapshotId"] = right["source"]!["snapshotId"]!.DeepClone(),
            ["exitCode"] = exitCode, ["cases"] = results,
            ["context"] = ComparisonContext.Create(new JsonObject { ["snapshotId"] = left["source"]!["snapshotId"]!.DeepClone(), ["build"] = left["source"]!["build"]!.DeepClone() }, new JsonObject { ["snapshotId"] = right["source"]!["snapshotId"]!.DeepClone(), ["build"] = right["source"]!["build"]!.DeepClone() })
        };
    }

    private static JsonObject Manifest(string id, string scope, string policy, string mode, JsonArray maps) => new()
    {
        ["schemaVersion"] = "0.1", ["mode"] = mode, ["scope"] = scope, ["policy"] = policy,
        ["cases"] = new JsonArray(new JsonObject { ["id"] = id, ["left"] = "root", ["right"] = "root", ["fields"] = maps })
    };

    private static JsonArray Eligibility(JsonObject snapshot, JsonObject layout, JsonObject prerequisites, string scope, string policy)
    {
        var maps = PairMaps(layout, prerequisites, layout, prerequisites, true);
        var comparison = LayoutComparer.CompareValidated(snapshot, snapshot, Manifest("eligibility", scope, policy, "regression", maps));
        var entry = Obj(comparison["cases"]![0], "case"); var result = new JsonArray();
        foreach (var unknown in Arr(entry["unknowns"], "unknowns").OfType<JsonObject>())
            result.Add(new JsonObject { ["path"] = unknown["path"]!.DeepClone(), ["kind"] = unknown["kind"]!.DeepClone(), ["message"] = unknown["message"]!.DeepClone() });
        if (entry.S("verdict") == "not-comparable")
            foreach (var message in Arr(entry["diagnostics"], "diagnostics")) result.Add(new JsonObject { ["path"] = "$", ["kind"] = "context", ["message"] = message!.DeepClone() });
        return result;
    }

    private static Dictionary<Binding, JsonObject> Bound(JsonArray array, string path)
    {
        var result = new Dictionary<Binding, JsonObject>();
        foreach (var node in array)
        {
            var entry = Obj(node, path); var key = ReadKey(entry["key"]);
            if (!result.TryAdd(key, entry)) throw new ProtocolException("Duplicate structural member key: " + key.Token);
        }
        return result;
    }
    private static Binding ReadKey(JsonNode? node)
    {
        var key = Obj(node, "member.key"); Keys(key, "kind id", "member.key"); OneOf(key.S("kind"), "explicit identity", "key.kind"); return new(key.S("kind"), key.S("id"));
    }
    private static bool Bool(JsonNode? node, string path) => node is JsonValue value && value.TryGetValue<bool>(out var b) ? b : throw new ProtocolException(path + ": expected boolean.");

    private static JsonArray PairMaps(JsonObject left, JsonObject lg, JsonObject right, JsonObject rg, bool eligibility)
    {
        var lm = Bound(Arr(left["members"], "members"), "member"); var rm = Bound(Arr(right["members"], "members"), "member");
        var lguards = Bound(Arr(lg["members"], "members"), "member guard"); var rguards = Bound(Arr(rg["members"], "members"), "member guard");
        var explicitFields = Bool(lg["explicitFields"], "explicitFields") || Bool(rg["explicitFields"], "explicitFields");
        var maps = new JsonArray();
        foreach (var key in lm.Keys.Union(rm.Keys).OrderBy(x => x.Kind, StringComparer.Ordinal).ThenBy(x => x.Id, StringComparer.Ordinal))
        {
            var l = lm.GetValueOrDefault(key); var r = rm.GetValueOrDefault(key);
            if (explicitFields && key.Kind == "identity") continue;
            if (eligibility && l?.S("presence") != "present" && r?.S("presence") != "present") continue;
            var map = new JsonObject { ["id"] = key.Token, ["left"] = key.Token, ["right"] = key.Token };
            var lguard = lguards.GetValueOrDefault(key); var rguard = rguards.GetValueOrDefault(key);
            if (l?["value"] is JsonObject lv && r?["value"] is JsonObject rv)
                map["children"] = PairMaps(lv, Obj(lguard!["value"], "guard.value"), rv, Obj(rguard!["value"], "guard.value"), eligibility);
            else if ((lguard is not null && Bool(lguard["childrenRequested"], "childrenRequested")) || (rguard is not null && Bool(rguard["childrenRequested"], "childrenRequested")))
                map["children"] = new JsonArray(new JsonObject { ["id"] = "unresolved-child-selection", ["left"] = "unresolved-child-selection", ["right"] = "unresolved-child-selection" });
            maps.Add(map);
        }
        return maps;
    }

    private static JsonObject Digest(JsonObject payload) => new()
    {
        ["algorithm"] = "sha256", ["value"] = Convert.ToHexStringLower(SHA256.HashData(Encoding.UTF8.GetBytes(Canonical(payload))))
    };

    private static void ValidateFactShape(JsonNode? node, string kind)
    {
        var fact = Obj(node, "fact"); Keys(fact, "state value", "signature fact");
        SnapshotValidator.ValidateFact(ExpandFact(fact), "signature fact", kind);
        if (kind == "ranges" && IsKnown(fact) && !JsonNode.DeepEquals(Value(fact), Ranges(fact))) throw new ProtocolException("Signature ranges are not in canonical union form.");
    }

    private static void ValidateTypeShape(JsonObject type, bool observed, int depth = 0)
    {
        if (depth > 64) throw new ProtocolException("Signature type nesting exceeds 64.");
        var kind = type.S("kind"); OneOf(kind, "scalar reference enum array record union opaque", "type.kind");
        var allowed = kind switch { "scalar" => "kind representation", "reference" => "kind representation referenceKind", "enum" => "kind underlying", "array" => observed ? "kind element" : "kind element fixedCount", _ => "kind" };
        Keys(type, allowed, "signature type");
        if (kind is "scalar" or "reference")
        {
            var repr = Obj(type["representation"], "representation"); Keys(repr, kind == "scalar" ? string.Join(' ', ScalarFacts) : "widthBits", "representation");
            foreach (var key in kind == "scalar" ? ScalarFacts : ["widthBits"]) ValidateFactShape(repr[key], key == "widthBits" ? "positive" : "string");
            if (kind == "reference") type.S("referenceKind");
        }
        if (kind == "enum") ValidateTypeShape(Obj(type["underlying"], "underlying"), false, depth + 1);
        if (kind == "array") { if (!observed) ValidateFactShape(type["fixedCount"], "nonnegative"); ValidateTypeShape(Obj(type["element"], "element"), observed, depth + 1); }
    }

    private static void ValidateTree(JsonObject layout, JsonObject guard, string scope, string policy, int depth)
    {
        if (depth > 64) throw new ProtocolException("Signature layout nesting exceeds 64.");
        Keys(layout, "type extentStartBit metrics members runtimeRegions arrayShape instanceShape", "layout");
        Keys(guard, "view contextKind originKind calibrated status coverage explicitFields unmappedFields members marshallingProfile elementIndex declaredType", "prerequisites");
        OneOf(guard.S("view"), "native managed marshaled", "view"); OneOf(guard.S("contextKind"), "complete-value embedded-value array-element heap-object boxed-value", "contextKind");
        OneOf(guard.S("originKind"), "value-start object-reference instance-data anchor-field", "originKind"); OneOf(guard.S("status"), "ok unsupported", "status");
        Bool(guard["calibrated"], "calibrated"); var explicitFields = Bool(guard["explicitFields"], "explicitFields");
        if (guard.ContainsKey("marshallingProfile")) Obj(guard["marshallingProfile"], "marshallingProfile");
        if (guard.ContainsKey("elementIndex")) Number(guard["elementIndex"], "elementIndex");
        Number(layout["extentStartBit"], "extentStartBit");
        var objectExtent = depth == 0 && guard.S("contextKind") is "heap-object" or "boxed-value";
        var type = Obj(layout["type"], "type"); var isArray = type.S("kind") == "array"; ValidateTypeShape(type, isArray);
        ValidateDeclaration(type, guard, isArray);
        var metrics = Obj(layout["metrics"], "metrics"); var metricNames = Metrics(scope, policy, depth, objectExtent).Select(x => x.Name).ToArray();
        Keys(metrics, string.Join(' ', metricNames), "metrics"); foreach (var key in metricNames) ValidateFactShape(metrics[key], "nonnegative");
        var coverage = Obj(guard["coverage"], "coverage"); Keys(coverage, string.Join(' ', CoverageKeys), "coverage");
        foreach (var key in CoverageKeys) OneOf(coverage.S(key), "complete partial unknown not-applicable", "coverage");
        if (isArray)
        {
            if (layout.ContainsKey("instanceShape")) throw new ProtocolException("Observed arrays use arrayShape.");
            var shape = Obj(layout["arrayShape"], "arrayShape"); Keys(shape, "count dimensions", "arrayShape");
            ValidateFactShape(shape["count"], "nonnegative"); ValidateFactShape(shape["dimensions"], "any");
            if (IsKnown(shape["count"]) != IsKnown(shape["dimensions"])) throw new ProtocolException("Array count and dimensions must be available together.");
            if (!IsKnown(shape["count"]) && (shape["count"]!["state"]!.GetValue<string>() != "unknown" || shape["dimensions"]!["state"]!.GetValue<string>() != "unknown")) throw new ProtocolException("Array cardinality is applicable; unavailable shape must be unknown.");
        }
        else if (layout.ContainsKey("arrayShape") || (layout.ContainsKey("instanceShape") && !objectExtent)) throw new ProtocolException("Shape is inapplicable to this layout.");
        if (layout.ContainsKey("instanceShape")) Obj(layout["instanceShape"], "instanceShape");
        if (guard.S("contextKind") == "array-element" && Number(guard["elementIndex"], "elementIndex") < 0) throw new ProtocolException("Negative element index.");
        var members = Bound(Arr(layout["members"], "members"), "member"); var guards = Bound(Arr(guard["members"], "members"), "member guard");
        if (!members.Keys.ToHashSet().SetEquals(guards.Keys)) throw new ProtocolException("Member prerequisites do not match layout members.");
        // Compare the structural tuple, never an encoded diagnostic path.
        var ordered = members.Keys.OrderBy(x => x.Kind, StringComparer.Ordinal).ThenBy(x => x.Id, StringComparer.Ordinal).ToArray();
        if (!members.Keys.SequenceEqual(ordered)) throw new ProtocolException("Signature members are not canonically ordered.");
        var unmapped = Arr(guard["unmappedFields"], "unmappedFields"); Strings(unmapped, "unmappedFields");
        var expectedUnmapped = explicitFields ? ordered.Where(x => x.Kind == "identity").Select(x => x.Id).ToArray() : [];
        if (!unmapped.Select(x => Str(x, "unmapped")).SequenceEqual(expectedUnmapped)) throw new ProtocolException("Unmapped fields do not match the binding plan.");
        if (!explicitFields && members.Keys.Any(x => x.Kind == "explicit")) throw new ProtocolException("Explicit member bindings require an explicit field plan.");
        foreach (var (key, member) in members)
        {
            var g = guards[key]; Keys(g, "key childrenRequested value declaredType", "member prerequisites"); var requested = Bool(g["childrenRequested"], "childrenRequested");
            OneOf(member.S("presence"), "present absent unknown", "presence");
            if (member.S("presence") != "present")
            {
                Keys(member, "key presence", "unresolved member");
                if (key.Kind != "explicit" || g.ContainsKey("value") || g.ContainsKey("declaredType")) throw new ProtocolException("Only an explicit unresolved selector may be absent/unknown.");
                if ((member.S("presence") == "absent") != (coverage.S("fieldEnumeration") == "complete")) throw new ProtocolException("Member absence is not supported by enumeration coverage.");
                continue;
            }
            Keys(member, "key presence role offsetBits bitWidth occupiedRanges type value", "member");
            OneOf(member.S("role"), "field base", "role"); ValidateFactShape(member["offsetBits"], "integer"); ValidateFactShape(member["bitWidth"], "nonnegative"); ValidateFactShape(member["occupiedRanges"], "ranges");
            var mt = Obj(member["type"], "member.type"); var child = member.ContainsKey("value") ? Obj(member["value"], "member.value") : null; ValidateTypeShape(mt, child is not null);
            ValidateDeclaration(mt, g, child is not null);
            if (requested && child is null && mt.S("kind") is "scalar" or "enum" or "reference") throw new ProtocolException("Children mapping selects a leaf without an observation.");
            if (member.ContainsKey("value") != g.ContainsKey("value")) throw new ProtocolException("Child prerequisites and layout disagree.");
            if (child is not null)
            {
                var childGuard = Obj(g["value"], "value prerequisites");
                if (!JsonNode.DeepEquals(mt, child["type"])) throw new ProtocolException("Inline member type differs from its child observation.");
                if (!JsonNode.DeepEquals(g["declaredType"], childGuard["declaredType"])) throw new ProtocolException("Inline member declaration differs from its child observation.");
                ValidateTree(child, childGuard, scope, policy, depth + 1);
            }
        }
        var roles = new HashSet<string>(StringComparer.Ordinal); string? previous = null;
        foreach (var region in Arr(layout["runtimeRegions"], "runtimeRegions").Select(x => Obj(x, "region")))
        {
            Keys(region, "role ranges", "region"); var role = region.S("role");
            if (!roles.Add(role) || (previous is not null && StringComparer.Ordinal.Compare(previous, role) >= 0)) throw new ProtocolException("Runtime region roles must be unique and ordinally ordered.");
            previous = role; ValidateFactShape(region["ranges"], "ranges");
        }
    }

    private static void ValidateDeclaration(JsonObject type, JsonObject guard, bool observed)
    {
        if (type.S("kind") != "array") { if (guard.ContainsKey("declaredType")) throw new ProtocolException("Only array types have conditional declarations."); return; }
        var declared = Obj(guard["declaredType"], "declaredType"); ValidateTypeShape(declared, false);
        if (!JsonNode.DeepEquals(type, observed ? EraseArrayCounts(declared) : declared)) throw new ProtocolException("Array declaration differs from its projected type.");
    }

    private static JsonObject Evidence() => new() { ["kind"] = "derived", ["method"] = Format, ["version"] = "1", ["inputs"] = new JsonArray() };
    private static JsonObject ExpandFact(JsonNode? node)
    {
        var source = Obj(node, "fact"); var result = (JsonObject)source.DeepClone();
        if (source.S("state") == "known") result["evidence"] = Evidence(); else result["reason"] = "Unavailable in the canonical policy projection.";
        return result;
    }

    private static JsonObject Restore(JsonObject signature, JsonObject entry)
    {
        var types = new JsonArray(); var observations = new JsonArray(); var typeIds = new Dictionary<string, string>(StringComparer.Ordinal);
        string RestoreType(JsonObject type)
        {
            var token = Canonical(type); if (typeIds.TryGetValue(token, out var existing)) return existing;
            var id = "t" + typeIds.Count; typeIds.Add(token, id);
            var kind = type.S("kind"); var result = new JsonObject { ["id"] = id, ["kind"] = kind, ["displayName"] = kind };
            types.Add(result);
            if (kind is "scalar" or "reference")
            {
                var repr = new JsonObject(); foreach (var p in Obj(type["representation"], "representation")) repr[p.Key] = ExpandFact(p.Value); result["representation"] = repr;
            }
            if (kind == "reference") result["referenceKind"] = type.S("referenceKind");
            if (kind == "enum") result["enumUnderlyingTypeRef"] = RestoreType(Obj(type["underlying"], "underlying"));
            if (kind == "array") { result["elementTypeRef"] = RestoreType(Obj(type["element"], "element")); result["fixedCount"] = ExpandFact(type["fixedCount"] ?? Unknown()); }
            if (kind == "opaque") result["opaqueTag"] = "canonical-opaque";
            return id;
        }
        string RestoreLayout(JsonObject layout, JsonObject guard, string? host, string? hostMember)
        {
            var id = host is null ? "root" : "o" + observations.Count;
            var context = new JsonObject { ["kind"] = guard.S("contextKind") };
            if (host is not null) { context["hostObservationId"] = host; context["hostMemberId"] = hostMember; }
            if (guard.ContainsKey("elementIndex")) context["elementIndex"] = guard["elementIndex"]!.DeepClone();
            var origin = new JsonObject { ["kind"] = guard.S("originKind"), ["extentStartBit"] = layout["extentStartBit"]!.DeepClone() };
            if (Bool(guard["calibrated"], "calibrated")) origin["conversionEvidence"] = Evidence();
            var metrics = new JsonObject(); foreach (var p in Obj(layout["metrics"], "metrics")) metrics[p.Key] = ExpandFact(p.Value);
            var members = new JsonArray(); var regions = new JsonArray();
            var result = new JsonObject
            {
                ["id"] = id, ["typeId"] = RestoreType(Obj(guard["declaredType"] ?? layout["type"], "type")), ["displayName"] = id, ["status"] = guard.S("status"),
                ["limitations"] = new JsonArray(), ["view"] = guard.S("view"), ["context"] = context, ["origin"] = origin, ["metrics"] = metrics,
                ["members"] = members, ["runtimeRegions"] = regions, ["coverage"] = guard["coverage"]!.DeepClone()
            };
            observations.Add(result);
            if (guard.ContainsKey("marshallingProfile")) result["marshallingProfile"] = guard["marshallingProfile"]!.DeepClone();
            if (layout["arrayShape"] is JsonObject shape && IsKnown(shape["count"])) result["instanceShape"] = new JsonObject { ["length"] = Value(shape["count"])!.DeepClone(), ["dimensions"] = Value(shape["dimensions"])!.DeepClone() };
            else if (layout.ContainsKey("instanceShape")) result["instanceShape"] = layout["instanceShape"]!.DeepClone();
            var guards = Bound(Arr(guard["members"], "members"), "member prerequisites");
            foreach (var member in Arr(layout["members"], "members").OfType<JsonObject>())
            {
                if (member.S("presence") != "present") continue;
                var key = ReadKey(member["key"]); var name = key.Token;
                var m = new JsonObject
                {
                    ["id"] = name, ["displayName"] = key.Id, ["declarationOrder"] = members.Count, ["role"] = member.S("role"),
                    ["typeRef"] = RestoreType(Obj(guards[key]["declaredType"] ?? member["type"], "member.type")), ["offsetBits"] = ExpandFact(member["offsetBits"]),
                    ["bitWidth"] = ExpandFact(member["bitWidth"]), ["declaredTypeSizeBits"] = ExpandFact(Unknown()), ["occupiedRanges"] = ExpandFact(member["occupiedRanges"])
                };
                members.Add(m);
                if (member["value"] is JsonObject child) m["childObservationId"] = RestoreLayout(child, Obj(guards[key]["value"], "guard.value"), id, name);
            }
            foreach (var region in Arr(layout["runtimeRegions"], "regions").OfType<JsonObject>()) regions.Add(new JsonObject { ["role"] = region.S("role"), ["ranges"] = ExpandFact(region["ranges"]) });
            return id;
        }
        RestoreLayout(Obj(entry["payload"]!["layout"], "layout"), Obj(entry["prerequisites"], "prerequisites"), null, null);
        return new JsonObject
        {
            ["schemaVersion"] = "0.1", ["snapshotId"] = signature["source"]!["snapshotId"]!.DeepClone(), ["build"] = signature["source"]!["build"]!.DeepClone(),
            ["producer"] = new JsonObject { ["id"] = Format, ["version"] = "1", ["capabilities"] = new JsonArray() },
            ["typeDescriptors"] = types, ["observations"] = observations, ["diagnostics"] = new JsonArray(), ["limitations"] = new JsonArray()
        };
    }
}
