using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;
using LayoutObserver.Core;

if (args.Length > 0 && args[0] == "fixture") return await Fixture(args[1..]);

var root = Path.Combine(Path.GetTempPath(), "layout-observer-process-checks-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);
var checks = new (string Name, Func<Task> Run)[]
{
    ("arguments, environment, both logs, and target exit code survive the worker gate", Echo),
    ("exited parent plus inherited descendant pipes shares the original timeout", PipeHolder),
    ("successful parent still cleans descendants that close inherited pipes", Detached),
    ("existing log files are not overwritten", ExistingLog)
};
var failures = 0;
foreach (var check in checks)
{
    try { await check.Run(); Console.WriteLine("PASS " + check.Name); }
    catch (Exception ex) { failures++; Console.Error.WriteLine("FAIL " + check.Name + ": " + ex); }
}
Console.WriteLine("Process check logs: " + root);
return failures == 0 ? 0 : 1;

async Task Echo()
{
    var command = Self("fixture", "echo", "space value", "\"quote\"; $() <script>", "");
    command.Environment["LAYOUT_OBSERVER_PROCESS_TEST"] = "private environment value";
    var output = Path.Combine(root, "echo.stdout.log"); var error = Path.Combine(root, "echo.stderr.log");
    var result = await BoundedProcess.ExecuteAsync(command, 10, output, error);
    Check(result == 7, "The target's exit code was not preserved.");
    var log = await File.ReadAllTextAsync(output);
    var expected = JsonSerializer.Serialize(new[] { "space value", "\"quote\"; $() <script>", "" });
    Check(log.Contains(expected, StringComparison.Ordinal), "Arguments were changed by worker transport: " + log);
    Check(log.Contains("private environment value", StringComparison.Ordinal), "Environment was not preserved.");
    Check(!log.Contains("layout-observer-worker-ready:", StringComparison.Ordinal), "Control handshake leaked into target log.");
    Check((await File.ReadAllTextAsync(error)).Contains("target stderr", StringComparison.Ordinal), "stderr was lost.");
}

async Task PipeHolder()
{
    var pidPath = Path.Combine(root, "pipe-holder.pid"); var marker = Path.Combine(root, "pipe-holder.survived");
    var clock = Stopwatch.StartNew(); var timedOut = false;
    try
    {
        await BoundedProcess.ExecuteAsync(Self("fixture", "parent", pidPath, marker, "keep-pipes"), 2,
            Path.Combine(root, "pipe-holder.stdout.log"), Path.Combine(root, "pipe-holder.stderr.log"));
    }
    catch (ProtocolException ex) when (ex.Message.Contains("timed out", StringComparison.Ordinal)) { timedOut = true; }
    clock.Stop();
    Check(timedOut, "Parent exit incorrectly treated the descendant pipe holder as complete.");
    Check(clock.Elapsed.TotalSeconds < 6, "Pipe drain restarted or escaped the deadline: " + clock.Elapsed);
    Check(File.Exists(pidPath), "Fixture descendant never started, so the pipe regression was not exercised.");
    var pid = int.Parse(await File.ReadAllTextAsync(pidPath), System.Globalization.CultureInfo.InvariantCulture);
    await EnsureTerminated(pid);
    Check(!File.Exists(marker), "Owned descendant survived termination.");
}

async Task Detached()
{
    var pidPath = Path.Combine(root, "detached.pid"); var marker = Path.Combine(root, "detached.survived");
    var result = await BoundedProcess.ExecuteAsync(Self("fixture", "parent", pidPath, marker, "close-pipes"), 10,
        Path.Combine(root, "detached.stdout.log"), Path.Combine(root, "detached.stderr.log"));
    Check(result == 0, "The normal parent should complete without timing out.");
    Check(File.Exists(pidPath), "Fixture descendant never started.");
    var pid = int.Parse(await File.ReadAllTextAsync(pidPath), System.Globalization.CultureInfo.InvariantCulture);
    await EnsureTerminated(pid);
    Check(!File.Exists(marker), "A pipe-free owned descendant leaked after successful completion.");
}

async Task ExistingLog()
{
    var output = Path.Combine(root, "existing.stdout.log"); await File.WriteAllTextAsync(output, "keep");
    var rejected = false;
    try { await BoundedProcess.ExecuteAsync(Self("fixture", "echo"), 10, output, Path.Combine(root, "unused.stderr.log")); }
    catch (IOException) { rejected = true; }
    Check(rejected && await File.ReadAllTextAsync(output) == "keep", "Existing logs were overwritten.");
}

static async Task<int> Fixture(string[] arguments)
{
    switch (arguments[0])
    {
        case "echo":
            Console.WriteLine(JsonSerializer.Serialize(arguments[1..]));
            Console.WriteLine(Environment.GetEnvironmentVariable("LAYOUT_OBSERVER_PROCESS_TEST"));
            Console.Error.WriteLine("target stderr");
            return 7;
        case "parent":
        {
            var childCommand = Self("fixture", "child", arguments[1], arguments[2], arguments[3]);
            if (arguments[3] == "close-pipes")
            {
                // Give the descendant private pipes at creation, so it cannot keep the command's output pipes alive.
                // Windows otherwise also inherits the parent's original pipe handles in addition to the new standard handles.
                FixtureNative.MakeOutputNonInheritable();
                childCommand.RedirectStandardOutput = true;
                childCommand.RedirectStandardError = true;
            }
            using var child = Process.Start(childCommand)
                ?? throw new InvalidOperationException("Cannot start descendant fixture.");
            using var startup = new CancellationTokenSource(TimeSpan.FromSeconds(5));
            while (!File.Exists(arguments[1])) await Task.Delay(10, startup.Token);
            Console.WriteLine("Fixture parent exited; descendant " + child.Id + " is still running.");
            return 0;
        }
        case "child":
            Console.WriteLine("Descendant owns inherited pipes: " + Environment.ProcessId);
            await Console.Out.FlushAsync();
            if (arguments[3] == "close-pipes") FixtureNative.CloseOutput();
            await File.WriteAllTextAsync(arguments[1], Environment.ProcessId.ToString(System.Globalization.CultureInfo.InvariantCulture));
            await Task.Delay(TimeSpan.FromSeconds(30));
            await File.WriteAllTextAsync(arguments[2], "owned descendant leaked");
            return 0;
        default: throw new InvalidOperationException("Unknown fixture.");
    }
}

static ProcessStartInfo Self(params string[] arguments)
{
    var executable = Environment.ProcessPath ?? throw new InvalidOperationException("Missing host path.");
    var info = new ProcessStartInfo(executable) { UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = Environment.CurrentDirectory };
    if (Path.GetFileNameWithoutExtension(executable).Equals("dotnet", StringComparison.OrdinalIgnoreCase))
        info.ArgumentList.Add(Assembly.GetExecutingAssembly().Location);
    foreach (var argument in arguments) info.ArgumentList.Add(argument);
    return info;
}

static async Task EnsureTerminated(int id)
{
    for (var attempt = 0; attempt < 40; attempt++)
    {
        try
        {
            using var process = Process.GetProcessById(id);
            if (process.HasExited) return;
            if (OperatingSystem.IsLinux())
            {
                // An orphan already killed by the scope can briefly remain a non-running zombie until PID 1 reaps it.
                var stat = await File.ReadAllTextAsync("/proc/" + id + "/stat");
                var endName = stat.LastIndexOf(')');
                if (endName >= 0 && stat[(endName + 2)..].StartsWith("Z ", StringComparison.Ordinal)) return;
            }
        }
        catch (ArgumentException) { return; }
        catch (FileNotFoundException) { return; }
        catch (DirectoryNotFoundException) { return; }
        await Task.Delay(25);
    }
    throw new Exception("Owned descendant is still running: " + id);
}

static void Check(bool condition, string message) { if (!condition) throw new Exception(message); }

internal static class FixtureNative
{
    internal static void MakeOutputNonInheritable()
    {
        if (!OperatingSystem.IsWindows()) return;
        if (!SetHandleInformation(GetStdHandle(-11), 1, 0) || !SetHandleInformation(GetStdHandle(-12), 1, 0))
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastPInvokeError());
    }
    internal static void CloseOutput()
    {
        if (OperatingSystem.IsWindows()) { CloseHandle(GetStdHandle(-11)); CloseHandle(GetStdHandle(-12)); }
        else { Close(1); Close(2); }
    }
    [DllImport("kernel32.dll")] private static extern IntPtr GetStdHandle(int kind);
    [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetHandleInformation(IntPtr handle, uint mask, uint flags);
    [DllImport("kernel32.dll")] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool CloseHandle(IntPtr handle);
    [DllImport("libc", EntryPoint = "close")] private static extern int Close(int descriptor);
}
