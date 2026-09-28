# Managed static collector

This executable uses generated, typed field access and managed byrefs. It supports CoreCLR and NativeAOT. It does not emit dynamic IL, scan object bytes, execute properties, or treat a class reference as the object's storage.

From the repository root, with the pinned SDK on PATH:

```powershell
dotnet run --project tools/layout-observer/dotnet/LayoutObserver.Managed -c Release -- --output managed.json --configuration Release --run-id local-release
dotnet run --project tools/layout-observer/dotnet/LayoutObserver.ManagedChecks -c Release
dotnet publish tools/layout-observer/dotnet/LayoutObserver.Managed -c Release -r win-x64 -p:PublishAot=true
tools/layout-observer/dotnet/LayoutObserver.Managed/bin/Release/net10.0/win-x64/publish/LayoutObserver.Managed.exe --self-test
```

`--configuration` validates the configuration embedded at compilation. It does not relabel the executable. Actual process architecture and runtime are reported. Source/artifact provenance is explicitly unknown until the orchestrator enriches the snapshot. Capture success is not a comparison verdict; the snapshot deliberately contains unsupported and partial-coverage cases.

The source generator discovers assembly registrations such as:

```csharp
[assembly: ObserveLayout(typeof(MyRecord), "my-record")]
[assembly: ObserveLayout(typeof(GenericRecord<int>), "generic-int")]

public partial struct MyRecord
{
    private byte tag;
    private int value;
}
```

Reference `LayoutObserver.Managed` for the attribute/runtime helpers and `LayoutObserver.Generator` as an analyzer (`OutputItemType="Analyzer"`, `ReferenceOutputAssembly="false"`). Partial top-level structs receive a generated static helper inside their declaration, so ordinary private and readonly fields remain accessible. Closed generic registrations generate one partial helper per original type and statically call its closed instantiations. No fields or layout attributes are injected. Public nonpartial structs and standalone primitives/enums also work.

Add `<CompilerVisibleProperty Include="Configuration" />` to the consumer project's `ItemGroup`. The generated registry is internal to each consuming assembly. Convert that assembly's results explicitly:

```csharp
var snapshot = ManagedCapture.CaptureRegistered(
    GeneratedProbeRegistry.Capture(),
    GeneratedProbeRegistry.CompilerVersion,
    GeneratedProbeRegistry.BuildConfiguration);
```

`ManagedCapture.Capture()` remains the built-in fixture entry point. `CaptureRegistered` includes only the supplied registrations, uses their compiler/configuration identity, and rejects empty/duplicate IDs or a mismatched requested configuration. `LayoutObserver.ManagedChecks/ConsumerChecks.cs` is an independently compiled consumer example; its own generated registry measures a private readonly Pack1 record and a scalar without importing built-in observations.

The minimal standalone consumer has no Core/comparer dependency and can itself be published with NativeAOT:

```powershell
dotnet run --project tools/layout-observer/dotnet/LayoutObserver.ManagedChecks/ConsumerSmoke -c Release -- --output consumer.json
dotnet publish tools/layout-observer/dotnet/LayoutObserver.ManagedChecks/ConsumerSmoke -c Release -r win-x64 -p:PublishAot=true
tools/layout-observer/dotnet/LayoutObserver.ManagedChecks/ConsumerSmoke/bin/Release/net10.0/win-x64/publish/ConsumerSmoke.exe --output consumer-aot.json
```

The initial generator requires adapters for nested declared types, ref structs, fixed buffers/pointer fields, and compiler-generated backing fields. It preserves inaccessible fields as unknown rather than dropping them; unsupported registered types stay in the output. Inline arrays with statically known element types are measured through actual indexed byrefs and expanded into bidirectional child observations. Arrays of references do not represent inline object bodies. Only explicitly registered nested records are expanded; otherwise their unobserved internal representation remains unavailable to a complete comparison.

Fixtures cover Pack4/Pack1, reordering, nested values, a real inline `int[3]`, enums, GC reference fields, Explicit overlap, private fields, closed generics, `bool`, an empty value, and deliberate unsupported/partial cases. The checks compare documented blittable offsets with `Marshal` only for those fixtures; the separate Boolean check demonstrates that unmanaged marshalling differs. A compacting-GC check retains managed interior byrefs into one object. No alignment is inferred from an incidental address or wrapper offset.

`LayoutObserver.ManagedChecks` runs independent layout assertions, validates protocol relations, and verifies that incomplete private-field/alignment knowledge cannot become a successful equality verdict. Optional arguments are additional snapshot files to validate, including output from the NativeAOT binary.

## Executed architecture matrix

On 2026-09-28, SDK **10.0.401**, runtime **10.0.12**, Windows x64 host:

| Target | Execution mode | Actual validation |
| --- | --- | --- |
| Windows x64 | CoreCLR Debug / Release | Built and executed; independent fixture and protocol checks passed |
| Windows x86 | CoreCLR Release, self-contained | Published and executed under Windows; 32-bit process identity and fixture checks passed |
| Windows x64 | NativeAOT Release | Published native binary executed; fixture checks passed with no AOT/trim warnings |
| Windows x86 | NativeAOT Release | Published native binary executed; 32-bit identity and fixture checks passed with no AOT/trim warnings |
| Windows ARM64 | CoreCLR / NativeAOT | Not executed: no ARM64 runner; a required matrix cell must fail as unavailable |

The runtime version above was read from the installed SDK/runtime and actual process output, then explicitly pinned for the x86 publish. For example:

```powershell
dotnet publish tools/layout-observer/dotnet/LayoutObserver.Managed -c Release -r win-x86 --self-contained true -p:RuntimeFrameworkVersion=10.0.12
tools/layout-observer/dotnet/LayoutObserver.Managed/bin/Release/net10.0/win-x86/publish/LayoutObserver.Managed.exe --self-test
```

NativeAOT uses `-p:PublishAot=true` in place of the self-contained switch. Its compiler/analyzer assembly remains a compile-time `netstandard2.0` dependency, not part of the native target. On Windows, the standard `OS=Windows_NT` environment variable must be present for the native SDK's platform check. This is an actual Windows build prerequisite, not an unsupported-cross-OS override.

The reference-containing struct measured **16 bytes with 8-byte reference slots** on x64 and **12 bytes with 4-byte slots** on x86. These are observations of these builds; Debug/Release or JIT/AOT comparisons need not necessarily differ. To validate the four actual snapshot identities, protocol, JIT/AOT comparisons and the expected reference-width difference:

```powershell
dotnet run --project tools/layout-observer/dotnet/LayoutObserver.ManagedChecks -c Release -- --architecture-matrix coreclr-x64.json coreclr-x86.json nativeaot-x64.json nativeaot-x86.json
```

Builds alone do not fill a matrix cell: the target binary must execute. The collector checks the requested configuration against its embedded configuration and reports actual process architecture. Existing output paths are rejected rather than overwritten.
