/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_compiler_directives.c
 * @brief `embed_compiler_directives.h` alone in one translation unit, which the build compiles.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-14
 *
 * @note The unit proves that the header compiles with no header ahead of it. The SonarCloud scan reads the header
 *       through this unit. The header declares nothing, so the unit adds one assertion that uses it.
 */
#include "embed_compiler_directives.h"

EMBED_STATIC_ASSERT((EMBED_BIG_ENDIAN == 0) || (EMBED_BIG_ENDIAN == 1), "EMBED_BIG_ENDIAN is 0 or 1 on each target");
