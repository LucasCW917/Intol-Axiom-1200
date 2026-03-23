# Intol Axiom 1200 — Opcode Reference

> **Processor:** Intol Axiom 1200
> **Opcode format:** `[32-bit namespace] x [32-bit opcode]`
> **Total opcode space:** ~1.84 × 10¹⁹ operations
> **Max args:** 64-bit

-----

## Table of Contents

1. [Processor Layout](#processor-layout)
1. [Memory & Registers](#memory--registers)
1. [Opcode Namespacing](#opcode-namespacing)
1. [Buffer Register System](#buffer-register-system)
1. [Math Operators](#math-operators)
1. [Logic Operators](#logic-operators)
1. [Collections (Lists & Dictionaries)](#collections-lists--dictionaries)
1. [User-Defined Functions](#user-defined-functions)
1. [Branching & Jumping](#branching--jumping)
1. [I/O & String Operations](#io--string-operations)
1. [Bitwise Operations](#bitwise-operations)
1. [Exception Handling](#exception-handling)
1. [Time Manipulation](#time-manipulation)
1. [Concurrency](#concurrency)
1. [QoL Extensions](#qol-extensions)

-----

## Processor Layout

```
┌─────────────────────────────────────────────────────────┐
│  MANAGER CORE        │  CORE A1  │  CORE B1  │  CORE C1 │
│  CORE ALPHA          │  CORE A2  │  CORE B2  │  CORE C2 │
│  MEM MANAGER         │  CORE A3  │  CORE B3  │  CORE C3 │
│  SHARED CACHE        │  CORE A4  │  CORE B4  │  CORE C4 │
└─────────────────────────────────────────────────────────┘
```

The **MEM MANAGER** core is responsible for all buffer register allocation, compaction, and deallocation. See [Buffer Register System](#buffer-register-system) for details.

-----

## Memory & Registers

|Type       |Prefix|Capacity                |Notes                                                                   |
|-----------|------|------------------------|------------------------------------------------------------------------|
|CPU Numeric|`R#`  |2^12 (~4,096)           |General-purpose integer registers                                       |
|Character  |`C#`  |2^10 (~1,024)           |Holds a single ASCII character each                                     |
|String     |`S#`  |2^8 (256)               |Variable-length strings                                                 |
|List       |`L#`  |2^14 (~16,384 registers)|Max 2^12 (~4,096) items per list                                        |
|Dictionary |`D#`  |2^12 (~4,096 registers) |Max 2^10 (~1,024) entries per dict                                      |
|Thread     |`T#`  |—                       |Returned by `FORK`; used by concurrency ops                             |
|Buffer     |`B#`  |—                       |Opcode-only scratch space. **Never accessible by user code.** See below.|


> **Immediate values** are supported as an advanced feature — you can pass raw literals directly in place of register arguments where noted.

-----

## Opcode Namespacing

Opcodes follow the format `[32-bit namespace] x [32-bit opcode]`, where `x` is a literal delimiter character separating the two 32-bit fields.

|Namespace         |Owner   |Description                                                            |
|------------------|--------|-----------------------------------------------------------------------|
|`0`               |CPU     |Built-in system opcodes. All opcodes in this document live here.       |
|`1` – `4294967295`|Programs|Per-program opcode space, registered in the central namespace database.|

Each registered program receives its own fully isolated 32-bit opcode field, giving it up to ~4 billion unique opcodes. With ~4 billion available program namespaces, the total addressable opcode space is approximately **1.84 × 10¹⁹ operations**.

**Namespace registration** is handled by a central database. Programs must register before defining custom opcodes. Namespace IDs are assigned at registration time and are guaranteed unique — conflicts are caught at registration, not at runtime.

> ℹ️ CPU opcodes in namespace `0` are shown throughout this document in their 32-bit opcode form for brevity (e.g. `0x0000000000000001`). In full notation, `ADD` is `0x00000000 x 0x00000001`.

-----

## Buffer Register System

Buffer registers (`B#`) are a protected, opcode-only memory type used as temporary scratch space during the execution of certain opcodes. They are entirely managed by the processor and the **MEM MANAGER** core — user code has no mechanism to address, read, or write them directly.

### Security Policy

> 🚨 **Any user code that attempts to access or modify a `B#` register will immediately crash the program. All memory associated with the offending program will be wiped to prevent damage to other programs.**

This is enforced at both the compiler level (the `B#` syntax is invalid in user code and will be rejected at compile time) and the runtime level (any runtime violation triggers immediate termination and memory wipe).

### `B0` — Buffer Count Register (BCR)

`B0` is a reserved system register that tracks the current highest allocated buffer address. It is **never freeable** and is always held by the system.

- At idle (no programs running), `B0 = 1`. It does not return to `0` because `B0` itself occupies register `0`.
- When a program allocates buffers, `B0` increments by the number of buffers allocated.
- When a program frees its buffers, the MEM MANAGER compacts the buffer space and decrements `B0` accordingly.

### Allocation & Compaction

Buffer space is always **contiguous** — there are never any unused buffer registers below `B0`. When a program frees its buffers out of order (i.e. not as the topmost allocation), the MEM MANAGER:

1. Frees the released range
1. Shifts all higher buffer allocations downward to close the gap
1. Updates all affected programs’ internal buffer references transparently
1. Decrements `B0`

This compaction is performed **atomically** by the MEM MANAGER to prevent concurrency issues during mid-shift.

> ⚠️ Buffer addresses are **not stable** for a program’s entire lifetime — they may shift downward if a lower program frees its buffers first. This is transparent to user code since `B#` is never directly addressable.

### Example — allocation and out-of-order freeing:

```
State         B0
──────────────────────────────────────────────────
Idle           1
A allocates   20,001    ; A holds B1–B20,000
B allocates   40,001    ; B holds B20,001–B40,000
C allocates   50,001    ; C holds B40,001–B50,000
B frees       30,001    ; MEM MANAGER shifts C down; C now holds B20,001–B30,000
C frees       20,001    ; C freed; A still holds B1–B20,000
A frees        1        ; all freed; B0 returns to 1
```

### `BUFF` — `0x00000000 x 0x00001073`

Allocates or frees a block of buffer registers. Used internally by opcodes such as `DSTR`. **Not intended for direct use in user programs.** Documented here for implementors.

```
BUFF ALLOC <count>   ; allocate <count> buffer registers, returns base address
BUFF FREE  <base>    ; free the buffer block starting at <base>
```

-----

## Math Operators

Opcodes `0x0001` – `0x0008`

-----

### `ADD` — `0x0000000000000001`

Adds two values and stores the result.

```
ADD <A> <B> <dest>
```

|Arg |Description                 |
|----|----------------------------|
|A   |First operand               |
|B   |Second operand              |
|dest|Register to store the result|

**Example:**

```asm
SAV R0 10
SAV R1 5
ADD R0 R1 R2   ; R2 = 15
```

-----

### `SUB` — `0x0000000000000010`

Subtracts B from A and stores the result.

```
SUB <A> <B> <dest>
```

**Example:**

```asm
SAV R0 10
SAV R1 3
SUB R0 R1 R2   ; R2 = 7
```

-----

### `MUL` — `0x0000000000000011`

Multiplies two values and stores the result.

```
MUL <A> <B> <dest>
```

**Example:**

```asm
SAV R0 6
SAV R1 7
MUL R0 R1 R2   ; R2 = 42
```

-----

### `DIV` — `0x0000000000000100`

Divides A by B and stores the result.

> ⚠️ If B is `0`, the program will be **flagged**. Always validate B before dividing.

```
DIV <A> <B> <dest>
```

**Example:**

```asm
SAV R0 20
SAV R1 4
DIV R0 R1 R2   ; R2 = 5
```

**Safe division pattern:**

```asm
SAV R0 20
SAV R1 0              ; potential zero divisor
SAV R2 0
IFEQ R1 R2 R3         ; R3 = 1 if R1 == 0
IFFL R3 DIV R0 R1 R2  ; only divide if R1 is not 0
```

-----

### `SHL` — `0x0000000000000101`

Shifts all bits in a register one position to the left. Equivalent to multiplying by 2.

```
SHL <reg>
```

**Example:**

```asm
SAV R0 4    ; binary: 0100
SHL R0      ; R0 = 8 (binary: 1000)
```

-----

### `SHR` — `0x0000000000000110`

Shifts all bits in a register one position to the right. Equivalent to integer division by 2.

```
SHR <reg>
```

**Example:**

```asm
SAV R0 8    ; binary: 1000
SHR R0      ; R0 = 4 (binary: 0100)
```

-----

### `NEG` — `0x0000000000000111`

Negates the value of a register. Has no effect on empty or non-existent registers.

```
NEG <reg>
```

**Example:**

```asm
SAV R0 5
NEG R0      ; R0 = -5
```

-----

### `SAV` — `0x0000000000001000`

Saves a literal decimal integer into a register, overwriting any existing value.

> ℹ️ Input must be in **decimal** (base-10).

```
SAV <dest> <value>
```

**Example:**

```asm
SAV R0 42   ; R0 = 42
SAV R0 0    ; R0 = 0 (overwrites previous value)
```

-----

## Logic Operators

Opcodes `0x0009` – `0x000D`

-----

### `IFEQ` — `0x0000000000001001`

Outputs `1` to dest if A equals B; otherwise outputs `0`.

```
IFEQ <A> <B> <dest>
```

**Example:**

```asm
SAV R0 5
SAV R1 5
IFEQ R0 R1 R2   ; R2 = 1

SAV R1 9
IFEQ R0 R1 R2   ; R2 = 0
```

-----

### `IFNQ` — `0x0000000000001010`

Outputs `0` to dest if A equals B; otherwise outputs `1`. Logical inverse of `IFEQ`.

```
IFNQ <A> <B> <dest>
```

**Example:**

```asm
SAV R0 5
SAV R1 9
IFNQ R0 R1 R2   ; R2 = 1 (they are NOT equal)
```

-----

### `IFTR` — `0x0000000000001011`

Executes a single instruction if the condition register holds `1`.

> ℹ️ `IFTR` guards exactly one instruction. For multi-instruction conditionals, use `JMP` with a set point.

```
IFTR <cond> <instruction...>
```

**Example:**

```asm
SAV R0 10
SAV R1 10
IFEQ R0 R1 R2           ; R2 = 1
IFTR R2 MUL R0 R1 R3    ; executes: R3 = 100
```

-----

### `IFFL` — `0x0000000000001100`

Executes a single instruction if the condition register holds `0`. Logical inverse of `IFTR`.

> ℹ️ `IFFL` guards exactly one instruction. For multi-instruction conditionals, use `JMP` with a set point.

```
IFFL <cond> <instruction...>
```

**Example:**

```asm
SAV R0 5
SAV R1 9
IFEQ R0 R1 R2            ; R2 = 0 (not equal)
IFFL R2 ADD R0 R1 R3     ; executes: R3 = 14
```

-----

### `WHLE` — `0x0000000000001101`

Repeatedly executes a single instruction for as long as the condition register is `1`.

> ⚠️ Ensure the loop body modifies the condition register, or the program will loop forever.

```
WHLE <cond> <instruction...>
```

**Example — count down from 5:**

```asm
SAV R0 5
SAV R1 1
WHLE R0 SUB R0 R1 R0   ; R0 decrements each iteration until 0
```

-----

## Collections (Lists & Dictionaries)

-----

### `SAVL` — `0x0000010000010001`

Appends a value to a list. Creates the list if it does not already exist. Uses dedicated `L#` memory.

```
SAVL <list> <value>
```

**Example:**

```asm
SAV R0 10
SAV R1 20
SAV R2 30
SAVL L0 R0   ; L0 = [10]
SAVL L0 R1   ; L0 = [10, 20]
SAVL L0 R2   ; L0 = [10, 20, 30]
```

-----

### `SAVD` — `0x0000010000010010`

Adds or updates an entry in a dictionary. The key (input B) maps to the value (input C). Uses dedicated `D#` memory.

```
SAVD <dict> <key> <value>
```

**Example:**

```asm
SAV R0 99
SAVD D0 R0 L0   ; D0["99"] = L0
```

-----

### `GET` — `0x0000010000010100`

Retrieves an item from a dictionary by key, or retrieves an item from a list by zero-based index.

```
GET <dict> <key>       ; dictionary lookup
GET <list>[<index>]    ; list index (zero-based)
```

**Example:**

```asm
; List indexing
SAVL L0 R0
SAVL L0 R1
GET L0[0]   ; retrieves first item of L0
GET L0[1]   ; retrieves second item of L0

; Dictionary lookup
SAVD D0 R0 L0
GET D0 R0   ; retrieves value at key R0 in D0
```

-----

### `LEN` — `0x0000010000010000`

Counts the number of items in a list or dictionary and stores the result in a register.

```
LEN <dest> <collection>
```

**Example:**

```asm
SAVL L0 R0
SAVL L0 R1
SAVL L0 R2
LEN R3 L0   ; R3 = 3
```

-----

## User-Defined Functions

User-defined functions occupy the program’s own opcode namespace. See [Opcode Namespacing](#opcode-namespacing) for details on registering a program namespace.

-----

### `DEF` — `0x0000000000001110`

Defines a named, reusable function and binds it to an opcode within the current program’s namespace.

- **Opcode slot:** Any opcode within the program’s registered namespace.
- **Function file:** A `.apo` source file containing the function body.
- **Arguments:** Declared here; must be received inside the function with `TAKE`.

```
DEF <opcode> <name> <file.apo> [args...]
```

**Example:**

```asm
DEF 0x0000000001100100 MIN minfunc.apo R0 R1
```

-----

### `TAKE` — `0x0000000000010000`

Receives arguments passed to the current function and stores them in CPU registers. **Only valid inside a function body.**

> ⚠️ Arguments are written directly into CPU register memory. Place `TAKE` at the very top of each function to avoid clobbering registers already in use by the caller.

```
TAKE <reg> [reg...]
```

**Example — inside `minfunc.apo`:**

```asm
TAKE R0 R1       ; receive two arguments
IFEQ R0 R1 R2
IFTR R2 RET R0   ; if equal, return R0
SUB R0 R1 R3
; ... determine minimum and return
RET R0
```

-----

### `RET` — `0x0000000000010001`

Returns a value from the current function. **Only valid inside a function body.**

```
RET <reg>
```

**Example:**

```asm
RET R0   ; return the value in R0 to the caller
```

-----

## Branching & Jumping

-----

### `STP` — `0x0000001111111010`

Marks a named jump target in the program. The program cursor can be directed here via `JMP`. Set points must have a name or the program will fail to compile.

```
STP <name> [; comment]
```

**Example:**

```asm
STP loop_start   ; marks the top of a loop
```

-----

### `JMP` — `0x0000001111111011`

Jumps execution to a named set point. Combine with `IFTR` or `IFFL` for conditional branching. This is the recommended pattern for multi-instruction conditionals.

```
JMP <name> [; comment]
```

**Example — conditional loop:**

```asm
SAV R0 0
SAV R1 1
SAV R2 5

STP count_up
ADD R0 R1 R0           ; R0 += 1
IFEQ R0 R2 R3          ; R3 = 1 when R0 reaches 5
IFFL R3 JMP count_up   ; keep looping if not yet 5
```

-----

## I/O & String Operations

### Creating strings

There are several ways to create strings on the Axiom 1200:

|Method|Description                                                       |Best for                            |
|------|------------------------------------------------------------------|------------------------------------|
|`DSTR`|Compile-time opcode. Resolves a string literal directly into `S#`.|Static strings known at compile time|
|`INP` |Reads a line from the user into `S#` at runtime.                  |User input                          |
|`GSTR`|Assembles a string from `C#` character registers.                 |Dynamically built strings           |
|`SAPP`|Appends one string register onto another.                         |String concatenation                |

For static strings, always prefer `DSTR` — it is the most concise and has zero runtime overhead.

-----

### `DSTR` — `0x0000010000110001` *(compile-time opcode)*

Resolves a string literal into a string register at compile time. `DSTR` is a **compile-time opcode** — a first-class language feature fully resolved before execution, producing no runtime instruction. Like C++ templates, it is part of the language specification rather than a preprocessor directive.

**Internally**, `DSTR` performs the following steps at compile time:

1. Calculates the byte length of the string literal
1. Allocates the required `B#` buffer registers via `BUFF`
1. Executes `SAV` → `TCHA` → `GSTR` into the buffer registers
1. Frees all allocated buffer registers
1. Saves the completed string into the target `S#` register

The user never interacts with `B#` directly. The only observable side effect is the populated `S#` register. Strings created by `DSTR` are mutable — they can be modified by `SAPP` and similar operations after creation.

```
DSTR <dest_string_reg> "<literal>"
```

**Example:**

```asm
DSTR S0 "Hello, World!"
OUT S0   ; prints: Hello, World!
```

**Example — error message:**

```asm
DSTR S0 "Value out of range"
THW S0
```

-----

### `TCHA` — `0x0000001111111101`

Converts a decimal ASCII code stored in a numeric register into a character and places it in a character register.

> ⚠️ If the value does not map to a valid ASCII character, behaviour is implementation-defined.

```
TCHA <src_reg> <char_reg>
```

**Example:**

```asm
SAV R0 72      ; ASCII 72 = 'H'
TCHA R0 C0     ; C0 = 'H'
```

-----

### `TINT` — `0x0000001111111110`

Converts a character in a character register back to its decimal ASCII code and stores it in a numeric register.

```
TINT <char_reg> <dest_reg>
```

**Example:**

```asm
SAV R0 72
TCHA R0 C0     ; C0 = 'H'
TINT C0 R1     ; R1 = 72
```

-----

### `UGST` — `0x0000010000000001`

Splits a string from string memory into individual characters, placing each into successive character registers.

- Extra character registers beyond the string length are left unchanged.
- Characters beyond the number of provided registers are discarded.

```
UGST <string_reg> <C0> [C1 C2 ...]
```

**Example:**

```asm
DSTR S0 "Hi"
UGST S0 C0 C1   ; C0 = 'H', C1 = 'i'
```

-----

### `GSTR` — `0x0000010000000010`

Concatenates a sequence of character registers into a string and stores it in string memory. Overwrites the target register if it already exists.

```
GSTR <dest_string_reg> <C0> [C1 C2 ...]
```

**Example:**

```asm
SAV R0 72    ; 'H'
SAV R1 105   ; 'i'
TCHA R0 C0
TCHA R1 C1
GSTR S0 C0 C1   ; S0 = "Hi"
OUT S0
```

-----

### `OUT` — `0x0000001111111111`

Prints the contents of a string register to standard output. Only accepts `S#` registers.

> ℹ️ To print a number, first convert it with `ITOS`. To print a character, first group it with `GSTR` or use `DSTR`.

```
OUT <string_reg>
```

**Example:**

```asm
DSTR S0 "Hello!"
OUT S0   ; prints: Hello!
```

-----

### `INP` — `0x0000010000000000`

Reads a line of input from the user and stores it as a string. Execution blocks until the user presses ENTER.

```
INP <string_reg>
```

**Example:**

```asm
DSTR S0 "Enter a number: "
OUT S0
INP S1           ; wait for input
ISNUM S1 R0      ; validate before parsing
IFTR R0 STOI S1 R1
```

-----

### `ITOS` — `0x0000010000101110`

Converts a numeric register’s value to its decimal string representation. Negative values include a leading `-` sign. Overwrites the target string register if it already exists.

```
ITOS <src_reg> <dest_string_reg>
```

**Example:**

```asm
SAV R0 -42
ITOS R0 S0   ; S0 = "-42"
OUT S0
```

-----

### `STOI` — `0x0000010000101111`

Parses a string register containing a decimal integer and stores the numeric result in a CPU register. The inverse of `ITOS`.

> ⚠️ If the string cannot be parsed as a valid integer, the program will be flagged. Use `ISNUM` to validate untrusted input before calling `STOI`.

```
STOI <src_string_reg> <dest_reg>
```

**Example:**

```asm
INP S0           ; user types "42"
STOI S0 R0       ; R0 = 42
SAV R1 10
MUL R0 R1 R2     ; R2 = 420
ITOS R2 S1
OUT S1           ; prints: 420
```

-----

### `SAPP` — `0x0000010000110000`

Appends the contents of a source string register onto the end of a destination string register. Creates the destination register if it does not exist. The source is not modified.

```
SAPP <src> <dest>
```

**Example:**

```asm
DSTR S0 "Hello"
DSTR S1 ", World!"
SAPP S1 S0   ; S0 = "Hello, World!"
OUT S0
```

-----

### `WRAW` — `0x0000010000110010`

Writes a raw opcode and its arguments to the program output stream without executing them. Intended for compiler and assembler construction.

```
WRAW <opcode> [args...]
```

> ℹ️ The emitted instruction is **not** executed by the current program — it is written to the output stream for use by downstream tools.

-----

## Bitwise Operations

All higher-level bitwise operations are derived from `NAND`.

|Opcode              |Mnemonic|Operation                                   |
|--------------------|--------|--------------------------------------------|
|`0x0000010000000011`|`NAND`  |`¬(A ∧ B)`                                  |
|`0x0000010000000100`|`NOT`   |`NAND(A, A)`                                |
|`0x0000010000000101`|`AND`   |`NAND(NAND(A,B), NAND(A,B))`                |
|`0x0000010000000110`|`OR`    |`NAND(NAND(A,A), NAND(B,B))`                |
|`0x0000010000000111`|`NOR`   |`NAND(OR(A,B), OR(A,B))`                    |
|`0x0000010000001000`|`XOR`   |`NAND(NAND(A,NAND(A,B)), NAND(B,NAND(A,B)))`|
|`0x0000010000001001`|`XNOR`  |`NAND(XOR(A,B), XOR(A,B))`                  |
|`0x0000010000001010`|`BUF`   |`NAND(NAND(A,A), NAND(A,A))` = A            |

-----

### `NAND` — `0x0000010000000011`

Bitwise NAND. The primitive from which all other bitwise operations are derived.

```
NAND <A> <B> <dest>
```

**Example:**

```asm
SAV R0 1
SAV R1 1
NAND R0 R1 R2   ; R2 = 0  (NOT(1 AND 1))

SAV R1 0
NAND R0 R1 R2   ; R2 = 1  (NOT(1 AND 0))
```

-----

### `NOT` — `0x0000010000000100`

Inverts a single value.

```
NOT <A> <dest>
```

-----

### `AND` — `0x0000010000000101`

Returns `1` if both inputs are `1`.

```
AND <A> <B> <dest>
```

-----

### `OR` — `0x0000010000000110`

Returns `1` if either input is `1`.

```
OR <A> <B> <dest>
```

-----

### `NOR` — `0x0000010000000111`

Returns `1` only if both inputs are `0`.

```
NOR <A> <B> <dest>
```

-----

### `XOR` — `0x0000010000001000`

Returns `1` if inputs differ; `0` if they are the same.

```
XOR <A> <B> <dest>
```

-----

### `XNOR` — `0x0000010000001001`

Returns `1` if both inputs are the same.

```
XNOR <A> <B> <dest>
```

-----

### `BUF` — `0x0000010000001010`

Buffer gate — passes input through unchanged. Useful for explicit signal routing.

```
BUF <A> <dest>
```

-----

## Exception Handling

-----

### `TRY` — `0x0000010000001011`

Attempts to execute a function. Stores `0` in a string register on success, or an error message string on failure.

```
TRY <function> [args...] <string_reg>
```

**Example:**

```asm
TRY MIN R0 R1 S0
OUT S0   ; prints "0" on success, or an error message on failure
```

-----

### `THW` — `0x0000010000001100`

Intentionally throws an error with a custom message string. Intended for use inside function bodies within a `TRY` block.

```
THW <string_reg>
```

**Example:**

```asm
DSTR S0 "Value out of range"
THW S0
```

-----

## Time Manipulation

-----

### `GTM` — `0x0000010000001101`

Returns the current time as a Unix timestamp (seconds since epoch) and stores it in a numeric register.

```
GTM <dest>
```

**Example:**

```asm
GTM R0   ; R0 = current Unix timestamp
```

-----

### `CTM` — `0x0000010000001110`

Converts a Unix timestamp into a human-readable string and stores it in a string register, ready for `OUT`.

```
CTM <timestamp_reg> <dest_string_reg>
```

**Example:**

```asm
GTM R0
CTM R0 S0
OUT S0   ; prints something like: "Mon Mar 23 12:00:00 2026"
```

-----

### `STAL` — `0x0000010000001111`

Pauses execution for a given number of seconds.

```
STAL <seconds_reg>
```

**Example:**

```asm
SAV R0 3
STAL R0   ; pause for 3 seconds
```

-----

## Concurrency

-----

### `FORK` — `0x0000010000010101`

Spawns a new thread that executes a function or opcode independently. Returns a thread ID into a `T#` register. The parent thread continues immediately without waiting.

```
FORK <thread_reg> <opcode_int>
```

**Example:**

```asm
SAV R0 1
FORK T0 R0   ; spawn thread; T0 = thread ID
```

-----

### `JOIN` — `0x0000010000010110`

Blocks the current thread until the specified thread finishes.

```
JOIN <thread_reg>
```

**Example:**

```asm
FORK T0 R0
JOIN T0   ; wait for thread to finish
```

-----

### `LOCK` — `0x0000010000010111`

Prevents other threads from accessing a register until `UNLO` is called.

```
LOCK <R#|L#|D#>
```

**Example:**

```asm
LOCK R0
ADD R0 R1 R0
UNLO R0
```

-----

### `UNLO` — `0x0000010000011000`

Releases a lock previously acquired with `LOCK`.

```
UNLO <R#|L#|D#>
```

-----

### `AADD` — `0x0000010000011001`

Thread-safe addition. Use in place of `ADD` when multiple threads may access the same register.

```
AADD <A> <B> <dest>
```

-----

### `ASUB` — `0x0000010000011101`

Thread-safe subtraction.

```
ASUB <A> <B> <dest>
```

-----

### `AMUL` — `0x0000010000011110`

Thread-safe multiplication.

```
AMUL <A> <B> <dest>
```

-----

### `ADIV` — `0x0000010000011111`

Thread-safe division.

```
ADIV <A> <B> <dest>
```

-----

### `YIEL` — `0x0000010000011010`

Voluntarily yields CPU time to allow other threads to execute. Use in tight loops to prevent thread starvation.

```
YIEL
```

-----

### `THID` — `0x0000010000011011`

Retrieves the unique ID of the currently executing thread.

```
THID <dest>
```

**Example:**

```asm
THID R0   ; R0 = current thread ID
```

-----

### `THST` — `0x0000010000011100`

Queries whether a thread is still running. Stores `1` if running, `0` if finished.

```
THST <thread_reg> <dest>
```

**Example:**

```asm
FORK T0 R0

STP wait_loop
THST T0 R1
IFTR R1 JMP wait_loop   ; spin until thread finishes
```

-----

## QoL Extensions

-----

### `MOD` — `0x0000010001110011`

Computes the modulo (remainder) of A divided by B. Flags the program if B is `0`.

```
MOD <A> <B> <dest>
```

**Example:**

```asm
SAV R0 17
SAV R1 5
MOD R0 R1 R2   ; R2 = 2
```

-----

### `ABS` — `0x0000010001110100`

Returns the absolute value of a register.

```
ABS <src> <dest>
```

**Example:**

```asm
SAV R0 -9
ABS R0 R1   ; R1 = 9
```

-----

### `MIN` — `0x0000010001110101`

Stores the smaller of two values in dest.

```
MIN <A> <B> <dest>
```

**Example:**

```asm
SAV R0 3
SAV R1 7
MIN R0 R1 R2   ; R2 = 3
```

-----

### `MAX` — `0x0000010001110110`

Stores the larger of two values in dest.

```
MAX <A> <B> <dest>
```

**Example:**

```asm
SAV R0 3
SAV R1 7
MAX R0 R1 R2   ; R2 = 7
```

-----

### `SLEN` — `0x0000010001110111`

Returns the character length of a string register and stores it in a numeric register.

```
SLEN <string_reg> <dest>
```

**Example:**

```asm
DSTR S0 "Hello"
SLEN S0 R0   ; R0 = 5
```

-----

### `SCMP` — `0x0000010001111000`

Compares two strings. Stores `1` if they are identical, `0` otherwise.

```
SCMP <S_A> <S_B> <dest>
```

**Example:**

```asm
DSTR S0 "abc"
DSTR S1 "abc"
SCMP S0 S1 R0   ; R0 = 1

DSTR S1 "xyz"
SCMP S0 S1 R0   ; R0 = 0
```

-----

### `ISNUM` — `0x0000010001111001`

Checks whether a string register contains a valid parseable integer. Stores `1` if it can be parsed as an integer, `0` otherwise. Always use before `STOI` when handling untrusted input.

```
ISNUM <string_reg> <dest>
```

**Example:**

```asm
INP S0
ISNUM S0 R0
IFTR R0 STOI S0 R1   ; only parse if valid
```

-----

### `CLRL` — `0x0000010001111010`

Clears all items from a list without destroying the register itself.

```
CLRL <list>
```

**Example:**

```asm
CLRL L0   ; L0 is now empty
```

-----

### `DELD` — `0x0000010001111011`

Removes a specific entry from a dictionary by key.

```
DELD <dict> <key>
```

**Example:**

```asm
SAV R0 5
SAVD D0 R0 L0
DELD D0 R0      ; entry removed
```

-----

### `DBG` — `0x0000010001111100`

Dumps the current contents of a register to stderr for debugging. Does not affect program state or register contents.

```
DBG <R#|C#|S#|L#|D#>
```

**Example:**

```asm
SAV R0 99
DBG R0   ; stderr: [DBG] R0 = 99

DSTR S0 "test"
DBG S0   ; stderr: [DBG] S0 = "test"
```

-----

*End of Intol Axiom 1200 Opcode Reference*