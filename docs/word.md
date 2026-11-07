# The word and the lane

Three widths come from the target: the word, the index and the lane. The compiler supplies the macros that set each one. A build can also set each width on the command line, and the header then records that it did.

## The word

The width of the word comes from `UINTPTR_MAX`, which the compiler supplies.

| `UINTPTR_MAX`        | [`EMBED_WORD_BITS`](api/embed_types.md#EMBED_WORD_BITS) |
| -------------------- | ------------------------------------------------------- |
| `0xFFFFFFFFFFFFFFFF` | 64                                                      |
| `0xFFFFFFFF`         | 32                                                      |
| `0xFFFF`             | 16                                                      |
| absent, or any other | `#error`                                                |

Define `EMBED_WORD_BITS` on the command line to set the width yourself. The header then sets `EMBED_WORD_BITS_WAS_OVERRIDDEN` to 1. Where the header derives the width, the flag is 0.

Set the width where the register width differs from the pointer width. The x32 and ILP32-on-64 ABIs are examples. A large-model eight-bit target is another: its pointer is wider than the register that indexes with it.

## The index

An offset does not need the full width of the register, so the index has its own width. [`EMBED_INDEX_BITS`](api/embed_types.md#EMBED_INDEX_BITS) is 16 where `EMBED_WORD_BITS` is 16, and 32 above it. For 16-bit offsets on a wider register, define `EMBED_INDEX_BITS` ahead of the header. An assertion stops an override wider than the word.

## The lane

The word is the register, and each count, mask and carry runs in it. The lane is the widest vector register of the target. Its width comes from the instruction set macros of the compiler, as the width of the word comes from `UINTPTR_MAX`. A bulk pass over memory steps one lane at a time: a copy, a fill, a compare, a blend of pixels. A build for a wider register widens each pass, with no edit to the code.

| target                                | [`EMBED_LANE_BITS`](api/embed_lane.md#EMBED_LANE_BITS) |
| ------------------------------------- | ------------------------------------------------------ |
| a CUDA device                         | 256, the eight 32-bit registers of a thread            |
| AVX-512                               | 512                                                    |
| AVX2                                  | 256                                                    |
| SSE2, NEON                            | 128                                                    |
| no vector unit, 64-bit word           | 64                                                     |
| no vector unit, 32-bit or 16-bit word | 32                                                     |

Define `EMBED_LANE_BITS` on the command line to set the width yourself. The header then sets `EMBED_LANE_BITS_WAS_OVERRIDDEN` to 1, and to 0 where it derives the width. Assertions hold each width to whole 32-bit lanes and whole words, and to 512 bits at most. The byte mask has room for 512 bits.

### The two arms

| arm      | `embed_lane`                                                   | the arm the header takes                                                    |
| -------- | -------------------------------------------------------------- | --------------------------------------------------------------------------- |
| vector   | `embed_u32` with `EMBED_VECTOR(EMBED_LANE_BYTES)`              | `EMBED_HAS_VECTOR_SIZE` is 1, `EMBED_LANE_PORTABLE` is 0, not a CUDA device |
| portable | a struct of `EMBED_LANE_U32S` `embed_u32`, aligned to the lane | each other case, a CUDA device among them                                   |

Both arms define the same operations. The vector arm uses the plain operators over the vector type. The portable arm uses loops, which the compiler makes into vector code where it can. A load or a store at any address goes through `embed_lane_raw`, the lane with `EMBED_RAW`, as `embed_raw_word` is the word.

The byte masks use the mask builtin of the compiler where the directives header finds one for the width of the lane. That is `EMBED_BYTE_MASK_256` under AVX2 and `EMBED_BYTE_MASK_128` under SSE2, one instruction each. On other targets, one multiply over the full lane collects the bits. The suite builds the vector arm at the derived width by default, and the portable arm with `-DEMBED_LANE_PORTABLE=1`.
