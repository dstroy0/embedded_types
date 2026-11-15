# embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

# One translation unit for each header, with that header alone. The build proves that each header compiles with no
# header ahead of it. The file sonar-project.properties names these units as the sources. The scan reads the headers
# through them.
add_library(embedded_types_header_units OBJECT
  embed_atomic.c
  embed_compiler_directives.c
  embed_dispatch_layout.c
  embed_lane.c
  embed_ticket_ring.c
  embed_turnstile.c
  embed_types.c)
target_compile_options(embedded_types_header_units PRIVATE ${EMBEDDED_TYPES_TEST_WARNINGS})
target_link_libraries(embedded_types_header_units PRIVATE embedded_types::embedded_types)
