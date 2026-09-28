using System.Runtime.InteropServices;
using LayoutObserver.Managed;

[assembly: ObserveLayout(typeof(Orders.Order), "orders.managed")]

try
{
    if (args.Length != 4 || args[0] != "--configuration" || args[2] != "--output")
        throw new ArgumentException("Use --configuration Debug|Release --output NEW-SNAPSHOT.");
    var snapshot = ManagedCapture.CaptureRegistered(GeneratedProbeRegistry.Capture(), GeneratedProbeRegistry.CompilerVersion,
        GeneratedProbeRegistry.BuildConfiguration, requestedConfiguration: args[1]);
    var path = Path.GetFullPath(args[3]); Directory.CreateDirectory(Path.GetDirectoryName(path)!);
    using var stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write);
    System.Text.Json.JsonSerializer.Serialize(stream, snapshot, new System.Text.Json.JsonSerializerOptions { WriteIndented = true });
    return 0;
}
catch (Exception error) { Console.Error.WriteLine(error.Message); return 3; }

namespace Orders
{
    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    public partial struct Order
    {
        public byte Status;
        public int OrderId;
        public short Quantity;
    }
}
