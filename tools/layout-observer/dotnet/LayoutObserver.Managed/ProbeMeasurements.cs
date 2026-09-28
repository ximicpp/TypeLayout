using System.Runtime.CompilerServices;

namespace LayoutObserver.Managed;

public sealed record ProbeType(string Id, string DisplayName, string Kind, int SizeBytes,
    string? ScalarCode = null, ProbeType? Element = null, int? FixedCount = null);

public sealed record ProbeField(string Name, ProbeType Type,
    int? SizeBytes, long? OffsetBytes, string? Limitation = null, ProbeResult? Child = null);

public sealed record ProbeResult(string TypeId, string TypeName, int? SizeBytes,
    long? ArrayStrideBytes, IReadOnlyList<ProbeField> Fields, string? Limitation = null, ProbeType? Type = null)
{
    public bool IsComplete => Limitation is null && Fields.All(f => f.Limitation is null);
}

/// <summary>Measurements retain managed byrefs; they never store raw GC addresses.</summary>
public static class ProbeMeasurements
{
    public static ProbeResult ScalarValue<T>(string typeId) where T : struct =>
        Value<T>(typeId, typeof(T).ToString(), Array.Empty<ProbeField>()) with { Type = Describe<T>() };

    public static ProbeField Field<T, TField>(ref T instance, in TField field,
        string name, ProbeType? type = null, ProbeResult? child = null) where T : struct
    {
        ref byte start = ref Unsafe.As<T, byte>(ref instance);
        ref byte member = ref Unsafe.As<TField, byte>(ref Unsafe.AsRef(in field));
        return new(name, type ?? Describe<TField>(), Unsafe.SizeOf<TField>(),
            (long)Unsafe.ByteOffset(ref start, ref member), Child: child);
    }

    public static ProbeField UnavailableField(string name, string typeName, string reason) =>
        new(name, new(typeName, typeName, "opaque", 0), null, null, reason);

    public static ProbeType InlineArray<T, TElement>(int count) where T : struct =>
        new(typeof(T).ToString(), typeof(T).ToString(), "array", Unsafe.SizeOf<T>(),
            Element: Describe<TElement>(), FixedCount: count);

    public static ProbeResult InlineValue<T, TElement>(int count, IReadOnlyList<ProbeField> fields) where T : struct
    {
        TElement[] elements = new TElement[2];
        ref byte first = ref Unsafe.As<TElement, byte>(ref elements[0]);
        ref byte next = ref Unsafe.As<TElement, byte>(ref elements[1]);
        long stride = (long)Unsafe.ByteOffset(ref first, ref next);
        ProbeType elementType = Describe<TElement>();
        var element = new ProbeResult("element", elementType.DisplayName, Unsafe.SizeOf<TElement>(), stride,
            Array.Empty<ProbeField>(), elementType.Kind == "record" ? "inline-record-element-needs-registered-probe" : null, Type: elementType);
        return Value<T>("inline-array", typeof(T).ToString(), fields.Select(f => f with { Child = element }).ToArray())
            with { Type = InlineArray<T, TElement>(count) };
    }

    public static ProbeType Describe<T>()
    {
        Type type = typeof(T);
        string name = type.ToString();
        int size = Unsafe.SizeOf<T>();
        if (type.IsEnum)
        {
            Type underlying = Enum.GetUnderlyingType(type);
            string code = PrimitiveCode(underlying) ?? throw new InvalidOperationException("Unsupported enum underlying type.");
            return new(name, name, "enum", size, Element: new(code, underlying.ToString(), "scalar", size, code));
        }
        string? scalar = PrimitiveCode(type);
        if (scalar is not null) return new(scalar, name, "scalar", size, scalar);
        if (!type.IsValueType) return new("ref:" + name, name + " reference", "reference", size,
            Element: new(name, name, "opaque", 0));
        return new(name, name, "record", size);
    }

    private static string? PrimitiveCode(Type type) =>
        type == typeof(byte) ? "u8" : type == typeof(sbyte) ? "i8" :
        type == typeof(short) ? "i16" : type == typeof(ushort) ? "u16" :
        type == typeof(int) ? "i32" : type == typeof(uint) ? "u32" :
        type == typeof(long) ? "i64" : type == typeof(ulong) ? "u64" :
        type == typeof(float) ? "f32" : type == typeof(double) ? "f64" :
        type == typeof(bool) ? "bool" : type == typeof(char) ? "char16" :
        type == typeof(nint) ? "nint" : type == typeof(nuint) ? "nuint" : null;

    public static ProbeResult Value<T>(string typeId, string typeName,
        IReadOnlyList<ProbeField> fields) where T : struct
    {
        T[] pair = new T[2];
        ref byte first = ref Unsafe.As<T, byte>(ref pair[0]);
        ref byte second = ref Unsafe.As<T, byte>(ref pair[1]);
        long stride = (long)Unsafe.ByteOffset(ref first, ref second);
        return new(typeId, typeName, Unsafe.SizeOf<T>(), stride, fields);
    }

    public static ProbeResult Unsupported(string typeId, string typeName, string reason) =>
        new(typeId, typeName, null, null, Array.Empty<ProbeField>(), reason);
}
