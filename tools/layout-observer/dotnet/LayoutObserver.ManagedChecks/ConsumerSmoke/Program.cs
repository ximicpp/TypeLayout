using System.Text.Json;
using System.Text.Json.Nodes;
using LayoutObserver.ManagedChecks;

try
{
    if (args.Length != 0 && (args.Length != 2 || args[0] != "--output"))
        throw new ArgumentException("Use [--output SNAPSHOT].");
    JsonObject snapshot = ConsumerChecks.Run();
    string json = snapshot.ToJsonString(new JsonSerializerOptions { WriteIndented = true });
    if (args.Length == 0) Console.WriteLine(json);
    else
    {
        string path = Path.GetFullPath(args[1]);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        using var stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write);
        using var writer = new StreamWriter(stream); writer.WriteLine(json);
    }
    return 0;
}
catch (Exception error)
{
    Console.Error.WriteLine(error.Message); return 3;
}
