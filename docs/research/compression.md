# Research: compression

Research for the Roadmap item "Compression" (docs/Roadmap.md, "From the
maintainer, 2026-11-26"). Not a card and not a decision: what the world
does, what breaks, what fits this library, and what is left for the
maintainer to decide. Written 2026-11-26.

## 1. The question

**As the maintainer asked it** (Turkish, translated): "Data compression
could be added."

**As the Roadmap holds it** (docs/Roadmap.md:1563-1566): a document or an
image read from, and written to, a compressed file (gzip, zstd), chosen by
name or by magic bytes, as a composed codec (D-103) so no reader changes;
a new optional dependency each, behind its own build option.

**What the library does today.** Nothing with compression. No build
option, no dependency, no code. The pieces it would sit on:

* **Whole buffers, by decision.** `lib/serialize/include/configforge/cf_serialize.h:36-40`:
  "Configuration files are kilobytes; a streaming parser would be twice
  the code ... The one thing streaming buys - bounded memory over a huge
  document - is not a property any configuration file needs." Every reader
  takes a `cf_blob` (`cf_serialize.h:43-47`: a pointer, a length, a name).
* **Reading a file.** `cf_blob_file` (`lib/serialize/src/blob.c:58-136`)
  reads the whole file into an arena, in chunks, trusting the size the
  system reports only as a first guess (D-76). It has no limit of its own
  but the arena's.
* **The one place between bytes and a reader.** `cfi_source_text`
  (`lib/config/src/bridge/file.c:292`, declared at
  `lib/config/include/configforge/cf_source.h:270-280`) is "what a file
  and a directory's file both do between reading their bytes and
  parsing": the encoding (D-108), then the format from the content when the
  name gave none. The file source calls it at `file.c:434`; the directory
  source at `dir.c:178`.
* **The format from the name.** `path_ext` (`lib/serialize/src/codec.c:172-187`)
  takes the text after the *last* dot. `app.toml.gz` gives `.gz`, which
  names no codec, so today the content is tried, and every reader refuses
  the compressed bytes.
* **The format from the content** (D-73, `cf_codec_detect`,
  `lib/serialize/src/detect.c:251-341`): never a guess at a few bytes; a
  look at the first bytes only orders candidates and each is decided by
  reading.
* **Composed codecs** (D-103, `cf_compose`, `cf_serialize.h:436-456`;
  `lib/serialize/src/compose.c:129-168`). A *repair*
  `cf_bool (*)(void* ctx, const cf_blob* in, cf_blob* out, cf_error* err)`
  runs on the bytes before the base codec reads them, and "lends its bytes
  until decoding returns". The result is `CF_CODEC_NAMED_ONLY`
  (`cf_serialize.h:172`): never chosen from content or from a second
  extension. Read only.
* **Writing.** Every encoder writes into a `cf_sink`
  (`cf_serialize.h:81-85`): a caller's buffer, measure-or-write. The
  image is written by `cf_image_write` and opened by `cf_image_open`
  (`lib/config/include/configforge/cf_image.h:54`, `:84`), which validates
  an 8-aligned buffer in place (docs/ImageFormat.md). The image source
  reads its file with `cf_blob_file` (`lib/config/src/bridge/image.c:690`).
* **Bounds.** The decode budget (D-82, docs/Limits.md "The decode
  budget"): a document of n bytes may make at most 64 x n + 1 MiB of keys
  and values. `CF_EXPAND_MAX` and `CF_EXPAND_TOTAL_MAX` bound `${...}`
  (D-77). YAML aliases have their own linear bounds (D-63). A refusal names
  a reason from `lib/base/include/configforge/cf_reason.def`
  (`CF_REASON_BUDGET`, `CF_REASON_TRUNCATED`, `CF_REASON_TRAILING`, ...;
  D-107).
* **Source flags.** A file source's flags already carry one
  `CF_ENCODING_*` value (`cf_source.h:246-249`, `CF_FILE_SOURCE_FLAGS`).
* **Dependencies.** None in a default build (docs/Dependencies.md, section
  1). Optional ones each have a row and a build option (libfyaml, expat,
  sqlite3), and `gate.dependencies` fails a build that looks for a package
  the page does not name.

## 2. What the world does

### 2.1 gzip, zlib and raw deflate (RFC 1950, 1951, 1952; zlib)

One algorithm, deflate (RFC 1951 [2]), in three wrappers. **Raw deflate**
has no header and no check. **zlib** (RFC 1950 [1]) adds two bytes (CMF,
FLG, with a check making the pair a multiple of 31) and an Adler-32 of the
output. **gzip** (RFC 1952 [3]) adds a ten-byte header starting `1f 8b 08`,
optional fields (a name, a comment, an "extra" field), and a trailer of a
CRC-32 and ISIZE, "the size of the original input data modulo 2^32".
A gzip file may hold several members one after another; `gzip -d`
concatenates their output.

The zlib library [4]: `inflateInit2` takes windowBits 8..15; add 16 for
gzip only, add 32 for "automatic header detection" of zlib or gzip;
negative for raw deflate. `inflate()` stops at the end of one gzip member;
"you must reset the state to decode the next one" (gzread does it for
you). `inflateGetHeader` hands the gzip header to the caller. Memory:
inflate needs a 32 KiB window and about 7 KiB of state; the window is not
allocated when the whole input is given at once with `Z_FINISH`.
Licence: zlib, the same as ConfigForge (D-36).

Other implementations of the same format:

* **libdeflate** [12]: "fast, whole-buffer DEFLATE-based compression and
  decompression"; raw deflate, zlib and gzip; "no support for streaming"
  on purpose. MIT. Whole-buffer is exactly how this library reads.
* **miniz** [13]: zlib's API in one C file; its inflater `tinfl` is a
  single function (~550 lines) usable alone. MIT in current releases;
  older copies of `tinfl.c` are public domain (Unlicense). Read the header
  of the copy taken.
* **tinf** [14]: a small inflate for embedded use, zlib licence.
* **zlib-ng**: a maintained fork with SIMD paths, zlib licence, a
  compatibility mode with zlib's API.

### 2.2 Zstandard (RFC 8878; libzstd)

RFC 8878 [5]. A frame starts with the magic `28 b5 2f fd` (0xFD2FB528,
little endian). **Skippable frames**, magic 0x184D2A50 to 0x184D2A5F,
carry user data a decoder skips. A frame header may carry
**Frame_Content_Size** (the decompressed size), a **Dictionary_ID** and a
**Window_Size** (1 KiB to 3.75 TiB). The RFC recommends decoders support
windows up to 8 MiB and encoders not need more; decoders "may set higher
or lower limits". An optional checksum is the low 32 bits of XXH64 of
the output. Its security section warns of well-formed frames with
unreasonable memory needs and of a Frame_Content_Size smaller than the
real output (a buffer overflow in a decoder that trusts it).

libzstd [6]: `ZSTD_decompressDCtx` for a whole buffer,
`ZSTD_decompressStream` for streaming, `ZSTD_d_windowLogMax` to refuse
frames that need a larger window, `ZSTD_getFrameContentSize` to read the
claimed size. **Dictionaries** [7]: `ZDICT_trainFromBuffer` builds one
from samples (about 100 KiB suggested; training takes ~6 MB of memory);
the gain is mostly in the first few kilobytes of each input, so it is for
many small documents of one shape. A frame names its dictionary by ID, so
a decoder can refuse the wrong one. Licence: dual BSD or GPLv2, the
recipient chooses (BSD for this library).

### 2.3 xz and LZMA (XZ Utils, liblzma)

The `.xz` container starts `fd 37 7a 58 5a 00`, holds LZMA2 blocks and a
CRC-32, CRC-64 or SHA-256 check. liblzma's decoders take a `memlimit` and
refuse a stream that would need more (`LZMA_MEMLIMIT_ERROR`); an LZMA2
dictionary at preset `-9` is 64 MiB. Compresses best of the common
formats and decompresses slowest after bzip2.

Licence: liblzma is 0BSD from 5.6.0, public domain before [8]. **Supply
chain**: releases 5.6.0 and 5.6.1 carried a backdoor (CVE-2024-3094,
CVSS 10) planted by a co-maintainer over two years: a modified
`build-to-host.m4` present only in the release tarballs, not in git,
extracted a payload hidden in test files (`bad-3-corrupt_lzma2.xz`) at
build time [9][10]. It was found by chance, from a half-second delay in
SSH logins.

### 2.4 LZ4 frame format

A frame starts with 0x184D2204 (`04 22 4d 18`) [11]; optional content size
and checksums (xxHash32). Very fast to decode, weaker compression than
zstd. Reference library BSD-2-Clause. The raw block format has no header
and is not a file format.

### 2.5 Brotli (RFC 7932, RFC 9841)

RFC 7932 [15] defines a stream with **no magic number**: it starts with
the window size bits. A `.br` file is told only by its name or by HTTP's
`Content-Encoding: br`. RFC 9841 [16] (2025, informational) adds shared
dictionaries, a larger window and a framing format. Built in with a
120 KiB static dictionary aimed at web text. MIT.

### 2.6 bzip2

Magic `BZh` and a digit, then the block magic `1AY&SY`. Slow, rarely
chosen for new data, a small upstream. CVE-2019-12900 (an out-of-bounds
write in `BZ2_decompress`, CVSS 9.8 in NVD) was disputed by upstream as
not exploitable, and the first fix broke valid files [17]. BSD-style
licence.

### 2.7 git

Every object, loose or packed, is zlib-compressed. `core.compression` and
`pack.compression` take -1 (zlib's default, level 6) or 0..9; 0 means
none; changing the level does not rewrite what is stored [18]. The
object's id is the SHA-1 or SHA-256 of the *uncompressed* object, so a
different level or a different zlib gives the same id. Lesson: digest
what was meant, not the compressed bytes.

### 2.8 systemd journal

`journald.conf` `Compress=` [19]: a boolean, on by default, or a byte
count; data objects larger than the threshold (512 bytes by default) are
compressed before being written. The algorithm (xz, lz4 or zstd) is chosen
when systemd is built, and readers support every one built in. Lesson:
compression per object above a threshold; small things are left alone
because a header costs more than it saves.

### 2.9 Kubernetes ConfigMaps and Helm

A ConfigMap holds `data` (UTF-8) and `binaryData` (base64) and "cannot
exceed 1 MiB" [20]. Kubernetes compresses nothing; a large configuration
is compressed by hand into `binaryData`, at a cost of a third for base64.
Helm 3 stores each release in a Secret as a gzipped, base64-encoded
record, base64-encoded again by the API [21][22]; large charts still
overflow the 1 MiB limit. Charts are distributed as `.tgz`. Lesson:
compression appears where a size limit bites, and a configuration library
is asked to read what such a tool wrote.

### 2.10 PostgreSQL

TOAST compresses large values per column: `default_toast_compression` is
`pglz` or `lz4` (since 14; `lz4` only when built `--with-lz4`), and a
column's `COMPRESSION` overrides it [23]. A 2026 commit makes `lz4` the
default when it is built in. Lesson: one built-in algorithm always there,
a faster one by build option, the choice stored with the data so either
reads it back.

### 2.11 HTTP Content-Encoding

RFC 9110 [24] registers content codings; `gzip`, `deflate` (which is the
*zlib* wrapper, a long-standing source of confusion), `br` and `zstd` are
in use. RFC 9842 [25] (2025) adds dictionary transport: a response
becomes a dictionary for later ones, with the codings `dcb` and `dcz`.
The coding is always **declared** (a header), never sniffed.
**BREACH** (2013) [26]: when a response compresses a secret together with
text an attacker chooses, the length of the encrypted result tells the
attacker whether a guess matched; CRIME did the same to TLS compression,
which was then removed.

### 2.12 Avro, Protocol Buffers, gRPC

Avro object container files [27] name a codec per file in the header
(`avro.codec`): `null` and `deflate` (raw RFC 1951) are required;
`bzip2`, `snappy` (with a CRC-32 after each block), `xz` and `zstandard`
are optional. Each block is compressed alone. Protocol Buffers has no
compression; gRPC compresses each message, declared in `grpc-encoding`,
offered in `grpc-accept-encoding`, and a message in an encoding the peer
lacks is an error, never a guess [28].

### 2.13 Runtimes that read compressed input

* **Python** `zlib.decompressobj().decompress(data, max_length)` [29]
  returns at most `max_length` bytes and keeps the rest of the input in
  `unconsumed_tail`, so a caller can stop a bomb; without it, the whole
  output is made.
* **Go** `compress/gzip` reads multi-member files by default
  (`Reader.Multistream`). Before 1.17.12 and 1.18.4 it recursed once per
  member, and a file of many empty members exhausted the stack
  (CVE-2022-30631) [30]; the fix made it a loop.
* **zlib** itself: CVE-2022-37434 [31], a heap overflow from an oversized
  gzip "extra" field, reachable only by programs that call
  `inflateGetHeader`; fixed in 1.2.13. CVE-2018-25032 [32], memory
  corruption when *compressing* some inputs, fixed in 1.2.12.

## 3. What breaks

1. **Decompression bombs.** Deflate's ratio is bounded near 1032:1 for
   one stream; zip bombs pass it by nesting (each layer multiplies by
   ~1032) or, as Fifield showed, by overlapping entries, so that a 46 MB
   zip makes 4.5 PB [33]. zstd and xz have no such ceiling worth relying
   on. A library that decompresses then parses multiplies the ratio by
   the parser's own budget (here 64 x n + 1 MiB of the *decompressed*
   n). Only an output limit checked while decompressing stops a bomb;
   a size in a header does not. [5][29][33]
2. **Sizes in headers are claims.** gzip's ISIZE is modulo 2^32 and is
   that of the last member only; zstd's Frame_Content_Size may be absent
   or wrong, and RFC 8878 warns of decoders that allocate by it [3][5].
   The same rule as docs/ImageFormat.md: "Every number in an image is a
   claim, not a fact."
3. **Memory a frame asks for.** A zstd window or an xz dictionary can ask
   for gigabytes in a valid file; the decoder must be given a ceiling
   (`ZSTD_d_windowLogMax`, liblzma's `memlimit`) [5][8].
4. **Many members, recursion.** Concatenated gzip members or zstd frames
   are valid; an implementation that recurses per member falls over
   (Go, CVE-2022-30631) [30]. Trailing bytes after the last member:
   `gzip -d` warns and ignores them; a library that ignores them reads two
   things from one file.
5. **Nested compression.** `a.gz` inside `a.gz` multiplies ratios, the
   oldest bomb (42.zip) [33].
6. **The rarely used parts of a header.** gzip's extra field, name and
   comment are where zlib's 2022 overflow lived [31]. Not calling
   `inflateGetHeader` avoids it.
7. **Supply chain.** A compression library is linked into almost every
   process; xz showed a maintainer can be the attacker, and that release
   tarballs can differ from the repository [9][10]. A disputed CVE stays
   on every scanner's report for years (bzip2) [17].
8. **No magic number.** Brotli and raw deflate cannot be told from
   content; zlib's two bytes are a weak check (one pair in 31 passes by
   chance) [1][15].
9. **Non-determinism on write.** A gzip header holds a time and an OS
   byte; zlib, zstd and xz each change their output between versions and
   levels. A digest of compressed bytes then changes when nothing meant
   did, which breaks reproducible releases and "the release built twice
   and compared" (docs/Maintaining.md, 1.9.0) [3][18].
10. **Compressing secrets beside chosen text** leaks the secret through
    the length (CRIME, BREACH) [26]. Here: a file written back that holds
    a secret and a value an outsider chose, stored where its size is
    seen. Rare for configuration, but real for a report or an image sent
    over a network.
11. **Locations.** A line and column refer to the decompressed text; a
    person opening the `.gz` file sees nothing at that place.
12. **The confused name.** HTTP's `deflate` is zlib, Avro's `deflate` is
    raw; a reader that takes one for the other fails or, with automatic
    detection, accepts both and so reads what a peer would refuse
    [24][27].

## 4. What fits this library

Measured against D-100 to D-108 and the C89 core.

### 4.1 Where compression goes: a composed codec, or a layer under the codecs

The Roadmap names D-103. A repair can inflate today: the program writes a
repair that calls its own zlib, `cf_codec_compose` makes `json+gzip`, and
the program names it for the file. Nothing in the library changes. But
as the general answer it fits badly:

* A composed codec is `CF_CODEC_NAMED_ONLY` (D-103: "never chosen from
  content"). The Roadmap wants the choice by name *or by magic bytes*.
* Its base is fixed, so every format needs its own composition: M
  compressions times N formats, each registered.
* The repair has no arena, no limit and no reason code; each program
  writes its own bomb check, or none.
* It reads only; writing compressed output is outside it.
* Its name is the program's, so `explain()` cannot say "gzip" the same
  way in every program.

Compression is a property of the **bytes**, not of the format, exactly as
an encoding is. D-108 already places the encoding in one step,
`cfi_source_text`, between reading the bytes and choosing the reader,
with the rule "a byte-order mark is a declaration, not a guess". A
compression magic number is the same kind of thing: a declaration at
offset 0, then *checked* by decompressing with the format's own checksum.
The layer would be:

```text
cf_blob_file -> unpack (by name, by the outer extension, or by magic)
             -> recode (D-108) -> the format (by the inner extension, or D-73)
             -> the codec -> the flattener under the decode budget (D-82)
```

D-73's rule is not broken: the *format* is still decided by reading. The
compression is decided by a declaration that the decompressor then
verifies end to end (gzip's CRC-32, zstd's XXH64 when present, xz's
check), the same standing a BOM has in D-108. Nothing in any reader
changes, which was the point of naming D-103.

Can the magic bytes be read as a text document by mistake? gzip starts
`1f` (a control character no text format here accepts at the start); zstd
`28 b5` (`(` then a UTF-8 continuation byte, invalid UTF-8); xz `fd`
(never valid UTF-8); lz4 `04` (a control character). As CBOR: `1f` and
`fd` are not well formed, `28` and `04` are single integers where the
top must be a map. The image's magic is `CFGB`. bzip2's `BZh9` is
printable, but followed by `1AY&SY` it is ten bytes no configuration
starts with. So detection by these magics cannot take a document that
reads today. Brotli, raw deflate and zlib have none fit for detection and
would be by name only.

### 4.2 The options

| Option | What it needs | Cost | What it rules out |
| ------ | ------------- | ---- | ----------------- |
| A. The D-103 recipe only: a Guide section and an example repair over the program's zlib | nothing | a page and an example | detection by magic; a library-wide bomb limit; writing |
| B. A byte layer in `lib/serialize`, reading, gzip and zstd, each behind `CF_WITH_ZLIB` / `CF_WITH_ZSTD` (OFF by default) | two optional dependencies; a new public interface; `cfi_source_text` and the image source call it | one component file per backend, limits, reasons, a fuzz target each, rows in Dependencies.md | nothing; A stays possible |
| C. B, and writing: `cf_emit` output and `cf_image_write` compressed, deterministically | the compress half of each library | a pack function per backend; fixed headers | nothing |
| D. A built-in inflate (RFC 1951), no dependency, whole buffer, C89 | none outside | ~600 lines under the `misra` rules, fuzzed against zlib | gzip reading only; writing still needs zlib (or stored blocks, which do not compress) |
| E. xz and lz4 frames as further backends | liblzma, liblz4 | one file each | - |
| F. zstd dictionaries | the program's dictionary bytes | an option on a source; a dictionary ID check | a file readable alone |
| G. Brotli, bzip2, raw deflate | their libraries | as E | - |
| H. Archives (zip, tar) | libarchive or own code | containers, paths inside, zip-slip, overlap bombs | - |

Against the decisions:

* **D-100.** The byte layer is serialization's: `lib/serialize/src/unpack.c`
  plus one file per backend (`zlib.c`, `zstd.c`), behind an interface, so
  the configuration library and `configforge codec` both get it. It uses
  `base` (arena, error) only. The layer table needs no new row.
* **D-101, D-102.** Untouched. A built-in inflate (D) would use the text
  helpers for nothing; it is bytes only.
* **D-103.** Kept for what it is for: repairing what is almost a format.
  The recipe of option A stays valid for a compression the library lacks.
  Compression itself is a layer under it, not a composition.
* **D-104.** A baked configuration is C tables; compression has no part.
  `#embed` of a compressed image is possible with D, and pointless with
  a dependency on a host-only library.
* **D-105.** `full` and `server` may set the backends ON; `default` leaves
  them OFF (no dependency in a default build, docs/Dependencies.md);
  `minimal`, `misra`, `embedded-json` and `embedded-cbor` set them OFF and
  refuse them by name, as they refuse an above-C89 file.
* **D-106.** zlib's and zstd's headers are C89-clean, so no entry. A
  vendored zstd would bring C99 files, one entry each, which the `misra`
  and `minimal` profiles refuse; another reason not to vendor it.
* **D-107.** New reasons in `cf_reason.def`: the output limit
  (`CF_REASON_BUDGET` may serve, with the message naming the
  decompression), a bad checksum (new, `CF_REASON_CHECKSUM`), a frame that
  asks for more memory than allowed (new, or `CF_REASON_BUDGET`), and
  trailing bytes (`CF_REASON_TRAILING` exists). Truncated input is
  `CF_REASON_TRUNCATED`. The same refusal from gzip and zstd.
* **D-108.** Same step, same rule of declaration. Order: unpack first,
  then recode; a compressed UTF-16 file is a gzip of UTF-16 bytes.
* **C89 core.** The library calls a dependency's C API; nothing above C89
  is compiled. The whole-buffer model holds: the decompressor streams
  internally, into a growing arena buffer, so the output limit is checked
  as bytes come out; the readers still get one `cf_blob`.

**The image.** `cf_image_open` reads in place, aligned, with no
allocation: that is why the format exists (docs/ImageFormat.md "Why it
exists"). A compressed image gives that up: it needs a buffer the size of
the image. For a hosted `cf_source_image_file` this is the arena
`cf_blob_file` already fills, and unpacking into an arena allocation
keeps the 8-byte alignment. For the freestanding `cf_source_image_mem`
path it would need the program's buffer and an inflater with no heap
(only D, or tinf, offers that). The image's digest covers the key space,
so it is unchanged by compression; the compressed file's own checksum
catches a damaged flash page earlier. Compressing *inside* the image
(per value) would break lookup by pointer arithmetic and is not wanted.

**CBOR and TLV output.** Binary already; zstd still shrinks CBOR of
repeated keys well. Nothing special: the bytes the encoder wrote into a
`cf_sink` are compressed whole. TLV as embedded devices write it is read
by a layout (D-92); a device that compresses its TLV records names the
compression on the source like any other.

### 4.3 Bounding a bomb with the library's limits

Three bounds, each checked while decompressing, never from a header:

1. **Output by input**: at most `CF_UNPACK_FACTOR` x n + `CF_UNPACK_FLOOR`
   bytes from n compressed bytes, the same form as the decode budget
   (D-82). With a factor of 64 and a floor of 1 MiB: a 10 KiB `.gz`
   may make 1.6 MiB; a 1 MiB one, 65 MiB. Real configuration compresses
   3x to 20x; a 64x allowance passes it with room and refuses every
   classic bomb (1000x and more) by name.
2. **An absolute ceiling**, `CF_UNPACK_MAX`, for what one file may make
   (256 MiB is P-1's largest run, docs/Limits.md "The largest document").
3. **The decoder's own memory**: zstd's `windowLogMax` at 23 (8 MiB, RFC
   8878's recommendation) and liblzma's `memlimit` at the same order. A
   frame that asks for more is refused, with the size it asked for in the
   message.

Then the decode budget applies to the decompressed bytes as it does to
any document. The product of the two bounds is the worst case, and it is
finite and stated. Multi-member input is read in a loop, never by
recursion; one layer only: unpacked bytes that start with a compression
magic again are refused, not unpacked again.

These are constants, not build options: like the decode budget, they size
no buffer (docs/Limits.md: "It is not a build option: it sizes no
buffer"). They go into the generated part of docs/Limits.md through
`scripts/generate/limits.cmake`, held by a test.

### 4.4 Licence and supply chain, per library

| Library | Licence | Fits the Zlib licence | Supply-chain note |
| ------- | ------- | --------------------- | ----------------- |
| zlib | Zlib | same | ubiquitous; system package; floor 1.2.13 (CVE-2022-37434, CVE-2018-25032) |
| zlib-ng (compat) | Zlib | same | found as `zlib` in compat mode; no separate support |
| libzstd | BSD or GPLv2 | yes, under BSD | one company's project, wide review; floor to set by the APIs used (`ZSTD_d_windowLogMax` is in the stable advanced API from 1.4.0) |
| liblzma | 0BSD (5.6.0+), public domain before | yes | CVE-2024-3094; refuse 5.6.0 and 5.6.1 by version at configure time |
| liblz4 | BSD-2-Clause | yes | small, stable |
| brotli | MIT | yes | no magic number |
| bzip2 | BSD-style | yes | small upstream, disputed CVE on every scanner |
| libdeflate | MIT | yes | whole-buffer, fast; no streaming, so the output limit must be the buffer given |
| miniz | MIT (current) | yes | vendored copies drift; one file |
| tinf | Zlib | same | small, embedded; vendored |

The memory note "Enforce version floors" applies: a floor is checked at
build, at run time (`zlibVersion()`, `ZSTD_versionNumber()`) and in the
`.pc` file, never only written down. Each dependency is a row in
docs/Dependencies.md, found by its header and library name as libfyaml and
expat are, and in the SBOM's options.

### 4.5 Embedded and minimal profiles

The embedded presets have no heap or a small one and often no file
system. A whole-buffer inflate (tinf, `tinfl`, libdeflate's design, or
option D) needs the output buffer and about 2 KiB to 11 KiB of tables;
the output is its own window, so no 32 KiB window is allocated. zstd's
decoder needs its window (up to 8 MiB by the RFC's advice) plus a context
of over 100 KiB; xz's dictionary is larger. So on a device: gzip by a
built-in inflate, or nothing. The `minimal` and `misra` profiles keep
compression OFF; it is never in the default build.

### 4.6 Dictionaries

A zstd dictionary pays when there are many small documents of one shape:
a fleet's device configurations, the store of Doctor reports, images sent
over a slow link. Most configuration files are read one at a time and are
already small. A dictionary also makes a file unreadable alone: the
reader must hold the same dictionary, named by ID. It is a later option on
a source (the program's bytes, borrowed, with the ID checked), not a first
step.

## 5. Recommendation

**First: reading gzip and zstd, as a byte layer.** Option B. It answers
the Roadmap item in full on the read side, keeps every reader unchanged,
and gives one bound against bombs instead of one per program.

* A new interface in `lib/serialize`, `cf_unpack`, with two backends,
  each behind its own option, both OFF by default: `CF_WITH_ZLIB`
  (gzip; zlib and zlib-ng in compat mode) and `CF_WITH_ZSTD`.
* Chosen in this order, the first that answers deciding, as D-73 does for
  formats: the program names it (a `CF_COMPRESSED_*` value in the file
  source's flags, as `CF_ENCODING_*` is); the outer extension (`.gz`,
  `.zst`), after which the inner extension (`.toml`) names the format; the
  magic bytes. `CF_COMPRESSED_NONE` refuses compressed bytes outright, for
  a program that wants no decompressor near its input.
* The layer is called from `cfi_source_text` before recoding, from the
  image file source, and from `configforge codec` (`detect` says "gzip,
  then TOML by the extension").
* Bounds as section 4.3; gzip's header read by the library's own code
  (fixed fields only; name, comment and extra skipped by length) so that
  `inflateGetHeader` is never called; raw inflate over the body;
  CRC-32 and ISIZE checked; members in a loop; trailing bytes refused.
* Provenance and errors name both: `app.toml.gz (gzip)`, and a line and
  column "in the decompressed text".
* A compression the library lacks (brotli, a program's own): the
  program registers a backend through the same interface, or uses the
  D-103 recipe. The Guide shows both.

**After: writing.** Option C. `cf_pack` over a finished buffer, for
`configforge codec convert --compress`, `cf_emit` output and
`cf_image_write`. Deterministic: gzip with MTIME 0 and OS 255 (set
through `deflateSetHeader`), a fixed level; the digest and the
provenance always of the uncompressed bytes. The whole-or-nothing write
of wave 6 (2.2.0) writes the compressed bytes like any other.

**Later, each when asked for:** a built-in inflate (D) for the embedded
presets that want a compressed image with no dependency; xz (with the
5.6.0/5.6.1 refusal and a `memlimit`) and lz4 frames (E); zstd
dictionaries (F).

**Not to build:** brotli and raw deflate as detected formats (no magic;
by name through a program's backend only); bzip2 (slow, legacy, nothing
a configuration needs); archives (H: zip and tar are containers of many
files with paths and their own attacks, a different feature); nested
decompression; compression inside the image format; a vendored zstd or
liblzma (C99 files, a large surface under the `misra` rules, and the xz
lesson).

**Public interface, sketched** (in `cf_serialize.h`; `size` first, as
D-51 does):

```c
typedef struct cf_unpacker {
    size_t      size;        /* sizeof(cf_unpacker) */
    const char* name;        /* "gzip", "zstd" */
    const char* extensions;  /* ".gz", ".zst;.zstd" */
    const unsigned char* magic; /* NULL: by name only */
    size_t      magic_len;
    /* Whole input in, whole output into `ar`, at most `limit` bytes. */
    cf_bool (*unpack)(const struct cf_unpacker* self, const cf_blob* in,
                      cf_arena* ar, size_t limit, cf_blob* out, cf_error* err);
    /* NULL when the backend only reads. Measure-or-write, like cf_sink. */
    cf_bool (*pack)(const struct cf_unpacker* self, const char* in, size_t n,
                    int level, cf_sink* out, cf_error* err);
    const void* data;        /* a dictionary, a program's state */
} cf_unpacker;

CF_API size_t             cf_unpack_count(void);
CF_API const cf_unpacker* cf_unpack_at(size_t i);
CF_API const cf_unpacker* cf_unpack_by_name(const char* name);
/* By the outer extension of `path`, then by magic; NULL when neither. */
CF_API const cf_unpacker* cf_unpack_detect(const cf_blob* in, const char* path);
/* The limit of section 4.3 for n compressed bytes. */
CF_API size_t  cf_unpack_limit(size_t n);
CF_API cf_bool cf_unpack(const cf_unpacker* u, const cf_blob* in, cf_arena* ar,
                         cf_blob* out, cf_error* err);
CF_API cf_bool cf_pack(const cf_unpacker* u, const char* in, size_t n, int level,
                       cf_sink* out, cf_error* err);

/* A program's own backend, looked at before the built-in ones. */
CF_API cf_bool cf_builder_unpacker(cf_builder* b, const cf_unpacker* u);
```

The name `cf_unpacker` is a placeholder; `cf_compression` reads as well.

**TO DECIDE (the maintainer):**

1. **Where compression sits.** (a) A composed codec, as the Roadmap
   says: no new interface, but named only, M x N compositions, no shared
   bound, read only. (b) A byte layer under the codecs beside D-108's
   recoding. **Recommended: (b)**, with (a) kept as the recipe for what
   the library lacks. This changes the Roadmap's wording, so it is the
   maintainer's.
2. **New optional dependencies.** zlib (`CF_WITH_ZLIB`) and libzstd
   (`CF_WITH_ZSTD`), both OFF by default, ON in `full` and `server`.
   Options: both; zlib only; none and a built-in inflate (D) instead.
   **Recommended: both**, D later if an embedded user asks.
3. **Detection by magic bytes when built in.** (a) On, refusable per
   source with `CF_COMPRESSED_NONE`. (b) Off unless the source names a
   compression or the outer extension does. **Recommended: (a)**: a magic
   is a declaration like a BOM, the bounds hold either way, and a program
   that built the backend in asked for it. (b) is the safer default for a
   program that never expects compressed input but links a build that
   has it.
4. **The bounds' numbers.** `CF_UNPACK_FACTOR` 64, `CF_UNPACK_FLOOR`
   1 MiB, `CF_UNPACK_MAX` 256 MiB, zstd `windowLogMax` 23.
   **Recommended as written**; each could be a run-time option on the
   builder later if a user brings a file that needs more.
5. **The new public interface** sketched above (names and the place of
   `pack` in the same struct). **Recommended: one struct for both
   directions**, `pack` NULL when a backend reads only.
6. **Which release.** Additive, so any 1.x minor. **Recommended: reading
   in 1.10.0** (after the layout of 1.8.0 gives it `lib/serialize`, and
   after 1.9.0's fuzzing and OSS-Fuzz exist to take a new target),
   writing with wave 6 in 2.2.0, the rest unscheduled.

## 6. The cards it implies

Sizes: S a day or less, M two or three, L a week. Versions from
docs/Maintaining.md "Versions".

| Card | Size | Version |
| ---- | ---- | ------- |
| The D-103 recipe: a Guide section and `examples/c/` repair that inflates with the program's zlib | S | any, before the rest |
| The `cf_unpacker` interface, the bounds in `docs/Limits.md` (generated), the reasons in `cf_reason.def`, a NULL backend and its tests | M | 1.10.0 |
| gzip through zlib: `CF_WITH_ZLIB`, own header reading, members in a loop, floor 1.2.13 checked three ways, Dependencies.md row, fuzz target | M | 1.10.0 |
| zstd through libzstd: `CF_WITH_ZSTD`, window limit, frames and skippable frames, fuzz target | M | 1.10.0 |
| The sources: `cfi_source_text`, the directory source, the image file source, `CF_COMPRESSED_*` flags, the inner extension, provenance; C++, Rust and Python bindings | M | 1.10.0 |
| `configforge codec`: `detect`, `check` and `dump` through the layer; a bomb corpus in the tests | S | 1.10.0 |
| Writing: `cf_pack`, deterministic gzip and zstd, `convert --compress`, a compressed image, the digest of the uncompressed bytes | M | 2.2.0 |
| A built-in inflate for the embedded presets, fuzzed differentially against zlib | L | when asked |
| xz and lz4 frames | M | when asked |
| zstd dictionaries on a source | M | when asked |

## 7. Sources

1. RFC 1950, ZLIB Compressed Data Format: https://www.rfc-editor.org/rfc/rfc1950
2. RFC 1951, DEFLATE Compressed Data Format: https://www.rfc-editor.org/rfc/rfc1951
3. RFC 1952, GZIP File Format: https://www.rfc-editor.org/rfc/rfc1952
4. zlib manual: https://www.zlib.net/manual.html
5. RFC 8878, Zstandard Compression and the application/zstd Media Type: https://www.rfc-editor.org/rfc/rfc8878.html
6. Zstandard, the project and its licence: https://github.com/facebook/zstd
7. zstd `zdict.h`, dictionary training: https://github.com/facebook/zstd/blob/dev/lib/zdict.h
8. XZ Utils, licence (COPYING) and documentation: https://tukaani.org/xz/
9. XZ Utils backdoor (CVE-2024-3094), summary and timeline: https://en.wikipedia.org/wiki/XZ_Utils_backdoor
10. Lins, Mayrhofer, Roland, Hofer and Schwaighofer, "On the critical path to implant backdoors and the effectiveness of potential mitigation techniques: Early learnings from XZ" (2024): https://arxiv.org/abs/2404.08987
11. LZ4 Frame Format Description: https://github.com/lz4/lz4/blob/dev/doc/lz4_Frame_format.md
12. libdeflate: https://github.com/ebiggers/libdeflate
13. miniz: https://github.com/richgel999/miniz
14. tinf: https://github.com/jibsen/tinf
15. RFC 7932, Brotli Compressed Data Format: https://www.rfc-editor.org/rfc/rfc7932
16. RFC 9841, Shared Brotli Data Format: https://www.rfc-editor.org/rfc/rfc9841
17. CVE-2019-12900 (bzip2), Debian tracker with the dispute: https://security-tracker.debian.org/tracker/CVE-2019-12900
18. git-config, `core.compression` and `pack.compression`: https://git-scm.com/docs/git-config
19. journald.conf(5), `Compress=`: https://www.freedesktop.org/software/systemd/man/latest/journald.conf.html
20. Kubernetes, ConfigMaps: https://kubernetes.io/docs/concepts/configuration/configmap/
21. Helm 3, HIP 0003, state: https://helm.sh/community/hips/archives/helm/helm-v3/003-state
22. Helm storage driver package: https://pkg.go.dev/helm.sh/helm/v3/pkg/storage/driver
23. PostgreSQL, `default_toast_compression`: https://www.postgresql.org/docs/current/runtime-config-client.html
24. RFC 9110, HTTP Semantics, content codings: https://www.rfc-editor.org/rfc/rfc9110#name-content-codings
25. RFC 9842, Compression Dictionary Transport: https://www.rfc-editor.org/rfc/rfc9842
26. CERT VU#987798, BREACH: https://kb.cert.org/vuls/id/987798
27. Apache Avro 1.12.0 specification, object container files and codecs: https://avro.apache.org/docs/1.12.0/specification/
28. gRPC compression: https://github.com/grpc/grpc/blob/master/doc/compression.md
29. Python `zlib`, `Decompress.decompress(data, max_length)`: https://docs.python.org/3/library/zlib.html
30. Go issue 53168, CVE-2022-30631, `compress/gzip` stack exhaustion: https://go.dev/issue/53168
31. CVE-2022-37434 (zlib `inflateGetHeader`): https://nvd.nist.gov/vuln/detail/CVE-2022-37434
32. CVE-2018-25032 (zlib deflate memory corruption): https://nvd.nist.gov/vuln/detail/CVE-2018-25032
33. Fifield, "A better zip bomb", WOOT 2019: https://www.usenix.org/conference/woot19/presentation/fifield
