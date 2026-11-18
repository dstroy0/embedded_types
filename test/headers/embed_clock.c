/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_clock.c
 * @brief `embed_clock.h` alone in one translation unit, which the build compiles.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-18
 *
 * @note The unit proves that the header compiles with no header ahead of it. The SonarCloud scan reads the header
 *       through this unit.
 * @note The unit states the frequency and the caller's clock, and so the header gives no warning.
 */
#define EMBED_CLOCK_HZ 1000000u
#define EMBED_CLOCK_SOURCE EMBED_CLOCK_CALLER

#include "embed_clock.h"
