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

#include "bytecraft/asm.hpp"
#include "bytecraft/bytecode.hpp"
#include "bytecraft/vm.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace bc;

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage:\n"
              << "  " << argv[0] << " asm <input.asm> -o <output.bvm>\n"
              << "  " << argv[0] << " run <program.bvm>\n";
    return 1;
  }

  std::string command = argv[1];

  if (command == "asm") {
    if (argc < 5) {
      std::cerr << "Missing arguments. Example: " << argv[0] << " asm prog.asm -o prog.bvm\n";
      return 1;
    }

    std::string input_path = argv[2];
    std::string output_path;

    for (int i = 3; i < argc; i += 1) {
      std::string arg = argv[i];
      if (arg == "-o" && (i + 1) < argc) {
        output_path = argv[i + 1];
        i += 1;
      }
    }

    if (output_path.empty()) {
      std::cerr << "Output file not specified (-o)\n";
      return 1;
    }

    Assembler assembler;
    Module module;
    std::string error_message;

    if (!assembler.assemble_file(input_path, module, error_message)) {
      std::cerr << "Assembly failed: " << error_message << "\n";
      return 1;
    }

    if (!save_bvm(output_path, module, error_message)) {
      std::cerr << "Save failed: " << error_message << "\n";
      return 1;
    }

    std::cout << "Assembled OK: entry=" << module.entry_point
              << " code=" << module.code_section.size() << "B"
              << " data=" << module.data_section.size() << "B\n";
    return 0;
  } else if (command == "run") {
    if (argc < 3) {
      std::cerr << "Missing program file\n";
      return 1;
    }

    Module module;
    std::string error_message;

    if (!load_bvm(argv[2], module, error_message)) {
      std::cerr << "Load failed: " << error_message << "\n";
      return 1;
    }

    std::vector<std::uint8_t> memory_image;
    memory_image.reserve(module.code_section.size() + module.data_section.size());
    memory_image.insert(memory_image.end(), module.code_section.begin(), module.code_section.end());
    memory_image.insert(memory_image.end(), module.data_section.begin(), module.data_section.end());

    VM vm(std::move(memory_image),
          module.entry_point,
          static_cast<std::uint32_t>(module.code_section.size()),
          static_cast<std::uint32_t>(module.data_section.size()));
    vm.run();

    return 0;
  } else {
    std::cerr << "Unknown command: " << command << "\n";
    return 1;
  }
}
