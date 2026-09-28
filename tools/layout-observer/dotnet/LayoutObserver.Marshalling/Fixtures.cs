using System.Runtime.InteropServices;

namespace LayoutObserver.Marshalling;

// These are explicit runtime-marshalling profiles, not general managed layout models.
[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
public struct DefaultBoolChar
{
    public bool enabled;
    public char letter;
    public int count;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
public struct ByteBoolChar
{
    [MarshalAs(UnmanagedType.U1)] public bool enabled;
    public char letter;
    public int count;
}

[StructLayout(LayoutKind.Sequential)]
public struct InlineInts
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3, ArraySubType = UnmanagedType.I4)]
    public int[] values;
    public short code;
}

internal static class NativeOracle
{
    private const string Library = "typelayout-marshalling-oracle";
    internal static void Load(string path)
    {
        // One exact, user-selected library. Keep loaded for all P/Invoke calls in this process.
        var handle = NativeLibrary.Load(Path.GetFullPath(path));
        NativeLibrary.SetDllImportResolver(typeof(NativeOracle).Assembly, (name, _, _) => name == Library ? handle : IntPtr.Zero);
    }
    [DllImport(Library, EntryPoint = "typelayout_marshal_size", CallingConvention = CallingConvention.Cdecl)]
    internal static extern ulong Size(int caseId);
    [DllImport(Library, EntryPoint = "typelayout_marshal_offset", CallingConvention = CallingConvention.Cdecl)]
    internal static extern ulong Offset(int caseId, int fieldId);
    [DllImport(Library, EntryPoint = "typelayout_marshal_validate_default", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int ValidateDefault(in DefaultBoolChar value);
    [DllImport(Library, EntryPoint = "typelayout_marshal_validate_byte", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int ValidateByte(in ByteBoolChar value);
    [DllImport(Library, EntryPoint = "typelayout_marshal_validate_array", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int ValidateArray(in InlineInts value);
}
