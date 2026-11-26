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
                            #Table_ " has a member that is not in its dispatch list, or padding between two members")
```

Asserts a table's members sit at consecutive slots and that nothing else is in it.

- `[in]`: `Table_` Struct type forwarded to the arity line and to sizeof.
- `[in]`: ... Member names in slot order, one to twenty-four.

> **Note:** The per-member assertions catch a member left out of the list. The size assertion catches padding, since a padded struct is larger than the count times [`EMBED_FUNCTION_POINTER_BYTES`](#EMBED_FUNCTION_POINTER_BYTES).

> **Note:** [`EMBED_CAT`](embed_compiler_directives.md#EMBED_CAT) builds the arity line's name from [`EMBED_NARG`](#EMBED_NARG)'s count of the member list. The caller writes each member once.

> _**Warning:**_ A member of some other type changes sizeof(`Table_`) and fails the size assertion.

> _**Warning:**_ MSVC counts `__VA_ARGS__` as the standard does only under /Zc:preprocessor. Its C11 mode turns that option on. A C++ unit under MSVC needs it on the command line, or the count is wrong and the build stops.

### `EMBED_TABLE_STORAGE` { #EMBED_TABLE_STORAGE }

```c
#define EMBED_TABLE_STORAGE static EMBED_PASS_MEMORY const
```

Expands to static const, and to `static __device__ const` in the device pass of nvcc.

> **Note:** Declares a dispatch table. static gives internal linkage. Each translation unit that includes the header gets its own copy. The copies do not collide at link time.

> **Note:** const lets the compiler resolve a call through a table entry to the function directly. That needs a table no other translation unit can reach and no code assigns to.

> **Note:** nvcc reads each translation unit twice, once for the host and once for the device. Each pass gets its own copy of the table. The host copy holds the host address of each function, and the device copy holds the device address. A function with [`EMBED_HOST_DEVICE`](embed_compiler_directives.md#EMBED_HOST_DEVICE) has both, and so one initializer fills both copies. A kernel then calls through the table as a host function does.

> **Note:** The type of the table is the same in both passes. Only the memory that holds it changes. The copy is static, and so no other translation unit and no host code reaches the device copy.

> **Note:** This macro does not supply [`EMBED_UNUSED`](embed_compiler_directives.md#EMBED_UNUSED). Write it on the table as well. A translation unit that calls nothing through the table warns without it.

> _**Warning:**_ Each member of a table with this macro must point at a function with [`EMBED_HOST_DEVICE`](embed_compiler_directives.md#EMBED_HOST_DEVICE). In the device pass, nvcc refuses the address of a function that has no device form.

### `EMBED_NARG` { #EMBED_NARG }

```c
#define EMBED_NARG(...) \
    EMBED_NARG_(__VA_ARGS__, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
```

Expands to [`EMBED_ARG_N`](#EMBED_ARG_N) with `__VA_ARGS__` followed by the constants 24 down to 0.

- `[in]`: ... The list to count.

\*\*Returns:\*\* The number of arguments, for one to twenty-four arguments.

> **Note:** A count of the arguments turns an arity into a number that [`EMBED_CAT`](embed_compiler_directives.md#EMBED_CAT) can paste into a macro name. The preprocessor has no loop.

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
#if defined(__cplusplus)
#define EMBED_CALL(entry_, ArgsType_, ...) entry_(embed_cfg_address(ArgsType_{__VA_ARGS__}))
#else
#define EMBED_CALL(entry_, ArgsType_, ...) entry_(&(ArgsType_){__VA_ARGS__})
#endif
```

Expands to `entry_`(&(`ArgsType_`){`__VA_ARGS__`}).

- `[in]`: `entry_` Function called with the address of the literal.
- `[in]`: `ArgsType_` Type of the compound literal.
- `[in]`: ... Initializers for the compound literal.

\*\*Returns:\*\* The value `entry_` returns.

> **Note:** One pointer costs one register at any arity. A long parameter list fills the registers and spills the rest onto the stack.

> **Note:** C sets to zero each member that the initializer does not name. A compound literal in an argument lives to the end of the block around it. So `entry_` can retain the pointer for the full call.

> **Note:** A call must give each member of each Cfg in these headers. No pointer member takes zero, and a call that leaves one out reads through a null pointer.

> **Note:** No revision of C++ has compound literals. In C++ the macro builds a braced temporary of `ArgsType_` and passes its address through [`embed_cfg_address`](#embed_cfg_address). The temporary lives to the end of the full-expression, which covers the call and no more.

> **Note:** A call in C++ before C++20 gives the initializers in the order of the members, with no designators. Each call in these headers does so, and so each header compiles as C++11.

> **Note:** [`EMBED_ENTRY`](#EMBED_ENTRY) and [`EMBED_ENTRY_V`](#EMBED_ENTRY_V) expand this macro at a definition site, which is a .c file. That file compiles as C, or as CUDA under nvcc. A C++ translation unit that is not CUDA sees the prototype under [`EMBED_BEGIN_DECLS`](embed_compiler_directives.md#EMBED_BEGIN_DECLS) and never the expansion.

> _**Warning:**_ `entry_` receives the address of the literal \[BORROWS\]. In C++ the address is good for the call alone, so `entry_` must not retain it.

### `EMBED_ENTRY` { #EMBED_ENTRY }

```c
#define EMBED_ENTRY(entry_prefix_, backend_prefix_, CtxType_, CfgType_, ReturnType_, name_, ...) \
    EMBED_HOST_DEVICE ReturnType_ entry_prefix_##name_(const CfgType_ *args) \
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

> **Note:** The entry has [`EMBED_HOST_DEVICE`](embed_compiler_directives.md#EMBED_HOST_DEVICE). Under nvcc it compiles into the host pass and the device pass, and its prototype and its backend must have the same mark.

> _**Warning:**_ The initializers dereference args, so it must not be NULL \[BORROWS\].

### `EMBED_ENTRY_V` { #EMBED_ENTRY_V }

```c
#define EMBED_ENTRY_V(entry_prefix_, backend_prefix_, CtxType_, CfgType_, name_, ...) \
    EMBED_HOST_DEVICE void entry_prefix_##name_(const CfgType_ *args) \
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

> **Note:** The entry has [`EMBED_HOST_DEVICE`](embed_compiler_directives.md#EMBED_HOST_DEVICE), as [`EMBED_ENTRY`](#EMBED_ENTRY) gives.

> _**Warning:**_ The initializers dereference args, so it must not be NULL \[BORROWS\].

## Functions

### `embed_cfg_address` { #embed_cfg_address }

```c
#if defined(__cplusplus)
extern "C++" template <typename Cfg_> EMBED_HOST_DEVICE inline Cfg_ *embed_cfg_address(Cfg_ &&cfg);
#endif
```

The address of a Cfg, for [`EMBED_CALL`](#EMBED_CALL) in C++, where a braced temporary has no address operator.

- `[in]`: cfg The Cfg \[BORROWS\].

\*\*Returns:\*\* The address of cfg, good to the end of the full-expression of the call.

> **Note:** `Cfg_` is the type of the Cfg. The template has C++ linkage, inside the C guard of this header.

> **Note:** The address is not const, as the address of a compound literal of C is not. A backend that takes a pointer to a Ctx that it can write to then takes the same call in C and in C++.

> **Note:** [`EMBED_CALL`](#EMBED_CALL) gives a braced temporary, which binds to the reference of a temporary. An object with a name does not bind, and so this template takes no address of an object that outlives the call.
