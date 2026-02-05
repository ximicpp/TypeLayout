# TypeLayout Signature Guarantee Diagram

## The Core Guarantee

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        TypeLayout Signature Guarantee                        │
│                                                                             │
│   "Same Signature ⟺ Same Memory Layout"                                    │
│                                                                             │
│   ┌──────────────────────────────────────────────────────────────────────┐ │
│   │                                                                      │ │
│   │    struct A              struct B              struct C             │ │
│   │    ┌────────────┐        ┌────────────┐        ┌────────────┐       │ │
│   │    │ int x;     │        │ int x;     │        │ int x;     │       │ │
│   │    │ double y;  │        │ double y;  │        │ float y;   │       │ │
│   │    │ char z[8]; │        │ char z[8]; │        │ char z[8]; │       │ │
│   │    └────────────┘        └────────────┘        └────────────┘       │ │
│   │                                                                      │ │
│   │         │                      │                      │             │ │
│   │         ▼                      ▼                      ▼             │ │
│   │                                                                      │ │
│   │    Signature:            Signature:            Signature:           │ │
│   │    0xABCD1234            0xABCD1234            0x5678EFGH           │ │
│   │                                                                      │ │
│   │         │                      │                      │             │ │
│   │         └──────────┬───────────┘                      │             │ │
│   │                    │                                  │             │ │
│   │                    ▼                                  ▼             │ │
│   │              ┌──────────┐                      ┌──────────┐         │ │
│   │              │  MATCH   │                      │ MISMATCH │         │ │
│   │              │    ✓     │                      │    ✗     │         │ │
│   │              └──────────┘                      └──────────┘         │ │
│   │                                                                      │ │
│   │         A and B are                        C has different          │ │
│   │         layout-compatible                  layout (float vs double) │ │
│   │                                                                      │ │
│   └──────────────────────────────────────────────────────────────────────┘ │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Signature Components

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          What Goes Into a Signature?                         │
│                                                                             │
│   struct Example {                                                          │
│       int32_t id;         ────────┐                                         │
│       double value;       ────────┼──► Member Types                         │
│       char name[32];      ────────┘                                         │
│   };                                                                        │
│                                                                             │
│   Signature captures:                                                       │
│                                                                             │
│   ┌─────────────────┬──────────────┬─────────────────┬──────────────────┐   │
│   │  Member Types   │   Offsets    │    Alignment    │   Total Size     │   │
│   ├─────────────────┼──────────────┼─────────────────┼──────────────────┤   │
│   │ int32_t         │ offset: 0    │ alignof: 4      │                  │   │
│   │ double          │ offset: 8    │ alignof: 8      │  sizeof: 48      │   │
│   │ char[32]        │ offset: 16   │ alignof: 1      │                  │   │
│   └─────────────────┴──────────────┴─────────────────┴──────────────────┘   │
│                                                                             │
│                              │                                              │
│                              ▼                                              │
│                                                                             │
│                    ┌───────────────────────┐                                │
│                    │   128-bit Dual Hash   │                                │
│                    │   ┌───────┬───────┐   │                                │
│                    │   │ FNV1a │ DJB2  │   │                                │
│                    │   │ 64bit │ 64bit │   │                                │
│                    │   └───────┴───────┘   │                                │
│                    │   = 0xABCD...1234     │                                │
│                    └───────────────────────┘                                │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Use Case: Cross-Process Verification

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Shared Memory IPC Safety Check                           │
│                                                                             │
│   ┌─────────────────────────┐         ┌─────────────────────────┐           │
│   │      Process A          │         │      Process B          │           │
│   │   (Data Producer)       │         │   (Data Consumer)       │           │
│   │                         │         │                         │           │
│   │  struct Message {       │         │  struct Message {       │           │
│   │    uint32_t type;       │         │    uint32_t type;       │           │
│   │    uint64_t timestamp;  │         │    uint64_t timestamp;  │           │
│   │    double payload[4];   │         │    double payload[4];   │           │
│   │  };                     │         │  };                     │           │
│   │                         │         │                         │           │
│   │  Signature: 0x1234ABCD  │         │  Signature: 0x1234ABCD  │           │
│   │         │               │         │         │               │           │
│   └─────────┼───────────────┘         └─────────┼───────────────┘           │
│             │                                   │                           │
│             │         ┌──────────────┐          │                           │
│             └────────►│ Shared Memory │◄────────┘                           │
│                       │              │                                      │
│                       │  Header:     │                                      │
│                       │  ┌────────┐  │                                      │
│                       │  │0x1234AB│  │  ◄── Signature stored in header      │
│                       │  │  CD    │  │                                      │
│                       │  └────────┘  │                                      │
│                       │              │                                      │
│                       │  Data:       │                                      │
│                       │  ┌────────┐  │                                      │
│                       │  │Message │  │                                      │
│                       │  │ data...│  │                                      │
│                       │  └────────┘  │                                      │
│                       │              │                                      │
│                       └──────────────┘                                      │
│                                                                             │
│   At runtime:                                                               │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ static_assert(get_layout_hash<Message>() == EXPECTED_HASH);        │   │
│   │                                                                     │   │
│   │ // Compile-time check: If Message struct changed, compilation fails │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Compile-Time vs Runtime

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      When Does Verification Happen?                          │
│                                                                             │
│   ┌───────────────────────────────┐    ┌───────────────────────────────┐    │
│   │        COMPILE TIME           │    │          RUNTIME              │    │
│   │                               │    │                               │    │
│   │  Source Code                  │    │  Executable                   │    │
│   │       │                       │    │       │                       │    │
│   │       ▼                       │    │       ▼                       │    │
│   │  ┌──────────────┐             │    │  ┌──────────────┐             │    │
│   │  │ TypeLayout   │             │    │  │   No cost!   │             │    │
│   │  │ Reflection   │             │    │  │              │             │    │
│   │  │ + Hashing    │             │    │  │  Hash is a   │             │    │
│   │  └──────────────┘             │    │  │  constant    │             │    │
│   │       │                       │    │  │              │             │    │
│   │       ▼                       │    │  └──────────────┘             │    │
│   │  ┌──────────────┐             │    │                               │    │
│   │  │ static_assert│             │    │  Equivalent to:               │    │
│   │  │ or constexpr │             │    │                               │    │
│   │  │ comparison   │             │    │  const uint64_t hash =        │    │
│   │  └──────────────┘             │    │      0x1234567890ABCDEF;      │    │
│   │       │                       │    │                               │    │
│   │       ▼                       │    │  // Just a constant!          │    │
│   │  ┌──────────────┐             │    │  // Zero computation          │    │
│   │  │ Compile error│             │    │                               │    │
│   │  │ if mismatch  │             │    │                               │    │
│   │  └──────────────┘             │    │                               │    │
│   │                               │    │                               │    │
│   │  Cost: ~0.1s per type         │    │  Cost: 0 cycles               │    │
│   │                               │    │                               │    │
│   └───────────────────────────────┘    └───────────────────────────────┘    │
│                                                                             │
│   TypeLayout = "Pay at compile time, free at runtime"                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## SVG Version (for web documentation)

For rendered documentation, use the following SVG diagram:

```html
<!-- Include in web docs as: <img src="signature_guarantee.svg" alt="TypeLayout Signature Guarantee"> -->
```

See `signature_guarantee.svg` for the vector version.
