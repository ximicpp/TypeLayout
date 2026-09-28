using System.ComponentModel;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using LayoutObserver.Core;
using Microsoft.Win32.SafeHandles;

/// <summary>Runs an argument-array command in an owned process group with one deadline for exit and pipe drain.</summary>
public static class BoundedProcess
{
    private const string ReadyPrefix = "layout-observer-worker-ready:";
    private const int WorkerFailureExitCode = 125;

    public static async Task<int> ExecuteAsync(ProcessStartInfo command, int timeoutSeconds,
        string stdoutLog, string stderrLog)
    {
        ArgumentNullException.ThrowIfNull(command);
        if (command.UseShellExecute || command.Arguments.Length != 0)
            throw new ProtocolException("Bounded commands require UseShellExecute=false and ArgumentList.");
        if (timeoutSeconds is < 1 or > 3600) throw new ProtocolException("Command timeout must be 1..3600 seconds.");
        if (!OperatingSystem.IsWindows() && !OperatingSystem.IsLinux())
            throw new ProtocolException("Owned process cleanup currently supports Windows and Linux.");

        var spec = new CommandSpec(command.FileName,
            string.IsNullOrEmpty(command.WorkingDirectory) ? Environment.CurrentDirectory : Path.GetFullPath(command.WorkingDirectory),
            command.ArgumentList.ToArray(), command.Environment.ToDictionary(x => x.Key, x => x.Value, StringComparer.Ordinal));
        using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(timeoutSeconds));
        await using var outputLog = OpenLog(stdoutLog);
        await using var errorLog = OpenLog(stderrLog);
        using var scope = new OwnedScope();
        using var worker = Process.Start(WorkerStartInfo()) ?? throw new ProtocolException("Cannot start the command worker.");
        scope.Attach(worker);
        Task? outputDrain = null;
        var errorDrain = DrainAsync(worker.StandardError, errorLog, deadline.Token);
        Task? exit = null;
        try
        {
            // The worker cannot start a target until it has both joined the owned group and received this gate.
            var ready = await worker.StandardOutput.ReadLineAsync(deadline.Token);
            if (ready != ReadyPrefix + worker.Id.ToString(System.Globalization.CultureInfo.InvariantCulture))
                throw new ProtocolException("Command worker failed to establish its owned process group; see stderr log.");
            scope.ConfirmReady();
            outputDrain = DrainAsync(worker.StandardOutput, outputLog, deadline.Token);
            await worker.StandardInput.WriteLineAsync(JsonSerializer.Serialize(spec).AsMemory(), deadline.Token);
            await worker.StandardInput.FlushAsync(deadline.Token);
            worker.StandardInput.Close();
            exit = worker.WaitForExitAsync(deadline.Token);
            // A target may exit while its descendant still owns a pipe. Both drains share the original deadline.
            await Task.WhenAll(exit, outputDrain, errorDrain).WaitAsync(deadline.Token);
            return worker.ExitCode;
        }
        catch (OperationCanceledException) when (deadline.IsCancellationRequested)
        {
            scope.Terminate();
            await SettleAsync(worker, outputDrain, errorDrain, exit);
            throw new ProtocolException($"Command timed out after {timeoutSeconds} seconds, including output drain; owned descendants were terminated; see logs.");
        }
        catch
        {
            deadline.Cancel();
            scope.Terminate();
            await SettleAsync(worker, outputDrain, errorDrain, exit);
            throw;
        }
        finally
        {
            // This also cleans up detached descendants that closed their pipes before the command finished.
            scope.Terminate();
        }
    }

    internal static async Task<int> RunChildAsync()
    {
        try
        {
            if (OperatingSystem.IsLinux() && NativeMethods.SetSid() < 0)
                throw new Win32Exception(Marshal.GetLastPInvokeError(), "setsid failed");
            if (!OperatingSystem.IsWindows() && !OperatingSystem.IsLinux()) return WorkerFailureExitCode;
            Console.Out.WriteLine(ReadyPrefix + Environment.ProcessId.ToString(System.Globalization.CultureInfo.InvariantCulture));
            await Console.Out.FlushAsync();
            var gate = await Console.In.ReadLineAsync();
            if (gate is null) return WorkerFailureExitCode;
            var spec = JsonSerializer.Deserialize<CommandSpec>(gate)
                ?? throw new ProtocolException("Missing worker command.");
            var info = new ProcessStartInfo(spec.FileName)
            {
                WorkingDirectory = spec.WorkingDirectory,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardInput = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };
            foreach (var argument in spec.Arguments) info.ArgumentList.Add(argument);
            info.Environment.Clear();
            foreach (var pair in spec.Environment) info.Environment[pair.Key] = pair.Value;
            if (OperatingSystem.IsWindows())
            {
                // Do not leak the worker's control/forwarding pipes as extra inheritable handles to the target.
                // The target gets only its newly redirected standard handles from Process.Start.
                foreach (var stream in new[] { -10, -11, -12 })
                    if (!NativeMethods.SetHandleInformation(NativeMethods.GetStdHandle(stream), 1, 0))
                        throw new Win32Exception(Marshal.GetLastPInvokeError(), "Cannot protect worker control handles");
            }
            using var target = Process.Start(info) ?? throw new ProtocolException("Cannot start target command.");
            target.StandardInput.Close();
            // Explicit redirection avoids relying on the host's inheritable console-handle flags on Windows.
            var output = target.StandardOutput.BaseStream.CopyToAsync(Console.OpenStandardOutput());
            var error = target.StandardError.BaseStream.CopyToAsync(Console.OpenStandardError());
            await Task.WhenAll(target.WaitForExitAsync(), output, error);
            return target.ExitCode;
        }
        catch (Exception ex) when (ex is IOException or InvalidOperationException or Win32Exception or JsonException or ArgumentException or ProtocolException)
        {
            Console.Error.WriteLine("Command worker: " + ex.Message);
            return WorkerFailureExitCode;
        }
    }

    private static ProcessStartInfo WorkerStartInfo()
    {
        var assembly = typeof(BoundedProcess).Assembly.Location;
        var entryIsCli = Assembly.GetEntryAssembly() == typeof(BoundedProcess).Assembly;
        var currentHost = Environment.ProcessPath;
        var info = new ProcessStartInfo
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardInputEncoding = new UTF8Encoding(false),
            StandardOutputEncoding = new UTF8Encoding(false),
            StandardErrorEncoding = new UTF8Encoding(false),
            WorkingDirectory = Environment.CurrentDirectory
        };
        if (entryIsCli && currentHost is not null)
        {
            info.FileName = currentHost;
            if (Path.GetFileNameWithoutExtension(currentHost).Equals("dotnet", StringComparison.OrdinalIgnoreCase))
                info.ArgumentList.Add(assembly);
        }
        else
        {
            var host = Environment.GetEnvironmentVariable("DOTNET_HOST_PATH");
            if (string.IsNullOrEmpty(host))
                host = Path.GetFullPath(Path.Combine(RuntimeEnvironment.GetRuntimeDirectory(), "..", "..", "..",
                    OperatingSystem.IsWindows() ? "dotnet.exe" : "dotnet"));
            if (!File.Exists(host)) throw new ProtocolException("Cannot locate the .NET host for the command worker.");
            info.FileName = host;
            info.ArgumentList.Add(assembly);
        }
        info.ArgumentList.Add("internal-child");
        return info;
    }

    private static StreamWriter OpenLog(string path) => new(
        new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read, 4096, FileOptions.Asynchronous),
        new UTF8Encoding(false));

    private static async Task DrainAsync(StreamReader reader, StreamWriter writer, CancellationToken cancellation)
    {
        var buffer = new char[4096];
        try
        {
            while (await reader.ReadAsync(buffer.AsMemory(), cancellation) is var count && count != 0)
                await writer.WriteAsync(buffer.AsMemory(0, count), cancellation);
        }
        finally { await writer.FlushAsync(CancellationToken.None); }
    }

    private static async Task SettleAsync(Process process, params Task?[] pending)
    {
        // Termination happens first. The short cleanup bound must never restart the command's full deadline.
        try
        {
            var tasks = pending.OfType<Task>().Append(process.WaitForExitAsync());
            await Task.WhenAll(tasks).WaitAsync(TimeSpan.FromSeconds(2));
        }
        catch (Exception ex) when (ex is OperationCanceledException or TimeoutException or IOException or InvalidOperationException) { }
    }

    private sealed record CommandSpec(string FileName, string WorkingDirectory, string[] Arguments,
        Dictionary<string, string?> Environment);

    private sealed class OwnedScope : IDisposable
    {
        private readonly SafeFileHandle? job;
        private Process? process;
        private bool ready;
        private bool terminated;

        public OwnedScope()
        {
            if (!OperatingSystem.IsWindows()) return;
            job = NativeMethods.CreateJobObject(IntPtr.Zero, null);
            if (job.IsInvalid) throw new Win32Exception(Marshal.GetLastPInvokeError(), "CreateJobObject failed");
            var limits = new JobExtendedLimitInformation();
            limits.BasicLimitInformation.LimitFlags = 0x00002000; // JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
            if (!NativeMethods.SetInformationJobObject(job, 9, ref limits, (uint)Marshal.SizeOf<JobExtendedLimitInformation>()))
            {
                var error = Marshal.GetLastPInvokeError(); job.Dispose();
                throw new Win32Exception(error, "SetInformationJobObject failed");
            }
        }

        public void Attach(Process worker)
        {
            process = worker;
            if (job is not null && !NativeMethods.AssignProcessToJobObject(job, worker.SafeHandle))
            {
                var error = Marshal.GetLastPInvokeError();
                try { worker.Kill(entireProcessTree: true); } catch (InvalidOperationException) { }
                throw new Win32Exception(error, "AssignProcessToJobObject failed before opening the worker gate");
            }
        }

        public void ConfirmReady() => ready = true;

        public void Terminate()
        {
            if (terminated) return;
            terminated = true;
            if (job is not null) { job.Dispose(); return; }
            if (process is null) return;
            if (OperatingSystem.IsLinux() && ready)
            {
                if (NativeMethods.Kill(-process.Id, 9) != 0 && Marshal.GetLastPInvokeError() != 3) // ESRCH: group already empty
                    throw new Win32Exception(Marshal.GetLastPInvokeError(), "Cannot terminate the owned process group");
            }
            else
            {
                try { process.Kill(entireProcessTree: true); }
                catch (InvalidOperationException) { }
            }
        }

        public void Dispose() => Terminate();
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct JobBasicLimitInformation
    {
        public long PerProcessUserTimeLimit, PerJobUserTimeLimit;
        public uint LimitFlags;
        public UIntPtr MinimumWorkingSetSize, MaximumWorkingSetSize;
        public uint ActiveProcessLimit;
        public UIntPtr Affinity;
        public uint PriorityClass, SchedulingClass;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct IoCounters
    {
        public ulong ReadOperationCount, WriteOperationCount, OtherOperationCount;
        public ulong ReadTransferCount, WriteTransferCount, OtherTransferCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct JobExtendedLimitInformation
    {
        public JobBasicLimitInformation BasicLimitInformation;
        public IoCounters IoInfo;
        public UIntPtr ProcessMemoryLimit, JobMemoryLimit, PeakProcessMemoryUsed, PeakJobMemoryUsed;
    }

    private static class NativeMethods
    {
        [DllImport("kernel32.dll", EntryPoint = "CreateJobObjectW", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern SafeFileHandle CreateJobObject(IntPtr attributes, string? name);
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool SetInformationJobObject(SafeFileHandle job, int informationClass,
            ref JobExtendedLimitInformation information, uint informationLength);
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool AssignProcessToJobObject(SafeFileHandle job, SafeProcessHandle process);
        [DllImport("kernel32.dll")] public static extern IntPtr GetStdHandle(int kind);
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool SetHandleInformation(IntPtr handle, uint mask, uint flags);
        [DllImport("libc", EntryPoint = "setsid", SetLastError = true)]
        public static extern int SetSid();
        [DllImport("libc", EntryPoint = "kill", SetLastError = true)]
        public static extern int Kill(int processId, int signal);
    }
}
