# ConfigForge Binary Image, version 2

The artifact behind decision D-28. Not a document format: an image of a
resolved key space, laid out so that loading it is validation followed
by pointer arithmetic.

## Why it exists

A configuration file is kilobytes and is read once, so neither size nor
parsing speed is the problem a binary format solves here. The one thing
it does that no text format can is **load without parsing and without
allocating**.

That is what a target with no filesystem, no heap and a boot-time budget
needs, and it is the artifact the freestanding profile was missing:
build-time configuration was described as a source and had nothing to
be.

## Layout

Every offset is from the start of the image and is a multiple of 8.
Every multi-byte field is little endian.

```text
0                   header        128 bytes (major 1: 64)
header.index_off    key index     count * 16 bytes
header.value_off    values        count * 16 bytes
header.chain_off    chain         optional, see below
header.string_off   strings       one blob, NUL-terminated entries
```

### Header

```text
off  size  field
  0     4  magic          'C' 'F' 'G' 'B'
  4     2  major          refuse on a mismatch
  6     2  minor          accept a higher one; fields only grow
  8     4  flags          CF_IMAGE_PROVENANCE
 12     4  count          number of keys
 16     4  index_off
 20     4  value_off
 24     4  string_off
 28     4  string_len
 32     4  chain_off      0 when there is no chain
 36     4  chain_count
 40     8  digest         the determinism digest of the key space
 48     4  total          the image size, so truncation is detectable
 52    12  reserved       zero
```

### Header, major 2

Bytes 0-51 are the major 1 header, with `major` 2 and `minor` 0; the
header grows to 128 bytes and `index_off` is never below 128.

```text
off  size  field
 52    12  reserved       zero
 64     2  key_max        the build's CF_KEY_MAX
 66     2  source_max     CF_SOURCE_MAX
 68     2  message_max    CF_MESSAGE_MAX
 70     2  max_depth      CF_MAX_DEPTH
 72    32  build_id       at most 31 bytes of cf_build_id(), NUL, NUL padded
104     4  manifest_off   reserved for Y-33: zero in 2.0.0
108     4  manifest_len   reserved for Y-33: zero in 2.0.0
112    16  reserved       zero
```

Offsets stay 32-bit. A reader ignores every reserved byte and every
field of a higher minor, so an image whose manifest fields are filled
opens. An image whose `key_max` exceeds the reader's `CF_KEY_MAX` is
refused ("image was made with longer keys (its limit N, this build's M)").

### Loading major 1

A major 1 image (a 64-byte header, `reserved` at 52-63) loads unchanged:
`cf_image` has `build_id` NULL and the limits zero. `cf_image_write` with
`CF_IMAGE_V1` writes one; the determinism digest is the same in both.

### Key index

Sorted, so lookup is a binary search straight out of the bytes.

```text
 0  4  key_off      into the string blob
 4  4  key_len
 8  4  value_index
12  4  chain_off    index of the first candidate, or 0xFFFFFFFF
```

### Value

Tag and payload, which is where the TLV idea belongs: inside a value,
not as a container around the file.

```text
 0  1  type         cf_type
 1  3  pad          zero
 4  4  text_len     text only
 8  8  payload      int64, double bits, or the text offset
```

## Decisions the format encodes

**Little endian, always.** An image that records its producer's byte
order and refuses the other one only loads on the machine that wrote it,
which defeats the purpose of having an artifact at all. A big-endian
host pays one conversion pass and gets identical semantics. The
determinism digest would mean nothing otherwise.

**Alignment is checked, not assumed.** The loader verifies the base
pointer and the declared offsets. An unaligned buffer is refused rather
than dereferenced, because the alternative is undefined behaviour on
every target that cares and silence on the ones that do not.

**Strings are NUL-terminated as well as length-carrying**, matching the
in-memory model, so the convenience accessor stays free and an embedded
NUL is caught when the image is written rather than much later.

**The digest travels with the image and is verified on load.** It costs
one pass and it catches truncation, a partial write and a corrupted
flash page - three failures that otherwise produce a configuration that
looks fine.

**Every number in an image is a claim, not a fact.** The reader validates
each region against the image's own length before dereferencing
anything, and the value index in every entry against the entry count.

The first version validated the header's regions and not the per-entry
index, and a mutation fuzzer found it on its first run: a corrupt index
pointed anywhere. This is the one reader in the library that
dereferences offsets taken from its input, so it is the one that has to
treat its input as hostile.

Bounds are written as subtraction — `len <= limit - off` — rather than
addition, because two 32-bit values and a one can wrap where `size_t` is
32 bits, and a wrapped bound passes every check it was meant to fail.

**Provenance is optional and it is the interesting half.** A
resolved-only image is what a boot path wants. An image carrying the
chain is what lets a device with no filesystem still answer why a value
is what it is, which is the whole premise of this library arriving
somewhere it otherwise could not. Loaded as a source, each value takes
its location from the winning candidate in its chain - `release.ini:2:1`
rather than `image` - and a key's candidates run from its `chain_off`
to the next key's.
