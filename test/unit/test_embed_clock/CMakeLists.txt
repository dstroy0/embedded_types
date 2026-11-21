# embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

embedded_types_add_suite(test_embed_clock)

# The generated runner includes embed_clock.h too. Each unit of the suite gets the two values here, so no unit takes
# a default or gives a warning.
target_compile_definitions(test_embed_clock PRIVATE EMBED_CLOCK_HZ=48000000u EMBED_CLOCK_SOURCE=EMBED_CLOCK_CALLER)
