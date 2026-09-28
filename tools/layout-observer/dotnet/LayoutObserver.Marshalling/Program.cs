using LayoutObserver.Core;
using LayoutObserver.Marshalling;
using System.Runtime.InteropServices;

try
{
    var options = new Dictionary<string, string>();
    for (var i = 0; i < args.Length; i += 2)
        if (i + 1 >= args.Length || !new[] { "--output", "--configuration", "--native-library" }.Contains(args[i]) || !options.TryAdd(args[i], args[i + 1])) throw new ProtocolException("Use --output FILE [--configuration Debug|Release] [--native-library LIBRARY].");
    if (options.TryGetValue("--native-library", out var library))
    {
        MarshaledCapture.CheckNative(library);
        Console.Error.WriteLine("PASS native sizeof/offsetof and P/Invoke field sentinels (including rejection controls)");
    }
    var snapshot = MarshaledCapture.Capture(options.GetValueOrDefault("--configuration"));
    if (options.TryGetValue("--output", out var output)) JsonIO.Write(output, snapshot);
    else Console.WriteLine(snapshot.ToJsonString(JsonIO.Options));
    return 0;
}
catch (Exception ex) when (ex is ProtocolException or IOException or ArgumentException or InvalidOperationException or DllNotFoundException or EntryPointNotFoundException or BadImageFormatException or MarshalDirectiveException)
{
    Console.Error.WriteLine("ERROR: " + ex.Message); return 3;
}
