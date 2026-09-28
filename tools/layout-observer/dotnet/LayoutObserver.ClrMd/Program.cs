using System.Text.Json;

namespace LayoutObserver.ClrMd;

internal static class Program
{
    public static async Task<int> Main(string[] args)
    {
        try
        {
            string? host = null, output = null, configuration = null, runId = null, dotnet = null, dac = null, runtime = null;
            int timeout = 30000, delay = 0; bool failFactory = false;
            for (int i = 0; i < args.Length; i++)
            {
                string option = args[i];
                if (option == "--fail-factory") { failFactory = true; continue; }
                if (++i == args.Length) throw new ArgumentException("Missing value for " + option);
                string value = args[i];
                switch (option)
                {
                    case "--host": host = value; break; case "--output": output = value; break;
                    case "--configuration": configuration = value; break; case "--run-id": runId = value; break;
                    case "--dotnet": dotnet = value; break; case "--dac": dac = value; break;
                    case "--require-runtime": runtime = value; break;
                    case "--timeout-ms": timeout = int.Parse(value); break; case "--delay-ready-ms": delay = int.Parse(value); break;
                    default: throw new ArgumentException("Unknown argument: " + option);
                }
            }
            if (host is null) throw new ArgumentException("--host must specify the controlled ObjectHost DLL.");
            var snapshot = await ObjectCapture.CaptureAsync(new(host, dotnet, dac, runtime, timeout, configuration, runId, failFactory, delay));
            string json = snapshot.ToJsonString(new JsonSerializerOptions { WriteIndented = true });
            if (output is null) Console.WriteLine(json);
            else
            {
                string path = Path.GetFullPath(output); Directory.CreateDirectory(Path.GetDirectoryName(path)!);
                using var stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write);
                using var writer = new StreamWriter(stream); writer.WriteLine(json);
            }
            return 0;
        }
        catch (CaptureException error) { Console.Error.WriteLine(error.Code + ": " + error.Message); return 3; }
        catch (Exception error) { Console.Error.WriteLine("capture-error: " + error.Message); return 3; }
    }
}
