/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_dispatch_layout.h
 * @brief Compile-time assertions that a struct of function pointers holds its members at
 *        consecutive slots and nothing else.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note This catches two mistakes, and neither shows up at a call site. Adding a member to the
 *       struct and leaving it out of the list moves every member below it off its slot. Padding
 *       appearing between two members does the same with no edit to the list at all.
 * @note The preprocessor has no loop. Each member count needs its own line. The family below runs
 *       out to twenty-four for that reason.
 */
#ifndef EMBED_DISPATCH_LAYOUT_H
#define EMBED_DISPATCH_LAYOUT_H

#include <stddef.h>

#include "embed_compiler_directives.h"

/**
 * @brief Expands to sizeof(void (*)(void)).
 *
 * @note A slot index times this gives a byte offset. sizeof(void *) would be wrong here. Code and
 *       data pointers differ in width on some targets.
 */
#define EMBED_FUNCTION_POINTER_BYTES (sizeof(void (*)(void)))

/**
 * @brief Asserts one member sits at a given dispatch slot.
 *
 * @param[in] Table_  Struct type passed to offsetof.
 * @param[in] member_ Member name passed to offsetof.
 * @param[in] slot_   Index, cast to size_t and multiplied by EMBED_FUNCTION_POINTER_BYTES.
 * @note Every EMBED_TABLE_SLOTS_N line below is built from this one check.
 * @note Table_, member_, and slot_ are stringized into the assertion message. A failure names the
 *       table, the member, and the index it was expected at.
 */
#define EMBED_TABLE_SLOT(Table_, member_, slot_)                                                                       \
    EMBED_STATIC_ASSERT(offsetof(Table_, member_) == (size_t)(slot_) * EMBED_FUNCTION_POINTER_BYTES,                   \
                        #Table_ "." #member_ " is not at dispatch slot " #slot_)

/**
 * @brief Asserts one member sits at slot 0.
 *
 * @param[in] Table_   Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_ The single member's name, at slot 0.
 * @note Every longer line in the family expands down to this one. This line calls only
 *       EMBED_TABLE_SLOT.
 */
#define EMBED_TABLE_SLOTS_1(Table_, member1_) EMBED_TABLE_SLOT(Table_, member1_, 0);

/**
 * @brief Asserts two members sit at consecutive slots from 0.
 *
 * @param[in] Table_   Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_ Member at slot 0.
 * @param[in] member2_ Member at slot 1.
 * @note Every longer line in the family works the same way. It expands the line one shorter, then
 *       asserts the next index.
 */
#define EMBED_TABLE_SLOTS_2(Table_, member1_, member2_)                                                                \
    EMBED_TABLE_SLOTS_1(Table_, member1_) EMBED_TABLE_SLOT(Table_, member2_, 1);

/**
 * @brief Asserts three members sit at consecutive slots from 0.
 *
 * @param[in] Table_   Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_ Member at slot 0.
 * @param[in] member2_ Member at slot 1.
 * @param[in] member3_ Member at slot 2, asserted after EMBED_TABLE_SLOTS_2 covers the first two.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with three members.
 */
#define EMBED_TABLE_SLOTS_3(Table_, member1_, member2_, member3_)                                                      \
    EMBED_TABLE_SLOTS_2(Table_, member1_, member2_) EMBED_TABLE_SLOT(Table_, member3_, 2);

/**
 * @brief Asserts four members sit at consecutive slots from 0.
 *
 * @param[in] Table_   Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_ Member at slot 0.
 * @param[in] member2_ Member at slot 1.
 * @param[in] member3_ Member at slot 2.
 * @param[in] member4_ Member at slot 3, asserted after EMBED_TABLE_SLOTS_3 covers the first three.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with four members.
 */
#define EMBED_TABLE_SLOTS_4(Table_, member1_, member2_, member3_, member4_)                                            \
    EMBED_TABLE_SLOTS_3(Table_, member1_, member2_, member3_) EMBED_TABLE_SLOT(Table_, member4_, 3);

/**
 * @brief Asserts five members sit at consecutive slots from 0.
 *
 * @param[in] Table_   Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_ Member at slot 0.
 * @param[in] member2_ Member at slot 1.
 * @param[in] member3_ Member at slot 2.
 * @param[in] member4_ Member at slot 3.
 * @param[in] member5_ Member at slot 4, asserted after EMBED_TABLE_SLOTS_4 covers the first four.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with five members.
 */
#define EMBED_TABLE_SLOTS_5(Table_, member1_, member2_, member3_, member4_, member5_)                                  \
    EMBED_TABLE_SLOTS_4(Table_, member1_, member2_, member3_, member4_) EMBED_TABLE_SLOT(Table_, member5_, 4);

/**
 * @brief Asserts six members sit at consecutive slots from 0.
 *
 * @param[in] Table_   Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_ Member at slot 0.
 * @param[in] member2_ Member at slot 1.
 * @param[in] member3_ Member at slot 2.
 * @param[in] member4_ Member at slot 3.
 * @param[in] member5_ Member at slot 4.
 * @param[in] member6_ Member at slot 5, asserted after EMBED_TABLE_SLOTS_5 covers the first five.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with six members.
 */
#define EMBED_TABLE_SLOTS_6(Table_, member1_, member2_, member3_, member4_, member5_, member6_)                        \
    EMBED_TABLE_SLOTS_5(Table_, member1_, member2_, member3_, member4_, member5_)                                      \
    EMBED_TABLE_SLOT(Table_, member6_, 5);

/**
 * @brief Asserts seven members sit at consecutive slots from 0.
 *
 * @param[in] Table_   Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_ Member at slot 0.
 * @param[in] member2_ Member at slot 1.
 * @param[in] member3_ Member at slot 2.
 * @param[in] member4_ Member at slot 3.
 * @param[in] member5_ Member at slot 4.
 * @param[in] member6_ Member at slot 5.
 * @param[in] member7_ Member at slot 6, asserted after EMBED_TABLE_SLOTS_6 covers the first six.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with seven members.
 */
#define EMBED_TABLE_SLOTS_7(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_)              \
    EMBED_TABLE_SLOTS_6(Table_, member1_, member2_, member3_, member4_, member5_, member6_)                            \
    EMBED_TABLE_SLOT(Table_, member7_, 6);

/**
 * @brief Asserts eight members sit at consecutive slots from 0.
 *
 * @param[in] Table_   Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_ Member at slot 0.
 * @param[in] member2_ Member at slot 1.
 * @param[in] member3_ Member at slot 2.
 * @param[in] member4_ Member at slot 3.
 * @param[in] member5_ Member at slot 4.
 * @param[in] member6_ Member at slot 5.
 * @param[in] member7_ Member at slot 6.
 * @param[in] member8_ Member at slot 7, asserted after EMBED_TABLE_SLOTS_7 covers the first seven.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with eight members.
 */
#define EMBED_TABLE_SLOTS_8(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_)    \
    EMBED_TABLE_SLOTS_7(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_)                  \
    EMBED_TABLE_SLOT(Table_, member8_, 7);

/**
 * @brief Asserts nine members sit at consecutive slots from 0.
 *
 * @param[in] Table_   Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_ Member at slot 0.
 * @param[in] member2_ Member at slot 1.
 * @param[in] member3_ Member at slot 2.
 * @param[in] member4_ Member at slot 3.
 * @param[in] member5_ Member at slot 4.
 * @param[in] member6_ Member at slot 5.
 * @param[in] member7_ Member at slot 6.
 * @param[in] member8_ Member at slot 7.
 * @param[in] member9_ Member at slot 8, asserted after EMBED_TABLE_SLOTS_8 covers the first eight.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with nine members.
 */
#define EMBED_TABLE_SLOTS_9(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,    \
                            member9_)                                                                                  \
    EMBED_TABLE_SLOTS_8(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_)        \
    EMBED_TABLE_SLOT(Table_, member9_, 8);

/**
 * @brief Asserts ten members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9, asserted after EMBED_TABLE_SLOTS_9 covers the first nine.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with ten members.
 */
#define EMBED_TABLE_SLOTS_10(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_)                                                                      \
    EMBED_TABLE_SLOTS_9(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,        \
                        member9_)                                                                                      \
    EMBED_TABLE_SLOT(Table_, member10_, 9);

/**
 * @brief Asserts eleven members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10, asserted after EMBED_TABLE_SLOTS_10 covers the first ten.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with eleven members.
 */
#define EMBED_TABLE_SLOTS_11(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_)                                                           \
    EMBED_TABLE_SLOTS_10(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_)                                                                          \
    EMBED_TABLE_SLOT(Table_, member11_, 10);

/**
 * @brief Asserts twelve members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10.
 * @param[in] member12_ Member at slot 11, asserted after EMBED_TABLE_SLOTS_11 covers the first eleven.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with twelve members.
 */
#define EMBED_TABLE_SLOTS_12(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_, member12_)                                                \
    EMBED_TABLE_SLOTS_11(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_, member11_)                                                               \
    EMBED_TABLE_SLOT(Table_, member12_, 11);

/**
 * @brief Asserts thirteen members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10.
 * @param[in] member12_ Member at slot 11.
 * @param[in] member13_ Member at slot 12, asserted after EMBED_TABLE_SLOTS_12 covers the first twelve.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with thirteen members.
 */
#define EMBED_TABLE_SLOTS_13(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_, member12_, member13_)                                     \
    EMBED_TABLE_SLOTS_12(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_, member11_, member12_)                                                    \
    EMBED_TABLE_SLOT(Table_, member13_, 12);

/**
 * @brief Asserts fourteen members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10.
 * @param[in] member12_ Member at slot 11.
 * @param[in] member13_ Member at slot 12.
 * @param[in] member14_ Member at slot 13, asserted after EMBED_TABLE_SLOTS_13 covers the first thirteen.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with fourteen members.
 */
#define EMBED_TABLE_SLOTS_14(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_, member12_, member13_, member14_)                          \
    EMBED_TABLE_SLOTS_13(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_, member11_, member12_, member13_)                                         \
    EMBED_TABLE_SLOT(Table_, member14_, 13);

/**
 * @brief Asserts fifteen members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10.
 * @param[in] member12_ Member at slot 11.
 * @param[in] member13_ Member at slot 12.
 * @param[in] member14_ Member at slot 13.
 * @param[in] member15_ Member at slot 14, asserted after EMBED_TABLE_SLOTS_14 covers the first fourteen.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with fifteen members.
 */
#define EMBED_TABLE_SLOTS_15(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_)               \
    EMBED_TABLE_SLOTS_14(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_, member11_, member12_, member13_, member14_)                              \
    EMBED_TABLE_SLOT(Table_, member15_, 14);

/**
 * @brief Asserts sixteen members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10.
 * @param[in] member12_ Member at slot 11.
 * @param[in] member13_ Member at slot 12.
 * @param[in] member14_ Member at slot 13.
 * @param[in] member15_ Member at slot 14.
 * @param[in] member16_ Member at slot 15, asserted after EMBED_TABLE_SLOTS_15 covers the first fifteen.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with sixteen members.
 */
#define EMBED_TABLE_SLOTS_16(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_)    \
    EMBED_TABLE_SLOTS_15(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_)                   \
    EMBED_TABLE_SLOT(Table_, member16_, 15);

/**
 * @brief Asserts seventeen members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10.
 * @param[in] member12_ Member at slot 11.
 * @param[in] member13_ Member at slot 12.
 * @param[in] member14_ Member at slot 13.
 * @param[in] member15_ Member at slot 14.
 * @param[in] member16_ Member at slot 15.
 * @param[in] member17_ Member at slot 16, asserted after EMBED_TABLE_SLOTS_16 covers the first sixteen.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with seventeen members.
 */
#define EMBED_TABLE_SLOTS_17(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,    \
                             member17_)                                                                                \
    EMBED_TABLE_SLOTS_16(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_)        \
    EMBED_TABLE_SLOT(Table_, member17_, 16);

/**
 * @brief Asserts eighteen members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10.
 * @param[in] member12_ Member at slot 11.
 * @param[in] member13_ Member at slot 12.
 * @param[in] member14_ Member at slot 13.
 * @param[in] member15_ Member at slot 14.
 * @param[in] member16_ Member at slot 15.
 * @param[in] member17_ Member at slot 16.
 * @param[in] member18_ Member at slot 17, asserted after EMBED_TABLE_SLOTS_17 covers the first seventeen.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with eighteen members.
 */
#define EMBED_TABLE_SLOTS_18(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,    \
                             member17_, member18_)                                                                     \
    EMBED_TABLE_SLOTS_17(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,        \
                         member17_)                                                                                    \
    EMBED_TABLE_SLOT(Table_, member18_, 17);

/**
 * @brief Asserts nineteen members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10.
 * @param[in] member12_ Member at slot 11.
 * @param[in] member13_ Member at slot 12.
 * @param[in] member14_ Member at slot 13.
 * @param[in] member15_ Member at slot 14.
 * @param[in] member16_ Member at slot 15.
 * @param[in] member17_ Member at slot 16.
 * @param[in] member18_ Member at slot 17.
 * @param[in] member19_ Member at slot 18, asserted after EMBED_TABLE_SLOTS_18 covers the first eighteen.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with nineteen members.
 */
#define EMBED_TABLE_SLOTS_19(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,    \
                             member17_, member18_, member19_)                                                          \
    EMBED_TABLE_SLOTS_18(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,        \
                         member17_, member18_)                                                                         \
    EMBED_TABLE_SLOT(Table_, member19_, 18);

/**
 * @brief Asserts twenty members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10.
 * @param[in] member12_ Member at slot 11.
 * @param[in] member13_ Member at slot 12.
 * @param[in] member14_ Member at slot 13.
 * @param[in] member15_ Member at slot 14.
 * @param[in] member16_ Member at slot 15.
 * @param[in] member17_ Member at slot 16.
 * @param[in] member18_ Member at slot 17.
 * @param[in] member19_ Member at slot 18.
 * @param[in] member20_ Member at slot 19, asserted after EMBED_TABLE_SLOTS_19 covers the first nineteen.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with twenty members.
 */
#define EMBED_TABLE_SLOTS_20(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,    \
                             member17_, member18_, member19_, member20_)                                               \
    EMBED_TABLE_SLOTS_19(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,        \
                         member17_, member18_, member19_)                                                              \
    EMBED_TABLE_SLOT(Table_, member20_, 19);

/**
 * @brief Asserts twenty-one members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10.
 * @param[in] member12_ Member at slot 11.
 * @param[in] member13_ Member at slot 12.
 * @param[in] member14_ Member at slot 13.
 * @param[in] member15_ Member at slot 14.
 * @param[in] member16_ Member at slot 15.
 * @param[in] member17_ Member at slot 16.
 * @param[in] member18_ Member at slot 17.
 * @param[in] member19_ Member at slot 18.
 * @param[in] member20_ Member at slot 19.
 * @param[in] member21_ Member at slot 20, asserted after EMBED_TABLE_SLOTS_20 covers the first twenty.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with twenty-one members.
 */
#define EMBED_TABLE_SLOTS_21(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,    \
                             member17_, member18_, member19_, member20_, member21_)                                    \
    EMBED_TABLE_SLOTS_20(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,        \
                         member17_, member18_, member19_, member20_)                                                   \
    EMBED_TABLE_SLOT(Table_, member21_, 20);

/**
 * @brief Asserts twenty-two members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10.
 * @param[in] member12_ Member at slot 11.
 * @param[in] member13_ Member at slot 12.
 * @param[in] member14_ Member at slot 13.
 * @param[in] member15_ Member at slot 14.
 * @param[in] member16_ Member at slot 15.
 * @param[in] member17_ Member at slot 16.
 * @param[in] member18_ Member at slot 17.
 * @param[in] member19_ Member at slot 18.
 * @param[in] member20_ Member at slot 19.
 * @param[in] member21_ Member at slot 20.
 * @param[in] member22_ Member at slot 21, asserted after EMBED_TABLE_SLOTS_21 covers the first twenty-one.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with twenty-two members.
 */
#define EMBED_TABLE_SLOTS_22(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,    \
                             member17_, member18_, member19_, member20_, member21_, member22_)                         \
    EMBED_TABLE_SLOTS_21(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,        \
                         member17_, member18_, member19_, member20_, member21_)                                        \
    EMBED_TABLE_SLOT(Table_, member22_, 21);

/**
 * @brief Asserts twenty-three members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10.
 * @param[in] member12_ Member at slot 11.
 * @param[in] member13_ Member at slot 12.
 * @param[in] member14_ Member at slot 13.
 * @param[in] member15_ Member at slot 14.
 * @param[in] member16_ Member at slot 15.
 * @param[in] member17_ Member at slot 16.
 * @param[in] member18_ Member at slot 17.
 * @param[in] member19_ Member at slot 18.
 * @param[in] member20_ Member at slot 19.
 * @param[in] member21_ Member at slot 20.
 * @param[in] member22_ Member at slot 21.
 * @param[in] member23_ Member at slot 22, asserted after EMBED_TABLE_SLOTS_22 covers the first twenty-two.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with twenty-three members.
 */
#define EMBED_TABLE_SLOTS_23(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,    \
                             member17_, member18_, member19_, member20_, member21_, member22_, member23_)              \
    EMBED_TABLE_SLOTS_22(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,        \
                         member17_, member18_, member19_, member20_, member21_, member22_)                             \
    EMBED_TABLE_SLOT(Table_, member23_, 22);

/**
 * @brief Asserts twenty-four members sit at consecutive slots from 0.
 *
 * @param[in] Table_    Struct type forwarded to EMBED_TABLE_SLOT.
 * @param[in] member1_  Member at slot 0.
 * @param[in] member2_  Member at slot 1.
 * @param[in] member3_  Member at slot 2.
 * @param[in] member4_  Member at slot 3.
 * @param[in] member5_  Member at slot 4.
 * @param[in] member6_  Member at slot 5.
 * @param[in] member7_  Member at slot 6.
 * @param[in] member8_  Member at slot 7.
 * @param[in] member9_  Member at slot 8.
 * @param[in] member10_ Member at slot 9.
 * @param[in] member11_ Member at slot 10.
 * @param[in] member12_ Member at slot 11.
 * @param[in] member13_ Member at slot 12.
 * @param[in] member14_ Member at slot 13.
 * @param[in] member15_ Member at slot 14.
 * @param[in] member16_ Member at slot 15.
 * @param[in] member17_ Member at slot 16.
 * @param[in] member18_ Member at slot 17.
 * @param[in] member19_ Member at slot 18.
 * @param[in] member20_ Member at slot 19.
 * @param[in] member21_ Member at slot 20.
 * @param[in] member22_ Member at slot 21.
 * @param[in] member23_ Member at slot 22.
 * @param[in] member24_ Member at slot 23, asserted after EMBED_TABLE_SLOTS_23 covers the first twenty-three.
 * @note EMBED_TABLE_LAYOUT selects this line for a table with twenty-four members.
 * @warning This is the longest line in the family. A table with twenty-five members has no line to
 *          expand to. EMBED_NARG stops at twenty-four as well. Raising the limit takes a new line
 *          here, one more constant in EMBED_NARG, and one more parameter in EMBED_ARG_N.
 */
#define EMBED_TABLE_SLOTS_24(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,   \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,    \
                             member17_, member18_, member19_, member20_, member21_, member22_, member23_, member24_)   \
    EMBED_TABLE_SLOTS_23(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_,       \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_,        \
                         member17_, member18_, member19_, member20_, member21_, member22_, member23_)                  \
    EMBED_TABLE_SLOT(Table_, member24_, 23);

/**
 * @brief Asserts a table's members sit at consecutive slots and that nothing else is in it.
 *
 * @param[in] Table_ Struct type forwarded to the arity line and to sizeof.
 * @param[in] ...    Member names in slot order, one to twenty-four.
 * @note The per-member assertions catch a member left out of the list. The size assertion catches
 *       padding, since a padded struct is larger than the count times EMBED_FUNCTION_POINTER_BYTES.
 * @note EMBED_CAT builds the arity line's name from EMBED_NARG's count of the member list. The
 *       caller writes each member once.
 * @warning A member of some other type changes sizeof(Table_) and fails the size assertion.
 */
#define EMBED_TABLE_LAYOUT(Table_, ...)                                                                                \
    EMBED_CAT(EMBED_TABLE_SLOTS_, EMBED_NARG(__VA_ARGS__))(Table_, __VA_ARGS__)                                        \
        EMBED_STATIC_ASSERT(sizeof(Table_) == (size_t)EMBED_NARG(__VA_ARGS__) * EMBED_FUNCTION_POINTER_BYTES,          \
                            #Table_ " has a member that is not in its dispatch list, or is padded")

/**
 * @brief Expands to static const.
 *
 * @note Declares a dispatch table. static gives internal linkage. Each translation unit including
 *       the header gets its own copy. The copies do not collide at link time.
 * @note const lets the compiler resolve a call through a table entry to the function directly. That
 *       needs a table no other translation unit can reach and no code assigns to.
 * @note This macro does not supply EMBED_UNUSED. Write it on the table as well. A translation unit
 *       that calls nothing through the table warns without it.
 */
#define EMBED_TABLE_STORAGE static const

#endif
