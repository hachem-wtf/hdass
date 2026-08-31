# hdass language reference

hdass emits NASM for x86-64; fasm and masm are planned. The compiler output itself isn't tied to an OS, but the examples and toolchain here target Linux (Linux syscall numbers, `nasm -f elf64`, `ld`). Pipeline: `lex → parse → analyze → emit`.

## A first program

```hdass
[entry: main]

const SYS_WRITE = 1
const SYS_EXIT = 60
const STDOUT = 1

data message = "type shi.\n"

proc main
{
    rax = SYS_WRITE
    rdi = STDOUT
    rsi = message
    rdx = message.len
    syscall

    rax = SYS_EXIT
    rdi = 0
    syscall
}
```

Writes `type shi.` to stdout and exits.

## Comments

```hdass
rax = 1        // line
/* block */
```

## Directives

Top-level `[key: value]`, configuring the whole program.

| Directive | Meaning |
| --- | --- |
| `[bits: 64]` / `[bits: 32]` | Target bitness. Default 64. |
| `[entry: NAME]` | Makes procedure `NAME` the entry point. |
| `[enable: NAME]` | Turns on an [extension](#extensions). |

Unknown keys, a `bits` value other than 32/64, and unknown extensions are errors.

## Constants and data

`const` names a constant integer expression — integer literals, character literals, other constants, and `+` `-` `*` `/`. Integers are decimal, `0x` hex, or `0b` binary (these forms work anywhere an integer does). `data` puts a string in `.data`; the name is its address and `.len` is its length in bytes.

```hdass
const STDOUT = 1
const MASK = 0xFF
const AREA = 8 * 6             // 48
data message = "type shi.\n"   // message -> address, message.len -> 10
```

## Enums and structs

Both describe compile-time values reached with `Name.member`, which folds to an integer.

`enum` names a set of constants numbered from 0:

```hdass
enum Status
{
    Ok,      // 0
    Warn,    // 1
    Fail     // 2
}

rax = Status.Fail    // mov rax, 2
```

`struct` describes a packed memory layout (no padding). Fields are `name` or `name: size`, where size defaults to `qword`. `Name.field` is the field's byte offset, and `Name.size` is the total size.

```hdass
struct Point
{
    x           // qword, offset 0
    y           // qword, offset 8
    flag: byte  //        offset 16
}

rsi += Point.y       // add rsi, 8
rax = Point.size     // mov rax, 17
```

A struct is layout only — it allocates nothing. Pair it with a `stack` buffer sized by `Name.size` and pointer arithmetic (see [examples/records.hdass](../examples/records.hdass)).

## Procedures

`proc` groups a body. Parameters name registers — `value` below is `rdi`.

```hdass
proc print_number(value: rdi)
{
    rax = value
}
```

Each procedure ends with `ret`, except the entry point. `[entry: NAME]` exports `NAME` with `global` and drops its `ret`, so it must end the program itself (an exit syscall). Link with `ld -e NAME`.

## Registers

Written by their architecture names — `rax`–`rdi`, `rbp`, `rsp`, `r8`–`r15` — and their sub-registers (`al`, `ax`, `eax`, `dl`, …), which imply a store's size. The [`logical_registers`](#extensions) extension adds `r1`–`r14`.

## Statements

```hdass
rax = SYS_WRITE     // mov
rcx -= 1            // += -= *= /=  ->  add sub imul idiv
rax = rbx * rcx     // + - * / in a value; / and /= use rax:rdx (see Gotchas)
rdx = buffer + 31   // address math
loop:               // label
goto loop
if rcx != 0         // == != < <= > >= ; runs the next statement only
    goto loop
syscall
print_number(r12)   // call; args go into the callee's parameter registers
stack buf[Point.size] // stack buffer (size is any constant); buf is its base address
```

## Dereference (`^`)

`^reg` is the memory at the address in `reg` — NASM's `[reg]`. On the left of `=` it stores there. The store width comes from the value operand, so a sized sub-register picks the size:

```hdass
^rsi = rdx          // mov [rsi], rdx    (qword)
^rsi = dl           // mov [rsi], dl     (byte)
^rsi = eax          // mov [rsi], eax    (dword)
```

A leading size keyword sets the width explicitly. It down-converts a full register to the matching sub-register, and gives an immediate a width NASM would otherwise reject:

```hdass
^byte  rsi = rdx    // mov byte [rsi], dl      (rdx -> its low byte)
^word  rsi = rax    // mov word [rsi], ax
^dword rsi = r12    // mov dword [rsi], r12d
^byte  rsi = '0'    // mov byte [rsi], '0'
^byte  rsi = 10     // mov byte [rsi], 10
```

`^reg` is also a value — it loads from that address. A size keyword loads a narrower value and zero-extends it into the target:

```hdass
rax = ^rsi          // mov rax, [rsi]
rbx = ^byte rsi     // movzx rbx, byte [rsi]
rcx = ^dword rsi    // mov ecx, [rsi]        (32-bit load zero-extends)
rdx = ^rsi + 4      // load, then add 4
```

## Expressions

Assignment values and `if` operands: registers, integers, chars (`'0'`), constants, data names, member access (`data.len`), and `+` `-` `*` `/`. Operators are left-associative and each right-hand operand must be a single term, so `a * b + c` works but `a + b * c` (a nested right operand) doesn't yet.

## Extensions

### logical_registers

Uniform names for the general-purpose registers, so you don't juggle the irregular `rax`/`rbx`/`rsi`/… spellings. `r1`–`r14` map to:

| `r1` | `r2` | `r3` | `r4` | `r5` | `r6` | `r7` | `r8` | `r9` | `r10` | `r11` | `r12` | `r13` | `r14` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| rax | rbx | rcx | rdx | rsi | rdi | r8 | r9 | r10 | r11 | r12 | r13 | r14 | r15 |

`rsp`/`rbp` and the instruction pointer keep their own names.

```hdass
r1 = 5              // mov rax, 5
r4 = r10            // mov rdx, r11
r6 += r1            // add rdi, rax
```

A `.8`/`.16`/`.32`/`.64` suffix selects the width, mapping to the sub-register:

```hdass
r1.8                // al
r1.16               // ax
r1.32               // eax
r1.64               // rax
^byte rsi = r4      // mov byte [rsi], dl   (r4 -> rdx -> dl)
```

Arch `r8`–`r15` share the `rN` spelling, so with the extension on a bare `r8` is the *logical* register (which is arch `r9`). Reach arch `r8`–`r15` through logical `r7`–`r14`. Architecture names like `rax` and `rsi` still work everywhere.

## Floating point

Floating-point values live in the SSE registers `xmm0`–`xmm15` (double precision). Float literals like `3.14` are placed in `.data` and loaded for you.

```hdass
xmm0 = 3.5          // movsd from a .data slot
xmm0 *= xmm1        // += -= *= /=  ->  addsd subsd mulsd divsd
```

An `=` between a float register and a general-purpose register converts:

```hdass
xmm0 = rax          // int -> float          (cvtsi2sd)
rbx = xmm0          // float -> int, truncating (cvttsd2si)
```

`^` loads and stores floats too, so float state can live in memory (a `stack` buffer or struct):

```hdass
^rsi = xmm0         // movsd [rsi], xmm0
xmm1 = ^rsi         // movsd xmm1, [rsi]
```

`if` compares floats too, when the left side is an `xmm` register (`ucomisd`):

```hdass
if xmm0 > 4.0
    goto escaped
```

See [examples/mandelbrot.hdass](../examples/mandelbrot.hdass) for a float program. Not yet supported: mixing floats and ints in one expression, and printing floats.

## Building a program

```bash
hdass program.hdass -o program.asm
nasm -f elf64 program.asm -o program.o
ld -e main program.o -o program
./program
```

The [README](../README.md) has a Docker setup with these tools.

## Gotchas

- **Clobbering is yours.** `syscall` trashes `rcx`/`r11`; a callee trashes what it touches. Nothing is saved for you — `examples/fibonacci.hdass` keeps its counter in `r15` for this reason.
- **Widths must match.** `rax = r1.8` becomes `mov rax, al`, which won't assemble.
- **Division uses `rax:rdx`.** `/` and `/=` go through `idiv`, so they clobber `rax` and `rdx` regardless of the target, and the divisor can't be `rax`, `rdx`, or an immediate — put it in another register first.
- **The entry procedure has no `ret`** — end it with an exit syscall.