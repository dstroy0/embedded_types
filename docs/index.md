<div class="embed-hero" markdown>

# embedded_types

Six C11 headers hold the fixed-width integers, the machine word, the lane, atomic words, turnstiles and the compile-time checks around them. Each library in a build takes them from one place.

</div>

The target sets three widths, and the build sets none. Pick a target to see them. A cell is one byte.

<div class="embed-register">
<fieldset class="embed-targets">
<legend>Target</legend>
<label><input type="radio" name="embed-target" value="mcu16"> 16-bit MCU</label>
<label><input type="radio" name="embed-target" value="mcu32"> 32-bit MCU</label>
<label><input type="radio" name="embed-target" value="neon"> AArch64 NEON</label>
<label><input type="radio" name="embed-target" value="sse2"> x86-64 SSE2</label>
<label><input type="radio" name="embed-target" value="avx2" checked> x86-64 AVX2</label>
<label><input type="radio" name="embed-target" value="avx512"> x86-64 AVX-512</label>
<label><input type="radio" name="embed-target" value="cuda"> CUDA device</label>
</fieldset>
<div class="embed-rows" aria-hidden="true">
<div class="embed-row embed-row--word"><span class="embed-row__name">word</span><span class="embed-row__cells"><i style="--i:0"></i><i style="--i:1"></i><i style="--i:2"></i><i style="--i:3"></i><i style="--i:4"></i><i style="--i:5"></i><i style="--i:6"></i><i style="--i:7"></i></span></div>
<div class="embed-row embed-row--index"><span class="embed-row__name">index</span><span class="embed-row__cells"><i style="--i:0"></i><i style="--i:1"></i><i style="--i:2"></i><i style="--i:3"></i></span></div>
<div class="embed-row embed-row--lane"><span class="embed-row__name">lane</span><span class="embed-row__cells"><i style="--i:0"></i><i style="--i:1"></i><i style="--i:2"></i><i style="--i:3"></i><i style="--i:4"></i><i style="--i:5"></i><i style="--i:6"></i><i style="--i:7"></i><i style="--i:8"></i><i style="--i:9"></i><i style="--i:10"></i><i style="--i:11"></i><i style="--i:12"></i><i style="--i:13"></i><i style="--i:14"></i><i style="--i:15"></i><i style="--i:16"></i><i style="--i:17"></i><i style="--i:18"></i><i style="--i:19"></i><i style="--i:20"></i><i style="--i:21"></i><i style="--i:22"></i><i style="--i:23"></i><i style="--i:24"></i><i style="--i:25"></i><i style="--i:26"></i><i style="--i:27"></i><i style="--i:28"></i><i style="--i:29"></i><i style="--i:30"></i><i style="--i:31"></i><i style="--i:32"></i><i style="--i:33"></i><i style="--i:34"></i><i style="--i:35"></i><i style="--i:36"></i><i style="--i:37"></i><i style="--i:38"></i><i style="--i:39"></i><i style="--i:40"></i><i style="--i:41"></i><i style="--i:42"></i><i style="--i:43"></i><i style="--i:44"></i><i style="--i:45"></i><i style="--i:46"></i><i style="--i:47"></i><i style="--i:48"></i><i style="--i:49"></i><i style="--i:50"></i><i style="--i:51"></i><i style="--i:52"></i><i style="--i:53"></i><i style="--i:54"></i><i style="--i:55"></i><i style="--i:56"></i><i style="--i:57"></i><i style="--i:58"></i><i style="--i:59"></i><i style="--i:60"></i><i style="--i:61"></i><i style="--i:62"></i><i style="--i:63"></i></span></div>
</div>
<pre class="embed-defines" aria-live="polite"><code>#define EMBED_WORD_BITS  <b class="embed-v embed-v--word"></b>
#define EMBED_INDEX_BITS <b class="embed-v embed-v--index"></b>
#define EMBED_LANE_BITS  <b class="embed-v embed-v--lane"></b></code></pre>
</div>

The word is [`embed_word`](api/embed_types.md#embed_word), the register that each count, mask and carry runs in. The index is [`embed_index`](api/embed_types.md#embed_index), for each offset and length. The lane is [`embed_lane`](api/embed_lane.md#embed_lane), the widest vector register of the target. [The word and the lane](word.md) tells how each width comes from the target.

## The headers

| header                                                            | what it holds                                                                    |
| ----------------------------------------------------------------- | -------------------------------------------------------------------------------- |
| [`embed_types.h`](api/embed_types.md)                             | the widths, the boolean, the word, the index, the raw word, and their assertions |
| [`embed_compiler_directives.h`](api/embed_compiler_directives.md) | feature checks, the static assertion, attribute wrappers, diagnostic pragmas     |
| [`embed_dispatch_layout.h`](api/embed_dispatch_layout.md)         | `EMBED_TABLE_LAYOUT`, `EMBED_CALL`, `EMBED_ENTRY`, and the macros behind them    |
| [`embed_atomic.h`](api/embed_atomic.md)                           | 32-bit and 64-bit atomic words for host, part and GPU threads, on four arms      |
| [`embed_turnstile.h`](api/embed_turnstile.md)                     | an atomic thread scheduler: turnstiles that readers pass through and that a writer drains |
| [`embed_lane.h`](api/embed_lane.md)                               | the lane, its loads, fills, arithmetic and byte masks                            |

No header allocates memory or runs code at startup. The first three include only `stddef.h` and `stdint.h`. Each entry of the atomics and the lane has `EMBED_HOST_DEVICE`, so a kernel calls the same functions that a host walk calls.

## A first use

```c
#include "embed_types.h"
#include "embed_dispatch_layout.h"

static embed_u32 crc;
static embed_word accumulator;
static embed_index offset;

static embed_word at_odd_offset(const embed_u8 *bytes)
{
    return *(const embed_raw_word *)(const void *)(bytes + 1);
}

typedef struct
{
    embed_index (*length)(const embed_u8 *at);
    embed_bool (*is_empty)(const embed_u8 *at);
} ScanNs;
EMBED_TABLE_LAYOUT(ScanNs, length, is_empty);
```

[`embed_raw_word`](api/embed_types.md#embed_raw_word) is `embed_word` with `EMBED_RAW` on it. That attribute lowers the alignment to 1 and permits access through an alias. C leaves a read through the plain `embed_word` undefined where the address is not a multiple of the word width.

[`EMBED_TABLE_LAYOUT`](api/embed_dispatch_layout.md#EMBED_TABLE_LAYOUT) emits one static assertion for the byte offset of each member, and one for `sizeof(ScanNs)`. The build fails where the list leaves out a member, or where padding comes between two members.

## What the headers guarantee

[`SECURITY.md`](https://github.com/dstroy0/embedded_types/blob/main/SECURITY.md) tells what the headers guarantee, and what they do not diagnose.
