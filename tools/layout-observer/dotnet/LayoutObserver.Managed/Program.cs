using System.Text.Json;

namespace LayoutObserver.Managed;

internal static class Program
{
    public static int Main(string[] args)
    {
        try
        {
            string? output = null, configuration = null, runId = null;
            for (int i = 0; i < args.Length; i++)
            {
                if (args[i] == "--self-test") { ManagedProbeChecks.Run(); Console.WriteLine("PASS: managed static probes"); return 0; }
                if (args[i] is "--output" or "--configuration" or "--run-id")
                {
                    string option = args[i];
                    if (++i == args.Length) throw new ArgumentException("Missing value for " + option);
                    if (option == "--output") output = args[i]; else if (option == "--configuration") configuration = args[i]; else runId = args[i];
                }
                else throw new ArgumentException("Unknown argument: " + args[i]);
            }
            string json = ManagedCapture.Capture(runId, configuration).ToJsonString(new JsonSerializerOptions { WriteIndented = true });
            if (output is null) Console.WriteLine(json);
            else
            {
                string path = Path.GetFullPath(output); Directory.CreateDirectory(Path.GetDirectoryName(path)!);
                using var stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write);
                using var writer = new StreamWriter(stream); writer.WriteLine(json);
            }
            return 0;
        }
        catch (Exception exception) { Console.Error.WriteLine(exception.Message); return 3; }
    }
}
