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
            ["producer"] = new JsonObject { ["id"] = "clrmd-snapshot", ["version"] = "0.1.0", ["capabilities"] = Strings("coreclr-objects", "frozen-snapshot", "logical-case-ids", "private-inherited-fields", "boxed-values", "runtime-length-objects", "calibrated-object-origin") },
            ["build"] = new JsonObject
            {
                ["buildId"] = "object-host-" + calibration["configuration"]!.GetValue<string>(), ["runId"] = runId,
                ["configuration"] = calibration["configuration"]!.GetValue<string>(),
                ["sourceRevision"] = "unknown", ["sourceDirty"] = null, ["sourceDigest"] = "unknown",
                ["artifactDigest"] = Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(hostPath))),
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
            observation["metrics"]!["arrayStrideBytes"] = Fact(stride, array.Length > 1 ? "ClrType.GetArrayElementAddress(1)-GetArrayElementAddress(0)" : "ClrType.ComponentSize");
            string elementType = Describe(type.ComponentType!, asReference: !type.ComponentType!.IsValueType, type.ComponentSize);
            for (int i = 0; i < array.Length; i++)
            {
                string memberId = i.ToString(System.Globalization.CultureInfo.InvariantCulture);
                long offset = checked((long)(type.GetArrayElementAddress(instance.Address, i) - instance.Address));
                JsonObject member = Member(memberId, elementType, i, offset, type.ComponentSize, "ClrType.GetArrayElementAddress");
                string childId = id + "/" + memberId;
                member["childObservationId"] = childId;
                var child = new JsonObject
                {
                    ["id"] = childId, ["typeId"] = elementType, ["displayName"] = type.ComponentType.Name,
                    ["status"] = "ok", ["limitations"] = new JsonArray(), ["view"] = "managed",
                    ["context"] = new JsonObject { ["kind"] = "array-element", ["hostObservationId"] = id, ["hostMemberId"] = memberId, ["elementIndex"] = i },
                    ["origin"] = new JsonObject { ["kind"] = "value-start", ["extentStartBit"] = 0 },
                    ["metrics"] = new JsonObject { ["valueSizeBytes"] = Fact(type.ComponentSize, "ClrType.ComponentSize"), ["arrayStrideBytes"] = Fact(stride, "ClrType.ComponentSize/adjacent-element-calibration"), ["alignmentBytes"] = Unknown("not-reported"), ["referenceSlotBytes"] = type.ComponentType.IsValueType ? NotApplicable("value-not-reference") : Fact(pointerBytes, "target.PointerSize") },
                    ["members"] = new JsonArray(), ["runtimeRegions"] = new JsonArray(), ["coverage"] = Coverage()
                };
                members.Add((JsonNode)member); _observations.Add((JsonNode)child);
            }
            return;
        }

        int ordinal = 0;
        foreach (ClrInstanceField field in type.Fields)
        {
            string name = field.Name ?? "field-" + ordinal;
            string memberId = name.ToLowerInvariant();
            if (type.Fields.Count(f => string.Equals(f.Name, name, StringComparison.OrdinalIgnoreCase)) > 1)
                memberId = field.ContainingType.Name + ":" + name;
            long offset = checked((long)(field.GetAddress(instance.Address) - instance.Address));
            string fieldType = Describe(field.Type, field.IsObjectReference, field.Size);
            members.Add((JsonNode)Member(memberId, fieldType, ordinal++, offset, field.Size, "ClrInstanceField.GetAddress(object,false)-ClrObject.Address"));
        }
        if (type.IsString)
        {
            int length = instance.AsString()!.Length;
            observation["instanceShape"] = new JsonObject { ["length"] = length, ["dimensions"] = new JsonArray(JsonValue.Create(length)) };
            ClrInstanceField firstChar = type.Fields.Single(f => f.ElementType == ClrElementType.Char);
            long charOffset = checked((long)(firstChar.GetAddress(instance.Address) - instance.Address));
            regions.Add((JsonNode)new JsonObject { ["role"] = "remaining-characters-and-terminator", ["ranges"] = Range((charOffset + 2) * 8, length * 16, "CoreCLR10-string-length-plus-terminator") });
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
            ["valueSizeBytes"] = NotApplicable("object-context"), ["arrayStrideBytes"] = NotApplicable("not-array-context"),
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
