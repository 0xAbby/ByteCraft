
# ByteCraft

A tiny toy CPU + assembler + virtual machine written in modern C++20.

- **VM registers (32-bit):** r1..r8, IP, rF, rS
- **Syscalls:** ID in `r1`, args in `r2+`, return in `r1`
- **Assembler:** `_main` (code), `_data` (DB buffers)
- **Binary format:** `"BVM\0"` + header + code + data

## Project layout
```bash
  bytecraft/
  ├─ CMakeLists.txt
  ├─ include/bytecraft/
  │ ├─ isa.hpp # ISA enums and constants
  │ ├─ util.hpp # small helpers (LE read/write, trim)
  │ ├─ bytecode.hpp # BVM load/save
  │ ├─ asm.hpp # assembler interface
  │ └─ vm.hpp # VM interface
  └─ src/
  ├─ bytecode.cpp # BVM load/save implementation
  ├─ asm.cpp # two-pass assembler
  ├─ vm.cpp # virtual machine
  └─ main.cpp # CLI: asm/run
```

## Build

```bash
cd build
cmake .. && cmake --build . -j
```


## Usage

```bash
./bytecraft asm ../test.asm -o bin.bvm
./bytecraft run program.bvm
```

# ByteCraft Architecture

## Registers

- General purpose: `r1`..`r8` (32-bit)
- Special: `IP` (instruction pointer), `rF` (flags), `rS` (sign mode bit)

### Flags (`rF`, low 8 bits)

- `EQ` (bit 0): last compare equal
- `GT` (bit 1): lhs > rhs
- `LT` (bit 2): lhs < rhs
- `TEST_TRUE` (bit 3): last conditional branch was taken
- `BAD_INSTR` (bit 4): illegal/unknown instruction
- `IP_OOB` (bit 5): IP outside code region
- `READ_OOB` (bit 6): invalid read
- `WRITE_OOB` (bit 7): invalid write

### Sign mode (`rS`)

- Bit 0: `1` = signed compares, `0` = unsigned compares

## Instruction set

- Data: `mov`
- ALU: `add`, `sub`, `xor`
- Compare: `cmp`
- Control flow: `jmp`, `jeq`, `jneq`, `jla` (greater), `jle` (less-or-eq)
- Syscall: `syscall`
- Misc: `nop`

### Operands

- Register: `r1..r8`, `IP`, `rF`, `rS`
- Immediate: decimal or `0xHEX`
- Memory: `[symbol]` or `[abs_address]` (32-bit absolute)

### Encoding (bytecode)

[op:1][mode:1][operands...]

- mode:
  -  high nibble = dst operand type
  -  low nibble = src operand type
  -  operand enc:
      - REG -> [reg_index:1]
      - IMM -> [u32_le:4]
      - MEM -> [u32_le:4] (absolute address)

- Two-operand ops (`mov`, `add`, `sub`, `xor`, `cmp`): `dst, src`
- Branches use a single source (IMM or REG)
- `syscall`/`nop`: opcode only

## Assembly format

Sections:

- `_main:` — instructions and labels
- `_data:` — buffers: `DB name[size]` (zero-initialized)

Labels resolve to code offsets. Data symbols resolve to absolute addresses at `code_size + data_offset`.

Example:

```asm
_main:
  start:
  mov r1, 0x1234
  jmp start

_data:
  DB buffer[64]
```

## Syscalls

Calling convention:

- r1 = syscall ID

- Args in r2..rN

- Return value (if any) in r1

- Current IDs: exit(0), write(1), read(2), open(3 stub)

```
Magic:  "BVM\0"    (4 bytes)
Header: entry_point:u32_le
        code_section_size:u32_le
        data_section_size:u32_le
Body:   code bytes
        data bytes
```
The VM loads [code][data] into a single memory image. IP starts at entry_point (typically 0).

## VM behavior

- Fetch/decode/execute loop.

- Bounds checks on code fetch and data read/write.

- Flags updated by cmp and branches set TEST_TRUE when taken.

- Dumps register state after each instruction (for tracing).

## Limitations / TODO

- _data only supports zero-initialized DB name[size] (no initializers yet).

- No relative addressing ([symbol + rX]) yet.

- No call/ret stack semantics or stack pointer register.