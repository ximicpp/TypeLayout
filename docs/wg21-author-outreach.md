# P2996 Author Outreach — Draft Email

## Recipients

Based on P2996R13 author list:

- **Wyatt Childers** — wcc@edg.com (EDG)
- **Peter Dimov** — pdimov@gmail.com (also Boost.Describe / Boost.PFR reviewer)
- **Dan Katz** — dkatz85@bloomberg.net (Bloomberg, maintains the Clang P2996 fork)
- **Barry Revzin** — barry.revzin@gmail.com (prolific WG21 contributor)
- **Andrew Sutton** — andrew.n.sutton@gmail.com
- **Faisal Vali** — faisalv@gmail.com
- **Daveed Vandevoorde** — daveed@edg.com (EDG)

## Recommended approach

Start with **Dan Katz** (Bloomberg, closest to the compiler implementation)
and **Barry Revzin** (most active in community engagement). They are the
most likely to respond to external library developers.

**Peter Dimov** is particularly relevant if pursuing a Boost submission —
he is a Boost veteran and may offer guidance on the Boost review process.

## Draft Email

---

**Subject:** Implementation experience report: type layout verification library built on P2996

Hi [Name],

I'm writing to share a non-trivial library implementation built entirely
on the P2996 reflection facilities that were adopted into C++26. I thought
the implementation experience might be useful to SG7 as the reflection
API is finalized.

**The library:** TypeLayout is a header-only library that generates
compile-time type layout signatures — deterministic strings encoding the
complete byte-level memory identity of C++ types. It enables compile-time
verification of binary compatibility:

```cpp
static_assert(
    get_layout_signature<SenderMsg>() ==
    get_layout_signature<ReceiverMsg>());
```

This replaces 7+ lines of sizeof/offsetof assertions with one line, and
provides strictly stronger guarantees (full field offsets, not just total
size).

**P2996 API surface used:** The entire library is built on two language
operators (`^^` and `[:...:]`) and six metafunctions
(`nonstatic_data_members_of`, `bases_of`, `type_of`, `offset_of`,
`is_bit_field`, `bit_size_of`). All tested with 200+ static_assert
statements across 17 type categories.

**Implementation experience:**

The API worked very well overall. Specific observations:

- `offset_of` with the `{.bytes, .bits}` return is excellent for our
  use case — it handles both regular fields and bit-fields uniformly.
- `access_context::unchecked()` is essential — layout verification must
  inspect all members regardless of access specifiers.
- `bases_of` returning offset-queryable info objects simplifies
  inheritance flattening considerably.

The main friction point is constexpr step limits: our compile-time
string concatenation hits O(n^2) steps for types with >50 fields.
This is not a P2996 issue per se, but it's a practical barrier for
P2996-based libraries that generate strings.

**Why I'm reaching out:**

1. I'm drafting a WG21 paper (targeting the Brno mailing) reporting
   this implementation experience, and would welcome any feedback on
   whether the SG7 audience would find it useful.

2. I'd appreciate any guidance on whether there are reflection API
   changes in flight (e.g., P3687 adjustments) that might affect
   this use case.

3. Longer term, I'm interested in submitting this as a Boost library
   proposal once mainstream compiler support for P2996 matures.

The library is open-source (Boost License):
https://github.com/ximicpp/TypeLayout

I'd be happy to share the draft paper if you're interested.

Best regards,
Fanchen Su

---

## Notes

- Adjust tone depending on recipient. Dan Katz / Barry Revzin can be
  more casual. Daveed Vandevoorde is more formal.
- For Peter Dimov, add a line about interest in the Boost review process.
- The paper draft is at `docs/wg21-paper.md` in the repository.
- Target mailing: **Brno (2026-06-08)**, deadline approximately
  **2026-05-11** (Monday 4 weeks before meeting, 14:00 UTC).
