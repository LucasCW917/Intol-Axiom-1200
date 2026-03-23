# Intol Axiom 1100 — Opcode Reference

> **Processor:** Intol Axiom 1100
> **Architecture:** CISC, x86-inspired
> **Data width:** 64-bit
> **Memory model:** Flat (single continuous 64-bit address space)
> **Instruction length:** Variable (1–15 bytes)
> **Endianness:** Little-endian

-----

## Table of Contents

1. [Processor Layout](#processor-layout)
1. [Registers](#registers)
1. [Memory Model](#memory-model)
1. [Instruction Encoding](#instruction-encoding)
1. [Addressing Modes](#addressing-modes)
1. [Data Transfer](#data-transfer)
1. [Arithmetic](#arithmetic)
1. [Logic & Bitwise](#logic--bitwise)
1. [Shift & Rotate](#shift--rotate)
1. [Control Flow](#control-flow)
1. [Stack Operations](#stack-operations)
1. [Procedures](#procedures)
1. [String Operations](#string-operations)
1. [I/O](#io)
1. [Flags & Conditionals](#flags--conditionals)
1. [System Instructions](#system-instructions)

-----

## Processor Layout

```
┌──────────────────────────────────────────────────────────────┐
│  MANAGER CORE         │  CORE A1   │  CORE B1   │  CORE C1  │
│  CORE ALPHA           │  CORE A2   │  CORE B2   │  CORE C2  │
│  MEM MANAGER          │  CORE A3   │  CORE B3   │  CORE C3  │
│  SHARED CACHE (L2)    │  CORE A4   │  CORE B4   │  CORE C4  │
└──────────────────────────────────────────────────────────────┘

Pipeline stages: Fetch → Decode → Execute → Memory → Writeback
```

-----

## Registers

### General-Purpose Registers (GPRs)

The Axiom 1100 has 16 general-purpose 64-bit registers. Each can be accessed at multiple widths using a suffix notation.

|64-bit|32-bit|16-bit|8-bit (low)|Conventional use                   |
|------|------|------|-----------|-----------------------------------|
|`RAX` |`EAX` |`AX`  |`AL`       |Accumulator / return value         |
|`RBX` |`EBX` |`BX`  |`BL`       |Base / callee-saved                |
|`RCX` |`ECX` |`CX`  |`CL`       |Counter (loop / shift)             |
|`RDX` |`EDX` |`DX`  |`DL`       |Data / second return value         |
|`RSI` |`ESI` |`SI`  |`SIL`      |Source index                       |
|`RDI` |`EDI` |`DI`  |`DIL`      |Destination index                  |
|`RSP` |`ESP` |`SP`  |`SPL`      |Stack pointer *(do not use freely)*|
|`RBP` |`EBP` |`BP`  |`BPL`      |Base pointer / frame pointer       |
|`R8`  |`R8D` |`R8W` |`R8B`      |General purpose                    |
|`R9`  |`R9D` |`R9W` |`R9B`      |General purpose                    |
|`R10` |`R10D`|`R10W`|`R10B`     |General purpose                    |
|`R11` |`R11D`|`R11W`|`R11B`     |General purpose                    |
|`R12` |`R12D`|`R12W`|`R12B`     |Callee-saved                       |
|`R13` |`R13D`|`R13W`|`R13B`     |Callee-saved                       |
|`R14` |`R14D`|`R14W`|`R14B`     |Callee-saved                       |
|`R15` |`R15D`|`R15W`|`R15B`     |Callee-saved                       |


> ⚠️ Writing to a 32-bit sub-register (e.g. `EAX`) **zero-extends** into the full 64-bit register. Writing to 8-bit or 16-bit sub-registers does **not** zero-extend — the upper bits are preserved.

-----

### Special-Purpose Registers

|Register|Width |Description                                                                                                                                                  |
|--------|------|-------------------------------------------------------------------------------------------------------------------------------------------------------------|
|`RIP`   |64-bit|Instruction pointer. Points to the next instruction to execute. Not directly writable; modified by `JMP`, `CALL`, `RET`.                                     |
|`RSP`   |64-bit|Stack pointer. Points to the top of the stack. Modified by `PUSH`, `POP`, `CALL`, `RET`.                                                                     |
|`RBP`   |64-bit|Base pointer. Conventionally used to anchor the current stack frame.                                                                                         |
|`RFLAGS`|64-bit|Flags register. Set automatically by arithmetic and logic instructions. Not directly writable by user code. See [Flags & Conditionals](#flags--conditionals).|

-----

### Segment Registers

Although the Axiom 1100 uses a flat memory model, segment registers are retained for compatibility and OS use.

|Register        |Description                                                      |
|----------------|-----------------------------------------------------------------|
|`CS`            |Code segment (read-only in user mode)                            |
|`DS`            |Data segment                                                     |
|`SS`            |Stack segment                                                    |
|`ES`, `FS`, `GS`|Extra segments; `FS`/`GS` used by the OS for thread-local storage|

-----

## Memory Model

The Axiom 1100 uses a **flat 64-bit address space**. All code, data, and stack share a single linear address space from `0x0000000000000000` to `0xFFFFFFFFFFFFFFFF`, subject to OS-level paging and protection.

### Conventional layout (user process)

```
High addresses  ┌─────────────────────┐
                │      Stack          │  grows downward ↓
                │         ↓           │
                │    (unmapped)        │
                │         ↑           │
                │       Heap          │  grows upward ↑
                │─────────────────────│
                │  BSS (uninit data)  │
                │─────────────────────│
                │  Data (init data)   │
                │─────────────────────│
Low addresses   │       Code          │
                └─────────────────────┘
```

### Alignment

|Access size|Required alignment|
|-----------|------------------|
|8-bit      |None              |
|16-bit     |2 bytes           |
|32-bit     |4 bytes           |
|64-bit     |8 bytes           |

Unaligned accesses are supported but incur a performance penalty.

-----

## Instruction Encoding

Instructions are variable-length, between 1 and 15 bytes. The general format is:

```
[Legacy Prefix] [Opcode] [ModRM] [SIB] [Displacement] [Immediate]
     0–1 bytes   1–3 bytes  0–1    0–1     0,1,2,4 bytes  0,1,2,4,8 bytes
```

- **Legacy Prefix:** Optional. Used for operand-size override, address-size override, etc.
- **Opcode:** 1–3 bytes identifying the instruction.
- **ModRM:** Encodes addressing mode and operand registers when present.
- **SIB (Scale-Index-Base):** Used for complex memory addressing when present.
- **Displacement:** Signed offset added to a base address.
- **Immediate:** Literal value encoded directly in the instruction.

-----

## Addressing Modes

The Axiom 1100 supports the following addressing modes:

|Mode                              |Syntax              |Description                                                                    |
|----------------------------------|--------------------|-------------------------------------------------------------------------------|
|Register                          |`RAX`               |Value is in a register                                                         |
|Immediate                         |`42`                |Literal value encoded in the instruction                                       |
|Direct memory                     |`[0x1000]`          |Value at a fixed memory address                                                |
|Register indirect                 |`[RAX]`             |Value at the address held in a register                                        |
|Base + displacement               |`[RAX + 8]`         |Value at register + signed offset                                              |
|Base + index                      |`[RAX + RBX]`       |Value at sum of two registers                                                  |
|Base + scaled index               |`[RAX + RBX*8]`     |Value at base + (index × scale); scale must be 1, 2, 4, or 8                   |
|Base + scaled index + displacement|`[RAX + RBX*4 + 16]`|Full SIB form                                                                  |
|RIP-relative                      |`[RIP + offset]`    |Address relative to the instruction pointer; used for position-independent code|

-----

## Data Transfer

-----

### `MOV` — Move

Copies a value from source to destination. The most fundamental instruction on the Axiom 1100.

```
MOV <dest>, <src>
```

Both operands must be the same width. `dest` may be a register or memory location. `src` may be a register, memory location, or immediate. Memory-to-memory moves are **not** permitted — use a register as an intermediate.

**Example:**

```asm
MOV RAX, 42          ; RAX = 42
MOV RBX, RAX         ; RBX = RAX
MOV [0x1000], RAX    ; store RAX into memory at address 0x1000
MOV RAX, [0x1000]    ; load from memory at 0x1000 into RAX
MOV RAX, [RBX + 8]   ; load from address RBX+8
```

-----

### `MOVZX` — Move with Zero-Extension

Copies a smaller value into a larger register, zero-filling the upper bits.

```
MOVZX <dest64/32>, <src8/16>
```

**Example:**

```asm
MOVZX RAX, AL        ; zero-extend AL into RAX
MOVZX EAX, BX        ; zero-extend BX into EAX (and implicitly RAX)
```

-----

### `MOVSX` — Move with Sign-Extension

Copies a smaller value into a larger register, sign-extending the upper bits.

```
MOVSX <dest>, <src>
```

**Example:**

```asm
MOVSX RAX, AL        ; sign-extend AL into RAX
MOVSX EAX, BX        ; sign-extend BX into EAX
```

-----

### `LEA` — Load Effective Address

Computes a memory address and stores it in a register **without** accessing memory. Useful for pointer arithmetic and offsetting.

```
LEA <dest>, [<address expression>]
```

**Example:**

```asm
LEA RAX, [RBX + RCX*8 + 16]   ; RAX = RBX + RCX*8 + 16 (no memory access)
LEA RDI, [RIP + myString]      ; RDI = address of myString (RIP-relative)
```

-----

### `XCHG` — Exchange

Atomically swaps the values of two registers or a register and a memory location.

```
XCHG <A>, <B>
```

**Example:**

```asm
XCHG RAX, RBX        ; swap RAX and RBX
```

-----

### `PUSH` — Push onto Stack

Decrements `RSP` by 8 and writes the operand to `[RSP]`.

```
PUSH <src>
```

**Example:**

```asm
PUSH RAX             ; RSP -= 8; [RSP] = RAX
PUSH 42              ; push immediate
```

-----

### `POP` — Pop from Stack

Reads the value at `[RSP]` into the destination and increments `RSP` by 8.

```
POP <dest>
```

**Example:**

```asm
POP RAX              ; RAX = [RSP]; RSP += 8
```

-----

## Arithmetic

Arithmetic instructions set flags in `RFLAGS`. See [Flags & Conditionals](#flags--conditionals).

-----

### `ADD` — Add

```
ADD <dest>, <src>
```

**Example:**

```asm
ADD RAX, RBX         ; RAX = RAX + RBX
ADD RAX, 10          ; RAX = RAX + 10
ADD [RBX], RAX       ; [RBX] = [RBX] + RAX
```

-----

### `SUB` — Subtract

```
SUB <dest>, <src>
```

**Example:**

```asm
SUB RAX, RBX         ; RAX = RAX - RBX
SUB RAX, 1           ; RAX = RAX - 1
```

-----

### `MUL` — Unsigned Multiply

Multiplies `RAX` by the operand. The 128-bit result is stored across `RDX:RAX` (high 64 bits in `RDX`, low 64 bits in `RAX`).

```
MUL <src>
```

**Example:**

```asm
MOV RAX, 6
MOV RBX, 7
MUL RBX              ; RDX:RAX = 6 * 7 = 42; RAX = 42, RDX = 0
```

-----

### `IMUL` — Signed Multiply

Two-operand form multiplies dest by src and stores the result in dest (truncated to 64 bits).

```
IMUL <dest>, <src>
IMUL <dest>, <src>, <imm>   ; dest = src * imm
```

**Example:**

```asm
IMUL RAX, RBX        ; RAX = RAX * RBX
IMUL RAX, RBX, 10    ; RAX = RBX * 10
```

-----

### `DIV` — Unsigned Divide

Divides the 128-bit value in `RDX:RAX` by the operand. Quotient → `RAX`, remainder → `RDX`.

> ⚠️ If the divisor is `0`, or the quotient overflows `RAX`, a divide fault is raised and the program is terminated.

```
DIV <src>
```

**Example:**

```asm
MOV RAX, 20
MOV RDX, 0           ; clear high bits
MOV RBX, 4
DIV RBX              ; RAX = 5, RDX = 0
```

-----

### `IDIV` — Signed Divide

Signed equivalent of `DIV`. Divides `RDX:RAX` by the operand. Quotient → `RAX`, remainder → `RDX`.

```
IDIV <src>
```

-----

### `INC` — Increment

Adds 1 to the operand. Does **not** affect the carry flag.

```
INC <dest>
```

**Example:**

```asm
INC RAX              ; RAX = RAX + 1
INC QWORD [RBX]      ; increment 64-bit value at address in RBX
```

-----

### `DEC` — Decrement

Subtracts 1 from the operand. Does **not** affect the carry flag.

```
DEC <dest>
```

**Example:**

```asm
DEC RCX              ; RCX = RCX - 1
```

-----

### `NEG` — Negate

Replaces the operand with its two’s complement negation. Equivalent to `SUB 0, dest`.

```
NEG <dest>
```

**Example:**

```asm
MOV RAX, 5
NEG RAX              ; RAX = -5
```

-----

### `CMP` — Compare

Performs `dest - src` and sets flags accordingly, **without** storing the result. Used before conditional jumps.

```
CMP <A>, <B>
```

**Example:**

```asm
CMP RAX, 10
JE  equal            ; jump if RAX == 10
JL  less             ; jump if RAX < 10
```

-----

## Logic & Bitwise

-----

### `AND` — Bitwise AND

```
AND <dest>, <src>
```

**Example:**

```asm
AND RAX, 0xFF        ; mask lower 8 bits; upper bits zeroed
AND RAX, RBX
```

-----

### `OR` — Bitwise OR

```
OR <dest>, <src>
```

**Example:**

```asm
OR RAX, 0x01         ; set bit 0
```

-----

### `XOR` — Bitwise XOR

```
XOR <dest>, <src>
```

> ℹ️ `XOR RAX, RAX` is the canonical idiom to zero a register — it is smaller and faster than `MOV RAX, 0`.

**Example:**

```asm
XOR RAX, RAX         ; RAX = 0
XOR RAX, RBX         ; RAX = RAX ^ RBX
```

-----

### `NOT` — Bitwise NOT

Inverts all bits of the operand.

```
NOT <dest>
```

**Example:**

```asm
NOT RAX              ; RAX = ~RAX
```

-----

### `TEST` — Bitwise Test

Performs `A AND B` and sets flags, **without** storing the result. Used before conditional jumps to test individual bits.

```
TEST <A>, <B>
```

**Example:**

```asm
TEST RAX, RAX        ; sets ZF if RAX == 0
JZ   zero            ; jump if zero

TEST RAX, 0x01       ; test bit 0
JNZ  odd             ; jump if bit 0 is set
```

-----

## Shift & Rotate

The shift count is either an immediate or the `CL` register (low byte of `RCX`).

-----

### `SHL` / `SAL` — Shift Left

Shifts bits left by count. Zeros fill from the right. Equivalent to multiplying by 2^count.

```
SHL <dest>, <count>
```

**Example:**

```asm
SHL RAX, 1           ; RAX *= 2
SHL RAX, CL          ; RAX <<= CL
```

-----

### `SHR` — Logical Shift Right

Shifts bits right by count. Zeros fill from the left. Equivalent to unsigned division by 2^count.

```
SHR <dest>, <count>
```

-----

### `SAR` — Arithmetic Shift Right

Shifts bits right by count. The sign bit fills from the left, preserving the sign. Equivalent to signed division by 2^count.

```
SAR <dest>, <count>
```

**Example:**

```asm
MOV RAX, -8
SAR RAX, 1           ; RAX = -4 (sign preserved)
```

-----

### `ROL` — Rotate Left

Rotates bits left by count. Bits shifted out of the MSB wrap to the LSB.

```
ROL <dest>, <count>
```

-----

### `ROR` — Rotate Right

Rotates bits right by count. Bits shifted out of the LSB wrap to the MSB.

```
ROR <dest>, <count>
```

-----

## Control Flow

-----

### `JMP` — Unconditional Jump

Transfers execution to a label or address unconditionally.

```
JMP <label>
JMP <reg>       ; indirect jump — jump to address in register
JMP [<mem>]     ; indirect jump — jump to address stored in memory
```

**Example:**

```asm
JMP loop_start
JMP RAX          ; jump to address in RAX
```

-----

### Conditional Jumps

All conditional jumps evaluate `RFLAGS` set by the preceding `CMP`, `TEST`, or arithmetic instruction.

|Mnemonic     |Condition                |Flags tested          |
|-------------|-------------------------|----------------------|
|`JE` / `JZ`  |Equal / Zero             |`ZF = 1`              |
|`JNE` / `JNZ`|Not equal / Not zero     |`ZF = 0`              |
|`JL` / `JNGE`|Less than (signed)       |`SF ≠ OF`             |
|`JLE` / `JNG`|Less or equal (signed)   |`ZF = 1` or `SF ≠ OF` |
|`JG` / `JNLE`|Greater than (signed)    |`ZF = 0` and `SF = OF`|
|`JGE` / `JNL`|Greater or equal (signed)|`SF = OF`             |
|`JB` / `JC`  |Below (unsigned)         |`CF = 1`              |
|`JBE`        |Below or equal (unsigned)|`CF = 1` or `ZF = 1`  |
|`JA`         |Above (unsigned)         |`CF = 0` and `ZF = 0` |
|`JAE`        |Above or equal (unsigned)|`CF = 0`              |
|`JS`         |Sign (negative)          |`SF = 1`              |
|`JNS`        |Not sign (positive)      |`SF = 0`              |
|`JO`         |Overflow                 |`OF = 1`              |
|`JNO`        |No overflow              |`OF = 0`              |

```
J<cc> <label>
```

**Example:**

```asm
MOV RCX, 5

.loop:
    DEC RCX
    CMP RCX, 0
    JNE .loop        ; repeat until RCX == 0
```

-----

### `LOOP` — Loop with Counter

Decrements `RCX` and jumps to the label if `RCX` is not zero.

```
LOOP <label>
```

**Example:**

```asm
MOV RCX, 5
.repeat:
    ; loop body here
    LOOP .repeat     ; RCX--; jump if RCX != 0
```

-----

## Stack Operations

The stack grows **downward**. `RSP` always points to the last value pushed (the top of the stack).

```
PUSH RAX    ; RSP -= 8; [RSP] = RAX
POP  RBX    ; RBX = [RSP]; RSP += 8
```

**Preserving registers across a block:**

```asm
PUSH RBX
PUSH R12
PUSH R13
; ... use RBX, R12, R13 freely ...
POP R13
POP R12
POP RBX     ; restore in reverse order
```

> ⚠️ Always maintain 16-byte stack alignment before a `CALL`. The `CALL` instruction itself pushes 8 bytes (the return address), so `RSP` must be 16-byte aligned **before** `CALL` is executed.

-----

## Procedures

The Axiom 1100 uses a hardware call stack. `CALL` pushes the return address and transfers control; `RET` pops it and returns.

### Calling Convention

|Role                            |Registers                                        |
|--------------------------------|-------------------------------------------------|
|Arguments (integer/pointer)     |`RDI`, `RSI`, `RDX`, `RCX`, `R8`, `R9` (in order)|
|Additional arguments            |Passed on the stack, right-to-left               |
|Return value                    |`RAX` (primary), `RDX` (secondary)               |
|Caller-saved (may be clobbered) |`RAX`, `RCX`, `RDX`, `RSI`, `RDI`, `R8`–`R11`    |
|Callee-saved (must be preserved)|`RBX`, `RBP`, `R12`–`R15`                        |

-----

### `CALL` — Call Procedure

Pushes `RIP` (the return address) onto the stack, then jumps to the target.

```
CALL <label>
CALL <reg>     ; indirect call
CALL [<mem>]   ; indirect call through memory
```

**Example:**

```asm
MOV RDI, 10    ; first argument
MOV RSI, 20    ; second argument
CALL add_nums  ; call the procedure
               ; result in RAX
```

-----

### `RET` — Return from Procedure

Pops the return address from the stack and jumps to it.

```
RET            ; return to caller
RET <imm>      ; return and pop <imm> additional bytes from stack
```

-----

### Standard Function Prologue & Epilogue

```asm
my_function:
    PUSH RBP           ; save caller's base pointer
    MOV  RBP, RSP      ; anchor the stack frame
    SUB  RSP, 32       ; allocate 32 bytes of local space

    ; --- function body ---

    MOV  RSP, RBP      ; restore stack pointer
    POP  RBP           ; restore caller's base pointer
    RET
```

**Full example — add two integers:**

```asm
; int add(int a, int b)
; a in RDI, b in RSI, result in RAX
add:
    PUSH RBP
    MOV  RBP, RSP

    MOV  RAX, RDI
    ADD  RAX, RSI      ; RAX = a + b

    MOV  RSP, RBP
    POP  RBP
    RET
```

-----

## String Operations

String instructions operate on memory buffers pointed to by `RSI` (source) and `RDI` (destination). They can be prefixed with `REP` to repeat for `RCX` iterations.

|Instruction  |Operation                                                           |
|-------------|--------------------------------------------------------------------|
|`MOVSB/W/D/Q`|Copy byte/word/dword/qword from `[RSI]` to `[RDI]`; advance pointers|
|`STOSB/W/D/Q`|Store `AL/AX/EAX/RAX` to `[RDI]`; advance `RDI`                     |
|`LODSB/W/D/Q`|Load from `[RSI]` into `AL/AX/EAX/RAX`; advance `RSI`               |
|`CMPSB/W/D/Q`|Compare `[RSI]` and `[RDI]`; advance both pointers; sets flags      |
|`SCASB/W/D/Q`|Compare `AL/AX/EAX/RAX` against `[RDI]`; advance `RDI`; sets flags  |

**Direction:** Controlled by the direction flag (`DF`). If `DF = 0` (default, set by `CLD`), pointers increment. If `DF = 1` (set by `STD`), pointers decrement.

**Example — copy 16 bytes:**

```asm
LEA RSI, [src_buffer]
LEA RDI, [dst_buffer]
MOV RCX, 16
CLD                    ; ensure forward direction
REP MOVSB              ; copy RCX bytes from RSI to RDI
```

**Example — zero out a buffer:**

```asm
LEA RDI, [buffer]
XOR RAX, RAX           ; value to store = 0
MOV RCX, 64            ; 64 iterations
REP STOSQ              ; store RAX (0) to [RDI] 64 times (512 bytes)
```

-----

## I/O

The Axiom 1100 uses **port-mapped I/O**. Devices are assigned port numbers and communicate via `IN` and `OUT` instructions. Port access in user mode requires OS privilege.

-----

### `IN` — Read from I/O Port

Reads a value from an I/O port into `AL`, `AX`, or `EAX`.

```
IN AL,  DX     ; read byte from port in DX
IN AX,  DX     ; read word
IN EAX, DX     ; read dword
IN AL,  <imm8> ; read from fixed port (0–255)
```

-----

### `OUT` — Write to I/O Port

Writes a value from `AL`, `AX`, or `EAX` to an I/O port.

```
OUT DX,  AL    ; write byte to port in DX
OUT DX,  AX    ; write word
OUT DX,  EAX   ; write dword
OUT <imm8>, AL ; write to fixed port (0–255)
```

**Example — write character to serial port 0x3F8:**

```asm
MOV AL, 'H'
OUT 0x3F8, AL
```

-----

## Flags & Conditionals

The `RFLAGS` register holds status bits set automatically by arithmetic and logic instructions.

|Flag|Bit|Name            |Set when…                                               |
|----|---|----------------|--------------------------------------------------------|
|`CF`|0  |Carry           |Unsigned overflow or borrow                             |
|`PF`|2  |Parity          |Low byte of result has even number of set bits          |
|`AF`|4  |Auxiliary carry |Carry out of bit 3 (BCD arithmetic)                     |
|`ZF`|6  |Zero            |Result is zero                                          |
|`SF`|7  |Sign            |Result is negative (MSB is 1)                           |
|`TF`|8  |Trap            |Single-step debug mode                                  |
|`IF`|9  |Interrupt enable|Hardware interrupts are enabled                         |
|`DF`|10 |Direction       |String ops decrement (if 1) or increment (if 0) pointers|
|`OF`|11 |Overflow        |Signed overflow                                         |

### Manually setting flags

|Instruction|Effect                                              |
|-----------|----------------------------------------------------|
|`CLD`      |Clear direction flag (`DF = 0`)                     |
|`STD`      |Set direction flag (`DF = 1`)                       |
|`CLI`      |Clear interrupt flag (`IF = 0`) — requires privilege|
|`STI`      |Set interrupt flag (`IF = 1`) — requires privilege  |
|`CLC`      |Clear carry flag (`CF = 0`)                         |
|`STC`      |Set carry flag (`CF = 1`)                           |
|`CMC`      |Complement (toggle) carry flag                      |

### `SETcc` — Set Byte on Condition

Stores `1` or `0` into an 8-bit register based on a flag condition. Useful for branchless conditionals.

```
SETcc <dest8>
```

**Example:**

```asm
CMP RAX, RBX
SETE  AL         ; AL = 1 if RAX == RBX, else 0
SETL  BL         ; BL = 1 if RAX < RBX (signed), else 0
SETNE CL         ; CL = 1 if RAX != RBX, else 0
```

### `CMOVcc` — Conditional Move

Moves src to dest only if the condition is true. No branch taken.

```
CMOVcc <dest>, <src>
```

**Example:**

```asm
CMP RAX, RBX
CMOVG RAX, RBX   ; RAX = RBX only if RAX > RBX (i.e. keep the greater value)
```

-----

## System Instructions

-----

### `NOP` — No Operation

Does nothing for one cycle. Used for alignment padding, timing, or as a placeholder.

```
NOP
```

-----

### `HLT` — Halt

Suspends execution until the next hardware interrupt. Requires privilege level 0 (kernel mode).

```
HLT
```

-----

### `INT` — Software Interrupt

Triggers a software interrupt with the given vector number. Used to invoke OS system calls.

```
INT <vector>
```

**Example:**

```asm
INT 0x80         ; invoke OS system call (vector 0x80)
```

-----

### `SYSCALL` — Fast System Call

A faster alternative to `INT 0x80` for invoking OS services. Transfers control to the OS kernel at the address stored in the `SYSCALL_TARGET` model-specific register.

```
SYSCALL
```

Calling convention for `SYSCALL`:

|Role              |Register|
|------------------|--------|
|System call number|`RAX`   |
|Arg 1             |`RDI`   |
|Arg 2             |`RSI`   |
|Arg 3             |`RDX`   |
|Arg 4             |`R10`   |
|Arg 5             |`R8`    |
|Arg 6             |`R9`    |
|Return value      |`RAX`   |

**Example — write “Hi” to stdout:**

```asm
section .data
    msg db "Hi", 0x0A   ; "Hi\n"

section .text
    MOV RAX, 1          ; syscall: write
    MOV RDI, 1          ; fd: stdout
    LEA RSI, [msg]      ; buffer address
    MOV RDX, 3          ; length
    SYSCALL
```

-----

### `CPUID` — CPU Identification

Returns processor information. Input: feature query code in `EAX` (and optionally `ECX`). Output: feature flags across `EAX`, `EBX`, `ECX`, `EDX`.

```
CPUID
```

-----

### `RDTSC` — Read Time-Stamp Counter

Reads the processor’s cycle counter into `EDX:EAX` (high 32 bits in `EDX`, low 32 bits in `EAX`). Useful for benchmarking.

```
RDTSC
```

-----

*End of Intol Axiom 1100 Opcode Reference*