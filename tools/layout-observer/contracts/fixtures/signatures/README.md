# Signature conformance vectors

These are protocol fixtures, not measurements of a compiler or runtime. Their payloads are hand-specified; canonical bytes and SHA-256 expectations are independently checked with the Python encoder.

| Files | Purpose |
| --- | --- |
| `scalar.*.json` | A complete scalar snapshot, selection manifest, signature and canonical byte vector |
| `unicode-record.*.json` | A record whose logical field names exercise Unicode and structural delimiters |
| `unicode-int64.codec.json` | UTF-16 key ordering, supplementary Unicode, control characters, Int64 endpoints and 2^53+1 |

`*.codec.json` contains `input`, exact ASCII `canonical` text, and lowercase `sha256`. It tests the byte encoder; the arbitrary codec input need not itself be a layout payload. `*.signature.json` must pass the full signature schema and semantic validator.

From `tools/layout-observer`:

```sh
dotnet run --project dotnet/LayoutObserver.SignatureChecks -c Release
python -m pip install -r scripts/requirements-validation.txt
python scripts/check-signature-vectors.py
```

The .NET suite also checks signature generation against the hand-specified wire files, rejection cases, and comparison parity. Passing these vectors verifies protocol behavior, not the accuracy of a new adapter's measurements; its declared collector capabilities still need an independent measurement oracle.
