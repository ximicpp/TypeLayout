using System.Security.Cryptography;
using System.Text.Json.Nodes;
using Microsoft.Diagnostics.Runtime;

namespace LayoutObserver.ClrMd;

internal sealed class ObjectSnapshotWriter(string runtimeVersion, int pointerBytes, string hostPath, JsonObject calibration)
{
    private readonly Dictionary<string, JsonObject> _types = new(StringComparer.Ordinal);
    private readonly JsonArray _observations = [];
    private string EvidenceVersion => ObjectCapture.ClrMdPackageVersion + "/CoreCLR-" + runtimeVersion;

    public JsonObject Write(IReadOnlyDictionary<string, ClrObject> cases, string runId)
    {
        foreach ((string id, ClrObject instance) in cases.OrderBy(pair => pair.Key, StringComparer.Ordinal)) AddObject(id, instance);
        var types = new JsonArray(); foreach (JsonObject type in _types.Values.OrderBy(t => t["id"]!.GetValue<string>(), StringComparer.Ordinal)) types.Add((JsonNode)type);
        return new JsonObject
        {
            ["schemaVersion"] = "0.1", ["snapshotId"] = "coreclr-objects-" + runId,
            ["producer"] = new JsonObject { ["id"] = "clrmd-snapshot", ["version"] = "0.1.1", ["capabilities"] = Strings("coreclr-objects", "frozen-snapshot", "logical-case-ids", "private-inherited-fields", "boxed-values", "runtime-length-objects", "calibrated-object-origin", "recursive-inline-values") },
            ["build"] = new JsonObject
            {
                ["buildId"] = "object-host-" + calibration["configuration"]!.GetValue<string>(), ["runId"] = runId,
                ["configuration"] = calibration["configuration"]!.GetValue<string>(),
                ["sourceRevision"] = "unknown", ["sourceDirty"] = null, ["sourceDigest"] = "unknown",
                ["artifactDigest"] = "sha256:" + Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(hostPath))),
                ["compiler"] = new JsonObject { ["name"] = "Roslyn", ["version"] = "unknown" },
                ["runtime"] = new JsonObject { ["name"] = "CoreCLR", ["version"] = runtimeVersion },
                ["target"] = new JsonObject { ["os"] = OperatingSystem.IsWindows() ? "windows" : OperatingSystem.IsLinux() ? "linux" : "macos", ["architecture"] = calibration["architecture"]!.GetValue<string>(), ["abi"] = ".NET managed", ["pointerBits"] = pointerBytes * 8, ["bitsPerByte"] = 8, ["endian"] = BitConverter.IsLittleEndian ? "little" : "big" },
                ["flags"] = Strings("frozen-snapshot", "forced-compacting-gc-before-barrier"),
                ["dependencies"] = new JsonObject { ["Microsoft.Diagnostics.Runtime"] = ObjectCapture.ClrMdPackageVersion, ["DAC"] = "matching-CoreCLR-" + runtimeVersion }
            },
            ["typeDescriptors"] = types, ["observations"] = _observations,
            ["diagnostics"] = new JsonArray(),
            ["limitations"] = Strings("source-and-compiler-identity-require-orchestrator-enrichment", "runtimeReportedObjectBytes-is-not-allocation-stride-or-retained-size", "no-reference-target-recursion", "origin-adapter-calibrated-for-CoreCLR-10")
        };
    }

    private void AddObject(string id, ClrObject instance)
    {
        ClrType type = instance.Type!;
        string typeId = Describe(type, asReference: false);
        var members = new JsonArray();
        var regions = new JsonArray
        {
            new JsonObject { ["role"] = "object-header", ["ranges"] = Range(-pointerBytes * 8, pointerBytes * 8, "CoreCLR10-object.h-ObjHeader") },
            new JsonObject { ["role"] = "method-table-reference", ["ranges"] = Range(0, pointerBytes * 8, "CoreCLR10-object.h-MethodTable") }
        };
        var observation = BaseObservation(id, typeId, type.Name ?? typeId, instance.IsBoxedValue ? "boxed-value" : "heap-object", instance.Size);
        observation["members"] = members; observation["runtimeRegions"] = regions;
        _observations.Add((JsonNode)observation);
        if (instance.IsArray)
        {
            ClrArray array = instance.AsArray();
            observation["instanceShape"] = new JsonObject { ["length"] = array.Length, ["dimensions"] = new JsonArray(JsonValue.Create(array.Length)) };
            long dataOffset = checked((long)(type.GetArrayElementAddress(instance.Address, 0) - instance.Address));
            regions.Add((JsonNode)new JsonObject { ["role"] = "array-metadata", ["ranges"] = Range(pointerBytes * 8, (dataOffset - pointerBytes) * 8, "ClrType.GetArrayElementAddress/CoreCLR10-array-header") });
            long stride = array.Length > 1 ? checked((long)(type.GetArrayElementAddress(instance.Address, 1) - type.GetArrayElementAddress(instance.Address, 0))) : type.ComponentSize;
            string elementType = Describe(type.ComponentType!, asReference: !type.ComponentType!.IsValueType, type.ComponentSize);
            for (int i = 0; i < array.Length; i++)
            {
                string memberId = i.ToString(System.Globalization.CultureInfo.InvariantCulture);
                long offset = checked((long)(type.GetArrayElementAddress(instance.Address, i) - instance.Address));
                JsonObject member = Member(memberId, elementType, i, offset, type.ComponentSize, "ClrType.GetArrayElementAddress");
                string childId = id + "/" + memberId;
                member["childObservationId"] = childId;
                members.Add((JsonNode)member);
                AddInlineValue(childId, elementType, type.ComponentType, type.GetArrayElementAddress(instance.Address, i), type.ComponentSize,
                    id, memberId, 1, i, stride);
            }
            return;
        }

        AddFields(observation, type, instance.Address, interior: false, depth: 0);
        if (type.IsString)
        {
            int length = instance.AsString()!.Length;
            observation["instanceShape"] = new JsonObject { ["length"] = length, ["dimensions"] = new JsonArray(JsonValue.Create(length)) };
            ClrInstanceField firstChar = type.Fields.Single(f => f.ElementType == ClrElementType.Char);
            long charOffset = checked((long)(firstChar.GetAddress(instance.Address) - instance.Address));
            regions.Add((JsonNode)new JsonObject { ["role"] = "remaining-characters-and-terminator", ["ranges"] = Range((charOffset + 2) * 8, length * 16, "CoreCLR10-string-length-plus-terminator") });
        }
    }

    private void AddInlineValue(string id, string typeId, ClrType? type, ulong address, int size, string hostId, string memberId,
        int depth, int? elementIndex = null, long? stride = null)
    {
        if (depth > 64) throw new CaptureException("inline-depth-limit", "Inline layout nesting exceeds the protocol's 64-level limit.");
        var context = new JsonObject { ["kind"] = elementIndex.HasValue ? "array-element" : "embedded-value", ["hostObservationId"] = hostId, ["hostMemberId"] = memberId };
        if (elementIndex.HasValue) context["elementIndex"] = elementIndex.Value;
        var observation = new JsonObject
        {
            ["id"] = id, ["typeId"] = typeId, ["displayName"] = type?.Name ?? "unresolved",
            ["status"] = "ok", ["limitations"] = new JsonArray(), ["view"] = "managed", ["context"] = context,
            ["origin"] = new JsonObject { ["kind"] = "value-start", ["extentStartBit"] = 0 },
            ["metrics"] = new JsonObject
            {
                ["valueSizeBytes"] = Fact(size, elementIndex.HasValue ? "ClrType.ComponentSize" : "ClrField.Size"),
                ["arrayStrideBytes"] = stride.HasValue ? Fact(stride.Value, "ClrType.ComponentSize/adjacent-element-calibration") : Unknown("embedded-value-array-stride-not-observed"),
                ["alignmentBytes"] = Unknown("not-reported"),
                ["referenceSlotBytes"] = _types[typeId]["kind"]!.GetValue<string>() == "reference" ? Fact(pointerBytes, "target.PointerSize") : NotApplicable("value-not-reference")
            },
            ["members"] = new JsonArray(), ["runtimeRegions"] = new JsonArray(), ["coverage"] = Coverage()
        };
        _observations.Add(observation);
        if (type?.IsValueType == true && !type.IsPrimitive) AddFields(observation, type, address, interior: true, depth);
    }

    private void AddFields(JsonObject observation, ClrType type, ulong address, bool interior, int depth)
    {
        var members = observation["members"]!.AsArray();
        int ordinal = 0;
        foreach (ClrInstanceField field in type.Fields)
        {
            string name = field.Name ?? "field-" + ordinal;
            string memberId = name.ToLowerInvariant();
            if (type.Fields.Count(f => string.Equals(f.Name, name, StringComparison.OrdinalIgnoreCase)) > 1)
                memberId = field.ContainingType.Name + ":" + name;
            ulong fieldAddress = field.GetAddress(address, interior);
            long offset = checked((long)(fieldAddress - address));
            string fieldType = Describe(field.Type, field.IsObjectReference, field.Size);
            string method = interior ? "ClrInstanceField.GetAddress(value,true)-valueAddress" : "ClrInstanceField.GetAddress(object,false)-ClrObject.Address";
            var member = Member(memberId, fieldType, ordinal++, offset, field.Size, method);
            members.Add(member);
            if (!field.IsObjectReference && field.Type?.IsValueType == true && !field.Type.IsPrimitive)
            {
                string hostId = observation["id"]!.GetValue<string>();
                string childId = hostId + "/" + memberId;
                member["childObservationId"] = childId;
                AddInlineValue(childId, fieldType, field.Type, fieldAddress, field.Size, hostId, memberId, depth + 1);
            }
        }
    }

    private JsonObject BaseObservation(string id, string typeId, string displayName, string context, ulong size) => new()
    {
        ["id"] = id, ["typeId"] = typeId, ["displayName"] = displayName, ["status"] = "ok",
        ["limitations"] = Strings("reported-object-extent-excludes-additional-allocation-rounding"), ["view"] = "managed",
        ["context"] = new JsonObject { ["kind"] = context },
        ["origin"] = new JsonObject { ["kind"] = "object-reference", ["extentStartBit"] = -pointerBytes * 8,
            ["conversionEvidence"] = Evidence("CoreCLR10-object.h; boxed/private-byref/variable-length-calibration") },
        ["metrics"] = new JsonObject
        {
            ["runtimeReportedObjectBytes"] = Fact(checked((long)size), "ClrObject.Size"),
            ["referenceSlotBytes"] = Fact(pointerBytes, "target.PointerSize"),
            ["valueSizeBytes"] = NotApplicable("object-context"), ["arrayStrideBytes"] = NotApplicable("heap-object-has-no-inline-value-stride"),
            ["alignmentBytes"] = Unknown("not-reported-by-object-api")
        },
        ["coverage"] = Coverage()
    };

    private JsonObject Member(string name, string typeId, int ordinal, long offset, int size, string method) => new()
    {
        ["id"] = name, ["displayName"] = name, ["declarationOrder"] = ordinal, ["role"] = "field", ["typeRef"] = typeId,
        ["offsetBits"] = Fact(offset * 8, method), ["bitWidth"] = Fact(size * 8, "ClrField.Size/ClrType.ComponentSize"),
        ["declaredTypeSizeBits"] = Fact(size * 8, "ClrField.Size/ClrType.ComponentSize"),
        ["occupiedRanges"] = Range(offset * 8, size * 8, method + "+field-size")
    };

    private string Describe(ClrType? type, bool asReference, int? sizeBytes = null)
    {
        string name = type?.Name ?? "unresolved";
        string id = asReference ? "gc-ref:" + name : name;
        if (_types.ContainsKey(id)) return id;
        var descriptor = new JsonObject { ["id"] = id, ["displayName"] = name, ["kind"] = "record" };
        _types.Add(id, descriptor);
        if (asReference)
        {
            descriptor["kind"] = "reference"; descriptor["referenceKind"] = "GC-reference";
            descriptor["targetTypeRef"] = Describe(type, false);
            descriptor["representation"] = new JsonObject { ["widthBits"] = Fact(pointerBytes * 8, "target.PointerSize") };
        }
        else if (type is null) { descriptor["kind"] = "opaque"; descriptor["opaqueTag"] = "unresolved-clr-type"; }
        else if (type.IsArray)
        {
            descriptor["kind"] = "array"; descriptor["elementTypeRef"] = Describe(type.ComponentType, type.ComponentType?.IsValueType == false, type.ComponentSize);
            descriptor["fixedCount"] = Unknown("runtime-length-array-type");
        }
        else if (type.IsPrimitive)
        {
            descriptor["kind"] = "scalar";
            ClrElementType element = type.ElementType;
            int? width = sizeBytes * 8;
            bool floating = element is ClrElementType.Float or ClrElementType.Double;
            bool character = element == ClrElementType.Char; bool boolean = element == ClrElementType.Boolean;
            bool unsigned = element is ClrElementType.UInt8 or ClrElementType.UInt16 or ClrElementType.UInt32 or ClrElementType.UInt64 or ClrElementType.NativeUInt;
            descriptor["representation"] = new JsonObject
            {
                ["widthBits"] = width.HasValue ? Fact(width.Value, "ClrField.Size/ClrType.ComponentSize") : Unknown("primitive-width-not-observed"),
                ["category"] = Fact(floating ? "float" : character ? "character" : boolean ? "boolean" : "integer", "ClrElementType"),
                ["signedness"] = Fact(floating || character || boolean ? "not-applicable" : unsigned ? "unsigned" : "signed", "ClrElementType"),
                ["encoding"] = Fact(floating ? "ieee754" : character ? "utf16-code-unit" : boolean ? "bool-0-or-1" : "binary-integer", "CoreCLR10-primitive-contract"),
                ["floatingFormat"] = floating ? Fact(width == 32 ? "binary32" : "binary64", "ClrElementType") : NotApplicable("not-floating")
            };
        }
        return id;
    }

    private JsonObject Fact(long value, string method) => new() { ["state"] = "known", ["value"] = value, ["evidence"] = Evidence(method) };
    private JsonObject Fact(string value, string method) => new() { ["state"] = "known", ["value"] = value, ["evidence"] = Evidence(method) };
    private JsonObject Range(long start, long length, string method) => new() { ["state"] = "known", ["value"] = new JsonArray(new JsonObject { ["startBit"] = start, ["lengthBits"] = length }), ["evidence"] = Evidence(method) };
    private JsonObject Evidence(string method) => new() { ["kind"] = "runtime", ["method"] = method, ["version"] = EvidenceVersion, ["inputs"] = new JsonArray() };
    private static JsonObject Unknown(string reason) => new() { ["state"] = "unknown", ["reason"] = reason };
    private static JsonObject NotApplicable(string reason) => new() { ["state"] = "not-applicable", ["reason"] = reason };
    private static JsonObject Coverage() => new() { ["fieldEnumeration"] = "complete", ["extent"] = "complete", ["occupiedRanges"] = "complete", ["hiddenRegions"] = "complete" };
    private static JsonArray Strings(params string[] values) { var result = new JsonArray(); foreach (string value in values) result.Add((JsonNode?)JsonValue.Create(value)); return result; }
}
