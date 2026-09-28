using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text.Json.Nodes;
using LayoutObserver.Core;

namespace LayoutObserver.Marshalling;

public static class MarshaledCapture
{
    public const string Configuration =
#if DEBUG
        "Debug";
#else
        "Release";
#endif
    private static string Version => Environment.Version.ToString();
    private static JsonObject Known(JsonNode value, string method) => new()
    {
        ["state"] = "known", ["value"] = value,
        ["evidence"] = new JsonObject { ["kind"] = "runtime", ["method"] = method, ["version"] = Version, ["inputs"] = new JsonArray() }
    };
    private static JsonObject K(long value, string method) => Known(JsonValue.Create(value)!, method);
    private static JsonObject K(string value, string method) => Known(JsonValue.Create(value)!, method);
    private static JsonObject U(string reason) => new() { ["state"] = "unknown", ["reason"] = reason };
    private static JsonObject NA(string reason) => new() { ["state"] = "not-applicable", ["reason"] = reason };
    private static JsonObject Scalar(string id, int width, string category, string signedness, string encoding) => new()
    {
        ["id"] = id, ["displayName"] = id, ["kind"] = "scalar",
        ["representation"] = new JsonObject
        {
            ["widthBits"] = K(width, "explicit-runtime-marshalling-profile"), ["category"] = K(category, "profile-storage-type"),
            ["signedness"] = K(signedness, "profile-storage-type"), ["encoding"] = K(encoding, "profile-storage-type"),
            ["floatingFormat"] = NA("not-floating")
        }
    };

    /// <summary>Collect only explicitly adapted runtime-marshalling profiles; never substitutes for managed probes.</summary>
    public static JsonObject Capture(string? requestedConfiguration = null)
    {
        if (!RuntimeFeature.IsDynamicCodeSupported || Type.GetType("Mono.Runtime") is not null || Environment.Version.Major != 10)
            throw new ProtocolException("This runtime-marshalling adapter requires CoreCLR 10; NativeAOT/Mono need separate marshalling adapters.");
        if (requestedConfiguration is not null && requestedConfiguration != Configuration) throw new ProtocolException("Requested configuration does not match marshalling collector.");
        var observations = new JsonArray();
        observations.Add(Observe<DefaultBoolChar>("marshal-default", "WinBOOL+i16char", ["enabled", "letter", "count"], ["i32", "char16", "i32"], [4, 2, 4],
            new JsonObject { ["CharSet"] = "Unicode", ["bool"] = "UnmanagedType.Bool (default)", ["Pack"] = "default" }));
        observations.Add(Observe<ByteBoolChar>("marshal-byte", "U1bool+i16char", ["enabled", "letter", "count"], ["u8", "char16", "i32"], [1, 2, 4],
            new JsonObject { ["CharSet"] = "Unicode", ["bool"] = "UnmanagedType.U1", ["Pack"] = "default" }));
        observations.Add(Observe<InlineInts>("marshal-array", "ByValArray-I4x3", ["values", "code"], ["i32x3", "i16"], [12, 2],
            new JsonObject { ["values"] = "ByValArray", ["SizeConst"] = 3, ["ArraySubType"] = "I4", ["Pack"] = "default" }));
        var types = new JsonArray(Scalar("i32", 32, "integer", "signed", "binary-integer"), Scalar("i16", 16, "integer", "signed", "binary-integer"),
            Scalar("u8", 8, "integer", "unsigned", "binary-integer"), Scalar("char16", 16, "character", "not-applicable", "utf16-code-unit"),
            new JsonObject { ["id"] = "i32x3", ["displayName"] = "inline int32[3]", ["kind"] = "array", ["elementTypeRef"] = "i32", ["fixedCount"] = K(3, "MarshalAs.SizeConst") });
        foreach (var observation in observations.OfType<JsonObject>()) types.Add(new JsonObject { ["id"] = observation["typeId"]!.GetValue<string>(), ["displayName"] = observation["displayName"]!.GetValue<string>(), ["kind"] = "record" });
        AddInlineArray(observations);
        // An unsupported custom marshaler must remain a requested, diagnosable case.
        types.Add(new JsonObject { ["id"] = "custom-marshaler", ["displayName"] = "Custom marshaller", ["kind"] = "opaque", ["opaqueTag"] = "adapter-required" });
        observations.Add(new JsonObject
        {
            ["id"] = "marshal-custom", ["typeId"] = "custom-marshaler", ["displayName"] = "Custom marshaller", ["status"] = "unsupported",
            ["limitations"] = new JsonArray("custom-marshaler-needs-an-explicit-adapter-and-instance-contract"), ["view"] = "marshaled",
            ["context"] = new JsonObject { ["kind"] = "complete-value" }, ["origin"] = new JsonObject { ["kind"] = "value-start", ["extentStartBit"] = 0 },
            ["metrics"] = new JsonObject { ["valueSizeBytes"] = U("adapter-required") }, ["members"] = new JsonArray(), ["runtimeRegions"] = new JsonArray(),
            ["coverage"] = new JsonObject { ["fieldEnumeration"] = "unknown", ["extent"] = "unknown", ["occupiedRanges"] = "unknown", ["hiddenRegions"] = "unknown" },
            ["marshallingProfile"] = new JsonObject { ["id"] = "custom", ["mechanism"] = "custom-marshalling", ["configuration"] = new JsonObject() }
        });
        var runId = Guid.NewGuid().ToString("N");
        var snapshot = new JsonObject
        {
            ["schemaVersion"] = "0.1", ["snapshotId"] = "marshaled-" + runId,
            ["producer"] = new JsonObject { ["id"] = "runtime-marshalling", ["version"] = "0.1.1", ["capabilities"] = new JsonArray("marshaled-values", "runtime-marshalling-profiles", "byval-array") },
            ["build"] = new JsonObject
            {
                ["buildId"] = "marshaling-" + Configuration, ["runId"] = runId, ["configuration"] = Configuration, ["languages"] = new JsonArray("csharp"),
                ["sourceRevision"] = "unknown", ["sourceDirty"] = null, ["sourceDigest"] = "unknown", ["artifactDigest"] = "unknown",
                ["compiler"] = new JsonObject { ["name"] = "Roslyn", ["version"] = "unknown" },
                ["runtime"] = new JsonObject { ["name"] = "CoreCLR", ["version"] = Version },
                ["target"] = new JsonObject { ["os"] = OperatingSystem.IsWindows() ? "windows" : OperatingSystem.IsLinux() ? "linux" : "macos", ["architecture"] = RuntimeInformation.ProcessArchitecture.ToString().ToLowerInvariant(), ["abi"] = "runtime-marshalling", ["pointerBits"] = IntPtr.Size * 8, ["bitsPerByte"] = 8, ["endian"] = BitConverter.IsLittleEndian ? "little" : "big" },
                ["flags"] = new JsonArray("runtime-marshalling-enabled"), ["dependencies"] = new JsonObject { ["System.Private.CoreLib"] = Version }
            },
            ["typeDescriptors"] = types, ["observations"] = observations, ["diagnostics"] = new JsonArray(),
            ["limitations"] = new JsonArray("fixed-explicit-runtime-marshalling-profiles-only", "no-general-ABI-guarantee", "managed-representation-requires-separate-probe", "alignment-not-reported", "source-and-compiler-identity-require-orchestrator-enrichment")
        };
        SnapshotValidator.Validate(snapshot); return snapshot;
    }

    private static void AddInlineArray(JsonArray observations)
    {
        var parent = observations[2]!.AsObject();
        var array = parent.DeepClone().AsObject();
        array["id"] = "marshal-array/values"; array["typeId"] = "i32x3"; array["displayName"] = "ByValArray int32[3]";
        array["context"] = new JsonObject { ["kind"] = "embedded-value", ["hostObservationId"] = "marshal-array", ["hostMemberId"] = "values" };
        var count = parent["marshallingProfile"]!["configuration"]!["SizeConst"]!.GetValue<int>();
        var elementBytes = Marshal.SizeOf<int>();
        // This profile defines one contiguous I4 buffer. The array value's stride
        // spans the entire buffer; an element observation has its own I4 stride.
        var wholeBuffer = K(checked(count * elementBytes), "ByValArray.SizeConst * Marshal.SizeOf<int>() for contiguous I4 buffer");
        wholeBuffer["evidence"]!["kind"] = "derived";
        wholeBuffer["evidence"]!["inputs"] = new JsonArray("typeDescriptors/i32x3/fixedCount", "typeDescriptors/i32/representation/widthBits");
        array["metrics"]!["valueSizeBytes"] = wholeBuffer;
        array["metrics"]!["arrayStrideBytes"] = wholeBuffer.DeepClone();
        var elements = new JsonArray(); array["members"] = elements;
        parent["members"]![0]!["childObservationId"] = "marshal-array/values";
        observations.Add(array);
        for (var i = 0; i < count; i++)
        {
            var id = i.ToString(System.Globalization.CultureInfo.InvariantCulture); var childId = "marshal-array/values/" + id;
            elements.Add(new JsonObject
            {
                ["id"] = id, ["displayName"] = id, ["declarationOrder"] = i, ["role"] = "field", ["typeRef"] = "i32", ["offsetBits"] = K(i * 32, "ByValArray.contiguous-I4-elements"),
                ["bitWidth"] = K(32, "UnmanagedType.I4"), ["declaredTypeSizeBits"] = K(32, "UnmanagedType.I4"),
                ["occupiedRanges"] = Known(new JsonArray(new JsonObject { ["startBit"] = i * 32, ["lengthBits"] = 32 }), "ByValArray.contiguous-I4-elements"), ["childObservationId"] = childId
            });
            var child = array.DeepClone().AsObject(); child["id"] = childId; child["typeId"] = "i32"; child["displayName"] = "int32";
            child["context"] = new JsonObject { ["kind"] = "array-element", ["hostObservationId"] = "marshal-array/values", ["hostMemberId"] = id, ["elementIndex"] = i };
            child["metrics"]!["valueSizeBytes"] = K(elementBytes, "Marshal.SizeOf<int>() for UnmanagedType.I4");
            child["metrics"]!["arrayStrideBytes"] = K(elementBytes, "ByValArray contiguous I4 element stride: Marshal.SizeOf<int>()");
            child["members"] = new JsonArray(); observations.Add(child);
        }
    }

    private static JsonObject Observe<T>(string id, string profileId, string[] names, string[] types, int[] widths, JsonObject configuration) where T : struct
    {
        var members = new JsonArray();
        for (var index = 0; index < names.Length; index++)
        {
            var offset = checked(Marshal.OffsetOf<T>(names[index]).ToInt64() * 8); var width = widths[index] * 8;
            members.Add(new JsonObject
            {
                ["id"] = names[index], ["displayName"] = names[index], ["declarationOrder"] = index, ["role"] = "field", ["typeRef"] = types[index],
                ["offsetBits"] = K(offset, "Marshal.OffsetOf<T>"), ["bitWidth"] = K(width, "explicit-field-marshalling-contract"),
                ["declaredTypeSizeBits"] = K(width, "explicit-field-marshalling-contract"),
                ["occupiedRanges"] = Known(new JsonArray(new JsonObject { ["startBit"] = offset, ["lengthBits"] = width }), "profile-field-offset-and-width")
            });
        }
        return new JsonObject
        {
            ["id"] = id, ["typeId"] = typeof(T).FullName, ["displayName"] = typeof(T).Name, ["status"] = "ok", ["limitations"] = new JsonArray(), ["view"] = "marshaled",
            ["context"] = new JsonObject { ["kind"] = "complete-value" }, ["origin"] = new JsonObject { ["kind"] = "value-start", ["extentStartBit"] = 0 },
            ["metrics"] = new JsonObject { ["valueSizeBytes"] = K(Marshal.SizeOf<T>(), "Marshal.SizeOf<T>"), ["alignmentBytes"] = U("Marshal-does-not-report-type-alignment"), ["arrayStrideBytes"] = U("buffer-array-contract-not-selected") },
            ["members"] = members, ["runtimeRegions"] = new JsonArray(),
            ["coverage"] = new JsonObject { ["fieldEnumeration"] = "complete", ["extent"] = "complete", ["occupiedRanges"] = "complete", ["hiddenRegions"] = "not-applicable" },
            ["marshallingProfile"] = new JsonObject { ["id"] = profileId, ["mechanism"] = "runtime-marshalling", ["configuration"] = configuration }
        };
    }

    public static void CheckNative(string libraryPath)
    {
        NativeOracle.Load(libraryPath);
        var snapshot = Capture(); var observations = snapshot["observations"]!.AsArray();
        for (var i = 0; i < 3; i++)
        {
            var observation = observations[i]!;
            if (NativeOracle.Size(i) != (ulong)observation["metrics"]!["valueSizeBytes"]!["value"]!.GetValue<long>()) throw new ProtocolException("Native sizeof differs for " + observation["id"]);
            var members = observation["members"]!.AsArray();
            for (var j = 0; j < members.Count; j++)
                if (NativeOracle.Offset(i, j) * 8 != (ulong)members[j]!["offsetBits"]!["value"]!.GetValue<long>()) throw new ProtocolException("Native offsetof differs for " + members[j]!["id"]);
        }
        var inlineArray = observations.Single(o => o!["id"]!.GetValue<string>() == "marshal-array/values")!;
        // The native trailing code field immediately follows the three int32 elements.
        // Its independently reported offset verifies the complete buffer span here.
        if ((ulong)inlineArray["metrics"]!["arrayStrideBytes"]!["value"]!.GetValue<long>() != NativeOracle.Offset(2, 1))
            throw new ProtocolException("ByValArray whole-buffer stride differs from the native field span.");
        for (var i = 0; i < 3; i++)
        {
            var element = observations.Single(o => o!["id"]!.GetValue<string>() == "marshal-array/values/" + i)!;
            if (element["metrics"]!["arrayStrideBytes"]!["value"]!.GetValue<long>() != 4)
                throw new ProtocolException("ByValArray element stride must remain the I4 width.");
        }
        var first = new DefaultBoolChar { enabled = true, letter = '\u4e2d', count = 0x12345678 };
        var second = new ByteBoolChar { enabled = true, letter = '\u4e2d', count = 0x12345678 };
        var third = new InlineInts { values = [0x11223344, -7, 42], code = -1234 };
        if (NativeOracle.ValidateDefault(in first) != 1 || NativeOracle.ValidateByte(in second) != 1 || NativeOracle.ValidateArray(in third) != 1) throw new ProtocolException("P/Invoke native field sentinel validation failed.");
        first.enabled = false; second.enabled = false; third.values[1] = 0;
        if (NativeOracle.ValidateDefault(in first) != 0 || NativeOracle.ValidateByte(in second) != 0 || NativeOracle.ValidateArray(in third) != 0) throw new ProtocolException("P/Invoke native oracle did not reject changed input.");
        // Same C# source declaration: bool occupies one managed byte, but four profile bytes.
        if (Unsafe.SizeOf<bool>() != 1 || Unsafe.SizeOf<DefaultBoolChar>() == Marshal.SizeOf<DefaultBoolChar>()) throw new ProtocolException("Expected managed/marshaled difference was not observed; investigate this runtime.");
    }
}
