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
#include <iostream>
#include <string>
#include <vector>

static void print_usage() {
  std::cerr << "Usage:\n"
            << "  bytecraft asm <input.asm> -o <output.bvm>\n"
            << "  bytecraft run [--quiet] <program.bvm>\n";
}


int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage();
    return 1;
  }

  std::string command = argv[1];

  if (command == "asm") {
    if (argc < 5) {
      std::cerr << "error: missing arguments for 'asm'\n";
      print_usage();
      return 1;
    }

    std::string input_path;
    std::string output_path;

    input_path = argv[2];

    for (int i = 3; i < argc; i += 1) {
      std::string arg = argv[i];
      if (arg == "-o" && (i + 1) < argc) {
        output_path = argv[i + 1];
        i += 1;
        continue;
      }
    }

    if (output_path.empty()) {
      std::cerr << "error: output file not specified (-o)\n";
      return 1;
    }

    bc::Assembler assembler;
    bc::Module module;
    std::string error_message;

    bool ok_asm = assembler.assemble_file(input_path, module, error_message);
    if (!ok_asm) {
      std::cerr << "Assembly failed: " << error_message << "\n";
      return 1;
    }

    bool ok_save = bc::save_bvm(output_path, module, error_message);
    if (!ok_save) {
      std::cerr << "Save failed: " << error_message << "\n";
      return 1;
    }

    std::cout << "Assembled OK: entry=" << module.entry_point
              << " code=" << module.code_section.size() << "B"
              << " data=" << module.data_section.size() << "B\n";
    return 0;
  }

  if (command == "run") {
    bool quiet = false;
    std::string program_path;

    for (int i = 2; i < argc; i += 1) {
      std::string arg = argv[i];
      if (arg == "--quiet") {
        quiet = true;
        continue;
      }
      if (program_path.empty() && !arg.empty() && arg[0] != '-') {
        program_path = arg;
        continue;
      }
    }

    if (program_path.empty()) {
      std::cerr << "error: missing program file for 'run'\n";
      print_usage();
      return 1;
    }

    bc::Module module;
    std::string error_message;

    bool ok_load = bc::load_bvm(program_path, module, error_message);
    if (!ok_load) {
      std::cerr << "Load failed: " << error_message << "\n";
      return 1;
    }

    std::vector<std::uint8_t> memory_image;
    memory_image.reserve(module.code_section.size() + module.data_section.size());
    memory_image.insert(memory_image.end(), module.code_section.begin(), module.code_section.end());
    memory_image.insert(memory_image.end(), module.data_section.begin(), module.data_section.end());

    bc::VM vm(std::move(memory_image),
              module.entry_point,
              static_cast<std::uint32_t>(module.code_section.size()),
              static_cast<std::uint32_t>(module.data_section.size()));

    if (quiet) {
      vm.set_tracing(false);
    }

    vm.run();
    return 0;
  }

  std::cerr << "error: unknown command '" << command << "'\n";
  print_usage();
  return 1;
}
