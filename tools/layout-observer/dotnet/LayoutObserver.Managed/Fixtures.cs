using System.Runtime.InteropServices;
using LayoutObserver.Managed;
using LayoutObserver.Managed.Fixtures;

[assembly: ObserveLayout(typeof(FixedRecord), "sample")]
[assembly: ObserveLayout(typeof(PackedRecord), "packed")]
[assembly: ObserveLayout(typeof(ReorderedRecord), "reordered")]
[assembly: ObserveLayout(typeof(NestedRecord), "nested")]
[assembly: ObserveLayout(typeof(ArrayRecord), "array")]
[assembly: ObserveLayout(typeof(EnumRecord), "enum")]
[assembly: ObserveLayout(typeof(ReferenceRecord), "reference-record")]
[assembly: ObserveLayout(typeof(OverlappingRecord), "explicit-record")]
[assembly: ObserveLayout(typeof(PrivateRecord), "private-record")]
[assembly: ObserveLayout(typeof(GenericRecord<int>), "generic-int-record")]
[assembly: ObserveLayout(typeof(GenericRecord<double>), "generic-double-record")]
[assembly: ObserveLayout(typeof(BooleanRecord), "boolean-record")]
[assembly: ObserveLayout(typeof(EmptyRecord), "empty-record")]
[assembly: ObserveLayout(typeof(NonPartialRecord), "external-public-record")]
[assembly: ObserveLayout(typeof(PrivateNonPartialRecord), "inaccessible-private-record")]
[assembly: ObserveLayout(typeof(UnsupportedClass), "unsupported-class")]
[assembly: ObserveLayout(typeof(ShortCode), "enum-scalar")]
[assembly: ObserveLayout(typeof(int), "int-scalar")]

namespace LayoutObserver.Managed.Fixtures
{
    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    public partial struct FixedRecord
    {
        public byte Tag;
        public int Count;
        public short Code;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public partial struct PackedRecord
    {
        public byte Tag;
        public int Count;
        public short Code;
    }

    [StructLayout(LayoutKind.Sequential)]
    public partial struct ReorderedRecord
    {
        public int Count;
        public short Code;
        public byte Tag;
    }

    public partial struct NestedRecord
    {
        public FixedRecord Payload;
        public int Extra;
    }

    [System.Runtime.CompilerServices.InlineArray(3)]
    public struct Int32Array3 { private int _element0; }
    public partial struct ArrayRecord { public Int32Array3 Values; }

    public enum ShortCode : ushort { None, Ready }
    public partial struct EnumRecord
    {
        public ShortCode Value;
    }

    public partial struct ReferenceRecord
    {
        public byte Tag;
        public object? Target;
        public int Count;
    }

    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public partial struct OverlappingRecord
    {
        [FieldOffset(0)] public int Whole;
        [FieldOffset(0)] public short Low;
        [FieldOffset(4)] public int Tail;
    }

    public partial struct PrivateRecord
    {
        private byte _tag;
        private long _count;
        public PrivateRecord(byte tag, long count) { _tag = tag; _count = count; }
    }

    public partial struct GenericRecord<T> where T : struct
    {
        private byte _tag;
        private T _value;
        public GenericRecord(byte tag, T value) { _tag = tag; _value = value; }
    }

    public partial struct BooleanRecord
    {
        public bool Flag;
        public byte Tag;
    }

    public partial struct EmptyRecord { }
    public struct NonPartialRecord { public int Value; public byte Tag; }
    public struct PrivateNonPartialRecord
    {
        private int _hidden;
        public int ReadHidden() => _hidden;
        public PrivateNonPartialRecord(int hidden) { _hidden = hidden; }
    }
    public sealed class UnsupportedClass { public int Value; }
}
