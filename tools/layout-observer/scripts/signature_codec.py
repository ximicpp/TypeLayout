"""Independent byte encoder for layout-signature-v1 payloads (not a signature validator)."""
import hashlib


def canonical_bytes(value):
    """Encode exact Int64 JSON with ordinal UTF-16 keys and fixed ASCII escapes."""
    def string(text):
        # Strict encoding rejects isolated UTF-16 surrogates instead of replacing
        # them and accidentally giving different identifiers the same bytes.
        utf16 = text.encode("utf-16-be", errors="strict")
        output = ['"']
        for index in range(0, len(utf16), 2):
            unit = int.from_bytes(utf16[index:index + 2], "big")
            if unit in (34, 92):
                output.append("\\" + chr(unit))
            elif unit < 32 or unit > 126:
                output.append(f"\\u{unit:04x}")
            else:
                output.append(chr(unit))
        output.append('"')
        return "".join(output)

    def encode(node):
        if node is None:
            return "null"
        if isinstance(node, bool):
            return "true" if node else "false"
        if isinstance(node, int):
            if not -(2**63) <= node < 2**63:
                raise ValueError("Payload integer is outside Int64")
            return str(node)
        if isinstance(node, str):
            return string(node)
        if isinstance(node, list):
            return "[" + ",".join(encode(item) for item in node) + "]"
        if isinstance(node, dict):
            if any(not isinstance(key, str) for key in node):
                raise ValueError("Payload keys must be strings")
            keys = sorted(node, key=lambda key: key.encode("utf-16-be", errors="strict"))
            return "{" + ",".join(string(key) + ":" + encode(node[key]) for key in keys) + "}"
        raise ValueError("Payload contains a non-Int64 number or non-JSON value")

    return encode(value).encode("ascii")


def payload_digest(payload):
    return hashlib.sha256(canonical_bytes(payload)).hexdigest()
