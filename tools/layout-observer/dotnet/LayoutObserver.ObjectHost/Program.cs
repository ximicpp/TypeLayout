using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text.Json;

namespace LayoutObserver.ObjectHost;

public sealed class CaptureCase(string id, object value)
{
    public readonly string Id = id;
    public readonly object Value = value;
}

public sealed class EmptyObject { }
public class PrivateBase
{
    private readonly byte _tag = 7;
    private readonly long _count = 1234;
    public long Inspect() => _tag + _count;
    internal ref readonly byte TagRef => ref _tag;
    internal ref readonly long CountRef => ref _count;
}
public sealed class PrivateDerived : PrivateBase
{
    private readonly short _code = 9;
    private readonly object _target = new();
    public object InspectTarget() => _code == 9 ? _target : this;
    internal long[] FieldDistances()
    {
        ref byte tag = ref Unsafe.AsRef(in TagRef);
        return [0, (long)Unsafe.ByteOffset(ref tag, ref Unsafe.As<long, byte>(ref Unsafe.AsRef(in CountRef))),
            (long)Unsafe.ByteOffset(ref tag, ref Unsafe.As<short, byte>(ref Unsafe.AsRef(in _code))),
            (long)Unsafe.ByteOffset(ref tag, ref Unsafe.As<object, byte>(ref Unsafe.AsRef(in _target)))];
    }
}
public struct ReferenceValue
{
    public byte Tag;
    public object Target;
    public int Count;
}
public struct PlainValue { public byte Tag; public int Count; public short Code; }

internal static class Program
{
    // These are roots, not stale object addresses. The collector discovers wrappers in the frozen snapshot.
    private static CaptureCase[] _roots = [];

    public static int Main(string[] args)
    {
        try
        {
            int delay = 0;
            for (int i = 0; i < args.Length; i++)
            {
                if (args[i] == "--fail-factory") throw new InvalidOperationException("fixture-factory-failed");
                if (args[i] == "--delay-ready-ms" && ++i < args.Length) delay = int.Parse(args[i]);
                else throw new ArgumentException("Unknown host argument.");
            }
            _roots =
            [
                new("empty-object", new EmptyObject()),
                new("private-derived", new PrivateDerived()),
                new("boxed-plain", new PlainValue { Tag = 1, Count = 2, Code = 3 }),
                new("boxed-reference", new ReferenceValue { Tag = 1, Target = new object(), Count = 2 }),
                new("byte-array-1", new byte[1]),
                new("byte-array-7", new byte[7]),
                new("byte-array-9", new byte[9]),
                new("int-array-1", new int[1]),
                new("int-array-3", new int[3]),
                new("reference-array-2", new object[] { new(), new() }),
                new("string-1", new string('x', 1)),
                new("string-2", new string('x', 2)),
                new("string-5", new string('x', 5))
            ];
            // Move freshly created objects before publishing readiness. No pre-GC addresses leave this process.
            GC.Collect(2, GCCollectionMode.Forced, blocking: true, compacting: true);
            GC.WaitForPendingFinalizers();
            GC.Collect(2, GCCollectionMode.Forced, blocking: true, compacting: true);
            if (delay > 0) Thread.Sleep(delay);
            var derived = (PrivateDerived)_roots.Single(c => c.Id == "private-derived").Value;
            ReferenceValue reference = default;
            ref byte start = ref Unsafe.As<ReferenceValue, byte>(ref reference);
            long[] referenceOffsets = [(long)Unsafe.ByteOffset(ref start, ref reference.Tag),
                (long)Unsafe.ByteOffset(ref start, ref Unsafe.As<object, byte>(ref reference.Target)),
                (long)Unsafe.ByteOffset(ref start, ref Unsafe.As<int, byte>(ref reference.Count))];
            var calibration = new
            {
                caseCount = _roots.Length, runtimeVersion = Environment.Version.ToString(),
                architecture = RuntimeInformation.ProcessArchitecture.ToString().ToLowerInvariant(), pointerBytes = IntPtr.Size,
#if DEBUG
                configuration = "Debug",
#else
                configuration = "Release",
#endif
                plainValueBytes = Unsafe.SizeOf<PlainValue>(), referenceValueBytes = Unsafe.SizeOf<ReferenceValue>(),
                referenceOffsets, derivedDistances = derived.FieldDistances()
            };
            Console.WriteLine("LAYOUT_READY_V1 " + JsonSerializer.Serialize(calibration));
            Console.Out.Flush();
            if (Console.ReadLine() != "RELEASE") return 3;
            GC.KeepAlive(_roots);
            return 0;
        }
        catch (Exception error) { Console.Error.WriteLine(error.Message); return 3; }
    }
}
