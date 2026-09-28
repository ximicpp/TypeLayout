using System.Text.Json.Nodes;
using static LayoutObserver.Core.Nodes;

namespace LayoutObserver.Core;

public static class SnapshotValidator
{
    // A signature is a policy projection: nominal type identity and a selected
    // root's enclosing placement are deliberately absent. All remaining facts
    // still obey the snapshot bounds and representation rules.
    internal static void ValidateProjection(JsonObject snapshot, string rootId)
    {
        ValidateBuild(Obj(snapshot["build"], "build"));
        var types = Index(Arr(snapshot["typeDescriptors"], "types"), "types");
        var observations = Index(Arr(snapshot["observations"], "observations"), "observations");
        foreach (var type in types.Values) ValidateType(type, types);
        ValidateTypeCycles(types);
        foreach (var observation in observations.Values) ValidateObservation(observation, types);
        foreach (var observation in observations.Values) ValidateRelations(observation, observations, types, observation.S("id") == rootId);
    }
    public static void Validate(JsonObject snapshot)
    {
        Keys(snapshot, "schemaVersion snapshotId producer build typeDescriptors observations diagnostics limitations", "snapshot");
        if (snapshot.S("schemaVersion") != "0.1") throw new ProtocolException("Unsupported snapshot schemaVersion.");
        snapshot.S("snapshotId");
        var producer = Obj(snapshot["producer"], "producer");
        Keys(producer, "id version capabilities", "producer");
        producer.S("id"); producer.S("version"); Strings(producer["capabilities"], "producer.capabilities");
        ValidateBuild(Obj(snapshot["build"], "build"));
        Strings(snapshot["limitations"], "limitations");
        foreach (var node in Arr(snapshot["diagnostics"], "diagnostics"))
        {
            var diagnostic = Obj(node, "diagnostic");
            Keys(diagnostic, "code message observationId", "diagnostic");
            diagnostic.S("code"); diagnostic.S("message");
            if (diagnostic.ContainsKey("observationId")) diagnostic.S("observationId");
        }
        var types = Index(Arr(snapshot["typeDescriptors"], "typeDescriptors"), "typeDescriptors");
        var observations = Index(Arr(snapshot["observations"], "observations"), "observations");
        if (observations.Count == 0) throw new ProtocolException("Snapshot has no observations.");
        foreach (var type in types.Values) ValidateType(type, types);
        ValidateTypeCycles(types);
        foreach (var observation in observations.Values) ValidateObservation(observation, types);
        foreach (var observation in observations.Values) ValidateRelations(observation, observations, types);
        var visiting = new HashSet<string>(); var visited = new HashSet<string>();
        void Visit(JsonObject observation, int depth = 0)
        {
            if (depth > 64) throw new ProtocolException("Inline observation nesting exceeds 64.");
            var id = observation.S("id");
            if (visited.Contains(id)) return;
            if (!visiting.Add(id)) throw new ProtocolException("Inline observation cycle at " + id);
            foreach (var member in Index(Arr(observation["members"], "members"), "members").Values)
                if (member["childObservationId"] is not null) Visit(observations[member.S("childObservationId")], depth + 1);
            visiting.Remove(id); visited.Add(id);
        }
        foreach (var observation in observations.Values) Visit(observation);
    }

    public static void ValidateFact(JsonNode? node, string path, string valueKind = "any")
    {
        var fact = Obj(node, path);
        Keys(fact, "state value evidence reason", path);
        var state = fact.S("state"); OneOf(state, "known unknown not-applicable", path + ".state");
        if (state != "known")
        {
            fact.S("reason");
            if (fact.ContainsKey("value") || fact.ContainsKey("evidence")) throw new ProtocolException(path + ": unavailable fact must not carry value/evidence.");
            return;
        }
        if (!fact.ContainsKey("value") || fact["value"] is null || fact.ContainsKey("reason")) throw new ProtocolException(path + ": known fact requires value and no reason.");
        ValidateEvidence(fact["evidence"], path + ".evidence");
        if (valueKind == "integer" || valueKind == "nonnegative" || valueKind == "positive")
        {
            var n = Number(fact["value"], path + ".value");
            if ((valueKind == "nonnegative" && n < 0) || (valueKind == "positive" && n <= 0)) throw new ProtocolException(path + ": invalid size/width.");
        }
        else if (valueKind == "string") Str(fact["value"], path + ".value");
        else if (valueKind == "ranges")
        {
            foreach (var entry in Arr(fact["value"], path + ".value"))
            {
                var range = Obj(entry, path + "[]"); Keys(range, "startBit lengthBits", path + "[]");
                var start = Number(range["startBit"], "startBit"); var length = Number(range["lengthBits"], "lengthBits");
                if (length < 0) throw new ProtocolException(path + ": negative range length.");
                try { _ = checked(start + length); } catch (OverflowException) { throw new ProtocolException(path + ": range overflow."); }
            }
        }
    }

    private static void ValidateEvidence(JsonNode? node, string path)
    {
        var evidence = Obj(node, path);
        Keys(evidence, "kind method version inputs", path);
        OneOf(evidence.S("kind"), "compiler runtime derived fixture", path + ".kind");
        evidence.S("method"); evidence.S("version"); Strings(evidence["inputs"], path + ".inputs");
    }

    internal static void ValidateBuild(JsonObject build)
    {
        Keys(build, "buildId runId configuration sourceRevision sourceDirty sourceDigest artifactDigest compiler runtime target flags dependencies requestedProfile languages collectorProvenance captureProvenance", "build");
        foreach (var key in new[] { "buildId", "runId", "configuration", "sourceRevision", "sourceDigest", "artifactDigest" }) build.S(key);
        if (!build.ContainsKey("sourceDirty") || (build["sourceDirty"] is not null && (build["sourceDirty"] is not JsonValue b || !b.TryGetValue<bool>(out _))))
            throw new ProtocolException("sourceDirty must be boolean or null (unknown).");
        foreach (var key in new[] { "compiler", "runtime" })
        {
            var identity = Obj(build[key], key); identity.S("name"); identity.S("version");
        }
        var target = Obj(build["target"], "target");
        Keys(target, "os architecture abi pointerBits bitsPerByte endian", "target");
        target.S("os"); target.S("architecture"); target.S("abi");
        if (Number(target["pointerBits"], "pointerBits") is not (32 or 64)) throw new ProtocolException("Supported pointer widths are 32/64.");
        if (Number(target["bitsPerByte"], "bitsPerByte") != 8) throw new ProtocolException("Only 8-bit bytes supported by protocol 0.1.");
        OneOf(target.S("endian"), "little big", "endian"); Strings(build["flags"], "flags"); Obj(build["dependencies"], "dependencies");
        if (build.ContainsKey("languages"))
        {
            var languages = Arr(build["languages"], "build.languages"); Strings(languages, "build.languages");
            if (languages.Count == 0 || languages.Select(n => Str(n, "language")).Distinct(StringComparer.Ordinal).Count() != languages.Count)
                throw new ProtocolException("build.languages must contain nonempty, unique language identities.");
        }
        if (build.ContainsKey("collectorProvenance")) Obj(build["collectorProvenance"], "build.collectorProvenance");
        if (build.ContainsKey("captureProvenance"))
        {
            var provenance = Obj(build["captureProvenance"], "build.captureProvenance");
            Keys(provenance, "sourceBinding sourceDigestScope artifactVerifiedStable sourceRelation", "captureProvenance");
            OneOf(provenance.S("sourceBinding"), "profile run-default", "captureProvenance.sourceBinding");
            provenance.S("sourceDigestScope");
            if (provenance["artifactVerifiedStable"] is not JsonValue stable || !stable.TryGetValue<bool>(out _))
                throw new ProtocolException("captureProvenance.artifactVerifiedStable must be boolean.");
            OneOf(provenance.S("sourceRelation"), "built-in-run unverified", "captureProvenance.sourceRelation");
        }
    }

    private static void TypeRef(JsonObject node, string key, Dictionary<string, JsonObject> types)
    {
        var id = node.S(key); if (!types.ContainsKey(id)) throw new ProtocolException(key + ": dangling type " + id);
    }

    private static void ValidateType(JsonObject type, Dictionary<string, JsonObject> types)
    {
        Keys(type, "id kind displayName representation enumUnderlyingTypeRef elementTypeRef fixedCount referenceKind targetTypeRef opaqueTag", "type");
        type.S("displayName"); var kind = type.S("kind");
        OneOf(kind, "scalar enum record union array reference opaque", "type.kind");
        if (kind is "scalar" or "reference")
        {
            var repr = Obj(type["representation"], "representation");
            Keys(repr, "widthBits category signedness encoding floatingFormat", "representation");
            ValidateFact(repr["widthBits"], "widthBits", "positive");
            if (kind == "scalar")
            {
                foreach (var key in new[] { "category", "signedness", "encoding", "floatingFormat" }) ValidateFact(repr[key], key, "string");
                var category = Value(repr["category"])?.GetValue<string>();
                var sign = Value(repr["signedness"])?.GetValue<string>();
                if (category is not null) OneOf(category, "integer float boolean character byte", "scalar.category");
                if (sign is not null) OneOf(sign, "signed unsigned not-applicable", "scalar.signedness");
                if (category == "float" && repr["floatingFormat"]!["state"]!.GetValue<string>() == "not-applicable")
                    throw new ProtocolException("Floating format is applicable to a floating-point scalar; use unknown if it cannot be determined.");
                if (category is not null && category != "float" && IsKnown(repr["floatingFormat"]))
                    throw new ProtocolException("A non-floating scalar cannot declare a floating format.");
            }
        }
        if (kind == "enum")
        {
            TypeRef(type, "enumUnderlyingTypeRef", types);
            var underlying = types[type.S("enumUnderlyingTypeRef")];
            var category = Value(underlying["representation"]?["category"])?.GetValue<string>();
            if (underlying.S("kind") != "scalar" || (category is not null && category is not ("integer" or "byte" or "boolean" or "character")))
                throw new ProtocolException("Enum underlying type must be an integral scalar descriptor (including C++ bool/character types).");
        }
        if (kind == "array") { TypeRef(type, "elementTypeRef", types); ValidateFact(type["fixedCount"], "fixedCount", "nonnegative"); }
        if (kind == "reference")
        {
            OneOf(type.S("referenceKind"), "native-pointer native-reference GC-reference function-pointer member-pointer", "referenceKind");
            if (type.ContainsKey("targetTypeRef")) TypeRef(type, "targetTypeRef", types);
        }
        if (kind == "opaque") type.S("opaqueTag");
    }

    private static void ValidateObservation(JsonObject observation, Dictionary<string, JsonObject> types)
    {
        var path = observation.S("id");
        Keys(observation, "id typeId displayName status limitations view context origin metrics members runtimeRegions coverage instanceShape marshallingProfile", path);
        TypeRef(observation, "typeId", types); observation.S("displayName");
        OneOf(observation.S("status"), "ok unsupported", path + ".status"); Strings(observation["limitations"], path + ".limitations");
        OneOf(observation.S("view"), "native managed marshaled", path + ".view");
        var context = Obj(observation["context"], "context");
        Keys(context, "kind hostObservationId hostMemberId elementIndex", "context");
        OneOf(context.S("kind"), "complete-value embedded-value array-element heap-object boxed-value", "context.kind");
        if (context.ContainsKey("elementIndex") && Number(context["elementIndex"], "elementIndex") < 0) throw new ProtocolException("Negative elementIndex.");
        var origin = Obj(observation["origin"], "origin");
        Keys(origin, "kind extentStartBit conversionEvidence", "origin");
        OneOf(origin.S("kind"), "value-start object-reference instance-data anchor-field", "origin.kind");
        var extentStart = Number(origin["extentStartBit"], "extentStartBit");
        if (origin.ContainsKey("conversionEvidence")) ValidateEvidence(origin["conversionEvidence"], path + ".origin.conversionEvidence");
        var metrics = Obj(observation["metrics"], "metrics");
        Keys(metrics, "valueSizeBytes standaloneSizeBytes referenceSlotBytes arrayStrideBytes runtimeReportedObjectBytes alignmentBytes", "metrics");
        foreach (var metric in metrics) ValidateFact(metric.Value, path + ".metrics." + metric.Key, "nonnegative");
        if (observation.S("status") == "ok")
        {
            var sizeKey = context.S("kind") is "heap-object" or "boxed-value" ? "runtimeReportedObjectBytes" : "valueSizeBytes";
            ValidateFact(metrics[sizeKey], path + ".metrics." + sizeKey, "nonnegative");
        }
        var alignment = Numeric(metrics["alignmentBytes"]);
        if (alignment is not null && (alignment <= 0 || (alignment & (alignment - 1)) != 0)) throw new ProtocolException("Alignment must be a positive power of two.");
        var coverage = Obj(observation["coverage"], "coverage");
        Keys(coverage, "fieldEnumeration extent occupiedRanges hiddenRegions", "coverage");
        foreach (var key in new[] { "fieldEnumeration", "extent", "occupiedRanges", "hiddenRegions" }) OneOf(coverage.S(key), "complete partial unknown not-applicable", "coverage." + key);
        if (observation.S("status") == "ok")
        {
            if (coverage.S("extent") == "not-applicable") throw new ProtocolException("An observed value/object cannot declare its extent not-applicable.");
            if (types[observation.S("typeId")].S("kind") is "record" or "union" or "array")
                foreach (var key in new[] { "fieldEnumeration", "occupiedRanges" })
                    if (coverage.S(key) == "not-applicable") throw new ProtocolException("Aggregate " + key + " cannot be not-applicable.");
        }
        var indexedMembers = Index(Arr(observation["members"], "members"), path + ".members");
        var observedType = types[observation.S("typeId")];
        if (observation.ContainsKey("instanceShape"))
        {
            if (observedType.S("kind") != "array" && context.S("kind") is not ("heap-object" or "boxed-value"))
                throw new ProtocolException("Instance shape is only applicable to arrays or variable-length objects.");
            var shape = Obj(observation["instanceShape"], "instanceShape");
            Keys(shape, "length dimensions", "instanceShape");
            var length = Number(shape["length"], "length");
            if (length < 0) throw new ProtocolException("Negative instance length.");
            var dimensions = Arr(shape["dimensions"], "dimensions").Select(n => Number(n, "dimension")).ToArray();
            if (dimensions.Length == 0 || dimensions.Any(n => n < 0)) throw new ProtocolException("Instance dimensions must be nonempty and nonnegative.");
            long product = dimensions.Contains(0) ? 0 : 1;
            try { if (product != 0) foreach (var dimension in dimensions) product = checked(product * dimension); }
            catch (OverflowException) { throw new ProtocolException("Instance dimension product overflow."); }
            if (length != product) throw new ProtocolException("Instance length differs from the product of its dimensions.");
            if (observedType.S("kind") == "array" && Numeric(observedType["fixedCount"]) is long fixedCount && fixedCount != length)
                throw new ProtocolException("Observed array length differs from its declared fixed count.");
        }
        if (observedType.S("kind") == "array" && coverage.S("fieldEnumeration") == "complete")
        {
            var expected = Numeric(observedType["fixedCount"]);
            if (expected is null && observation["instanceShape"] is JsonObject arrayShape) expected = Number(arrayShape["length"], "array.length");
            if (expected is null || indexedMembers.Count != expected) throw new ProtocolException(path + ": complete array enumeration requires all elements and an observed count.");
            if (indexedMembers.Values.Any(m => m.S("typeRef") != observedType.S("elementTypeRef"))) throw new ProtocolException(path + ": array element type mismatch.");
        }
        foreach (var member in indexedMembers.Values)
        {
            Keys(member, "id displayName declarationOrder role typeRef offsetBits bitWidth declaredTypeSizeBits occupiedRanges overlapGroup childObservationId", "member");
            member.S("displayName"); OneOf(member.S("role"), "field base", "member.role"); TypeRef(member, "typeRef", types);
            if (member.ContainsKey("overlapGroup")) member.S("overlapGroup");
            if (Number(member["declarationOrder"], "declarationOrder") < 0) throw new ProtocolException("Negative declarationOrder.");
            ValidateFact(member["offsetBits"], "offsetBits", "integer"); ValidateFact(member["bitWidth"], "bitWidth", "nonnegative");
            ValidateFact(member["declaredTypeSizeBits"], "declaredTypeSizeBits", "nonnegative"); ValidateFact(member["occupiedRanges"], "occupiedRanges", "ranges");
            if (types[member.S("typeRef")].S("kind") is "scalar" or "enum" or "reference")
                ValidateScalarRanges(member, path);
            var extentSize = Numeric(metrics[context.S("kind") is "heap-object" or "boxed-value" ? "runtimeReportedObjectBytes" : "valueSizeBytes"]);
            if (extentSize is not null && Value(member["occupiedRanges"]) is JsonArray ranges)
            {
                long extentEnd;
                try { extentEnd = checked(extentStart + extentSize.Value * 8); } catch (OverflowException) { throw new ProtocolException("Extent overflow."); }
                foreach (var range in ranges.OfType<JsonObject>())
                {
                    var start = Number(range["startBit"], "startBit"); var length = Number(range["lengthBits"], "lengthBits");
                    if (start < extentStart || checked(start + length) > extentEnd) throw new ProtocolException(path + ": member range outside extent.");
                }
            }
        }
        foreach (var node in Arr(observation["runtimeRegions"], "runtimeRegions"))
        {
            var region = Obj(node, "region"); Keys(region, "role ranges", "region"); region.S("role"); ValidateFact(region["ranges"], "region.ranges", "ranges");
            var size = Numeric(metrics[context.S("kind") is "heap-object" or "boxed-value" ? "runtimeReportedObjectBytes" : "valueSizeBytes"]);
            if (size is not null && Value(region["ranges"]) is JsonArray ranges)
            {
                long end;
                try { end = checked(extentStart + size.Value * 8); } catch (OverflowException) { throw new ProtocolException("Extent overflow."); }
                foreach (var range in ranges.OfType<JsonObject>())
                    if (Number(range["startBit"], "startBit") < extentStart || checked(Number(range["startBit"], "startBit") + Number(range["lengthBits"], "lengthBits")) > end)
                        throw new ProtocolException(path + ": runtime region outside extent.");
            }
        }
        if (observation.S("view") == "marshaled" || observation.ContainsKey("marshallingProfile"))
        {
            var profile = Obj(observation["marshallingProfile"], "marshallingProfile");
            Keys(profile, "id mechanism configuration", "marshallingProfile"); profile.S("id"); profile.S("mechanism"); Obj(profile["configuration"], "configuration");
        }
    }

    private static void ValidateScalarRanges(JsonObject member, string path)
    {
        var offset = Numeric(member["offsetBits"]); var width = Numeric(member["bitWidth"]);
        if (offset is null || width is null || Value(member["occupiedRanges"]) is not JsonArray ranges) return;
        long expectedEnd;
        try { expectedEnd = checked(offset.Value + width.Value); }
        catch (OverflowException) { throw new ProtocolException(path + ": scalar range overflow."); }
        var cursor = offset.Value;
        foreach (var range in ranges.OfType<JsonObject>().OrderBy(r => Number(r["startBit"], "startBit")))
        {
            var start = Number(range["startBit"], "startBit"); var length = Number(range["lengthBits"], "lengthBits");
            if (length == 0) continue;
            var end = checked(start + length);
            if (start < offset || start > cursor || end > expectedEnd)
                throw new ProtocolException(path + ": scalar occupied ranges disagree with offsetBits/bitWidth.");
            cursor = Math.Max(cursor, end);
        }
        if (cursor != expectedEnd) throw new ProtocolException(path + ": scalar occupied ranges do not cover offsetBits/bitWidth.");
    }

    private static void ValidateRelations(JsonObject observation, Dictionary<string, JsonObject> observations, Dictionary<string, JsonObject> types, bool detachedRoot = false)
    {
        var id = observation.S("id"); var context = Obj(observation["context"], "context");
        if (context.ContainsKey("hostObservationId"))
        {
            if (context.S("kind") is not ("embedded-value" or "array-element"))
                throw new ProtocolException("Only inline embedded values or array elements may have a host observation.");
            if (!observations.TryGetValue(context.S("hostObservationId"), out var host)) throw new ProtocolException("Missing host observation.");
            var members = Index(Arr(host["members"], "members"), "members");
            if (!members.TryGetValue(context.S("hostMemberId"), out var member) || member["childObservationId"]?.GetValue<string>() != id)
                throw new ProtocolException("Child/host relation is not bidirectional.");
            var hostIsArray = types[host.S("typeId")].S("kind") == "array";
            if (hostIsArray != (context.S("kind") == "array-element")) throw new ProtocolException("Array element context must correspond to an array host.");
            if (hostIsArray)
            {
                var index = Number(context["elementIndex"], "elementIndex");
                var count = host["instanceShape"] is JsonObject shape ? Number(shape["length"], "length") : Numeric(types[host.S("typeId")]["fixedCount"]);
                if (count is not null && index >= count) throw new ProtocolException("Array element index is outside the observed array.");
            }
        }
        else if (context.ContainsKey("hostMemberId")) throw new ProtocolException("hostMemberId without hostObservationId.");
        else if (!detachedRoot && context.S("kind") is "embedded-value" or "array-element") throw new ProtocolException("Inline observation requires a host placement.");
        if (context.S("kind") != "array-element" && context.ContainsKey("elementIndex")) throw new ProtocolException("elementIndex is only applicable to array elements.");
        var elementIndices = new HashSet<long>();
        foreach (var member in Index(Arr(observation["members"], "members"), "members").Values)
        {
            if (!member.ContainsKey("childObservationId")) continue;
            if (!observations.TryGetValue(member.S("childObservationId"), out var child)) throw new ProtocolException("Missing child observation.");
            var childContext = Obj(child["context"], "child.context");
            if (childContext["hostObservationId"]?.GetValue<string>() != id || childContext["hostMemberId"]?.GetValue<string>() != member.S("id") || child.S("typeId") != member.S("typeRef"))
                throw new ProtocolException("Invalid child/host placement or type.");
            if (childContext.S("kind") == "array-element" && !elementIndices.Add(Number(childContext["elementIndex"], "elementIndex")))
                throw new ProtocolException("Duplicate array element index.");
        }
    }

    private static void ValidateTypeCycles(Dictionary<string, JsonObject> types)
    {
        var visiting = new HashSet<string>(); var visited = new HashSet<string>();
        void Visit(string id, int depth = 0)
        {
            if (depth > 64) throw new ProtocolException("Inline type nesting exceeds 64.");
            if (visited.Contains(id)) return;
            if (!visiting.Add(id)) throw new ProtocolException("Impossible inline type descriptor cycle at " + id);
            var type = types[id];
            if (type.S("kind") == "array") Visit(type.S("elementTypeRef"), depth + 1);
            if (type.S("kind") == "enum") Visit(type.S("enumUnderlyingTypeRef"), depth + 1);
            visiting.Remove(id); visited.Add(id);
        }
        foreach (var id in types.Keys) Visit(id);
    }
}
