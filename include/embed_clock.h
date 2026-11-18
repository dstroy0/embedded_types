/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_clock.h
 * @brief The clock selection: the architecture, its counter, the clock that the build reads, and the scale
 *        of the ticks of that clock to a microsecond.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-18
 *
 * @note Nothing below names a part, a vendor or a board. A list of parts covers the parts that its writer
 *       knew, and it misses each part released after. The test here is the architecture level, which
 *       each member of a family defines. An ATSAMD21 answers the ARMv6-M question as each Cortex-M0+ does.
 * @note The three families are ARM, RISC-V and Xtensa, through GCC or clang. A build that is none of them
 *       is a host build, and the suites run there.
 * @note The clock is the caller's, or a counter pinned to a core that the caller names. A counter that
 *       runs and that a port can read is the full requirement. This header scales its ticks against the
 *       frequency that the caller states.
 * @note Sources for the macros below:
 *       Arm C Language Extensions, https://arm-software.github.io/acle/main/acle.html.
 *       RISC-V C API specification, https://github.com/riscv-non-isa/riscv-c-api-doc.
 *       GCC, gcc/config/xtensa/xtensa.h, TARGET_CPU_CPP_BUILTINS.
 * @warning Each of EMBED_CLOCK_HZ, EMBED_CLOCK_SOURCE and EMBED_CLOCK_CORE that the build does not set
 *          gives a #warning that names the value this header took. A build that states all three gets
 *          none.
 */
#ifndef EMBED_CLOCK_H
#define EMBED_CLOCK_H

#include "embed_compiler_directives.h"
#include "embed_types.h"

/**
 * @brief Set where this is an ARM build, in either execution state.
 *
 * @note __arm__ covers each 32-bit ARM from ARMv4, and __aarch64__ covers the 64-bit state. ACLE keeps
 *       both, and so a test of the pair catches each ARM subfamily and names none.
 */
#if defined(__arm__) || defined(__aarch64__) || defined(__thumb__)
#define EMBED_ARCH_ARM 1
#else
#define EMBED_ARCH_ARM 0
#endif

/**
 * @brief Set where this is a RISC-V build, at any register width.
 *
 * @note The RISC-V C API defines __riscv as 1 on each RISC-V target. It has no second name and no
 *       variant for a vendor.
 */
#if defined(__riscv)
#define EMBED_ARCH_RISCV 1
#else
#define EMBED_ARCH_RISCV 0
#endif

/**
 * @brief Set where this is an Xtensa build.
 *
 * @note GCC defines both names on each Xtensa build. A clang build for Xtensa is a different front end,
 *       and so the test reads both names.
 */
#if defined(__XTENSA__) || defined(__xtensa__)
#define EMBED_ARCH_XTENSA 1
#else
#define EMBED_ARCH_XTENSA 0
#endif

/**
 * @brief Set where this is none of the three families, which is a host build.
 */
#if !EMBED_ARCH_ARM && !EMBED_ARCH_RISCV && !EMBED_ARCH_XTENSA
#define EMBED_ARCH_HOST 1
#else
#define EMBED_ARCH_HOST 0
#endif

EMBED_STATIC_ASSERT((EMBED_ARCH_ARM + EMBED_ARCH_RISCV + EMBED_ARCH_XTENSA + EMBED_ARCH_HOST) == 1,
                    "the architecture test selected more than one family, or none");

#if EMBED_ARCH_ARM

// The ARM subfamilies come from the architecture level and the profile, and no part has a name here.
// __ARM_ARCH is the level as an integer. __ARM_ARCH_PROFILE is the profile as a character constant,
// 'A', 'R' or 'M'. A core older than Cortex defines no profile and no level above 6. An undefined
// __ARM_ARCH_PROFILE has the value 0, which is none of the three
#if defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'M')
#if __ARM_ARCH >= 8

/**
 * @brief The name of this architecture in a diagnostic.
 *
 * @note ARMv8-M and ARMv8.1-M, which are Cortex-M23 through M85. The cycle counter test below makes the
 *       split between baseline and mainline that matters.
 */
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

/**
 * @brief Set where the architecture defines a counter that a port can read.
 *
 * @note ARMv7-M and later have the DWT, and a port reads its cycle counter. ARMv6-M has no DWT. A Cortex-M0
 *       or M0+ has no counter, and the caller supplies the clock.
 * @note The application and real-time profiles have the generic timer and the performance monitors. A
 *       port can read either of them.
 * @warning Says that the architecture defines a counter, and never that the part has it. DWT_CYCCNT is
 *          optional where the DWT is present. The port finds out whether the counter is there. This
 *          only selects the default.
 */
#if defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'M')
#if __ARM_ARCH >= 7
#define EMBED_ARCH_CYCLE_COUNTER 1
#else
#define EMBED_ARCH_CYCLE_COUNTER 0
#endif
#else
#define EMBED_ARCH_CYCLE_COUNTER 1
#endif

/**
 * @brief Bits in a general purpose register of this architecture.
 *
 * @note ACLE gives the execution state and not the register width. On ARM the 64-bit state has 64-bit
 *       registers, and so the two are one question.
 */
#if defined(__ARM_64BIT_STATE)
#define EMBED_REGISTER_BITS 64u
#else
#define EMBED_REGISTER_BITS 32u
#endif

#elif EMBED_ARCH_RISCV

// The RISC-V subfamilies come from the register width and the register file. __riscv_xlen is 32, 64 or
// 128. The embedded profiles have sixteen registers, and __riscv_32e, __riscv_64e or __riscv_abi_rve
// give them where the ABI is the embedded one
#if defined(__riscv_32e) || defined(__riscv_64e) || defined(__riscv_abi_rve)

/**
 * @brief The name of this architecture in a diagnostic.
 */
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

/**
 * @brief Set where the architecture defines a counter that a port can read.
 *
 * @note The counter is the Zicntr CSR, which each profile has. A port reads it with rdcycle, or from
 *       mcycle in machine mode.
 * @warning Says that the architecture defines a counter, and never that this build can read it. The
 *          mcounteren register controls a read of cycle from a lower privilege level, and a platform can
 *          keep it closed.
 */
#define EMBED_ARCH_CYCLE_COUNTER 1

/**
 * @brief Bits in a general purpose register of this architecture.
 *
 * @note The value of __riscv_xlen, which the C API defines as 32, 64 or 128 on each RISC-V target.
 */
#define EMBED_REGISTER_BITS __riscv_xlen

#elif EMBED_ARCH_XTENSA

// The Xtensa subfamilies come from the call convention. GCC defines one of __XTENSA_WINDOWED_ABI__ and
// __XTENSA_CALL0_ABI__ for each Xtensa configuration. The register windows change each access to the
// stack, and so that split has a name
#if defined(__XTENSA_WINDOWED_ABI__)

/**
 * @brief The name of this architecture in a diagnostic.
 */
#define EMBED_ARCH_NAME "Xtensa, windowed ABI"
#elif defined(__XTENSA_CALL0_ABI__)
#define EMBED_ARCH_NAME "Xtensa, call0 ABI"
#else
#define EMBED_ARCH_NAME "Xtensa"
#endif

/**
 * @brief Set where the architecture defines a counter that a port can read.
 *
 * @note CCOUNT, the special register of the cycle count. A port reads it with rsr.
 * @warning Says that the architecture defines a counter, and never that this core has it. An Xtensa core
 *          is configurable and the timer option can be absent. The port finds out.
 */
#define EMBED_ARCH_CYCLE_COUNTER 1

/**
 * @brief Bits in a general purpose register of this architecture.
 *
 * @note Thirty-two on each Xtensa configuration. The configuration changes the depth of the register file
 *       and its options, and never its width.
 */
#define EMBED_REGISTER_BITS 32u

#else

/**
 * @brief The name of this architecture in a diagnostic.
 *
 * @note A host build. The suites run here, and the caller reaches the host's clock through its own
 *       library. This header has no core to pin there.
 */
#define EMBED_ARCH_NAME "this host"

/**
 * @brief Set where the architecture defines a counter that a port can read.
 *
 * @note Zero, and not because a host has no counter. A host build has no core to pin a timer to and no
 *       architectural register to read. So the clock is the caller's, and a host build takes the arm that a
 *       Cortex-M0 takes, for the same reason.
 */
#define EMBED_ARCH_CYCLE_COUNTER 0

/**
 * @brief Bits in a general purpose register of this architecture.
 *
 * @note The width of a pointer, read from UINTPTR_MAX. A host is the machine that runs the suites, and
 *       this is the value that it gives.
 */
#if UINTPTR_MAX > 0xFFFFFFFFu
#define EMBED_REGISTER_BITS 64u
#elif UINTPTR_MAX > 0xFFFFu
#define EMBED_REGISTER_BITS 32u
#else
#define EMBED_REGISTER_BITS 16u
#endif

#endif

// A word wider than the register that carries it is wrong. A build at 16 or 32 bits on a 64-bit host is
// correct, and so a narrower word passes
EMBED_STATIC_ASSERT(EMBED_WORD_BITS <= EMBED_REGISTER_BITS, "EMBED_WORD_BITS is wider than this architecture's register");

/**
 * @brief The core that a default pins a timer to.
 *
 * @note Core zero, on each family. A part with one core has it, and a part with more cores starts at it.
 *       It is the correct core on no part in particular. The caller says which core is correct, and
 *       EMBED_CLOCK_CORE is where the caller says it.
 */
#define EMBED_CLOCK_CORE_DEFAULT 0u

/**
 * @brief The clock is the caller's, and the port reads it.
 */
#define EMBED_CLOCK_CALLER 1

/**
 * @brief The clock is a counter pinned to the core that EMBED_CLOCK_CORE names.
 */
#define EMBED_CLOCK_OWN 0

/**
 * @brief Ticks that the clock counts in one second.
 *
 * @note The figure of the PLL, and each scale below starts from it. No part gives it at compile time, and
 *       so the build states it.
 * @note The default is one megahertz, which reads a tick as a microsecond. It is not an estimate of the
 *       frequency of the part. An estimate makes each deadline wrong by its error. One megahertz is the
 *       lowest frequency that this header accepts. A build with the default gets ticks without a scale,
 *       and a warning that says so.
 */
#ifndef EMBED_CLOCK_HZ
#define EMBED_CLOCK_HZ 1000000u
#define EMBED_UNSET_CLOCK_HZ 1
EMBED_BUILD_WARNING(
    "No clock is declared, so EMBED_CLOCK_HZ took the default of 1000000 and a tick is read as one microsecond. Each deadline is then wrong by the real frequency. Set it to the frequency of the clock this reads.")
#endif

/**
 * @brief Whether the clock is the caller's or a counter pinned to a core.
 *
 * @note EMBED_CLOCK_CALLER where the caller runs a counter that the port can read. That is the usual
 *       answer on a part whose application owns the timers. EMBED_CLOCK_OWN where it does not, and the port
 *       pins a counter to a core.
 * @note The default follows from EMBED_ARCH_CYCLE_COUNTER. An architecture with no counter has nothing to
 *       pin, and its default is the caller's clock.
 */
#ifndef EMBED_CLOCK_SOURCE
#define EMBED_UNSET_CLOCK_SOURCE 1
#if EMBED_ARCH_CYCLE_COUNTER
#define EMBED_CLOCK_SOURCE EMBED_CLOCK_OWN
EMBED_BUILD_WARNING(
    "EMBED_CLOCK_SOURCE was not set and took EMBED_CLOCK_OWN, because this architecture defines a cycle counter that a port can read. Say so, or set EMBED_CLOCK_CALLER to read a clock that you run.")
#else
#define EMBED_CLOCK_SOURCE EMBED_CLOCK_CALLER
EMBED_BUILD_WARNING(
    "EMBED_CLOCK_SOURCE was not set and took EMBED_CLOCK_CALLER, because this architecture defines no cycle counter to pin a timer to. Say so, and supply the clock.")
#endif
#endif

// A source is one of the two named arms. A third value is a frequency, a core number or a flag in the
// wrong place, and each test of it below takes the caller's arm without a word
#if (EMBED_CLOCK_SOURCE != EMBED_CLOCK_CALLER) && (EMBED_CLOCK_SOURCE != EMBED_CLOCK_OWN)
#error "EMBED_CLOCK_SOURCE must be EMBED_CLOCK_CALLER or EMBED_CLOCK_OWN."
#endif

// A counter pinned on an architecture that defines no counter is a timer that never ticks. The build
// stops here, and the caller supplies the clock
#if (EMBED_CLOCK_SOURCE == EMBED_CLOCK_OWN) && !EMBED_ARCH_CYCLE_COUNTER
#error                                                                                                                 \
    "EMBED_CLOCK_SOURCE is EMBED_CLOCK_OWN, but this architecture defines no cycle counter to pin a timer to. Set EMBED_CLOCK_SOURCE to EMBED_CLOCK_CALLER and supply the clock. An ARMv6-M part and a host build both stop here."
#endif

#if EMBED_CLOCK_SOURCE == EMBED_CLOCK_OWN

/**
 * @brief The core that holds the pinned counter.
 *
 * @note A field of EMBED_CLOCK_SOURCE at EMBED_CLOCK_OWN. The caller selects the core. The platform gives
 *       the number its sense, and nothing here reads it past the port.
 */
#ifndef EMBED_CLOCK_CORE
#define EMBED_CLOCK_CORE EMBED_CLOCK_CORE_DEFAULT
#define EMBED_UNSET_CLOCK_CORE 1
EMBED_BUILD_WARNING(
    "EMBED_CLOCK_CORE was not set and took EMBED_CLOCK_CORE_DEFAULT, which is core zero on each family. Set it to the core to pin the timer to.")
#endif

#else

// A field of the arm that this build did not take. Nothing pins a counter in this build, and a set core
// would look like a setting that has an effect
#ifdef EMBED_CLOCK_CORE
#error                                                                                                                 \
    "EMBED_CLOCK_CORE is set but EMBED_CLOCK_SOURCE is EMBED_CLOCK_CALLER. Nothing pins a timer in this build, so no core takes it. Set EMBED_CLOCK_SOURCE to EMBED_CLOCK_OWN, or remove EMBED_CLOCK_CORE."
#endif

#endif

/**
 * @brief Ticks that the clock counts in one microsecond.
 *
 * @note A constant at compile time, and so the division below becomes a multiply and a shift. No divide
 *       instruction runs on a part that has none.
 */
#define EMBED_CLOCK_TICKS_PER_MICRO (EMBED_CLOCK_HZ / 1000000u)

/**
 * @brief A tick count of the clock as microseconds.
 *
 * @param[in] ticks_ Ticks read from the clock.
 * @return           The ticks as whole microseconds, rounded down.
 * @note Rounded down. A deadline reached a fraction of a microsecond early is a window that was a little
 *       short. A deadline reached late is a stall that the build reports after it occurs.
 */
#define EMBED_CLOCK_MICROS(ticks_) ((ticks_) / (embed_word)EMBED_CLOCK_TICKS_PER_MICRO)

// A tick divides a microsecond with no remainder. Otherwise the conversion above loses a different
// amount at each frequency, and nothing says how much. Each part of the three families runs at a whole
// number of megahertz
EMBED_STATIC_ASSERT((EMBED_CLOCK_HZ % 1000000u) == 0u,
                    "EMBED_CLOCK_HZ is not a whole number of megahertz, and the clock scales in whole ticks");

EMBED_STATIC_ASSERT(EMBED_CLOCK_TICKS_PER_MICRO >= 1u,
                    "EMBED_CLOCK_HZ is below one megahertz, which cannot resolve a deadline in microseconds");

#endif
