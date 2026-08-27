# CppCon 2026 Main-Deck Content and Speaker Script

**Talk:** *Can I memcpy This Type Across a Boundary? Verifying Object Representation at Compile Time With C++26 Reflection*

**Status:** content design only. This document fixes the audience-facing content and the English speaker script for Slides 1–45. It does not define the final layout, artwork, animation, or PowerPoint implementation. Appendix Slides 46–61 keep the outline in the approved deck design and do not yet have full scripts.

**Delivery target:** about 50–55 minutes for the main deck, leaving time for the appendix and questions.

**Staging rule:** when an **On screen** section contains several blocks, treat them as successive reveal states. Keep the current inference visible and move explanatory detail into the speaker script instead of showing every block at once.

**Speaker-language rule:** the audience is a technical C++ audience, but the speaker is not a native English speaker. Keep the technical content exact. Use short sentences, common words, stable terms, and clear pause points. Avoid contractions, idioms, and long nested clauses.

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

Good morning. I want to start with a simple question.

Can I copy this C++ object as bytes across a boundary?

We already do this in shared memory, plugin interfaces, and files. We also do it between different ABIs.

But when bytes leave the build that made them, local checks are not enough.

Today I will show how C++26 reflection can turn this question into a compile-time decision. The evidence comes from the type itself.

**Transition to Slide 2**

Let us start with a type that looks safe.

### Slide 2 — Would you permit these bytes on all declared builds?

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

PERMIT?
```

**Speaker script**

Here is a small measurement record. It has an integer ID and a `long double` value.

It is trivially copyable. It has no pointer. It has no ownership. It has no virtual function.

On this build, it looks safe for a raw byte copy.

Now suppose several builds may write or read these bytes. Would you permit this type on every declared build?

The question is not whether one local `memcpy` compiles. The question is whether these native bytes can become a boundary format.

For now, the answer is open. Permit, or no permit?

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

Inside one build, object layout can stay an implementation detail. The compiler chooses it. All code in that build uses the same choice.

At a boundary, another consumer uses the bytes. That consumer may use another build. It may use another address space. It may also read the bytes much later.

The object representation is now part of a binary contract.

In this talk, a boundary is any place where the consumer cannot assume the producer's build or address space.

This includes another process, a separate plugin build, a file, or another ABI.

Once the representation becomes a contract, we need evidence for it.

**Transition to Slide 4**

We must track two separate assumptions.

### Slide 4 — Build identity and address space are separate

**Target time:** 65 seconds

**On screen**

| | Same address space | Different address space |
|---|---|---|
| **Same build** | local case | Process A / Process B |
| **Different build** | Plugin / Host | stored bytes / cross-target |

```text
A boundary may lose either assumption—or both.
```

**Speaker script**

First, separate build identity from address-space identity.

The top-left cell is the normal local case. The build is the same. The address space is also the same. Most local language guarantees apply here.

Now run the same executable in another process. The build stays the same, but the address space changes.

Next, load a separate plugin build into one process. The address space stays the same, but the build changes.

Finally, write native bytes and read them later on another target. Now both assumptions may be gone.

These cases can fail for different reasons. A pointer problem is not a layout problem. Both can still make the byte transfer unsafe.

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

Imagine two processes running the same executable. The compiler made the same layout in both processes. Layout is not the first problem here.

Now copy a pointer value from Process A to Process B. Every pointer bit survives. The field keeps the same size, alignment, and offset.

But the target object belongs to Process A. The same address in Process B may name another object. It may name no object at all.

The representation is the same, but the value cannot stand alone. Some bits need the context that created them.

We will name this check later. For now, remember the failure. The bits survived, but the target did not.

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

A plugin and its host can share one address space. An agreed pointer may still name the same object.

But the plugin and host can be separate builds. The compiler may differ. Flags, headers, packing, or the standard-library ABI may also differ.

One address space does not tell us the member offsets. It does not tell us the bit-field layout. It does not tell us the representation of `long double`.

This case is the opposite of the process case. The pointer target may still be valid. The two layouts may still differ.

Later, I will use a stricter profile. It will not depend on a shared address space. Then all three boundary cases can use the same rule.

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

A file makes the problem clear. Build A writes an object's native bytes today. Build B reads them later. Build B may run on another target.

The first address space is gone. A process-local reference may no longer work. The first build may also be gone. The new build may use another layout.

Stored data also needs versioning, trust, durability, and crash consistency. These are important, but they are outside today's main question.

Today I will focus on object representation. May this type travel as native bytes? Did every declared build produce the same representation?

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
ordinary object copy
zero fixup
source-address-independent bytes
finite declared build set
```

```text
Two questions
1. May the bytes stand without producer-local context?
2. Do all declared builds produce the same object representation?
```

**Speaker script**

This talk uses one strict transfer profile.

First, we use an ordinary object copy. Second, we do no pointer fixup and no field conversion. Third, the bytes cannot depend on the producer's address space. Fourth, we declare a finite set of builds.

Some plugin or shared-memory systems can use a less strict profile. For example, they may support relocation. They may also allow agreed local references. Such a profile needs different evidence.

Our strict profile gives all three cases the same two questions. Can the bytes stand on their own? Are they the same on every declared build?

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

A boundary creates a representation question. Local checks cannot compare separate builds. So every build must emit the same kind of evidence.

We compare that evidence for Agreement. We check Admission separately. Then CI checks the full declared contract.

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

`std::is_trivially_copyable_v<T>` answers an important local question. It tells us whether the language allows the normal byte-copy operation for this type.

Build A checks the trait for itself. Build B does the same. Both results can be true, but the two builds may still use different layouts.

The trait does not compare offsets. It does not compare alignment. It does not compare the representation of `long double`.

We still need this trait. It will be part of Admission. But it cannot prove Agreement between builds.

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

Different sizes give a clear answer. If one build says sixteen bytes and another says twenty-four, the representations are different.

Equal size does not give the opposite answer. This record has sixteen-byte alignment. With a two-byte `wchar_t`, `code` starts at offset two. `tail` starts at offset four.

With a four-byte `wchar_t`, `code` starts at offset four. `tail` starts at offset eight. Both records are still sixteen bytes.

Equal size can hide different offsets, padding, bit layout, alignment, and leaf representation.

So different sizes can reject a pair. Equal sizes cannot approve it.

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
Did we record every required representation fact?

Canonical form
Do the same facts produce the same normalized form?

Detect differences
Does an encoded difference change the certificate?

Fail closed
Does missing evidence stop generation?
```

```text
Not a hash of the type name.
A certificate for one supported representation domain.
```

**Speaker script**

Before we build a signature, we need four clear rules.

First, the certificate must be complete. We must record every representation fact that the check needs.

Second, the form must be canonical. The same supported facts must produce the same form.

Third, the certificate must detect differences. If an encoded fact changes, the certificate must change.

Fourth is fail closed. If required evidence is missing, generation must stop. Partial evidence must never look complete.

This is not a hash of the type name. The same name can have different layouts. Different names can also have the same layout.

The certificate covers representation facts in one supported domain. It does not claim to model every C++ type.

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

The declaration tells us which parts exist. It also tells us their types. But it does not give the final byte map for this build.

For `PacketHeader`, this compiler puts `magic` at offset zero. The two sixteen-bit fields are at offsets four and six. `payload_len` is at eight. `checksum` is at twelve.

The full record is sixteen bytes. Its alignment is four.

These are facts from this build. Another compiler or ABI may make another valid choice.

TypeLayout does not guess these facts from declaration order. It asks the current compiler. Every declared build must therefore generate its own evidence.

The input is still an ordinary C++ type. We add no second IDL. We generate no serialization stubs. We do no runtime inspection.

The compiler builds the certificate at compile time from the real type.

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

C++26 reflection lets a constant-evaluated program inspect the compiler's type.

For a record, we list its non-static data members and bases. For each member, we get its type and position. For a bit-field, we also get its bit width.

Then we classify the type. If it contains more structure, we repeat the same process.

Reflection gives us compiler facts. TypeLayout decides which facts to record and how to normalize them.

Reflection alone does not define compatibility. The recursive policy builds one certificate. Another build can produce the same form, and CI can compare them.

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

The dispatcher has a small set of clear results.

A supported leaf becomes a canonical token. We also record its size and alignment.

For an enum, we record the underlying representation. For an array, we record the count and the element representation. For a record or supported union, we record its layout and inspect its parts.

An opaque type is different. Reflection does not see its hidden details. The application must provide a named trust contract for that region.

An unsupported kind causes a compile-time error. There is no fallback based on a reasonable-looking size.

This keeps the fail-closed rule from the last slide.

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

A leaf token tells us which representation the compiler selected. It does not keep the source spelling.

An alias of `std::uint32_t` still becomes `u32`. A supported IEC 559 `float` becomes `f32`. The extended x86 `long double` becomes `fld80`.

A pointer gets a pointer-like token. Its local size and alignment are also recorded.

The size and alignment stay explicit. The token does not need to carry every layout fact.

We do not record member names or typedef spelling. This check is about representation, not schema identity.

The appendix lists the exact rules for `char`, `bool`, `wchar_t`, and floating-point types.

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

The same leaf can appear inside a nested member. It can appear inside a base. It can also appear directly in a flat record.

These source paths are different. But the byte-transfer check needs the position inside the full object.

So the walk adds the offsets. If the parent starts at eight and the child starts at four, the leaf is at absolute offset twelve.

We remove the source path, but we keep the byte position.

This does not mean the declarations have the same schema or meaning. A protocol that needs names or nesting needs an explicit schema. Our certificate answers only the representation question.

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

Virtual inheritance is a useful fail-closed example. Its final representation depends on hidden implementation details. This signature domain does not encode all of them.

We must not record only the visible fields and ignore the missing facts. That would create a certificate that looks complete but is not complete.

TypeLayout rejects signature generation for this case.

This does not mean virtual inheritance can never cross a boundary. It only means this check does not have enough evidence.

The rule is simple. Encode visible supported structure. Use a named trust contract for an opaque region. Reject any required fact that the check cannot cover.

The appendix has the full list of difficult cases.

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

Here is the full certificate for `PacketHeader`.

The `[64-le]` prefix records two facts. Pointers are sixty-four bits wide. The target uses little-endian byte order.

This prefix is not a CPU name. It is not an ISA name. CI will bind the exact compiler and target later.

The record header says the object is sixteen bytes. Its alignment is four.

Each entry has an absolute offset and a canonical leaf token. It also has the leaf size and alignment.

This evidence is readable. It is not only a hash. A hash can help with lookup, but CI still needs the encoded facts.

The four rules now hold. Required facts are present. The form is canonical. An encoded difference changes the text. A missing required fact stops generation.

The same walk also gives us facts for Admission. First, let us ask what equal certificates prove.

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

Agreement checks one registered key on one declared build edge.

The key tells us that both artifacts refer to the same boundary type. The signature tells us what representation each build produced.

Both parts matter. Equal signatures under different keys do not join two application concepts. The same key with different signatures does not hide a layout change.

Inside one signature domain and version, exact equality gives Agreement for this edge.

This result covers only Build A and Build B. It does not yet permit the transfer. It also says nothing about the full build set.

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

The certificate is canonical, so the comparison can be exact.

If the strings match, every encoded fact matches. If they differ, the text shows where the first useful difference appears.

An offset may move from eight to twelve. A leaf may change from `f64` to `fld80`. An alignment may change from four to eight.

The compile-time check uses the function shown here. If the two stored signatures differ, the `static_assert` fails.

We do not need a score or a close-enough rule. A hash can make lookup faster. But we still keep the full certificate for a clear diagnostic.

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

Agreement proves exact equality of the encoded certificates in the declared domain.

It does not prove that both builds used the same source declaration. We removed names and source paths when they did not affect the byte map.

It does not prove that the application gives the bits the same meaning. Reflection cannot discover the application's rules.

It also does not prove that the value can stand alone. A field can have the same representation on every build and still refer to local state.

These limits are important. Agreement proves one exact claim. It does not pretend to prove source identity, meaning, or independence.

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

This `BufferView` can have the same layout on two builds. Both use sixty-four-bit pointers and little-endian byte order. The size field matches. The pointer token, offset, size, and alignment also match.

Agreement correctly reports a match.

Now move the bytes to another process. The pointer bits survive. But the consumer has no known object at that address.

The value depends on the producer's address space. Our strict profile does not allow that dependency.

Agreement must not report a layout error, because the layouts match. Another check must reject the real problem.

We call that check Admission. Here, Agreement matches and Admission fails.

**Transition to Slide 24**

Admission is local to one build and one transfer profile.

### Slide 24 — Admission checks one type on one build under one profile

**Target time:** 65 seconds

**On screen**

```text
Admission_P(K,B)

K  registered boundary type
B  one actual build
P  ordinary copy · zero fixup · source-address-independent
```

```text
Pointer rejection follows from this profile.
It is not a universal rule for every possible boundary.
```

**Speaker script**

Admission checks one registered type, one real build, and one transfer profile.

The profile must be part of the input. The phrase “safe to transfer” is too broad without clear rules.

Our profile uses an ordinary object copy. It does no fixup. The bytes cannot depend on the producer's address space.

Under this profile, an ordinary pointer fails. Another system may support shared addresses or relocation. That system needs another profile and other evidence.

TypeLayout is not saying that pointers are always bad. It is saying that pointer-dependent bytes fail this profile.

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

The first condition is local copy legality. The language must allow the normal byte-copy operation for this type.

The second condition is no detected source-context dependency. A pointer is the clear example. Its value may need the producer's address space.

This is a structural check. It does not prove that no hidden semantic dependency exists.

The third condition is complete representation evidence. Every required part must be encoded or covered by a named trust contract.

If a required part is unsupported, signature generation already failed. Admission cannot turn missing evidence into a pass.

The three conditions are separate. A type can be trivially copyable but depend on its source process. A simple type can also fall outside the supported signature domain.

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

For the ordinary-copy profile, the implementation checks the full local rule.

First, signature generation must succeed. This shows that the required representation evidence is complete.

Next, `is_admitted_v` checks three things. The type must be trivially copyable. It must pass the recursive byte-copy-safe check. It must also be independent of the source context.

The `FramedPacket` tree shows why recursion matters. A pointer may be hidden inside a nested record. It may also be hidden inside an array element.

`is_byte_copy_safe_v<T>` alone is not the full Admission rule. It is only one part of the check.

Together, signature generation and the `static_assert` give us local evidence for `PacketHeader`. We still need Agreement to compare builds.

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

This type contains only a fixed-width integer. Its structure looks self-contained. It may pass trivial copyability, byte-copy safety, and signature generation.

But the application may use `descriptor` as an operating-system file descriptor. Copying the integer to another process does not transfer the open file.

Reflection cannot learn that meaning from an integer type. Structural Admission may pass, while the application still rejects the field.

This is why the talk claims representation compatibility, not semantic compatibility. The application still owns rules that are not visible in the type.

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

The rule for one edge is simple.

Build A must pass Admission under profile P. Build B must also pass. Then their signatures must match on the declared edge.

If either build fails Admission, matching layouts cannot save the transfer. This is the pointer case.

If Agreement fails, local Admission cannot save the transfer. This will be the `long double` case.

Only the top-left cell gives `EDGE PASS`. Admission passes at both ends, and Agreement matches on the edge.

This formula assumes valid evidence from the correct builds. CI will check that condition.

`EDGE PASS` is not the final Permit. It covers one type on one edge. The full contract may contain more types, builds, and edges.

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

To move from one edge to a real Permit, we must name the full scope.

I write the contract as `C = R, V, E, P`.

`R` is the set of registered type keys. `V` is the exact set of builds. `E` is the set of required transfer edges. `P` is the transfer profile.

Each part changes the claim. Add a type, and CI needs another type decision. Add a build, and CI needs another node. Add an edge, and CI needs another Agreement check.

Change the profile, and the meaning of Admission changes.

The contract is finite and clear. CI proves this declared set. It does not prove every compiler, every target, or a future ABI.

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

Each real build compiles the C++ type. It creates its own certificate and checks its own Admission facts.

The Linux GCC build cannot predict the Linux Clang result. The Linux Clang build cannot speak for Apple ARM64. The Apple build cannot use a signature copied from another target.

The compiler and ABI are part of what we measure. If one machine creates every artifact, we replace evidence with an assumption.

All builds can use the same source registration. But each build must run reflection and the local checks in its own environment.

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

The generated header contains three things. It has the contract key, the layout signature, and a local byte-copy-safe result.

This header is not the full Admission proof. In the same job, signature generation must succeed. The compile-time Admission check must also pass.

The exporter also rejects a non-trivially-copyable type on this path. But that is only one part of Admission.

CI uses both results from each build. It uses the successful local gate and the emitted artifact. It does not rebuild the full Admission decision from `TypeEntry` alone.

The header still cannot prove who made it. A string can claim any platform name. The file cannot prove the compiler, source revision, or build run.

So we keep two questions separate. What did the build report? Which declared build produced that report?

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

CI ties the artifact and the local gate result to one declared build and one CI run.

The full record may include the source revision, compiler version, and target. It may also include headers, ABI flags, TypeLayout version, job identity, build result, and artifact digest.

That full list belongs in the appendix. The main idea is simple.

The artifact says what the build observed. CI proves who produced it and when.

CI checks provenance before it uses Admission or Agreement. Provenance is not a third compatibility gate.

Missing or old provenance does not prove a layout difference. It means CI cannot make the decision. We will call that state `INCOMPLETE`.

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

For one key `K`, every build in `V` must pass Admission under the same profile. This result comes from the compile-time gate in that build.

Then every required edge in `E` must pass Agreement for the same key.

Only the full result gives `ClosedPermit_C(K)`.

If any build may write and any other build may read, every pair is part of the claim. CI may compare all signatures with one reference signature. Equality makes that safe. But the logical requirement still covers every declared edge.

CI repeats the decision for every key in `R`. One type can receive a Permit while another type is rejected. There is no unclear global Permit for a mixed result.

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

CI has three possible results. We must keep them separate.

If evidence is missing, old, or not tied to the right build, the result is `INCOMPLETE`. CI could not check the full contract. It gives no Permit.

If the evidence is valid but either gate fails, the result is `REJECT`. CI checked the claim, and one gate failed.

If valid evidence covers the full graph and both gates pass, the key receives `PERMIT`.

For example, suppose the Apple job did not run. The three-build contract does not become a two-build contract. The result is `INCOMPLETE`.

The type did not pass. It also did not fail.

A complete run gives each key either Permit or Reject. A project may also require every key to pass. That is a separate project rule, not a new type-level Permit.

The demo will show three clear report shapes. A type can be safe and match. It can match but fail byte-copy safety. Or its layout can differ.

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
  ordinary copy · zero fixup · source-address-independent
```

```text
Can all four native types use one raw-byte path?
```

**Speaker script**

Here is the full example. It is a fixed-size telemetry capture block.

A recorder writes a capture file. Later, an analyzer reads it. Any of the three builds may write or read the file.

So the contract needs Agreement between every pair of builds.

The production set has four keys. They are `PacketHeader`, `MeasurementSample`, `CaptureTrailer`, and `CaptureBlock`.

We use the same strict profile. It allows an ordinary object copy. It does no fixup. The bytes cannot depend on the writer's address space.

The short build names on the slide stand for exact build identities in CI.

Now we have a clear question. Can all four native types use one raw-byte path across these three builds?

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
             → raw read into an existing aligned CaptureBlock

no field encoding · no endian conversion · no fixup
```

```text
Inside C_capture, CaptureBlock may use
its native bytes as the stored representation.
```

**Speaker script**

The positive layout is simple. `PacketHeader`, `MeasurementSample`, and `CaptureTrailer` are each sixteen bytes.

`CaptureBlock` has one header, four samples, and one trailer. Its total size is ninety-six bytes.

Every key passes Admission on all three builds. Every required edge has matching signatures. CI gives four separate Permits.

The `CaptureBlock` Permit allows one useful operation inside `C_capture`. We may write the full object representation as raw bytes.

Later, we may copy those bytes into an existing and correctly aligned `CaptureBlock` object.

This path uses no field encoding, endian conversion, or pointer fixup.

Each key still has its own Permit. The `CaptureBlock` Permit allows the full-block input and output path.

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

Suppose the in-memory sample adds a cached metadata pointer. This may be a useful local optimization.

The full layout can still match on all three builds. Agreement correctly reports `MATCH`.

But Admission fails on every build. The copied address depends on the recorder process.

Our profile allows no fixup and no source-address dependency. So this type cannot enter the raw-byte set.

The report gives the exact reason: layout match, but not byte-copy safe.

We test `UnsafeWithPointer` with the same builds, edges, and profile. It stays outside `R_capture`. The four working types keep their Permits.

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

Now return to the `Measurement` type from the start. It has no pointer. It passes ordinary-copy Admission on every build.

But its representations are different.

On Linux x86-64, `long double` uses the extended representation shown here. The value starts at offset sixteen. It uses sixteen bytes of storage.

The full record is thirty-two bytes, with alignment sixteen.

On Apple ARM64, `long double` uses the same representation as `double`. The value starts at offset eight. The full record is sixteen bytes, with alignment eight.

Both signatures start with `[64-le]`. The pointer width and byte order match. But the leaf representation, offset, size, and alignment differ.

Admission passes on every build. The two Linux builds agree. Each Linux build disagrees with Apple. Agreement rejects the candidate.

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

Every production key passes Admission on every build. It also passes Agreement on every required edge. These four keys receive separate Permits.

`UnsafeWithPointer` has matching layouts. But Admission rejects its source-dependent address.

`Measurement` passes Admission. But Agreement rejects its different platform representations.

Agreement cannot fix source dependence. Admission cannot fix a representation difference.

The demo must show all four Permits and both expected Rejections. Missing evidence is not success. An extra failure is also not the expected result.

This also answers the question from Slide 2. The type looked safe on one build, but that question had no contract.

Under this candidate contract, the Linux and Apple signatures differ. So `Measurement` cannot enter the production raw-byte set.

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

It tells us three things. Every declared build passed ordinary-copy Admission. Every required edge passed Agreement. CI also had complete and valid evidence.

The Permit does not prove that the application gives the bytes the right meaning. It does not check values or invariants.

It does not create object lifetime or aligned storage. It does not synchronize access. It does not validate a file. It does not define a versioning policy.

These are real safety requirements. But they are not part of this Permit.

The Permit is useful because its meaning is small and exact.

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

First are object rules. The application needs valid storage, correct lifetime, and correct alignment. Raw file bytes do not automatically become a live C++ object.

Second are concurrency and transport rules. The application must handle publication, synchronization, coherence, and data races.

Third are external-data rules. The application must handle validation, versioning, durability, and failures. Matching representation does not make untrusted bytes safe.

The exact list depends on the boundary. Shared memory, plugins, files, and devices need different runtime work.

The appendix has a longer table. The main rule is simple. Compile time checks the representation. Runtime still owns the operation.

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

Suppose we add one known build or one required edge. We can generate new evidence. Then we update the contract and run the full check again.

The new result may be Permit or Reject. The method still works because the set is finite.

An explicit representation is better when the set cannot stay closed. Examples include unknown future peers, different platform representations, and a required byte order.

Other examples are independent schema changes, value conversion, and process-local handles.

Untrusted input is a separate problem. Serialization does not validate data. The application still needs validation.

The key question is not native bytes or serialization. The key question is whether we can declare, check, and keep a closed representation contract.

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

The first question was incomplete.

“Can I `memcpy` this type?” is a local question. `trivially_copyable` and `sizeof` give useful local facts.

“Can I use these native bytes across this boundary?” is a contract question.

The bytes must stand without the producer's context. Every declared build must also give them the same representation.

For `Measurement`, the local copy was legal. Admission passed under our candidate contract. But Apple disagreed with both Linux builds. The result was Reject.

This is not a rule for every use of `Measurement`. It is one exact decision for one type under one contract.

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

First, declare the contract. Name the registered keys, exact builds, required edges, and transfer profile.

For each key, every build checks local Admission. It also emits a representation signature from reflection.

CI accepts only complete and current evidence from the right build. This checks the input. It is not a third compatibility gate.

Then CI applies the two gates. Admission must pass on every build. Agreement must pass on every required edge.

CI makes a separate decision for each key. A failed gate gives Reject. Missing evidence gives Incomplete. Only the full passing graph gives `ClosedPermit_C(K)`.

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

Here is the rule I want you to remember.

Declare the contract before you ask for a decision.

Check Admission and Agreement separately. They catch different failures.

Keep every Permit with one type and one declared contract.

After a finite change, generate new evidence and check the contract again. If the set cannot stay closed, use an explicit representation.

The result is narrow. It proves representation compatibility. It does not prove semantic compatibility or schema evolution.

So, can you `memcpy` this type across a boundary?

Only after you name the contract. Only after every declared build provides evidence. And only after both gates pass over the full set.

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
