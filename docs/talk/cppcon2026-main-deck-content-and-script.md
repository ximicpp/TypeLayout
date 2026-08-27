# CppCon 2026 Main-Deck Content and Speaker Script

**Talk:** *Can I memcpy This Type Across a Boundary? Verifying Object Representation at Compile Time With C++26 Reflection*

**Status:** content design only. This document fixes the audience-facing content and the English speaker script for Slides 1–45. It does not define the final layout, artwork, animation, or PowerPoint implementation. Appendix Slides 46–61 keep the outline in the approved deck design and do not yet have full scripts.

**Delivery target:** about 50–55 minutes for the main deck, leaving time for the appendix and questions.

**Staging rule:** when an **On screen** section contains several blocks, treat them as successive reveal states. Keep the current inference visible and move explanatory detail into the speaker script instead of showing every block at once.

**Speaker-language rule:** the audience is a technical C++ audience, but the speaker is not a native English speaker. Keep the technical content exact. Use short sentences, common words, stable terms, and clear pause points. Avoid contractions, idioms, and long nested clauses.

**Delivery-load rule:** each slide is a speaking map, not a transcript. Every spoken beat should follow a visible title, code line, table cell, formula, or reveal. Prefer about 60–85 spoken words on an ordinary one-minute slide. Use longer scripts only when visible code or data provides the speaking path. Keep necessary technical content, but remove repeated explanation. Include the transition and short pauses in the time budget.

**Communication job:** By the end, C++ engineers should be able to decide when native object bytes may cross a declared boundary, because they understand the separate Admission and Agreement checks and the limited scope of the resulting Permit.

**Source order:**

1. `docs/talk/cppcon2026-sched-listing.md`
2. `docs/superpowers/specs/2026-08-23-cppcon2026-typelayout-deck-design.md`
3. repository code, tests, and retained signature artifacts

## Stage 1 — Why Evidence Is Needed

### Slide 1 — Can I memcpy this type across a boundary?

**Target time:** 50 seconds

**On screen**

```text
Can I memcpy This Type Across a Boundary?

Checking native-byte compatibility at compile time
with C++26 reflection

Fanchen Su · CppCon 2026
```

The title is the only main content. A short strip of bytes may support it visually later.

**Speaker script**

Good morning. I want to start with one question.

Can I copy this C++ object as raw bytes across a boundary?

Native C++ types often become byte formats in shared memory, plugin interfaces, and files.

Across a boundary, the build or address space may change. Then local checks are not enough.

C++26 reflection lets each build create compile-time evidence. A verification build uses that evidence to decide.

**Transition to Slide 2**

Let us start with a type that looks safe.

### Slide 2 — Would you permit these bytes across all declared builds?

**Target time:** 65 seconds

**On screen**

```cpp
struct Measurement {
    std::uint64_t id;
    long double value;
};

static_assert(std::is_trivially_copyable_v<Measurement>);
```

```text
✓ trivially copyable
✓ no pointers
✓ no ownership
✓ no virtual functions

PERMIT ACROSS BUILDS?
```

**Speaker script**

Here is a small record: a 64-bit unsigned ID and a `long double` value.

The checklist looks good. The type is trivially copyable, with no pointer, ownership, or virtual function.

On this build, a raw byte copy looks valid.

But several builds may produce or consume these bytes. Should we permit transfer across all of them?

A local `memcpy` is not enough. Can these native bytes serve as a format across builds?

We do not have the evidence yet. Permit, or reject?

**Transition to Slide 3**

The key word in the title is not `memcpy`. It is `boundary`.

### Slide 3 — At a boundary, representation becomes a contract

**Target time:** 70 seconds

**On screen**

```text
producer                 boundary                 consumer

C++ object  ───────→  native bytes  ───────→  C++ object

implementation detail   binary contract          needs evidence
```

```text
A boundary is where the consumer cannot assume
the producer's build or address space.
```

**Speaker script**

Inside one build, object layout is usually an implementation detail. The compiler chooses it.

At a boundary, another consumer receives the bytes. It may use another build or address space. It may also read them later.

Now, the object representation is part of a binary contract.

Here, boundary means the consumer cannot assume the producer's build or address space.

Examples include another process, a separately built plugin, a file, or a consumer built for another ABI.

Once representation becomes part of a contract, we need evidence.

**Transition to Slide 4**

We must track two separate assumptions.

### Slide 4 — Build identity and address space are separate

**Target time:** 65 seconds

**On screen**

| | Same address space | Different address space |
|---|---|---|
| **Same build** | local case | same executable, two processes |
| **Different build** | Plugin / Host | another build reads stored bytes |

```text
A boundary may change the build,
the address space, or both.
```

**Speaker script**

First, separate two questions. Is the build the same? Is the address space the same?

Top left: both are the same. This is the local case.

Top right: the same build runs in two processes. The address spaces are different.

Bottom left: a host and plugin share one address space, but come from different builds.

Bottom right: another build reads stored bytes in another address space.

The cases fail for different reasons. Pointers depend on the address space. Object representation may depend on the build. Either difference can reject raw-byte transfer.

**Transition to Slide 5**

First, keep the build and change the address space.

### Slide 5 — A new process keeps layout, but not the pointer target

**Target time:** 70 seconds

**On screen**

```text
same executable

Process A                         Process B
─────────                         ─────────
pointer bits: 0x7F20...  ─────→   pointer bits: 0x7F20...
target: real object               target: unknown

build identity: retained
address-space identity: lost
```

**Speaker script**

Now keep the build and change only the address space.

Two processes run the same executable, so the layout is the same.

Copy a pointer value from Process A to Process B. Every bit survives. The field keeps the same size, alignment, and offset.

But the target object belongs to Process A. In Process B, the same address may name another object, or no object.

The representation matches, but the value depends on its source address space.

The bits survived. The target did not.

**Transition to Slide 6**

Now keep the address space and change the build.

### Slide 6 — One address space does not guarantee one layout

**Target time:** 70 seconds

**On screen**

```text
one process

Host build H  ←──── shared address space ────→  Plugin build P

build identity: lost
address-space identity: retained
```

```text
compiler · flags · headers · packing · standard-library ABI
```

```text
Pointers may still work, but layout Agreement is not automatic.
```

**Speaker script**

Now keep one address space, but change the build.

A plugin and its host can share one process. An agreed pointer may still name the same object.

But they are separate builds. The compiler, flags, headers, packing, or standard-library ABI may differ.

A shared address space does not guarantee matching offsets, bit-fields, or `long double` representation.

The pointer may work while the layouts differ.

Later, one strict profile will cover all three cases without relying on shared addresses.

**Transition to Slide 7**

Stored bytes can remove both assumptions.

### Slide 7 — Stored bytes outlive builds and address spaces

**Target time:** 60 seconds

**On screen**

```text
Writer build A  ───→  capture.bin  ───→  Reader build B
today                                      later

build identity: lost
address-space identity: lost
```

```text
Other obligations: versioning · trust · crash consistency
```

**Speaker script**

A file can lose both assumptions.

Build A writes native bytes. Build B reads them later, perhaps on another target.

The first address space is gone, so local references may fail. The build may also change, so the layout may differ.

Stored data also needs versioning, trust, durability, and crash consistency. Those topics are outside this talk.

Here we ask only two questions. May the type travel as native bytes? Did every declared build produce the same representation?

**Transition to Slide 8**

To answer both questions, we need one clear transfer profile.

### Slide 8 — One strict profile covers all three cases

**Target time:** 65 seconds

**On screen**

```text
Process IPC ─┐
Plugin/Host ─┼─→ strict native-byte profile
Stored bytes ─┘
```

```text
memcpy-style object transfer
no fixup or field conversion
source-address-independent bytes
finite declared build set
```

```text
Two questions
1. May the bytes stand without producer-local context?
2. Do all declared builds produce the same object representation?
```

**Speaker script**

This talk uses one strict transfer profile for all three cases.

It uses `memcpy`-style object transfer. There is no fixup and no field conversion. The bytes cannot depend on the source address space. The build set is finite and declared.

Some systems support relocation or agreed local references. They need a different profile and different evidence.

Under this strict profile, two questions remain. Can the bytes stand on their own? Are they the same on every declared build?

**Transition to Slide 9**

These two questions lead to seven steps.

### Slide 9 — Seven steps turn the question into a decision

**Target time:** 55 seconds

**On screen**

```text
Boundary
→ Local checks cannot compare builds
→ Each build emits canonical evidence
→ Check Agreement
→ Check Admission
→ CI checks the full contract
→ Issue a narrow Permit
```

**Speaker script**

This is the full path through the talk.

A boundary creates the question. Local checks cannot compare builds, so each build emits canonical evidence.

We use that evidence to check Agreement. We check Admission separately.

Then CI checks the full declared contract.

Only after every check passes do we issue a narrow Permit.

**Transition to Slide 10**

First, look at the trait we often trust: `trivially_copyable`.

### Slide 10 — `trivially_copyable` is local; it does not compare builds

**Target time:** 65 seconds

**On screen**

```text
Build A                              Build B

is_trivially_copyable_v<T>           is_trivially_copyable_v<T>
              true                                true

        local fact                         local fact
```

```text
Two local results do not compare the builds.
```

**Speaker script**

`std::is_trivially_copyable_v<T>` answers an important local question. It tells us whether this type supports the ordinary byte-copy operation.

Build A checks itself. Build B does the same. Both results may be true, but the builds may still use different layouts.

The trait does not compare offsets, alignment, or the representation of `long double`.

We still need it for Admission. But it cannot prove Agreement between builds.

**Transition to Slide 11**

Maybe `sizeof` can compare the builds. It helps, but only in one direction.

### Slide 11 — `sizeof` can reject a match; equal size cannot prove it

**Target time:** 60 seconds

**On screen**

```text
sizeof_A(T) != sizeof_B(T)  →  representation differs
sizeof_A(T) == sizeof_B(T)  →  still unknown
```

```cpp
struct alignas(16) SameSize {
    char tag;
    wchar_t code;
    char tail;
};
```

```text
Build A: sizeof = 16   code @2   tail @4
Build B: sizeof = 16   code @4   tail @8
```

```text
Equal size can hide different offsets and leaf layouts.
```

**Speaker script**

Different sizes prove that two representations differ. Equal size does not prove a match.

This record has 16-byte alignment, so both builds report a total size of 16.

With a two-byte `wchar_t`, `code` starts at offset two and `tail` at four.

With a four-byte `wchar_t`, `code` starts at four and `tail` at eight.

The total size is equal, but the offsets and leaf representation differ. Equal size can also hide padding, bit layout, or alignment differences.

Size can reject a mismatch. It cannot approve a match.

**Transition to Slide 12**

We need each build to describe the representation it produced.

**[Sources for Slides 10–11]**

- C++ working draft N5032, `[basic.types]`, for the local byte-copy guarantees of trivially copyable types.
- Repository examples under `example/sigs/` for equal-size and platform-dependent layout comparisons.

## Stage 2 — How One Build Produces Evidence

### Slide 12 — A useful signature must meet four rules

**Target time:** 80 seconds

**On screen**

```text
Complete
Record every required representation fact.

Canonical form
The same facts produce the same normalized form.

Detect differences
A changed fact changes the certificate.

Fail closed
A missing required fact stops generation.
```

```text
Not a hash of the type name.
A certificate for one supported representation domain.
```

**Speaker script**

Before we build a signature, we need four rules.

Complete means every required representation fact is present.

Canonical means the same supported facts produce the same form.

Detect differences means a changed fact changes the certificate.

Fail closed means generation stops when a required fact is unavailable. Partial evidence must not look complete.

This is not a hash of the type name. A name does not prove a layout.

The certificate covers one supported representation domain. It does not model every C++ type.

**Transition to Slide 13**

The first source is the compiler that laid out the type.

### Slide 13 — The compiler gives us the byte map

**Target time:** 85 seconds

**On screen**

```cpp
struct PacketHeader {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t type;
    std::uint32_t payload_len;
    std::uint32_t checksum;
};
```

```text
compiler-produced byte map

0        4      6      8             12            16
| magic  | ver  | type | payload_len | checksum     |

sizeof = 16    alignof = 4
```

```text
Do not guess layout from declaration order.
Ask the compiler what it produced.
```

```text
ordinary C++ type · no IDL · no generated serialization stubs
```

**Speaker script**

The declaration gives us the members and their types. It does not give the final byte map for this build.

Here, the compiler places `magic` at zero. The two 16-bit fields start at four and six. The last two fields start at eight and twelve.

The full record is 16 bytes, with alignment four.

These are facts from this build. Another compiler or ABI may make another valid choice.

TypeLayout asks the current compiler instead of guessing from declaration order. Each declared build produces its own evidence.

The input stays an ordinary C++ type. There is no second IDL, generated serialization stub, or runtime inspection.

**Transition to Slide 14**

C++26 reflection gives us facts. A recursive walk gives them structure.

### Slide 14 — Reflection gives facts; recursion builds structure

**Target time:** 70 seconds

**On screen**

```cpp
nonstatic_data_members_of(^^T)
type_of(member)
offset_of(member)
bases_of(^^T)
bit_size_of(member)
```

```text
enumerate
→ recover type
→ read position
→ classify
→ recurse
```

```text
reflection: compiler facts
TypeLayout: normalization policy
```

**Speaker script**

C++26 reflection lets constant-evaluated code inspect the compiler's type.

For a record, we list its data members and bases. For each member, we read its type and position. For a bit-field, we also read its width.

Then we classify the type. If it contains more structure, we recurse.

Reflection supplies compiler facts. TypeLayout chooses which facts to keep and how to normalize them.

Reflection alone does not define compatibility. The policy builds a certificate that another build can produce and CI can compare.

**Transition to Slide 15**

The policy needs one clear result for every supported kind.

### Slide 15 — One consteval dispatcher handles every supported kind

**Target time:** 65 seconds

**On screen**

```text
leaf         → encode canonical token + layout
enum         → encode enum + underlying representation
array        → encode extent + element representation
record       → encode record + recurse
union        → encode supported union facts
opaque       → emit a named trust contract
unsupported  → compile-time rejection
```

```text
There is no “probably compatible” fallback.
```

**Speaker script**

The dispatcher gives one clear result for every supported kind.

A leaf becomes a canonical token with size and alignment.

An enum records its underlying representation. An array records its extent and element representation. A record or supported union records its layout and then recurses.

For an opaque type, the application must provide a named trust contract.

An unsupported kind causes a compile-time error. There is no fallback based only on size.

That keeps the check fail closed.

**Transition to Slide 16**

At a leaf, we record representation, not source spelling.

### Slide 16 — Leaf tokens describe representation, not spelling

**Target time:** 65 seconds

**On screen**

| Source example | Canonical token | Recorded layout |
|---|---:|---:|
| `std::uint32_t`, alias of same type | `u32` | `s:4,a:4` |
| IEC 559 `float` | `f32` | `s:4,a:4` |
| x86 extended `long double` | `fld80` | `s:16,a:16` |
| object pointer | `ptr` | build-local size/alignment |

```text
member names and typedef spellings are not recorded
```

**Speaker script**

A leaf token describes representation, not source spelling.

An alias of `std::uint32_t` still becomes `u32`. A supported 32-bit float becomes `f32`. The x86 extended format becomes `fld80`.

A pointer gets the token `ptr`. Its local size and alignment are recorded too.

Size and alignment stay explicit beside every token.

We do not record member names or typedef spellings. This is representation evidence, not schema identity.

The appendix contains the exact rules for the less common leaf types.

**Transition to Slide 17**

We use the same idea for position. Keep the byte fact, and remove the source path.

### Slide 17 — Absolute offsets remove unneeded source paths

**Target time:** 75 seconds

**On screen**

```text
Nested:   outer.inner.value     parent @8 + child @4
Base:     Derived::Base::value  base @8   + child @4
Flat:     value                 root @12

                ↓ normalize

             @12:i32[s:4,a:4]
```

```text
absolute leaf offset = parent absolute offset + child offset
```

```text
Normalization removes source paths,
but keeps every byte position.
```

**Speaker script**

A leaf may be nested, inherited from a base, or stored directly in the record.

Those source paths differ. The byte-transfer check needs the final position inside the complete object.

So we add offsets. A parent at eight plus a child at four gives absolute offset twelve.

We remove the source path, but keep the byte position.

This does not prove equal schema or meaning. A protocol that needs names or nesting still needs an explicit schema.

**Transition to Slide 18**

If the walk cannot record a required fact, it must stop.

### Slide 18 — If required layout facts are hidden, reject the type

**Target time:** 75 seconds

**On screen**

```cpp
struct Base { std::uint32_t id; };
struct Derived : virtual Base { std::uint32_t flags; };
```

```text
required layout facts are hidden
→ the signature cannot encode them all
→ compile-time rejection
```

```text
Encode visible structure.
Name explicit trust.
Reject missing facts.
```

**Speaker script**

Virtual inheritance is a fail-closed example. Its final representation uses hidden implementation details that this signature does not encode.

We must not record only the visible fields. That would make partial evidence look complete.

So TypeLayout rejects signature generation for this type.

This does not prove that transfer is impossible. It means this check lacks the required evidence.

The rule is simple. Encode supported visible structure. Name an explicit trust contract for an opaque region. Reject any required fact that remains uncovered.

**Transition to Slide 19**

For a supported type, the walk produces one readable certificate.

### Slide 19 — A consteval walk builds the certificate

**Target time:** 85 seconds

**On screen**

```text
[64-le]record[s:16,a:4]{
  @0:u32[s:4,a:4],
  @4:u16[s:2,a:2],
  @6:u16[s:2,a:2],
  @8:u32[s:4,a:4],
  @12:u32[s:4,a:4]
}
```

```text
[64-le]          pointer width + endianness
record[s:16,a:4] root size + alignment
@12              absolute byte offset
u32[s:4,a:4]     leaf token + leaf layout
```

**Speaker script**

Here is the certificate for `PacketHeader`.

The `[64-le]` prefix records pointer width and endianness. It is not a CPU or ISA name. CI will bind the exact producer identity later.

The record header gives the root size and alignment.

Each entry gives an absolute offset, a canonical token, and the leaf size and alignment.

The certificate stays readable. A hash may help with lookup, but the encoded facts remain the evidence.

Because generation stops when a required fact is missing, a completed certificate meets all four rules.

This certificate supports Agreement. The same walk also supplies facts for Admission.

**Transition to Slide 20**

That comparison gives us the first gate: Agreement.

**[Sources for Slides 12–19]**

- WG21 P2996R12 and P3687R1 for C++26 reflection facilities and wording.
- `include/boost/typelayout/detail/reflect.hpp`
- `include/boost/typelayout/detail/signature_impl.hpp`
- `include/boost/typelayout/detail/type_map.hpp`
- `include/boost/typelayout/signature.hpp`
- `example/compat_ci_types.hpp`
- `example/sigs/x86_64_linux_clang.sig.hpp`

## Stage 3 — How the Two Gates Decide One Edge

### Slide 20 — Agreement checks one key on one edge

**Target time:** 75 seconds

**On screen**

```text
Build A ───────────── declared transfer edge ───────────── Build B
```

```text
Agreement(K,A,B)
iff both artifacts use the registered key K
    and Signature(K,A) = Signature(K,B)
```

```text
same contract key
+ exact certificate equality
= Agreement on this declared edge
```

**Speaker script**

Agreement checks one registered key on one declared edge.

The key says that both artifacts refer to the same boundary type. The signature says what representation each build produced.

Both are required. Equal signatures under different keys do not join two application concepts. One key with different signatures exposes a layout change.

Within one signature domain and version, exact equality gives Agreement on this edge.

Agreement alone does not permit transfer. It also says nothing about the full build set.

**Transition to Slide 21**

Exact equality gives us a gate and a clear diagnostic.

### Slide 21 — Exact equality gives a gate and a clear diagnostic

**Target time:** 70 seconds

**On screen**

```text
MATCH
A  ... @8:u32[s:4,a:4] ...
B  ... @8:u32[s:4,a:4] ...

DIFFER
A  ... @8:f64[s:8,a:8] ...
B  ... @16:fld80[s:16,a:16] ...
             ^ first decisive difference
```

```cpp
static_assert(layout_match(
    linux_plat::PacketHeader_layout,
    macos_plat::PacketHeader_layout));
```

```text
No compatibility score is needed.
```

**Speaker script**

The certificate is canonical, so the comparison is exact.

If the strings match, every encoded fact matches. If they differ, the text shows the first useful difference.

Here, one build has `f64` at offset eight. The other has `fld80` at offset sixteen.

The `static_assert` fails when the stored signatures differ.

We do not need a score or a close-enough rule. A hash may speed up lookup, but the full certificate gives the diagnostic.

**Transition to Slide 22**

Now we must state what a match does not prove.

### Slide 22 — Agreement proves representation equality—nothing more

**Target time:** 70 seconds

**On screen**

```text
Agreement proves:
  encoded representation equality in the declared domain

Agreement does not prove:
  source identity
  application meaning
  source-context independence
```

```text
Matching certificates do not prove that the value can stand alone.
```

**Speaker script**

Agreement proves equality of the encoded representation in the declared domain.

It does not prove source identity. We removed names and source paths when they did not affect the byte map.

It does not prove application meaning. Reflection cannot discover those rules.

It also does not prove that the value can stand alone. Matching bits may still refer to producer-local state.

Agreement makes one exact claim, and nothing more.

**Transition to Slide 23**

A pointer shows the source-context problem.

### Slide 23 — A matching layout does not make a pointer transferable

**Target time:** 70 seconds

**On screen**

```cpp
struct BufferView {
    std::uint64_t size;
    const std::byte* data;
};
```

```text
Build A signature  MATCH  Build B signature
pointer bits       MATCH  pointer bits

Producer process: data → real buffer
Consumer process: data → no known target object
```

```text
Agreement MATCH / Admission FAIL

The pointer bits survived. The target object did not cross the boundary.
```

**Speaker script**

`BufferView` can have the same layout on two builds. The size field matches. The pointer token, offset, size, and alignment also match.

Agreement correctly reports a match.

Now move the bytes to another process. The pointer bits survive, but the consumer has no known object at that address.

The value depends on the producer's address space. Our strict profile rejects that dependency.

This is not a layout error. A separate check must reject it.

That check is Admission. Here, Agreement matches and Admission fails.

**Transition to Slide 24**

Admission is local to one build and one transfer profile.

### Slide 24 — Admission checks one type on one build under one profile

**Target time:** 65 seconds

**On screen**

```text
Admission_P(K,B)

K  registered boundary type
B  one actual build
P  ordinary copy · no fixup · source-address-independent
```

```text
Pointer rejection follows from this profile.
It is not a universal rule for every possible boundary.
```

**Speaker script**

Admission checks one registered type, one build, and one transfer profile.

The profile must be explicit. “Safe to transfer” has no clear meaning without it.

Our profile uses `memcpy`-style transfer. It performs no fixup, and the bytes cannot depend on the source address space.

An ordinary pointer fails this profile.

Another system may support shared addresses or relocation. It needs another profile and other evidence.

Pointers are not universally invalid. Pointer-dependent bytes simply fail the profile used in this talk.

**Transition to Slide 25**

Inside this profile, Admission needs three conditions.

### Slide 25 — Admission needs three separate conditions

**Target time:** 75 seconds

**On screen**

```text
Admission_P(T,B)
= LocalCopyLegal
  and NoDetectedSourceContextDependency
  and RepresentationEvidenceComplete
```

```text
LocalCopyLegal
  the local byte-copy operation is permitted

NoDetectedSourceContextDependency
  no encoded component requires producer-local context

RepresentationEvidenceComplete
  every reachable component is encoded or explicitly trusted
```

**Speaker script**

Admission has three conditions under this profile.

First, the local byte-copy operation must be legal.

Second, the structural check must find no dependency on producer-local context.

Third, every required representation fact must be encoded or explicitly trusted.

If a required fact is unsupported, signature generation fails. Admission cannot turn missing evidence into a pass.

These conditions are independent. They are also structural, so they cannot prove hidden application meaning.

**Transition to Slide 26**

The implementation combines these conditions in one local check.

### Slide 26 — The recursive check finds hidden structural problems

**Target time:** 65 seconds

**On screen**

```cpp
constexpr auto packet_header_signature =
    get_layout_signature<PacketHeader>();

static_assert(is_admitted_v<
    PacketHeader,
    TransferProfile::ordinary_copy>);
```

```text
FramedPacket
├─ PacketHeader
│  └─ fixed-width leaves
└─ words[4]
   └─ uint32_t

classify → check → recurse
```

```text
signature generation → complete representation evidence
is_admitted_v        → the type passes the local profile
```

**Speaker script**

For the ordinary-copy profile, `is_admitted_v` applies the three local conditions.

The type must be trivially copyable. It must be recursively byte-copy safe. Its source context must be independent.

The tree shows why recursion matters. A pointer may be hidden inside a nested record or an array element.

`is_byte_copy_safe_v<T>` is only one part of Admission.

Signature generation supplies the evidence-completeness condition. It proves that the required representation facts can be encoded.

Together, the signature and `static_assert` provide local evidence. Agreement must still compare the builds.

**Transition to Slide 27**

Even a complete structural check cannot see application meaning.

### Slide 27 — Reflection cannot see application meaning

**Target time:** 60 seconds

**On screen**

```cpp
struct FileReference {
    std::uint32_t descriptor;
};
```

```text
Structural Admission: may PASS
Application contract: may REJECT
```

```text
TypeLayout cannot recognize a handle disguised as an integer.
```

**Speaker script**

This type contains one fixed-width integer. Its structure looks self-contained, so structural Admission may pass.

But the application may use `descriptor` as an operating-system file descriptor.

Copying that integer to another process does not transfer the open file.

Reflection cannot learn this meaning from the type. The application may still reject the field.

This is why the talk claims representation compatibility, not semantic compatibility. Application rules remain the application's responsibility.

**Transition to Slide 28**

Now we can combine the two gates for one edge.

### Slide 28 — Admission and Agreement catch different failures

**Target time:** 80 seconds

**On screen**

| | Agreement MATCH | Agreement DIFFER |
|---|---:|---:|
| **Admission PASS at both ends** | **EDGE PASS** | REJECT |
| **Admission FAIL at either end** | REJECT | REJECT |

```text
EdgePass_P(K,A,B)
= Admission_P(K,A)
  and Admission_P(K,B)
  and Agreement(K,A,B)
```

```text
Assume valid evidence from the correct builds.
EDGE PASS is not the final Permit.
```

**Speaker script**

For one edge, both builds must pass Admission. Their signatures must also agree.

If either Admission fails, matching layouts cannot save the transfer. That is the pointer case.

If Agreement fails, local Admission cannot save it. That will be the `long double` case.

Only the top-left cell gives `EDGE PASS`.

The formula assumes valid evidence from the correct builds. CI will check that next.

`EDGE PASS` is not the final Permit. It covers one type on one edge, not the full contract.

**Transition to Slide 29**

One edge can pass both gates. We still need to check the full build set.

**[Sources for Slides 20–28]**

- `include/boost/typelayout/admission.hpp`
- `include/boost/typelayout/tools/compat_check.hpp`
- `example/compat_check.cpp`
- `test/test_core.cpp`
- C++ working draft N5032, `[basic.types]`, for the ordinary byte-copy condition.

## Stage 4 — How CI Closes the Finite Contract

### Slide 29 — A Permit needs a finite declared contract

**Target time:** 70 seconds

**On screen**

```text
C = (R, V, E, P)
```

| Part | Meaning |
|---|---|
| `R` | registered boundary-type keys |
| `V` | exact participating builds |
| `E` | required transfer edges |
| `P` | transfer profile |

```text
CI proves only the declared finite set.
It does not prove every present or future build.
```

**Speaker script**

A Permit must name its full scope. We write it as `C = R, V, E, P`.

`R` is the registered type keys. `V` is the exact build set. `E` is the required transfer edges. `P` is the transfer profile.

Each part changes the claim.

A new type needs another type decision. A new build needs another node. A new edge needs another Agreement check. A new profile changes Admission.

CI proves only this finite contract. It does not prove every compiler, target, or future ABI.

**Transition to Slide 30**

Every build in the set must report its own facts.

### Slide 30 — Every build must emit its own evidence

**Target time:** 65 seconds

**On screen**

```text
Linux x86-64 / GCC 16
  → Signature_B(K)
  → Admission facts

Linux x86-64 / Clang P2996
  → Signature_B(K)
  → Admission facts

Apple ARM64 / Clang P2996
  → Signature_B(K)
  → Admission facts
```

```text
No build can speak for another build.
```

**Speaker script**

Each real build compiles the type. It creates its own signature and runs its own Admission check.

Linux GCC cannot predict Linux Clang. Linux Clang cannot speak for Apple ARM64. No build may reuse a signature from another target.

The compiler and ABI are part of what we measure. One build cannot create evidence for every environment.

The builds may share one source registration. But reflection and local checks must run inside each declared build.

**Transition to Slide 31**

The artifact tells us what the build saw. Next, CI must identify the producer.

### Slide 31 — The header has evidence, not producer identity

**Target time:** 65 seconds

**On screen**

```cpp
inline constexpr TypeEntry types[] = {
    {
        "PacketHeader",
        PacketHeader_layout,
        PacketHeader_byte_copy_safe
    }
};
```

```text
emitted header
  contract key
  layout signature
  byte-copy-safe diagnostic fact

same build job
  signature generation must succeed
  ordinary-copy Admission must pass
```

```text
The header says what was observed.
CI must prove which build produced it.
```

**Speaker script**

The generated header contains the contract key, layout signature, and a byte-copy-safe diagnostic fact.

It is not the full Admission proof. In the same job, signature generation must succeed and the ordinary-copy Admission check must pass.

CI consumes both results: the successful local gate and the emitted artifact. It does not rebuild Admission from `TypeEntry` alone.

The header also cannot prove who made it. A string cannot prove the compiler, source revision, or build run.

So CI asks two questions. What did the build report? Which declared build produced that report?

**Transition to Slide 32**

The second question is provenance: where did the evidence come from?

### Slide 32 — CI ties evidence to the build that made it

**Target time:** 70 seconds

**On screen**

```text
declared build
        +
producer proof and local gate result
        +
artifact digest and current CI run
        ↓
accepted evidence input
```

```text
The artifact says what the build observed.
CI proves who produced it and when.
```

```text
Provenance checks the input.
It is not a third compatibility gate.
```

**Speaker script**

CI ties the artifact and local gate result to one declared build and one CI run.

The full record may include the source revision, compiler, target, ABI flags, job identity, and artifact digest. The appendix has the full list.

The main idea is simple. The artifact says what the build observed. CI proves who produced it and when.

CI checks provenance before Admission or Agreement. Provenance is not a third compatibility gate.

Missing or old provenance means CI cannot decide. The result is `INCOMPLETE`.

**Transition to Slide 33**

With valid inputs, CI applies both gates to the full graph.

### Slide 33 — CI checks both gates across the full graph

**Target time:** 75 seconds

**On screen**

```text
                 Agreement
Linux GCC  ───────────────────── Linux Clang
Admission PASS                  Admission PASS
     \                              /
      \          Agreement         /
       \                          /
        Apple ARM64 / Clang
           Admission PASS
```

```text
ClosedPermit_C(K)
= Admission_P(K,B) for every B in V
  and Agreement(K,A,B) for every (A,B) in E
```

```text
Make a separate decision for every K in R.
```

**Speaker script**

This is the one-edge rule applied to the full contract.

For one key `K`, every build in `V` must pass Admission under the same profile.

Then every required edge in `E` must pass Agreement for that key.

Only the full result gives `ClosedPermit_C(K)`.

If every build can communicate with every other build, every pair belongs to the claim. CI may compare each signature with one reference, because equality is transitive.

CI repeats this decision for every key in `R`. One type may receive a Permit while another is rejected.

**Transition to Slide 34**

Now we can name all three CI results.

### Slide 34 — Missing evidence means INCOMPLETE, not PASS

**Target time:** 85 seconds

**On screen**

| Evidence and gates | Result |
|---|---|
| missing, old, or not tied to a build | `INCOMPLETE / NO PERMIT` |
| valid evidence; Admission or Agreement fails | `REJECT / NO PERMIT` |
| valid evidence; complete graph passes | `PERMIT` |

```text
Skipped Apple job
→ three-build claim is INCOMPLETE
→ the type did not pass or fail
```

```text
Next: safe + match · match + not safe · layout difference
```

**Speaker script**

CI has three results, and we must keep them separate.

Missing, old, or untrusted evidence gives `INCOMPLETE`. CI cannot check the full contract, so there is no Permit.

Valid evidence with a failed gate gives `REJECT`.

Valid evidence with a complete passing graph gives `PERMIT`.

If the Apple job is skipped, the three-build contract does not become a two-build contract. It is `INCOMPLETE`; the type did not pass or fail.

A complete run decides each key. A project may require every key to pass, but that is a separate project rule.

The demo will show a Permit, an Admission failure, and an Agreement failure.

**Transition to Slide 35**

The model is complete. Now let us apply it to a useful raw-byte path.

**[Sources for Slides 29–34]**

- `include/boost/typelayout/admission.hpp`
- `include/boost/typelayout/tools/sig_export.hpp`
- `include/boost/typelayout/tools/sig_types.hpp`
- `include/boost/typelayout/tools/compat_check.hpp`
- Retained generated headers under `example/sigs/`
- Exact build provenance fields and attestation design remain appendix material.

## Stage 5 — Apply the Model to a Real Raw-Byte Contract

### Slide 35 — The demo declares one real raw-byte contract

**Target time:** 70 seconds

**On screen**

```text
declared recorder build
        ↓
portable capture file
        ↓
declared analyzer build
```

```text
C_capture = (R_capture, V, E, P)

V
  Linux x86-64 / GCC 16
  Linux x86-64 / Clang P2996
  Apple ARM64 / Clang P2996

E
  every pair; either endpoint may write or read
```

```text
R_capture
  PacketHeader · MeasurementSample · CaptureTrailer · CaptureBlock

P
  ordinary copy · no fixup · source-address-independent
```

```text
Can all four native types use one raw-byte path?
```

**Speaker script**

The demo is a fixed-size telemetry capture block.

A recorder writes the file. An analyzer reads it later. Any of the three declared builds may write or read.

So every pair of builds is a required edge.

The registered set has four keys: `PacketHeader`, `MeasurementSample`, `CaptureTrailer`, and `CaptureBlock`.

The profile allows ordinary copy, but no fixup or source-address dependency.

The short names on the slide represent exact build identities in CI.

Now the question is precise. Can all four types use one raw-byte path across these builds?

**Transition to Slide 36**

For the positive set, both gates pass.

### Slide 36 — Four native types pass on all three builds

**Target time:** 75 seconds

**On screen**

```text
CaptureBlock · 96 bytes

[ PacketHeader 16 ]
[ MeasurementSample 16 ] × 4
[ CaptureTrailer 16 ]
```

| Key | Admission on all builds | Agreement on all edges | Result |
|---|---:|---:|---:|
| `PacketHeader` | PASS | MATCH | PERMIT |
| `MeasurementSample` | PASS | MATCH | PERMIT |
| `CaptureTrailer` | PASS | MATCH | PERMIT |
| `CaptureBlock` | PASS | MATCH | PERMIT |

```text
CaptureBlock → whole-object raw write → bytes
             → raw read into a live, aligned CaptureBlock

no field encoding · no endian conversion · no fixup
```

```text
Inside C_capture, CaptureBlock may use
its native bytes as the stored representation.
```

**Speaker script**

The layout at the top gives the whole block. It has one header, four samples, and one trailer. The total is 96 bytes.

The table shows the decision. Every key passes Admission on every build. Every required edge matches.

CI issues four separate Permits.

Inside `C_capture`, the `CaptureBlock` Permit allows a whole-object raw write. The bytes may later be copied into a live and correctly aligned `CaptureBlock`.

There is no field encoding, endian conversion, or pointer fixup.

The Permit applies only to this type inside this contract.

**Transition to Slide 37**

Now add one pointer. Only Admission will fail.

### Slide 37 — A cached pointer fails Admission

**Target time:** 65 seconds

**On screen**

```cpp
struct UnsafeWithPointer {
    std::uint64_t id;
    std::int64_t value_microunits;
    const Metadata* cached_metadata;
};
```

```text
C_candidate(T)
= same V, E, and P; test T before adding it to R_capture

C_candidate(UnsafeWithPointer)

Agreement  MATCH on every required edge
Admission  FAIL on every node
Result     REJECT
```

```text
Layout match, but not byte-copy safe

Matching pointer bits do not transfer the target object.
```

**Speaker script**

Now add a cached metadata pointer. It may be useful inside one process.

The full layout still matches on all three builds, so Agreement reports `MATCH`.

But Admission fails everywhere. The copied address depends on the recorder's address space.

Our profile allows no fixup and no source-address dependency. This type cannot enter the raw-byte set.

We test it with the same builds, edges, and profile. It stays outside `R_capture`, and the four existing Permits remain unchanged.

**Transition to Slide 38**

The next type passes Admission but fails Agreement.

### Slide 38 — `long double` passes Admission but fails Agreement

**Target time:** 75 seconds

**On screen**

```cpp
struct Measurement {
    std::uint64_t id;
    long double value;
};
```

```text
Linux x86-64
  value @16:fld80[s:16,a:16]  · record[s:32,a:16]

Apple ARM64
  value @8:fld64[s:8,a:8]     · record[s:16,a:8]

Both begin [64-le]; leaf representation and layout still differ.
```

```text
[DIFFER] Measurement layout signatures

Admission  PASS on every node
Agreement  Linux↔Linux MATCH; Linux↔Apple DIFFER
Result     REJECT
```

```text
Bytes can stand alone and still have different representations.
```

**Speaker script**

Now return to `Measurement`. It has no pointer, so ordinary-copy Admission passes on every build.

But the representations differ.

On Linux x86-64, the value uses `fld80` at offset 16. The full record is 32 bytes with alignment 16.

On Apple ARM64, the value uses `fld64` at offset eight. The full record is 16 bytes with alignment eight.

Both signatures begin with `[64-le]`. Pointer width and byte order match, but the leaf representation and record layout do not.

The Linux builds agree with each other. Each Linux build disagrees with Apple. Agreement rejects the candidate.

**Transition to Slide 39**

We now have one working set and one failure for each gate.

### Slide 39 — Four Permits and two Rejections show both gates

**Target time:** 75 seconds

**On screen**

| Type set or candidate | Admission | Agreement | Decision |
|---|---:|---:|---:|
| every `K ∈ R_capture` | PASS everywhere | MATCH everywhere | four PERMITS |
| `UnsafeWithPointer` | FAIL everywhere | MATCH everywhere | REJECT |
| `Measurement` | PASS everywhere | DIFFER on Linux–Apple | REJECT |

```text
Agreement cannot fix source dependence.
Admission cannot fix a representation difference.
```

```text
Measurement under C_candidate(Measurement)
→ Agreement DIFFER
→ REJECT
```

**Speaker script**

This matrix shows why we need both gates.

Every production key passes Admission and Agreement. The four keys receive separate Permits.

`UnsafeWithPointer` has matching layouts, but Admission rejects its source-dependent address.

`Measurement` passes Admission, but Agreement rejects its different platform representations.

Agreement cannot fix source dependence. Admission cannot fix a representation difference.

The demo succeeds only when it sees all four Permits and both expected Rejections.

This also answers the question from Slide 2. Under this candidate contract, Linux and Apple disagree, so `Measurement` cannot enter the production raw-byte set.

**Transition to Slide 40**

The positive result is useful. But its meaning must stay narrow.

**[Sources for Slides 35–39]**

- Portable-capture implementation contract: `docs/superpowers/specs/2026-08-23-cppcon2026-typelayout-deck-design.md`, Section 11.1.
- Required final sources include the retained positive and negative build artifacts specified by the implementation contract; they control the final demo values and diagnostics.
- System V AMD64 ABI: <https://gitlab.com/x86-psABIs/x86-64-ABI/blob/master/x86-64-ABI/low-level-sys-info.tex>
- Apple ARM64 ABI guidance: <https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms>

## Stage 6 — Bound the Permit

### Slide 40 — A Permit proves one narrow representation claim

**Target time:** 65 seconds

**On screen**

| TypeLayout proves | Application still owns |
|---|---|
| ordinary-copy Admission on every declared build | schema and application meaning |
| representation Agreement on every required edge | valid values and invariants |
| complete evidence for the declared contract | storage, lifetime, and alignment |
| per-type `ClosedPermit_C(K)` | synchronization, trust, and versioning |

```text
Representation Permit ≠ end-to-end safety
```

**Speaker script**

The Permit proves one representation claim inside one declared contract.

The left column shows that claim. Every build passed Admission. Every required edge passed Agreement. CI had complete, valid evidence.

The right column remains with the application.

The Permit does not prove meaning, values, or invariants. It does not create object lifetime or aligned storage. It does not provide synchronization, validation, trust, or versioning.

Those requirements are real, but they are outside this Permit.

The Permit is useful because its meaning is narrow and exact.

**Transition to Slide 41**

The remaining work depends on how the application uses the bytes.

### Slide 41 — Runtime safety still depends on the boundary

**Target time:** 65 seconds

**On screen**

```text
Object obligations
  storage · lifetime · alignment

Concurrency and transport obligations
  publication · synchronization · coherence

External-data obligations
  validation · versioning · durability · failure handling
```

```text
Compile time checks the representation.
Runtime still owns the operation.
```

**Speaker script**

The remaining work has three groups.

Object rules cover storage, lifetime, and alignment. File bytes do not automatically become a live C++ object.

Concurrency rules cover publication, synchronization, coherence, and data races.

External-data rules cover validation, versioning, durability, and failure handling. Matching representation does not make untrusted bytes safe.

The exact work depends on the boundary. Shared memory, plugins, files, and devices need different runtime checks.

The main rule is simple. Compile time checks representation. Runtime still owns the operation.

**Transition to Slide 42**

The contract also tells us when native bytes are the wrong tool.

### Slide 42 — Re-check closed sets; convert for open-ended peers

**Target time:** 75 seconds

**On screen**

```text
Finite controlled change
  add a declared build or edge
  → generate fresh evidence
  → check the new closed contract

Contract cannot stay closed
  unknown peers · different platform representations
  canonical bytes · independent evolution
  value conversion · process-local handles
  → explicit representation + conversion
```

```text
Untrusted input still requires validation.
Serialization alone does not make it safe.
```

```text
Re-check a finite contract after each change.
Use an explicit representation when the set stays open.
```

**Speaker script**

A finite change does not always require serialization.

For one known build or edge, generate fresh evidence. Then update the contract and check the complete set again.

The result may be Permit or Reject. The method still works because the set remains finite.

Use an explicit representation for unknown peers, different platform layouts, independent evolution, conversion, or process-local handles.

Untrusted input still needs validation. Serialization alone does not make data safe.

The real question is whether we can declare, check, and keep a closed representation contract.

**Transition to Slide 43**

Now we can ask the opening question in the right way.

## Stage 7 — Summarize the Problem, Method, and Takeaway

### Slide 43 — The real question is: under which contract?

**Target time:** 60 seconds

**On screen**

```text
Local operation
May this object be copied as bytes here?

Boundary contract
May these bytes stand independently?
Do all declared builds give them the same representation?
```

```text
Measurement under C_candidate(Measurement)
→ Agreement DIFFER
→ REJECT
```

```text
Across a boundary, a native C++ type becomes a binary contract.
```

**Speaker script**

The opening question was incomplete.

“Can I `memcpy` this type?” asks about one local operation.

“Can I use these native bytes across this boundary?” asks about a contract.

The bytes must stand without producer-local context. Every declared build must also produce the same representation.

For `Measurement`, local copy was legal and Admission passed. But Apple disagreed with both Linux builds, so the candidate was rejected.

That is one decision for one type under one contract. It is not a universal rule for `Measurement`.

**Transition to Slide 44**

Here is the full method in one chain.

**[Sources for Slide 43]**

- C++ working draft N5032, `[basic.types]`, for object representation and trivially copyable byte-copy guarantees.

### Slide 44 — Reflection creates evidence; CI decides

**Target time:** 70 seconds

**On screen**

```text
declare C = (R,V,E,P) and contract key K
        ↓
each B: check Admission + emit Signature
        ↓
CI checks the inputs
   ├─ missing, old, or wrong build → INCOMPLETE
   └─ valid
        ↓
all nodes pass Admission
+ all edges pass Agreement
   ├─ no  → REJECT
   └─ yes → ClosedPermit_C(K)
```

```text
The compiler gives us the facts.
The contract gives those facts a scope.
```

**Speaker script**

Follow the chain from the top.

First, declare the type keys, builds, edges, and transfer profile.

Each build checks Admission and emits a reflection-based signature.

CI accepts only complete, current evidence from the correct build. Missing or invalid input gives `INCOMPLETE`.

With valid inputs, CI applies the two gates. Every node must pass Admission. Every required edge must pass Agreement.

A failed gate gives `REJECT`. Only the complete passing graph gives `ClosedPermit_C(K)`.

The compiler gives us the facts. The contract gives those facts a scope.

**Transition to Slide 45**

The final slide gives four rules for your next design review.

**[Sources for Slide 44]**

- WG21 P2996R12 and P3687R1.
- `include/boost/typelayout/detail/reflect.hpp`
- `include/boost/typelayout/detail/signature_impl.hpp`
- `include/boost/typelayout/signature.hpp`
- `include/boost/typelayout/admission.hpp`
- `include/boost/typelayout/tools/sig_export.hpp`
- `include/boost/typelayout/tools/compat_check.hpp`

### Slide 45 — Permit native bytes only inside a closed contract

**Target time:** 60 seconds

**On screen**

```text
1. Declare C = (R,V,E,P).

2. Check Admission and Agreement separately.

3. Keep every Permit per type and inside C.

4. Re-check finite changes; use an explicit representation
   when C cannot stay closed.
```

```text
Representation compatibility—
not semantic compatibility or schema evolution.
```

```text
Permit native bytes only inside a closed contract.
```

```text
github.com/ximicpp/TypeLayout                    Q&A → Appendix 46
```

**Speaker script**

Here are the four rules to remember.

Declare the contract before asking for a decision.

Check Admission and Agreement separately.

Keep every Permit with one type and one declared contract.

After a finite change, generate new evidence and check again. If the contract cannot stay closed, use an explicit representation.

This proves representation compatibility, not semantic compatibility or schema evolution.

So, can you `memcpy` a type across a boundary?

Only after the contract is named, every build provides evidence, and both gates pass across the full contract.

Permit native bytes only inside a closed contract.

**[Sources for Slide 45]**

- Repository and examples: <https://github.com/ximicpp/TypeLayout>
- This slide summarizes claims already sourced in Slides 10–44 and introduces no new technical claim.

## Appendix Scope for the Next Content Pass

Slides 46–61 keep the approved appendix titles and Q&A routing from the deck design. They do not receive full speaker scripts in this pass. Each appendix slide will later contain:

- the exact audience question it answers;
- the minimum supporting code, standard wording, table, or diagnostic;
- a short answer suitable for Q&A;
- a `[Sources]` block for every external claim.

The appendix content remains subordinate to the main-deck decision chain. It must not introduce a new compatibility gate or broaden the meaning of Permit.
