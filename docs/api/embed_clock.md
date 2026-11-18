<!-- tools/api_pages.py writes this page from include/embed_clock.h. Edit the header, then run the tool. -->

# embed_clock.h

Source: [`include/embed_clock.h`](https://github.com/dstroy0/embedded_types/blob/main/include/embed_clock.h)

The clock selection: the architecture, its counter, the clock that the build reads, and the scale of the ticks of that clock to a microsecond.

> **Note:** Nothing below names a part, a vendor or a board. A list of parts covers the parts that its writer knew, and it misses each part released after. The test here is the architecture level, which each member of a family defines. An ATSAMD21 answers the ARMv6-M question as each Cortex-M0+ does.

> **Note:** The three families are ARM, RISC-V and Xtensa, through GCC or clang. A build that is none of them is a host build, and the suites run there.

> **Note:** The clock is the caller's, or a counter pinned to a core that the caller names. A counter that runs and that a port can read is the full requirement. This header scales its ticks against the frequency that the caller states.

> **Note:** Sources for the macros below: Arm C Language Extensions, https://arm-software.github.io/acle/main/acle.html. RISC-V C API specification, https://github.com/riscv-non-isa/riscv-c-api-doc. GCC, gcc/config/xtensa/xtensa.h, `TARGET_CPU_CPP_BUILTINS`.

> _**Warning:**_ Each of [`EMBED_CLOCK_HZ`](#EMBED_CLOCK_HZ), [`EMBED_CLOCK_SOURCE`](#EMBED_CLOCK_SOURCE) and [`EMBED_CLOCK_CORE`](#EMBED_CLOCK_CORE) that the build does not set gives a `#warning` that names the value this header took. A build that states all three gets none.

## Macros

### `EMBED_ARCH_ARM` { #EMBED_ARCH_ARM }

```c
#if defined(__arm__) || defined(__aarch64__) || defined(__thumb__)
#define EMBED_ARCH_ARM 1
#else
#define EMBED_ARCH_ARM 0
#endif
```

Set where this is an ARM build, in either execution state.

> **Note:** `__arm__` covers each 32-bit ARM from ARMv4, and `__aarch64__` covers the 64-bit state. ACLE keeps both, and so a test of the pair catches each ARM subfamily and names none.

### `EMBED_ARCH_RISCV` { #EMBED_ARCH_RISCV }

```c
#if defined(__riscv)
#define EMBED_ARCH_RISCV 1
#else
#define EMBED_ARCH_RISCV 0
#endif
```

Set where this is a RISC-V build, at any register width.

> **Note:** The RISC-V C API defines `__riscv` as 1 on each RISC-V target. It has no second name and no variant for a vendor.

### `EMBED_ARCH_XTENSA` { #EMBED_ARCH_XTENSA }

```c
#if defined(__XTENSA__) || defined(__xtensa__)
#define EMBED_ARCH_XTENSA 1
#else
#define EMBED_ARCH_XTENSA 0
#endif
```

Set where this is an Xtensa build.

> **Note:** GCC defines both names on each Xtensa build. A clang build for Xtensa is a different front end, and so the test reads both names.

### `EMBED_ARCH_HOST` { #EMBED_ARCH_HOST }

```c
#if !EMBED_ARCH_ARM && !EMBED_ARCH_RISCV && !EMBED_ARCH_XTENSA
#define EMBED_ARCH_HOST 1
#else
#define EMBED_ARCH_HOST 0
#endif
```

Set where this is none of the three families, which is a host build.

### `EMBED_ARCH_NAME` { #EMBED_ARCH_NAME }

```c
#if EMBED_ARCH_ARM
#if defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'M')
#if __ARM_ARCH >= 8
#define EMBED_ARCH_NAME "ARMv8-M"
#elif __ARM_ARCH == 7
#define EMBED_ARCH_NAME "ARMv7-M"
#else
#define EMBED_ARCH_NAME "ARMv6-M"
#endif
#elif defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'R')
#define EMBED_ARCH_NAME "ARM real-time profile"
#elif defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'A')
#if defined(__ARM_64BIT_STATE)
#define EMBED_ARCH_NAME "ARM application profile, 64 bit"
#else
#define EMBED_ARCH_NAME "ARM application profile, 32 bit"
#endif
#else
#define EMBED_ARCH_NAME "ARM, pre-Cortex"
#endif
#elif EMBED_ARCH_RISCV
#if defined(__riscv_32e) || defined(__riscv_64e) || defined(__riscv_abi_rve)
#if __riscv_xlen == 32
#define EMBED_ARCH_NAME "RV32E"
#else
#define EMBED_ARCH_NAME "RV64E"
#endif
#elif __riscv_xlen == 32
#define EMBED_ARCH_NAME "RV32"
#elif __riscv_xlen == 64
#define EMBED_ARCH_NAME "RV64"
#else
#define EMBED_ARCH_NAME "RV128"
#endif
#elif EMBED_ARCH_XTENSA
#if defined(__XTENSA_WINDOWED_ABI__)
#define EMBED_ARCH_NAME "Xtensa, windowed ABI"
#elif defined(__XTENSA_CALL0_ABI__)
#define EMBED_ARCH_NAME "Xtensa, call0 ABI"
#else
#define EMBED_ARCH_NAME "Xtensa"
#endif
#else
#define EMBED_ARCH_NAME "this host"
#endif
```

The name of this architecture in a diagnostic.

> **Note:** ARMv8-M and ARMv8.1-M, which are Cortex-M23 through M85. The cycle counter test below makes the split between baseline and mainline that matters.

The name of this architecture in a diagnostic.

The name of this architecture in a diagnostic.

The name of this architecture in a diagnostic.

> **Note:** A host build. The suites run here, and the caller reaches the host's clock through its own library. This header has no core to pin there.

### `EMBED_ARCH_CYCLE_COUNTER` { #EMBED_ARCH_CYCLE_COUNTER }

```c
#if EMBED_ARCH_ARM
#if defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'M')
#if __ARM_ARCH >= 7
#define EMBED_ARCH_CYCLE_COUNTER 1
#else
#define EMBED_ARCH_CYCLE_COUNTER 0
#endif
#else
#define EMBED_ARCH_CYCLE_COUNTER 1
#endif
#elif EMBED_ARCH_RISCV
#define EMBED_ARCH_CYCLE_COUNTER 1
#elif EMBED_ARCH_XTENSA
#define EMBED_ARCH_CYCLE_COUNTER 1
#else
#define EMBED_ARCH_CYCLE_COUNTER 0
#endif
```

Set where the architecture defines a counter that a port can read.

> **Note:** ARMv7-M and later have the DWT, and a port reads its cycle counter. ARMv6-M has no DWT. A Cortex-M0 or M0+ has no counter, and the caller supplies the clock.

> **Note:** The application and real-time profiles have the generic timer and the performance monitors. A port can read either of them.

> _**Warning:**_ Says that the architecture defines a counter, and never that the part has it. `DWT_CYCCNT` is optional where the DWT is present. The port finds out whether the counter is there. This only selects the default.

Set where the architecture defines a counter that a port can read.

> **Note:** The counter is the Zicntr CSR, which each profile has. A port reads it with rdcycle, or from mcycle in machine mode.

> _**Warning:**_ Says that the architecture defines a counter, and never that this build can read it. The mcounteren register controls a read of cycle from a lower privilege level, and a platform can keep it closed.

Set where the architecture defines a counter that a port can read.

> **Note:** CCOUNT, the special register of the cycle count. A port reads it with rsr.

> _**Warning:**_ Says that the architecture defines a counter, and never that this core has it. An Xtensa core is configurable and the timer option can be absent. The port finds out.

Set where the architecture defines a counter that a port can read.

> **Note:** Zero, and not because a host has no counter. A host build has no core to pin a timer to and no architectural register to read. So the clock is the caller's, and a host build takes the arm that a Cortex-M0 takes, for the same reason.

### `EMBED_REGISTER_BITS` { #EMBED_REGISTER_BITS }

```c
#if EMBED_ARCH_ARM
#if defined(__ARM_64BIT_STATE)
#define EMBED_REGISTER_BITS 64u
#else
#define EMBED_REGISTER_BITS 32u
#endif
#elif EMBED_ARCH_RISCV
#define EMBED_REGISTER_BITS __riscv_xlen
#elif EMBED_ARCH_XTENSA
#define EMBED_REGISTER_BITS 32u
#else
#if UINTPTR_MAX > 0xFFFFFFFFu
#define EMBED_REGISTER_BITS 64u
#elif UINTPTR_MAX > 0xFFFFu
#define EMBED_REGISTER_BITS 32u
#else
#define EMBED_REGISTER_BITS 16u
#endif
#endif
```

Bits in a general purpose register of this architecture.

> **Note:** ACLE gives the execution state and not the register width. On ARM the 64-bit state has 64-bit registers, and so the two are one question.

Bits in a general purpose register of this architecture.

> **Note:** The value of `__riscv_xlen`, which the C API defines as 32, 64 or 128 on each RISC-V target.

Bits in a general purpose register of this architecture.

> **Note:** Thirty-two on each Xtensa configuration. The configuration changes the depth of the register file and its options, and never its width.

Bits in a general purpose register of this architecture.

> **Note:** The width of a pointer, read from `UINTPTR_MAX`. A host is the machine that runs the suites, and this is the value that it gives.

### `EMBED_CLOCK_CORE_DEFAULT` { #EMBED_CLOCK_CORE_DEFAULT }

```c
#define EMBED_CLOCK_CORE_DEFAULT 0u
```

The core that a default pins a timer to.

> **Note:** Core zero, on each family. A part with one core has it, and a part with more cores starts at it. It is the correct core on no part in particular. The caller says which core is correct, and [`EMBED_CLOCK_CORE`](#EMBED_CLOCK_CORE) is where the caller says it.

### `EMBED_CLOCK_CALLER` { #EMBED_CLOCK_CALLER }

```c
#define EMBED_CLOCK_CALLER 1
```

The clock is the caller's, and the port reads it.

### `EMBED_CLOCK_OWN` { #EMBED_CLOCK_OWN }

```c
#define EMBED_CLOCK_OWN 0
```

The clock is a counter pinned to the core that [`EMBED_CLOCK_CORE`](#EMBED_CLOCK_CORE) names.

### `EMBED_CLOCK_HZ` { #EMBED_CLOCK_HZ }

```c
#ifndef EMBED_CLOCK_HZ
#define EMBED_CLOCK_HZ 1000000u
#endif
```

Ticks that the clock counts in one second.

> **Note:** The figure of the PLL, and each scale below starts from it. No part gives it at compile time, and so the build states it.

> **Note:** The default is one megahertz, which reads a tick as a microsecond. It is not an estimate of the frequency of the part. An estimate makes each deadline wrong by its error. One megahertz is the lowest frequency that this header accepts. A build with the default gets ticks without a scale, and a warning that says so.

### `EMBED_CLOCK_SOURCE` { #EMBED_CLOCK_SOURCE }

```c
#ifndef EMBED_CLOCK_SOURCE
#if EMBED_ARCH_CYCLE_COUNTER
#define EMBED_CLOCK_SOURCE EMBED_CLOCK_OWN
#else
#define EMBED_CLOCK_SOURCE EMBED_CLOCK_CALLER
#endif
#endif
```

Whether the clock is the caller's or a counter pinned to a core.

> **Note:** [`EMBED_CLOCK_CALLER`](#EMBED_CLOCK_CALLER) where the caller runs a counter that the port can read. That is the usual answer on a part whose application owns the timers. [`EMBED_CLOCK_OWN`](#EMBED_CLOCK_OWN) where it does not, and the port pins a counter to a core.

> **Note:** The default follows from [`EMBED_ARCH_CYCLE_COUNTER`](#EMBED_ARCH_CYCLE_COUNTER). An architecture with no counter has nothing to pin, and its default is the caller's clock.

### `EMBED_CLOCK_CORE` { #EMBED_CLOCK_CORE }

```c
#if EMBED_CLOCK_SOURCE == EMBED_CLOCK_OWN
#ifndef EMBED_CLOCK_CORE
#define EMBED_CLOCK_CORE EMBED_CLOCK_CORE_DEFAULT
#endif
#endif
```

The core that holds the pinned counter.

> **Note:** A field of [`EMBED_CLOCK_SOURCE`](#EMBED_CLOCK_SOURCE) at [`EMBED_CLOCK_OWN`](#EMBED_CLOCK_OWN). The caller selects the core. The platform gives the number its sense, and nothing here reads it past the port.

### `EMBED_CLOCK_TICKS_PER_MICRO` { #EMBED_CLOCK_TICKS_PER_MICRO }

```c
#define EMBED_CLOCK_TICKS_PER_MICRO (EMBED_CLOCK_HZ / 1000000u)
```

Ticks that the clock counts in one microsecond.

> **Note:** A constant at compile time, and so the division below becomes a multiply and a shift. No divide instruction runs on a part that has none.

### `EMBED_CLOCK_MICROS` { #EMBED_CLOCK_MICROS }

```c
#define EMBED_CLOCK_MICROS(ticks_) ((ticks_) / (embed_word)EMBED_CLOCK_TICKS_PER_MICRO)
```

A tick count of the clock as microseconds.

- `[in]`: `ticks_` Ticks read from the clock.

\*\*Returns:\*\* The ticks as whole microseconds, rounded down.

> **Note:** Rounded down. A deadline reached a fraction of a microsecond early is a window that was a little short. A deadline reached late is a stall that the build reports after it occurs.
