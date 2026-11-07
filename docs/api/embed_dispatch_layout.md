<!-- tools/api_pages.py writes this page from include/embed_dispatch_layout.h. Edit the header, then run the tool. -->

# embed_dispatch_layout.h

Source: [`include/embed_dispatch_layout.h`](https://github.com/dstroy0/embedded_types/blob/main/include/embed_dispatch_layout.h)

Compile-time assertions that a struct of function pointers holds its members at consecutive slots, with no padding between them and no member of another type.

> **Note:** This header finds two mistakes, and a call site shows neither. A member added to the struct but not to the list moves each member below it off its slot. Padding between two members does the same, with no edit to the list.

> **Note:** The preprocessor has no loop. Each count of members needs its own line. For that reason, the family below goes up to twenty-four.

## Macros

### `EMBED_FUNCTION_POINTER_BYTES` { #EMBED_FUNCTION_POINTER_BYTES }

```c
#define EMBED_FUNCTION_POINTER_BYTES (sizeof(void (*)(void)))
```

Expands to sizeof(void (\*)(void)).

> **Note:** A slot index times this gives a byte offset. sizeof(void \*) would be wrong here. Code and data pointers differ in width on some targets.

### `EMBED_TABLE_SLOT` { #EMBED_TABLE_SLOT }

```c
#define EMBED_TABLE_SLOT(Table_, member_, slot_) \
    EMBED_STATIC_ASSERT(offsetof(Table_, member_) == (size_t)(slot_) * EMBED_FUNCTION_POINTER_BYTES, \
                        #Table_ "." #member_ " is not at dispatch slot " #slot_)
```

Asserts one member sits at a given dispatch slot.

- `[in]`: `Table_` Struct type passed to offsetof.
- `[in]`: `member_` Member name passed to offsetof.
- `[in]`: `slot_` Index, cast to `size_t` and multiplied by [`EMBED_FUNCTION_POINTER_BYTES`](#EMBED_FUNCTION_POINTER_BYTES).

> **Note:** Each `EMBED_TABLE_SLOTS_N` line below expands to this one check.

> **Note:** The assertion message holds `Table_`, `member_`, and `slot_` as strings. A failure names the table, the member, and the index where the member should be.

### `EMBED_TABLE_SLOTS_1` { #EMBED_TABLE_SLOTS_1 }

```c
#define EMBED_TABLE_SLOTS_1(Table_, member1_) EMBED_TABLE_SLOT(Table_, member1_, 0);
```

Asserts one member sits at slot 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` The single member's name, at slot 0.

> **Note:** Every longer line in the family expands down to this one. This line calls only [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).

### `EMBED_TABLE_SLOTS_2` { #EMBED_TABLE_SLOTS_2 }

```c
#define EMBED_TABLE_SLOTS_2(Table_, member1_, member2_) \
    EMBED_TABLE_SLOTS_1(Table_, member1_) EMBED_TABLE_SLOT(Table_, member2_, 1);
```

Asserts two members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.

> **Note:** Every longer line in the family works the same way. It expands the line one shorter, then asserts the next index.

### `EMBED_TABLE_SLOTS_3` { #EMBED_TABLE_SLOTS_3 }

```c
#define EMBED_TABLE_SLOTS_3(Table_, member1_, member2_, member3_) \
    EMBED_TABLE_SLOTS_2(Table_, member1_, member2_) EMBED_TABLE_SLOT(Table_, member3_, 2);
```

Asserts three members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2, asserted after [`EMBED_TABLE_SLOTS_2`](#EMBED_TABLE_SLOTS_2) covers the first two.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with three members.

### `EMBED_TABLE_SLOTS_4` { #EMBED_TABLE_SLOTS_4 }

```c
#define EMBED_TABLE_SLOTS_4(Table_, member1_, member2_, member3_, member4_) \
    EMBED_TABLE_SLOTS_3(Table_, member1_, member2_, member3_) EMBED_TABLE_SLOT(Table_, member4_, 3);
```

Asserts four members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3, asserted after [`EMBED_TABLE_SLOTS_3`](#EMBED_TABLE_SLOTS_3) covers the first three.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with four members.

### `EMBED_TABLE_SLOTS_5` { #EMBED_TABLE_SLOTS_5 }

```c
#define EMBED_TABLE_SLOTS_5(Table_, member1_, member2_, member3_, member4_, member5_) \
    EMBED_TABLE_SLOTS_4(Table_, member1_, member2_, member3_, member4_) EMBED_TABLE_SLOT(Table_, member5_, 4);
```

Asserts five members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4, asserted after [`EMBED_TABLE_SLOTS_4`](#EMBED_TABLE_SLOTS_4) covers the first four.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with five members.

### `EMBED_TABLE_SLOTS_6` { #EMBED_TABLE_SLOTS_6 }

```c
#define EMBED_TABLE_SLOTS_6(Table_, member1_, member2_, member3_, member4_, member5_, member6_) \
    EMBED_TABLE_SLOTS_5(Table_, member1_, member2_, member3_, member4_, member5_) \
    EMBED_TABLE_SLOT(Table_, member6_, 5);
```

Asserts six members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5, asserted after [`EMBED_TABLE_SLOTS_5`](#EMBED_TABLE_SLOTS_5) covers the first five.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with six members.

### `EMBED_TABLE_SLOTS_7` { #EMBED_TABLE_SLOTS_7 }

```c
#define EMBED_TABLE_SLOTS_7(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_) \
    EMBED_TABLE_SLOTS_6(Table_, member1_, member2_, member3_, member4_, member5_, member6_) \
    EMBED_TABLE_SLOT(Table_, member7_, 6);
```

Asserts seven members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6, asserted after [`EMBED_TABLE_SLOTS_6`](#EMBED_TABLE_SLOTS_6) covers the first six.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with seven members.

### `EMBED_TABLE_SLOTS_8` { #EMBED_TABLE_SLOTS_8 }

```c
#define EMBED_TABLE_SLOTS_8(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_) \
    EMBED_TABLE_SLOTS_7(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_) \
    EMBED_TABLE_SLOT(Table_, member8_, 7);
```

Asserts eight members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7, asserted after [`EMBED_TABLE_SLOTS_7`](#EMBED_TABLE_SLOTS_7) covers the first seven.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with eight members.

### `EMBED_TABLE_SLOTS_9` { #EMBED_TABLE_SLOTS_9 }

```c
#define EMBED_TABLE_SLOTS_9(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                            member9_) \
    EMBED_TABLE_SLOTS_8(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_) \
    EMBED_TABLE_SLOT(Table_, member9_, 8);
```

Asserts nine members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8, asserted after [`EMBED_TABLE_SLOTS_8`](#EMBED_TABLE_SLOTS_8) covers the first eight.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with nine members.

### `EMBED_TABLE_SLOTS_10` { #EMBED_TABLE_SLOTS_10 }

```c
#define EMBED_TABLE_SLOTS_10(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_) \
    EMBED_TABLE_SLOTS_9(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                        member9_) \
    EMBED_TABLE_SLOT(Table_, member10_, 9);
```

Asserts ten members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9, asserted after [`EMBED_TABLE_SLOTS_9`](#EMBED_TABLE_SLOTS_9) covers the first nine.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with ten members.

### `EMBED_TABLE_SLOTS_11` { #EMBED_TABLE_SLOTS_11 }

```c
#define EMBED_TABLE_SLOTS_11(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_) \
    EMBED_TABLE_SLOTS_10(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_) \
    EMBED_TABLE_SLOT(Table_, member11_, 10);
```

Asserts eleven members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10, asserted after [`EMBED_TABLE_SLOTS_10`](#EMBED_TABLE_SLOTS_10) covers the first ten.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with eleven members.

### `EMBED_TABLE_SLOTS_12` { #EMBED_TABLE_SLOTS_12 }

```c
#define EMBED_TABLE_SLOTS_12(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_, member12_) \
    EMBED_TABLE_SLOTS_11(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_, member11_) \
    EMBED_TABLE_SLOT(Table_, member12_, 11);
```

Asserts twelve members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10.
- `[in]`: `member12_` Member at slot 11, asserted after [`EMBED_TABLE_SLOTS_11`](#EMBED_TABLE_SLOTS_11) covers the first eleven.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with twelve members.

### `EMBED_TABLE_SLOTS_13` { #EMBED_TABLE_SLOTS_13 }

```c
#define EMBED_TABLE_SLOTS_13(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_, member12_, member13_) \
    EMBED_TABLE_SLOTS_12(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_, member11_, member12_) \
    EMBED_TABLE_SLOT(Table_, member13_, 12);
```

Asserts thirteen members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10.
- `[in]`: `member12_` Member at slot 11.
- `[in]`: `member13_` Member at slot 12, asserted after [`EMBED_TABLE_SLOTS_12`](#EMBED_TABLE_SLOTS_12) covers the first twelve.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with thirteen members.

### `EMBED_TABLE_SLOTS_14` { #EMBED_TABLE_SLOTS_14 }

```c
#define EMBED_TABLE_SLOTS_14(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_, member12_, member13_, member14_) \
    EMBED_TABLE_SLOTS_13(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_, member11_, member12_, member13_) \
    EMBED_TABLE_SLOT(Table_, member14_, 13);
```

Asserts fourteen members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10.
- `[in]`: `member12_` Member at slot 11.
- `[in]`: `member13_` Member at slot 12.
- `[in]`: `member14_` Member at slot 13, asserted after [`EMBED_TABLE_SLOTS_13`](#EMBED_TABLE_SLOTS_13) covers the first thirteen.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with fourteen members.

### `EMBED_TABLE_SLOTS_15` { #EMBED_TABLE_SLOTS_15 }

```c
#define EMBED_TABLE_SLOTS_15(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_) \
    EMBED_TABLE_SLOTS_14(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_, member11_, member12_, member13_, member14_) \
    EMBED_TABLE_SLOT(Table_, member15_, 14);
```

Asserts fifteen members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10.
- `[in]`: `member12_` Member at slot 11.
- `[in]`: `member13_` Member at slot 12.
- `[in]`: `member14_` Member at slot 13.
- `[in]`: `member15_` Member at slot 14, asserted after [`EMBED_TABLE_SLOTS_14`](#EMBED_TABLE_SLOTS_14) covers the first fourteen.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with fifteen members.

### `EMBED_TABLE_SLOTS_16` { #EMBED_TABLE_SLOTS_16 }

```c
#define EMBED_TABLE_SLOTS_16(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_) \
    EMBED_TABLE_SLOTS_15(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_) \
    EMBED_TABLE_SLOT(Table_, member16_, 15);
```

Asserts sixteen members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10.
- `[in]`: `member12_` Member at slot 11.
- `[in]`: `member13_` Member at slot 12.
- `[in]`: `member14_` Member at slot 13.
- `[in]`: `member15_` Member at slot 14.
- `[in]`: `member16_` Member at slot 15, asserted after [`EMBED_TABLE_SLOTS_15`](#EMBED_TABLE_SLOTS_15) covers the first fifteen.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with sixteen members.

### `EMBED_TABLE_SLOTS_17` { #EMBED_TABLE_SLOTS_17 }

```c
#define EMBED_TABLE_SLOTS_17(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                             member17_) \
    EMBED_TABLE_SLOTS_16(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_) \
    EMBED_TABLE_SLOT(Table_, member17_, 16);
```

Asserts seventeen members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10.
- `[in]`: `member12_` Member at slot 11.
- `[in]`: `member13_` Member at slot 12.
- `[in]`: `member14_` Member at slot 13.
- `[in]`: `member15_` Member at slot 14.
- `[in]`: `member16_` Member at slot 15.
- `[in]`: `member17_` Member at slot 16, asserted after [`EMBED_TABLE_SLOTS_16`](#EMBED_TABLE_SLOTS_16) covers the first sixteen.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with seventeen members.

### `EMBED_TABLE_SLOTS_18` { #EMBED_TABLE_SLOTS_18 }

```c
#define EMBED_TABLE_SLOTS_18(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                             member17_, member18_) \
    EMBED_TABLE_SLOTS_17(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                         member17_) \
    EMBED_TABLE_SLOT(Table_, member18_, 17);
```

Asserts eighteen members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10.
- `[in]`: `member12_` Member at slot 11.
- `[in]`: `member13_` Member at slot 12.
- `[in]`: `member14_` Member at slot 13.
- `[in]`: `member15_` Member at slot 14.
- `[in]`: `member16_` Member at slot 15.
- `[in]`: `member17_` Member at slot 16.
- `[in]`: `member18_` Member at slot 17, asserted after [`EMBED_TABLE_SLOTS_17`](#EMBED_TABLE_SLOTS_17) covers the first seventeen.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with eighteen members.

### `EMBED_TABLE_SLOTS_19` { #EMBED_TABLE_SLOTS_19 }

```c
#define EMBED_TABLE_SLOTS_19(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                             member17_, member18_, member19_) \
    EMBED_TABLE_SLOTS_18(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                         member17_, member18_) \
    EMBED_TABLE_SLOT(Table_, member19_, 18);
```

Asserts nineteen members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10.
- `[in]`: `member12_` Member at slot 11.
- `[in]`: `member13_` Member at slot 12.
- `[in]`: `member14_` Member at slot 13.
- `[in]`: `member15_` Member at slot 14.
- `[in]`: `member16_` Member at slot 15.
- `[in]`: `member17_` Member at slot 16.
- `[in]`: `member18_` Member at slot 17.
- `[in]`: `member19_` Member at slot 18, asserted after [`EMBED_TABLE_SLOTS_18`](#EMBED_TABLE_SLOTS_18) covers the first eighteen.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with nineteen members.

### `EMBED_TABLE_SLOTS_20` { #EMBED_TABLE_SLOTS_20 }

```c
#define EMBED_TABLE_SLOTS_20(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                             member17_, member18_, member19_, member20_) \
    EMBED_TABLE_SLOTS_19(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                         member17_, member18_, member19_) \
    EMBED_TABLE_SLOT(Table_, member20_, 19);
```

Asserts twenty members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10.
- `[in]`: `member12_` Member at slot 11.
- `[in]`: `member13_` Member at slot 12.
- `[in]`: `member14_` Member at slot 13.
- `[in]`: `member15_` Member at slot 14.
- `[in]`: `member16_` Member at slot 15.
- `[in]`: `member17_` Member at slot 16.
- `[in]`: `member18_` Member at slot 17.
- `[in]`: `member19_` Member at slot 18.
- `[in]`: `member20_` Member at slot 19, asserted after [`EMBED_TABLE_SLOTS_19`](#EMBED_TABLE_SLOTS_19) covers the first nineteen.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with twenty members.

### `EMBED_TABLE_SLOTS_21` { #EMBED_TABLE_SLOTS_21 }

```c
#define EMBED_TABLE_SLOTS_21(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                             member17_, member18_, member19_, member20_, member21_) \
    EMBED_TABLE_SLOTS_20(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                         member17_, member18_, member19_, member20_) \
    EMBED_TABLE_SLOT(Table_, member21_, 20);
```

Asserts twenty-one members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10.
- `[in]`: `member12_` Member at slot 11.
- `[in]`: `member13_` Member at slot 12.
- `[in]`: `member14_` Member at slot 13.
- `[in]`: `member15_` Member at slot 14.
- `[in]`: `member16_` Member at slot 15.
- `[in]`: `member17_` Member at slot 16.
- `[in]`: `member18_` Member at slot 17.
- `[in]`: `member19_` Member at slot 18.
- `[in]`: `member20_` Member at slot 19.
- `[in]`: `member21_` Member at slot 20, asserted after [`EMBED_TABLE_SLOTS_20`](#EMBED_TABLE_SLOTS_20) covers the first twenty.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with twenty-one members.

### `EMBED_TABLE_SLOTS_22` { #EMBED_TABLE_SLOTS_22 }

```c
#define EMBED_TABLE_SLOTS_22(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                             member17_, member18_, member19_, member20_, member21_, member22_) \
    EMBED_TABLE_SLOTS_21(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                         member17_, member18_, member19_, member20_, member21_) \
    EMBED_TABLE_SLOT(Table_, member22_, 21);
```

Asserts twenty-two members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10.
- `[in]`: `member12_` Member at slot 11.
- `[in]`: `member13_` Member at slot 12.
- `[in]`: `member14_` Member at slot 13.
- `[in]`: `member15_` Member at slot 14.
- `[in]`: `member16_` Member at slot 15.
- `[in]`: `member17_` Member at slot 16.
- `[in]`: `member18_` Member at slot 17.
- `[in]`: `member19_` Member at slot 18.
- `[in]`: `member20_` Member at slot 19.
- `[in]`: `member21_` Member at slot 20.
- `[in]`: `member22_` Member at slot 21, asserted after [`EMBED_TABLE_SLOTS_21`](#EMBED_TABLE_SLOTS_21) covers the first twenty-one.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with twenty-two members.

### `EMBED_TABLE_SLOTS_23` { #EMBED_TABLE_SLOTS_23 }

```c
#define EMBED_TABLE_SLOTS_23(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                             member17_, member18_, member19_, member20_, member21_, member22_, member23_) \
    EMBED_TABLE_SLOTS_22(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                         member17_, member18_, member19_, member20_, member21_, member22_) \
    EMBED_TABLE_SLOT(Table_, member23_, 22);
```

Asserts twenty-three members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10.
- `[in]`: `member12_` Member at slot 11.
- `[in]`: `member13_` Member at slot 12.
- `[in]`: `member14_` Member at slot 13.
- `[in]`: `member15_` Member at slot 14.
- `[in]`: `member16_` Member at slot 15.
- `[in]`: `member17_` Member at slot 16.
- `[in]`: `member18_` Member at slot 17.
- `[in]`: `member19_` Member at slot 18.
- `[in]`: `member20_` Member at slot 19.
- `[in]`: `member21_` Member at slot 20.
- `[in]`: `member22_` Member at slot 21.
- `[in]`: `member23_` Member at slot 22, asserted after [`EMBED_TABLE_SLOTS_22`](#EMBED_TABLE_SLOTS_22) covers the first twenty-two.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with twenty-three members.

### `EMBED_TABLE_SLOTS_24` { #EMBED_TABLE_SLOTS_24 }

```c
#define EMBED_TABLE_SLOTS_24(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                             member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                             member17_, member18_, member19_, member20_, member21_, member22_, member23_, member24_) \
    EMBED_TABLE_SLOTS_23(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, \
                         member9_, member10_, member11_, member12_, member13_, member14_, member15_, member16_, \
                         member17_, member18_, member19_, member20_, member21_, member22_, member23_) \
    EMBED_TABLE_SLOT(Table_, member24_, 23);
```

Asserts twenty-four members sit at consecutive slots from 0.

- `[in]`: `Table_` Struct type forwarded to [`EMBED_TABLE_SLOT`](#EMBED_TABLE_SLOT).
- `[in]`: `member1_` Member at slot 0.
- `[in]`: `member2_` Member at slot 1.
- `[in]`: `member3_` Member at slot 2.
- `[in]`: `member4_` Member at slot 3.
- `[in]`: `member5_` Member at slot 4.
- `[in]`: `member6_` Member at slot 5.
- `[in]`: `member7_` Member at slot 6.
- `[in]`: `member8_` Member at slot 7.
- `[in]`: `member9_` Member at slot 8.
- `[in]`: `member10_` Member at slot 9.
- `[in]`: `member11_` Member at slot 10.
- `[in]`: `member12_` Member at slot 11.
- `[in]`: `member13_` Member at slot 12.
- `[in]`: `member14_` Member at slot 13.
- `[in]`: `member15_` Member at slot 14.
- `[in]`: `member16_` Member at slot 15.
- `[in]`: `member17_` Member at slot 16.
- `[in]`: `member18_` Member at slot 17.
- `[in]`: `member19_` Member at slot 18.
- `[in]`: `member20_` Member at slot 19.
- `[in]`: `member21_` Member at slot 20.
- `[in]`: `member22_` Member at slot 21.
- `[in]`: `member23_` Member at slot 22.
- `[in]`: `member24_` Member at slot 23, asserted after [`EMBED_TABLE_SLOTS_23`](#EMBED_TABLE_SLOTS_23) covers the first twenty-three.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) selects this line for a table with twenty-four members.

> _**Warning:**_ This is the longest line in the family. A table with twenty-five members has no line to expand to. [`EMBED_NARG`](#EMBED_NARG) also stops at twenty-four. To raise the limit, add a new line here, one more constant in [`EMBED_NARG`](#EMBED_NARG), and one more parameter in [`EMBED_ARG_N`](#EMBED_ARG_N).

### `EMBED_TABLE_LAYOUT` { #EMBED_TABLE_LAYOUT }

```c
#define EMBED_TABLE_LAYOUT(Table_, ...) \
    EMBED_CAT(EMBED_TABLE_SLOTS_, EMBED_NARG(__VA_ARGS__))(Table_, __VA_ARGS__) \
        EMBED_STATIC_ASSERT(sizeof(Table_) == (size_t)EMBED_NARG(__VA_ARGS__) * EMBED_FUNCTION_POINTER_BYTES, \
                            #Table_ " has a member that is not in its dispatch list, or is padded")
```

Asserts a table's members sit at consecutive slots and that nothing else is in it.

- `[in]`: `Table_` Struct type forwarded to the arity line and to sizeof.
- `[in]`: ... Member names in slot order, one to twenty-four.

> **Note:** The per-member assertions catch a member left out of the list. The size assertion catches padding, since a padded struct is larger than the count times [`EMBED_FUNCTION_POINTER_BYTES`](#EMBED_FUNCTION_POINTER_BYTES).

> **Note:** [`EMBED_CAT`](#EMBED_CAT) builds the arity line's name from [`EMBED_NARG`](#EMBED_NARG)'s count of the member list. The caller writes each member once.

> _**Warning:**_ A member of some other type changes sizeof(`Table_`) and fails the size assertion.

### `EMBED_TABLE_STORAGE` { #EMBED_TABLE_STORAGE }

```c
#define EMBED_TABLE_STORAGE static const
```

Expands to static const.

> **Note:** Declares a dispatch table. static gives internal linkage. Each translation unit that includes the header gets its own copy. The copies do not collide at link time.

> **Note:** const lets the compiler resolve a call through a table entry to the function directly. That needs a table no other translation unit can reach and no code assigns to.

> **Note:** This macro does not supply [`EMBED_UNUSED`](embed_compiler_directives.md#EMBED_UNUSED). Write it on the table as well. A translation unit that calls nothing through the table warns without it.

### `EMBED_CAT_` { #EMBED_CAT_ }

```c
#define EMBED_CAT_(left_, right_) left_##right_
```

Expands to `left_`#`#right_`.

- `[in]`: `left_` Left operand of ##.
- `[in]`: `right_` Right operand of ##.

\*\*Returns:\*\* The single token that `left_` and `right_` make together.

> **Note:** [`EMBED_CAT`](#EMBED_CAT) calls this macro. ## does not expand its own operands, and [`EMBED_CAT`](#EMBED_CAT) expands them before it gives them to this macro. Call [`EMBED_CAT`](#EMBED_CAT) when either operand is itself a macro.

### `EMBED_CAT` { #EMBED_CAT }

```c
#define EMBED_CAT(left_, right_) EMBED_CAT_(left_, right_)
```

Expands to [`EMBED_CAT_`](#EMBED_CAT_)(`left_`, `right_`).

- `[in]`: `left_` Left operand, forwarded to [`EMBED_CAT_`](#EMBED_CAT_).
- `[in]`: `right_` Right operand, forwarded to [`EMBED_CAT_`](#EMBED_CAT_).

\*\*Returns:\*\* The single token that `left_` and `right_` make together.

> **Note:** This expands its arguments first, then [`EMBED_CAT_`](#EMBED_CAT_) pastes the results. A macro operand reaches [`EMBED_CAT_`](#EMBED_CAT_) as its value.

> **Note:** [`EMBED_TABLE_LAYOUT`](#EMBED_TABLE_LAYOUT) uses this to build an `EMBED_TABLE_SLOTS_`&lt;n&gt; name from [`EMBED_NARG`](#EMBED_NARG)'s count.

### `EMBED_NARG` { #EMBED_NARG }

```c
#define EMBED_NARG(...) \
    EMBED_NARG_(__VA_ARGS__, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
```

Expands to [`EMBED_ARG_N`](#EMBED_ARG_N) with `__VA_ARGS__` followed by the constants 24 down to 0.

- `[in]`: ... The list to count.

\*\*Returns:\*\* The number of arguments, for one to twenty-four arguments.

> **Note:** A count of the arguments turns an arity into a number that [`EMBED_CAT`](#EMBED_CAT) can paste into a macro name. The preprocessor has no loop.

> _**Warning:**_ An empty list gives 1. The preprocessor cannot tell an empty argument from no argument.

> _**Warning:**_ Twenty-five or more arguments make [`EMBED_ARG_N`](#EMBED_ARG_N) select an argument instead of a constant.

### `EMBED_NARG_` { #EMBED_NARG_ }

```c
#define EMBED_NARG_(...) EMBED_ARG_N(__VA_ARGS__)
```

Expands to `EMBED_ARG_N(__VA_ARGS__)`.

- `[in]`: ... The list from [`EMBED_NARG`](#EMBED_NARG), followed by the constants 24 down to 0.

\*\*Returns:\*\* The value [`EMBED_ARG_N`](#EMBED_ARG_N) selects.

> **Note:** [`EMBED_NARG`](#EMBED_NARG) calls this macro. One more macro level expands the list of the caller before [`EMBED_ARG_N`](#EMBED_ARG_N) picks a slot from it.

### `EMBED_ARG_N` { #EMBED_ARG_N }

```c
#define EMBED_ARG_N(slot1_, slot2_, slot3_, slot4_, slot5_, slot6_, slot7_, slot8_, slot9_, slot10_, slot11_, slot12_, \
                    slot13_, slot14_, slot15_, slot16_, slot17_, slot18_, slot19_, slot20_, slot21_, slot22_, slot23_, \
                    slot24_, selected_, ...) \
    selected_
```

Expands to its twenty-fifth argument.

- `[in]`: `slot1_` Arguments one through twenty-four, discarded.
- `[in]`: `selected_` The twenty-fifth argument.
- `[in]`: ... Arguments beyond the twenty-fifth, discarded.

\*\*Returns:\*\* `selected_`.

> **Note:** [`EMBED_NARG`](#EMBED_NARG) appends the constants 24 down to 0 after the caller's list. The caller's arguments push those constants to the right. Whichever one lands twenty-fifth is how many arguments the caller passed.

### `EMBED_CALL` { #EMBED_CALL }

```c
#define EMBED_CALL(entry_, ArgsType_, ...) entry_(&(ArgsType_){__VA_ARGS__})
```

Expands to `entry_`(&(`ArgsType_`){`__VA_ARGS__`}).

- `[in]`: `entry_` Function called with the address of the literal.
- `[in]`: `ArgsType_` Type of the compound literal.
- `[in]`: ... Initializers for the compound literal.

\*\*Returns:\*\* The value `entry_` returns.

> **Note:** One pointer costs one register at any arity. A long parameter list fills the registers and spills the rest onto the stack.

> **Note:** C sets to zero each member that the initializer does not name. A compound literal in an argument lives to the end of the block around it. So `entry_` can keep the pointer for the full call.

> **Note:** Use this macro from C only. No revision of C++ has compound literals. GNU C++ accepts one as an extension, but its lifetime there ends at the full-expression. That breaks the \[BORROWS\] contract below.

> **Note:** [`EMBED_ENTRY`](#EMBED_ENTRY) and [`EMBED_ENTRY_V`](#EMBED_ENTRY_V) expand this macro at a definition site, which is a .c file that compiles as C. A C++ translation unit sees the prototype under [`EMBED_BEGIN_DECLS`](embed_compiler_directives.md#EMBED_BEGIN_DECLS) and never the expansion.

> _**Warning:**_ `entry_` receives the address of the literal \[BORROWS\].

### `EMBED_ENTRY` { #EMBED_ENTRY }

```c
#define EMBED_ENTRY(entry_prefix_, backend_prefix_, CtxType_, CfgType_, ReturnType_, name_, ...) \
    ReturnType_ entry_prefix_##name_(const CfgType_ *args) \
    { \
        return EMBED_CALL(backend_prefix_##name_, CtxType_, __VA_ARGS__); \
    }
```

Defines an entry point that returns a value and forwards an argument pack.

- `[in]`: `entry_prefix_` Prefix of the emitted entry point, pasted ahead of `name_`.
- `[in]`: `backend_prefix_` Prefix of the backend the entry calls, pasted ahead of `name_`.
- `[in]`: `CtxType_` Type of the compound literal the backend receives.
- `[in]`: `CfgType_` Type the emitted entry takes a pointer to.
- `[in]`: `ReturnType_` Return type of the emitted function.
- `[in]`: `name_` Core name, pasted onto both prefixes.
- `[in]`: ... Initializers for the `CtxType_` literal, written from members of args.

\*\*Returns:\*\* What the backend returns.

> **Note:** One shape for each entry in the library. A caller writes the same call at each module. The entry tests nothing. The backend that it names does each check that an operation needs.

> _**Warning:**_ The initializers dereference args, so it must not be NULL \[BORROWS\].

### `EMBED_ENTRY_V` { #EMBED_ENTRY_V }

```c
#define EMBED_ENTRY_V(entry_prefix_, backend_prefix_, CtxType_, CfgType_, name_, ...) \
    void entry_prefix_##name_(const CfgType_ *args) \
    { \
        EMBED_CALL(backend_prefix_##name_, CtxType_, __VA_ARGS__); \
    }
```

Defines a void entry point that forwards an argument pack.

- `[in]`: `entry_prefix_` Prefix of the emitted entry point, pasted ahead of `name_`.
- `[in]`: `backend_prefix_` Prefix of the backend the entry calls, pasted ahead of `name_`.
- `[in]`: `CtxType_` Type of the compound literal the backend receives.
- `[in]`: `CfgType_` Type the emitted entry takes a pointer to.
- `[in]`: `name_` Core name, pasted onto both prefixes.
- `[in]`: ... Initializers for the `CtxType_` literal, written from members of args.

> **Note:** The same body as [`EMBED_ENTRY`](#EMBED_ENTRY), without the return. Two macros exist because void does not work as `ReturnType_`. With `void` there, the macro still emits `return backend(...)`, and C forbids a return with an expression in a function that returns void.

> _**Warning:**_ The initializers dereference args, so it must not be NULL \[BORROWS\].
