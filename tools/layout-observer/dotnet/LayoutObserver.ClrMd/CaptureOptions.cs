namespace LayoutObserver.ClrMd;

public sealed record CaptureOptions(string HostPath, string? DotnetPath = null,
    string? DacPath = null, string? RequiredRuntimeVersion = null,
    int TimeoutMilliseconds = 30000, string? Configuration = null,
    string? RunId = null, bool FailFactory = false, int DelayReadyMilliseconds = 0);

public sealed class CaptureException(string code, string message, Exception? inner = null) : Exception(message, inner)
{
    public string Code { get; } = code;
}
