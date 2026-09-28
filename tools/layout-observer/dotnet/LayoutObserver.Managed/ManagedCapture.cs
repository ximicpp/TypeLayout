using System.Runtime.InteropServices;
using System.Text.Json.Nodes;

namespace LayoutObserver.Managed;

/// <summary>Converts static probe measurements to protocol 0.1, preserving unknown facts.</summary>
public static class ManagedCapture
{
    private static readonly string RuntimeVersion = Environment.Version.ToString();

    public static JsonObject Capture(string? runId = null, string? requestedConfiguration = null) =>
        CaptureRegistered(GeneratedProbeRegistry.Capture(), GeneratedProbeRegistry.CompilerVersion,
            GeneratedProbeRegistry.BuildConfiguration, runId, requestedConfiguration);

    /// <summary>Converts the caller's generated registry without including this assembly's built-in fixtures.</summary>
    public static JsonObject CaptureRegistered(IReadOnlyList<ProbeResult> results, string compilerVersion,
        string buildConfiguration, string? runId = null, string? requestedConfiguration = null)
    {
#if !LAYOUT_NATIVEAOT
        if (Type.GetType("Mono.Runtime") is not null)
            throw new PlatformNotSupportedException("This collector is validated for CoreCLR and NativeAOT; Mono requires a runtime adapter.");
#endif
        ArgumentNullException.ThrowIfNull(results);
        ArgumentException.ThrowIfNullOrWhiteSpace(compilerVersion);
        ArgumentException.ThrowIfNullOrWhiteSpace(buildConfiguration);
        if (results.Count == 0 || results.Any(result => string.IsNullOrWhiteSpace(result.TypeId)) ||
            results.Select(result => result.TypeId).Distinct(StringComparer.Ordinal).Count() != results.Count)
            throw new ArgumentException("Registered probes require at least one result and unique nonempty observation IDs.", nameof(results));
        if (requestedConfiguration is not null && requestedConfiguration != buildConfiguration)
            throw new ArgumentException($"Requested configuration '{requestedConfiguration}' does not match actual '{buildConfiguration}'.");
        return new SnapshotWriter(compilerVersion).Capture(results, buildConfiguration, runId);
    }

    private sealed class SnapshotWriter(string compilerVersion)
    {
        public JsonObject Capture(IReadOnlyList<ProbeResult> results, string buildConfiguration, string? runId)
        {
            runId ??= Guid.NewGuid().ToString("N");
            var descriptors = new Dictionary<string, JsonObject>(StringComparer.Ordinal);
            var observations = new JsonArray();
            var diagnostics = new JsonArray();
            var byType = results.Where(r => r.Limitation is null).GroupBy(r => r.TypeName).ToDictionary(g => g.Key, g => g.First());
            foreach (ProbeResult result in results)
            {
                AddType(result.Type ?? new(result.TypeName, result.TypeName, result.Limitation is null ? "record" : "opaque", result.SizeBytes ?? 0), descriptors);
                AddObservation(result, result.TypeId, null, null, byType, descriptors, observations, diagnostics, new HashSet<string>());
            }

            var types = new JsonArray();
            foreach (JsonObject descriptor in descriptors.Values.OrderBy(d => d["id"]!.GetValue<string>(), StringComparer.Ordinal)) types.Add((JsonNode)descriptor);
            string architecture = RuntimeInformation.ProcessArchitecture.ToString().ToLowerInvariant();
            string os = OperatingSystem.IsWindows() ? "windows" : OperatingSystem.IsLinux() ? "linux" : OperatingSystem.IsMacOS() ? "macos" : "unknown";
            string runtime =
    #if LAYOUT_NATIVEAOT
                "NativeAOT";
    #else
                "CoreCLR";
    #endif
            return new JsonObject
            {
                ["schemaVersion"] = "0.1", ["snapshotId"] = "managed-" + runId,
                ["producer"] = new JsonObject { ["id"] = "managed-static", ["version"] = "0.1.0", ["capabilities"] = Strings("managed-values", "typed-byref", "array-stride", "inline-array", "partial-private-fields", "closed-generics") },
                ["build"] = new JsonObject
                {
                    ["buildId"] = "managed-" + buildConfiguration, ["runId"] = runId,
                    ["configuration"] = buildConfiguration,
                    ["sourceRevision"] = "unknown", ["sourceDirty"] = null, ["sourceDigest"] = "unknown", ["artifactDigest"] = "unknown",
                    ["compiler"] = new JsonObject { ["name"] = "Roslyn", ["version"] = compilerVersion },
                    ["runtime"] = new JsonObject { ["name"] = runtime, ["version"] = RuntimeVersion, ["description"] = RuntimeInformation.FrameworkDescription },
                    ["target"] = new JsonObject { ["os"] = os, ["architecture"] = architecture, ["abi"] = ".NET managed", ["pointerBits"] = IntPtr.Size * 8, ["bitsPerByte"] = 8, ["endian"] = BitConverter.IsLittleEndian ? "little" : "big" },
                    ["flags"] = Strings("static-generated-probes"), ["dependencies"] = new JsonObject { ["System.Private.CoreLib"] = RuntimeVersion }
                },
                ["typeDescriptors"] = types, ["observations"] = observations, ["diagnostics"] = diagnostics,
                ["limitations"] = Strings("source-and-artifact-identity-require-orchestrator-enrichment", "managed-type-alignment-not-reported", "static-probes-do-not-measure-class-object-size")
            };
        }

        private void AddObservation(ProbeResult result, string id, string? parentId, string? parentMember,
            IReadOnlyDictionary<string, ProbeResult> byType, Dictionary<string, JsonObject> descriptors,
            JsonArray observations, JsonArray diagnostics, HashSet<string> ancestry, int? elementIndex = null)
        {
            bool unsupported = result.Limitation is not null;
            var context = new JsonObject { ["kind"] = elementIndex.HasValue ? "array-element" : parentId is null ? "complete-value" : "embedded-value" };
            if (parentId is not null) { context["hostObservationId"] = parentId; context["hostMemberId"] = parentMember; }
            if (elementIndex.HasValue) context["elementIndex"] = elementIndex.Value;
            var members = new JsonArray();
            var limitations = new JsonArray();
            if (result.Limitation is not null) limitations.Add((JsonNode?)JsonValue.Create(result.Limitation));
            var observation = new JsonObject
            {
                ["id"] = id, ["typeId"] = result.Type?.Id ?? result.TypeName, ["displayName"] = result.TypeName,
                ["status"] = unsupported ? "unsupported" : "ok", ["limitations"] = limitations,
                ["view"] = "managed", ["context"] = context,
                ["origin"] = new JsonObject { ["kind"] = "value-start", ["extentStartBit"] = 0 },
                ["metrics"] = new JsonObject
                {
                    ["valueSizeBytes"] = result.SizeBytes is int size ? Fact(size, "Unsafe.SizeOf") : Unknown(result.Limitation!),
                    ["standaloneSizeBytes"] = result.SizeBytes is int standalone ? Fact(standalone, "Unsafe.SizeOf") : Unknown(result.Limitation!),
                    ["referenceSlotBytes"] = result.Type?.Kind == "reference" ? Fact(IntPtr.Size, "Unsafe.SizeOf(reference)") : NotApplicable("value-not-reference"),
                    ["arrayStrideBytes"] = result.ArrayStrideBytes is long stride ? Fact(stride, "Unsafe.ByteOffset(array[0],array[1])") : Unknown(result.Limitation!),
                    ["runtimeReportedObjectBytes"] = NotApplicable("not-object-context"),
                    ["alignmentBytes"] = Unknown("managed-type-alignment-not-reported")
                },
                ["members"] = members, ["runtimeRegions"] = new JsonArray(),
                ["coverage"] = new JsonObject
                {
                    ["fieldEnumeration"] = unsupported ? "unknown" : "complete",
                    ["extent"] = unsupported ? "unknown" : "complete",
                    ["occupiedRanges"] = unsupported ? "unknown" : result.IsComplete ? "complete" : "partial",
                    ["hiddenRegions"] = unsupported ? "unknown" : "not-applicable"
                }
            };
            observations.Add((JsonNode)observation);
            if (unsupported) diagnostics.Add((JsonNode)new JsonObject { ["code"] = "unsupported-type", ["message"] = result.Limitation, ["observationId"] = id });
            var nextAncestry = new HashSet<string>(ancestry, StringComparer.Ordinal) { result.TypeName };
            int ordinal = 0;
            foreach (ProbeField field in result.Fields)
            {
                AddType(field.Type, descriptors);
                string memberId = result.Fields.Count(f => string.Equals(f.Name, field.Name, StringComparison.OrdinalIgnoreCase)) > 1 ? field.Name : field.Name.ToLowerInvariant();
                long? offset = field.OffsetBytes * 8;
                long? width = (long?)field.SizeBytes * 8;
                var member = new JsonObject
                {
                    ["id"] = memberId, ["displayName"] = field.Name, ["declarationOrder"] = ordinal++, ["role"] = "field", ["typeRef"] = field.Type.Id,
                    ["offsetBits"] = offset.HasValue ? Fact(offset.Value, "Unsafe.ByteOffset(value,field)") : Unknown(field.Limitation!),
                    ["bitWidth"] = width.HasValue ? Fact(width.Value, "Unsafe.SizeOf(field)") : Unknown(field.Limitation!),
                    ["declaredTypeSizeBits"] = width.HasValue ? Fact(width.Value, "Unsafe.SizeOf(field)") : Unknown(field.Limitation!),
                    ["occupiedRanges"] = offset.HasValue && width.HasValue ? Range(offset.Value, width.Value, id + "/members/" + memberId) : Unknown(field.Limitation!)
                };
                if (offset.HasValue && width.HasValue && result.Fields.Any(other => !ReferenceEquals(other, field) && other.OffsetBytes.HasValue && other.SizeBytes.HasValue &&
                    other.OffsetBytes.Value < field.OffsetBytes!.Value + field.SizeBytes!.Value && field.OffsetBytes.Value < other.OffsetBytes.Value + other.SizeBytes.Value))
                    member["overlapGroup"] = "overlap:" + id;
                ProbeResult? child = field.Child;
                if (field.Type.Kind == "record" && byType.TryGetValue(field.Type.Id, out ProbeResult? registeredChild)) child = registeredChild;
                if (child is not null && !nextAncestry.Contains(child.TypeName))
                {
                    string childId = id + "/" + memberId;
                    member["childObservationId"] = childId;
                    AddType(child.Type ?? new(child.TypeName, child.TypeName, "record", child.SizeBytes ?? 0), descriptors);
                    AddObservation(child, childId, id, memberId, byType, descriptors, observations, diagnostics, nextAncestry,
                        result.Type?.Kind == "array" ? int.Parse(field.Name, System.Globalization.CultureInfo.InvariantCulture) : null);
                }
                if (field.Limitation is not null)
                {
                    limitations.Add((JsonNode?)JsonValue.Create(field.Name + ": " + field.Limitation));
                    diagnostics.Add((JsonNode)new JsonObject { ["code"] = "field-not-observed", ["message"] = field.Name + ": " + field.Limitation, ["observationId"] = id });
                }
                members.Add((JsonNode)member);
            }
        }

        private void AddType(ProbeType type, Dictionary<string, JsonObject> descriptors)
        {
            if (descriptors.ContainsKey(type.Id)) return;
            var descriptor = new JsonObject { ["id"] = type.Id, ["kind"] = type.Kind, ["displayName"] = type.DisplayName };
            descriptors.Add(type.Id, descriptor);
            if (type.Kind == "scalar") descriptor["representation"] = ScalarRepresentation(type);
            else if (type.Kind == "enum") { AddType(type.Element!, descriptors); descriptor["enumUnderlyingTypeRef"] = type.Element!.Id; }
            else if (type.Kind == "array")
            {
                AddType(type.Element!, descriptors); descriptor["elementTypeRef"] = type.Element!.Id;
                descriptor["fixedCount"] = Fact(type.FixedCount!.Value, "InlineArrayAttribute", "compiler");
            }
            else if (type.Kind == "reference")
            {
                AddType(type.Element!, descriptors); descriptor["referenceKind"] = "GC-reference"; descriptor["targetTypeRef"] = type.Element!.Id;
                descriptor["representation"] = new JsonObject { ["widthBits"] = Fact(type.SizeBytes * 8, "Unsafe.SizeOf(reference)") };
            }
            else if (type.Kind == "opaque") descriptor["opaqueTag"] = "not-observed:" + type.Id;
        }

        private JsonObject ScalarRepresentation(ProbeType type)
        {
            string code = type.ScalarCode!;
            bool floating = code is "f32" or "f64";
            bool boolean = code == "bool";
            bool character = code == "char16";
            string category = floating ? "float" : boolean ? "boolean" : character ? "character" : "integer";
            string sign = floating || boolean || character ? "not-applicable" : code.StartsWith('u') || code == "nuint" ? "unsigned" : "signed";
            return new JsonObject
            {
                ["widthBits"] = Fact(type.SizeBytes * 8, "Unsafe.SizeOf(primitive)"),
                ["category"] = Fact(category, "C# primitive type", "compiler"),
                ["signedness"] = Fact(sign, "C# primitive type", "compiler"),
                ["encoding"] = Fact(floating ? "ieee754" : boolean ? "bool-0-or-1" : character ? "utf16-code-unit" : "binary-integer", "C# primitive type", "compiler"),
                ["floatingFormat"] = floating ? Fact(code == "f32" ? "binary32" : "binary64", "C# primitive type", "compiler") : NotApplicable("not-floating")
            };
        }

        private JsonObject Fact(long value, string method, string kind = "runtime") => new() { ["state"] = "known", ["value"] = value, ["evidence"] = Evidence(method, kind) };
        private JsonObject Fact(string value, string method, string kind = "runtime") => new() { ["state"] = "known", ["value"] = value, ["evidence"] = Evidence(method, kind) };
        private JsonObject Evidence(string method, string kind) => new() { ["kind"] = kind, ["method"] = method, ["version"] = kind == "compiler" ? compilerVersion : RuntimeVersion, ["inputs"] = new JsonArray() };
        private JsonObject Range(long start, long length, string path)
        {
            JsonObject evidence = Evidence("member-offset-plus-width", "derived");
            evidence["inputs"] = Strings(path + "/offsetBits", path + "/bitWidth");
            return new() { ["state"] = "known", ["value"] = new JsonArray(new JsonObject { ["startBit"] = start, ["lengthBits"] = length }), ["evidence"] = evidence };
        }
        private static JsonObject Unknown(string reason) => new() { ["state"] = "unknown", ["reason"] = reason };
        private static JsonObject NotApplicable(string reason) => new() { ["state"] = "not-applicable", ["reason"] = reason };
        private static JsonArray Strings(params string[] values) { var array = new JsonArray(); foreach (string value in values) array.Add((JsonNode?)JsonValue.Create(value)); return array; }
    }
}
