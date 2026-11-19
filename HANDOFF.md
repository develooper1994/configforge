# Handoff

State: on 2026-11-19 the 1.8.0 milestone was planned whole. Cards A-1 to
A-6 were read cold against the tree and corrected; A-1b, A-7 to A-13,
W5-V2, W5-G4 and P-1b were written the same afternoon and have not yet
been read by anyone who did not write them. Do step 1 before the loop
takes its next card, T-3 (1.7.0).

1. **A cold reading of the new cards** (docs/Plan.md, "Read again after
   the layout changes"): A-1b, A-7, A-8, A-9, A-10, A-11, A-12, A-13 in
   `docs/plan/A-architecture.md`; W5-V2 and W5-G4 at the end of
   `docs/plan/W5-convert-and-gen.md`; P-1b in
   `docs/plan/P-word-at-a-time.md`. Whoever reads did not write them,
   checks every file and line they cite against today's tree (they name
   files after A-1's move: map `lib/<c>/...` back to `src/...` and
   `include/configforge/...`), and lists every place where the one who
   carries the card out would have to ask or guess. Done when every
   finding is fixed in the cards, in one commit.
2. **Points the drafts flagged, to settle in that reading:**
   * W5-V2's `CF_HOLDS_NUL` cell is read from the code, not tested: the
     card's first run decides it; the card says to hand back on any
     silent loss.
   * The serialization library alone has no decode budget (D-82's
     budget lives in the flattener): `configforge codec` reads a
     document with no bound. Decide whether a card of 1.8.0 adds one.
   * `docs/Architecture.md`, "What is deliberately not here", is out of
     date (interpolation exists, D-77; CBOR is a second binary format,
     D-91; a document tree comes with W5-V1); A-13 rewrites it - check
     that it does.
   * README: `CF_MESSAGE_MAX` is said to bound JSON and TOML strings, but
     `json.c` uses a fixed `char local[256]` (today :797); A-13 fixes
     the text - check.
3. Delete this file in the commit that finishes step 1.
