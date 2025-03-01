//  asm.hpp:
//
//

#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "bytecode.hpp"
#include "isa.hpp"

namespace bc {

/**
 * @brief Two-pass assembler for ByteCraft assembly.
 *
 * Supports sections:
 *   _main: instructions and labels.
 *   _data: DB name[size] declarations (zero-initialized).
 *
 * Instructions:
 *   mov, add, sub, xor, cmp, jmp, jeq, jneq, jla, jle, syscall, nop.
 *
 * Operands:
 *   - Register: r1..r8, IP, rF, rS.
 *   - Immediate: decimal or 0xHEX.
 *   - Memory: [symbol] or [address].
 *
 * Encoding:
 *   [op:1][mode:1][operands...]
 *   mode: high nibble = dst type, low nibble = src type.
 *   REG enc: [reg:1], IMM enc: [u32:4], MEM enc: [addr:u32:4].
 *   Branches use only a single source operand (IMM or REG).
 */
class Assembler {
 public:
  /**
   * @brief Assemble from a source string into a Module.
   *
   * @param source_text    Input assembly text.
   * @param out_module     Output module (entry_point, code_section, data_section).
   * @param error_message  Human-readable error message on failure.
   * @return true on success, false on error.
   */
  bool assemble_string(const std::string& source_text,
                       Module& out_module,
                       std::string& error_message);

  /**
   * @brief Assemble from a file into a Module.
   *
   * @param path           Input file path.
   * @param out_module     Output module (entry_point, code_section, data_section).
   * @param error_message  Human-readable error message on failure.
   * @return true on success, false on error.
   */
  bool assemble_file(const std::string& path,
                     Module& out_module,
                     std::string& error_message);
};

}  // namespace bc
