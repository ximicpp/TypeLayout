# CoreCLR object collector

The collector launches its own `ObjectHost` process, waits for its factory/compacting-GC barrier, obtains a frozen process snapshot, and rediscovers strongly rooted wrappers by logical case ID inside that snapshot. No raw object addresses cross the handshake. It never inspects an unsuspended live heap or injects code into another application.

```powershell
dotnet build tools/layout-observer/dotnet/LayoutObserver.ClrMd -c Release
dotnet run --no-build --project tools/layout-observer/dotnet/LayoutObserver.ClrMd -c Release -- --host tools/layout-observer/dotnet/LayoutObserver.ObjectHost/bin/Release/net10.0/LayoutObserver.ObjectHost.dll --configuration Release --output coreclr-objects.json
dotnet run --project tools/layout-observer/dotnet/LayoutObserver.ClrMdChecks -c Release -- tools/layout-observer/dotnet/LayoutObserver.ObjectHost/bin/Release/net10.0/LayoutObserver.ObjectHost.dll
```

Use `--dotnet` to select the matching runtime host explicitly; otherwise the collector locates the host for its own runtime. `--dac` requests a specific DAC with ClrMD's mismatch checks enabled. Missing/mismatched DACs fail; there is no fallback to a moving live process. `--require-runtime`, `--configuration`, `--run-id`, and `--timeout-ms` are supported. The latter bounds readiness; the outer run manifest also bounds the entire collector operation. The process created by the collector is released or terminated in `finally`, including failure paths. Existing output files are never overwritten.

This adapter is calibrated for **CoreCLR 10**, using **Microsoft.Diagnostics.Runtime 4.1.745802**. ClrMD chooses the matching DAC. Header regions and origin conversion bind to the CoreCLR 10 object model, with the source contract in `dotnet/runtime`'s `src/coreclr/vm/object.h`. Unsupported runtime major versions fail calibration. Module/SDK provenance beyond the host artifact hash is left for the orchestrator to enrich.

The fixtures cover an empty object; private base/derived fields; plain and reference-containing boxed structs; byte, integer and reference arrays; and strings of different lengths. The host supplies independent typed-byref offsets and field distances, never addresses. The collector checks those against frozen ClrMD field addresses and checks variable-size slopes. Instance members use `object-reference` origin: ClrMD `GetAddress(object, false)` adds the method-table pointer relative to `Offset`. The negative ObjHeader region is represented explicitly.

`runtimeReportedObjectBytes` is exactly `ClrObject.Size`. It includes the runtime base size but must not be relabeled as allocation stride or retained memory. On the calibrated x64 target, `byte[1]` reports 25 bytes and a two-character string reports 26; allocation traversal may round further. Class references remain reference slots, and reference targets are not recursively included. Alignment stays unknown. The inspector reads no object-header state into the snapshot or fingerprint.

Checks validate protocol/ranges, repeat capture in a fresh child, factory failure, readiness timeout, missing host/DAC, an invalid existing DAC artifact, and configuration/runtime mismatch. These deliberately injected failures produce no successful snapshot. The logical-ID registry is fixture code; adding business types requires adding an explicit factory to the controlled host, not arbitrary reflection construction.
