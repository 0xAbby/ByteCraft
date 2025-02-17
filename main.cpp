// main.cpp:
//  A toy project of a CPU architecture called ByteCraft.
//
// Basic implementation:
// - Registers: 
//      r1..r8, IP, rF, rS (all 32-bit; rS uses 1-bit)

// - Flags (using rF register low 8 bits):
//      bit0 EQ, 
//      bit1 GT, 
//      bit2 LT, 
//      bit3 TEST_TRUE, 
//      bit4 BAD_INSTR,
//      bit5 IP_OOB, 
//      bit6 READ_OOB, 
//      bit7 WRITE_OOB

// - Syscall id in rF high byte (bits 24..31) to avoid stepping on status bits.
// - Instructions: 
//          mov, add, sub, xor, cmp, jmp, jeq, jneq, jla, jle, syscall

// - Memory bounds checks set fault bits in rF

// - Assembler source file with two sections: _main (mandatory) and _data (optional)
//   - Labels in _main (e.g., "loop:")
//   - DB name[size] in _data (zero-initialized)
//   - Symbols usable as immediates or as [symbol] memory operands
//   - Numeric literals: decimal or 0x.. hex

// - Bytecode format:
//   [ 'B','V','M','\0' ][entry:u32][codeSize:u32][dataSize:u32][code...][data...]

//
// Usage:
//   bytecraft asm input.asm -o output.bvm
//   bytecraft run program.bvm

//
// NOTE: This is a compact implementation meant to be extended.

#include <bits/stdc++.h>
using namespace std;


int main(int argc, char** argv){

  return 0;
}
