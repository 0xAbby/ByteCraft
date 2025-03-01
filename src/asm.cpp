//  asm.cpp:
//    Implementation of handling and interperting isntructions.
//

#include "bytecraft/asm.hpp"
#include "bytecraft/util.hpp"
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>


namespace bc {

namespace {

/**
 * @brief Logical section kinds used during assembly.
 */
enum class Section {
  NONE,
  MAIN,
  DATA
};

/**
 * @brief A preprocessed source line with line number.
 */
struct SourceLine {
  std::size_t line_number = 0;
  std::string text;
};

/**
 * @brief Check if a string is empty or whitespace only.
 *
 * @param s  Input string to test.
 * @return true if @p s has no non-space characters, false otherwise.
 */
static bool is_blank(const std::string& s) {
  for (char ch : s) {
    if (!std::isspace(static_cast<unsigned char>(ch))) {
      return false;
    }
  }
  return true;
}

/**
 * @brief Convert a string to lowercase in a copy.
 *
 * @param s  Input string to convert.
 * @return Lowercased copy of @p s.
 */
static std::string to_lower(std::string s) {
  for (char& ch : s) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return s;
}

/**
 * @brief Split a comma-separated list into trimmed tokens.
 *
 * Note: does not support quotes or escapes (simple CSV).
 *
 * @param s  Input string with comma-separated tokens.
 * @return Vector of trimmed tokens.
 */
static std::vector<std::string> split_csv(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (char ch : s) {
    if (ch == ',') {
      out.push_back(trim(cur));
      cur.clear();
    } else {
      cur.push_back(ch);
    }
  }
  if (!cur.empty()) {
    out.push_back(trim(cur));
  }
  return out;
}

/**
 * @brief Parse decimal or hex (0x...) string to 32-bit unsigned integer.
 *
 * @param token       Input numeric token.
 * @param out_value   Output parsed value if successful.
 * @return true on success, false on parse error.
 */
static bool parse_number(const std::string& token, std::uint32_t& out_value) {
  std::string s = trim(token);
  if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    char* end_ptr = nullptr;
    unsigned long v = std::strtoul(s.c_str(), &end_ptr, 16);
    if (end_ptr && *end_ptr == '\0') {
      out_value = static_cast<std::uint32_t>(v);
      return true;
    }
    return false;
  }
  char* end_ptr = nullptr;
  long long v = std::strtoll(s.c_str(), &end_ptr, 10);
  if (end_ptr && *end_ptr == '\0') {
    out_value = static_cast<std::uint32_t>(v);
    return true;
  }
  return false;
}

/**
 * @brief Determine if token denotes a register and return its enum index.
 *
 * Accepts r1..r8, ip, rf, rs (case-insensitive).
 *
 * @param token    Candidate token.
 * @param out_reg  Output register index if recognized.
 * @return true if token is a register, false otherwise.
 */
static bool is_register_token(const std::string& token, std::uint8_t& out_reg) {
  std::string s = to_lower(trim(token));
  if (s == "ip") {
    out_reg = IP;
    return true;
  }
  if (s == "rf") {
    out_reg = RF;
    return true;
  }
  if (s == "rs") {
    out_reg = RS;
    return true;
  }
  if (s.size() == 2 && s[0] == 'r') {
    char c = s[1];
    if (c >= '1' && c <= '8') {
      out_reg = static_cast<std::uint8_t>(R1 + (c - '1'));
      return true;
    }
  }
  return false;
}

/**
 * @brief Check for [ ... ] memory operand and extract inner token.
 *
 * @param token   Candidate token.
 * @param inner   Output inner text without brackets if recognized.
 * @return true if token is a bracketed memory operand, false otherwise.
 */
static bool is_mem_bracket(const std::string& token, std::string& inner) {
  std::string s = trim(token);
  if (s.size() >= 2 && s.front() == '[' && s.back() == ']') {
    inner = trim(s.substr(1, s.size() - 2));
    return true;
  }
  return false;
}

/**
 * @brief Parse an opcode mnemonic into an Op enum value.
 *
 * @param token  Mnemonic token (case-insensitive).
 * @return Parsed Op, or 255 cast if unknown.
 */
static Op parse_op(const std::string& token) {
  std::string s = to_lower(trim(token));
  if (s == "mov") {
    return OP_MOV;
  }
  if (s == "add") {
    return OP_ADD;
  }
  if (s == "sub") {
    return OP_SUB;
  }
  if (s == "xor") {
    return OP_XOR;
  }
  if (s == "cmp") {
    return OP_CMP;
  }
  if (s == "jmp") {
    return OP_JMP;
  }
  if (s == "jeq") {
    return OP_JEQ;
  }
  if (s == "jneq") {
    return OP_JNEQ;
  }
  if (s == "jla") {
    return OP_JLA;
  }
  if (s == "jle") {
    return OP_JLE;
  }
  if (s == "syscall") {
    return OP_SYSCALL;
  }
  if (s == "nop") {
    return OP_NOP;
  }
  return static_cast<Op>(255);
}

/**
 * @brief Return encoded byte size of a single operand by type.
 *
 * @param operand_type  One of OT_REG, OT_IMM, OT_MEM.
 * @return Size in bytes of the encoded operand.
 */
static std::size_t encoded_operand_size(std::uint8_t operand_type) {
  if (operand_type == OT_REG) {
    return 1;
  }
  if (operand_type == OT_IMM) {
    return 4;
  }
  if (operand_type == OT_MEM) {
    return 4;
  }
  return 0;
}

/**
 * @brief Compute the encoded size of an instruction given operand kinds.
 *
 * @param op        Opcode.
 * @param dst_type  Destination operand type nibble.
 * @param src_type  Source operand type nibble.
 * @return Total encoded size in bytes.
 */
static std::size_t encoded_size(Op op, std::uint8_t dst_type, std::uint8_t src_type) {
  switch (op) {
    case OP_NOP:
      return 1;
    case OP_SYSCALL:
      return 1;
    case OP_JMP:
    case OP_JEQ:
    case OP_JNEQ:
    case OP_JLA:
    case OP_JLE:
      return 1 + 1 + encoded_operand_size(src_type);
    case OP_MOV:
    case OP_ADD:
    case OP_SUB:
    case OP_XOR:
    case OP_CMP:
      return 1 + 1 + encoded_operand_size(dst_type) + encoded_operand_size(src_type);
    default:
      return 1;
  }
}

}  // namespace

/**
 * @brief Assemble from a source string into a Module.
 *
 * Performs a two-pass assembly:
 *   1) Parse and size to build symbol tables and compute layout.
 *   2) Encode instructions and resolve symbols.
 *
 * Supports sections _main and _data, labels, and DB name[size] in _data.
 *
 * @param source_text    Input assembly text.
 * @param out_module     Output module (entry_point, code_section, data_section).
 * @param error_message  Populated with a human-readable error on failure.
 * @return true on success, false on error.
 */
bool Assembler::assemble_string(const std::string& source_text,
                                Module& out_module,
                                std::string& error_message) {
  std::vector<SourceLine> lines;
  {
    std::istringstream iss(source_text);
    std::string raw;
    std::size_t line_no = 0;
    while (std::getline(iss, raw)) {
      line_no += 1;
      std::string no_comment = raw;
      std::size_t pos = no_comment.find_first_of(";#");
      if (pos != std::string::npos) {
        no_comment = no_comment.substr(0, pos);
      }
      no_comment = trim(no_comment);
      if (is_blank(no_comment)) {
        continue;
      }
      lines.push_back({line_no, no_comment});
    }
  }

  std::unordered_map<std::string, std::uint32_t> code_symbols;
  std::unordered_map<std::string, std::uint32_t> data_symbols;
  std::unordered_map<std::string, std::uint32_t> data_sizes;

  std::vector<std::uint8_t> code_buffer;
  std::vector<std::uint8_t> data_buffer;

  Section current_section = Section::NONE;

  std::uint32_t code_pc = 0;
  std::vector<std::pair<std::string, std::uint32_t>> data_decls;

  for (const auto& line : lines) {
    const std::string& s = line.text;

    if (s == "_main:") {
      current_section = Section::MAIN;
      continue;
    }
    if (s == "_data:") {
      current_section = Section::DATA;
      continue;
    }

    if (current_section == Section::MAIN) {
      if (!s.empty() && s.back() == ':') {
        std::string label = trim(s.substr(0, s.size() - 1));
        if (label.empty()) {
          error_message = "empty label at line " + std::to_string(line.line_number);
          return false;
        }
        if (code_symbols.count(label) != 0u) {
          error_message = "duplicate label '" + label + "' at line " + std::to_string(line.line_number);
          return false;
        }
        code_symbols[label] = code_pc;
        continue;
      }

      std::string op_token;
      std::string operand_tail;
      {
        std::string tmp = s;
        std::size_t sp = tmp.find_first_of(" \t");
        if (sp == std::string::npos) {
          op_token = tmp;
        } else {
          op_token = tmp.substr(0, sp);
          operand_tail = trim(tmp.substr(sp + 1));
        }
      }

      Op op = parse_op(op_token);
      if (op == static_cast<Op>(255)) {
        error_message = "unknown opcode '" + op_token + "' at line " + std::to_string(line.line_number);
        return false;
      }

      if (op == OP_NOP || op == OP_SYSCALL) {
        code_pc += static_cast<std::uint32_t>(encoded_size(op, 0, 0));
        continue;
      }

      std::vector<std::string> operands = operand_tail.empty() ? std::vector<std::string>{}
                                                               : split_csv(operand_tail);

      auto operand_type_of = [](const std::string& tok) -> std::uint8_t {
        std::uint8_t r = 0;
        std::string inner;
        if (is_register_token(tok, r)) {
          return OT_REG;
        }
        if (is_mem_bracket(tok, inner)) {
          return OT_MEM;
        }
        return OT_IMM;
      };

      if (op == OP_JMP || op == OP_JEQ || op == OP_JNEQ || op == OP_JLA || op == OP_JLE) {
        if (operands.size() != 1) {
          error_message = "branch takes 1 operand at line " + std::to_string(line.line_number);
          return false;
        }
        std::uint8_t src_type = operand_type_of(operands[0]);
        if (src_type == OT_MEM) {
          error_message = "branch target cannot be [mem] at line " + std::to_string(line.line_number);
          return false;
        }
        code_pc += static_cast<std::uint32_t>(encoded_size(op, 0, src_type));
      } else {
        if (operands.size() != 2) {
          error_message = "instruction needs 2 operands at line " + std::to_string(line.line_number);
          return false;
        }
        std::uint8_t dst_type = operand_type_of(operands[0]);
        std::uint8_t src_type = operand_type_of(operands[1]);

        if (op == OP_CMP) {
          if (dst_type != OT_REG) {
            error_message = "cmp lhs must be register at line " + std::to_string(line.line_number);
            return false;
          }
        } else if (op == OP_MOV) {
          if (!(dst_type == OT_REG || dst_type == OT_MEM)) {
            error_message = "mov dst must be reg or [mem] at line " + std::to_string(line.line_number);
            return false;
          }
          if (dst_type == OT_MEM && src_type == OT_MEM) {
            error_message = "mov [mem],[mem] not allowed at line " + std::to_string(line.line_number);
            return false;
          }
        } else {
          if (dst_type != OT_REG) {
            error_message = "arith dst must be register at line " + std::to_string(line.line_number);
            return false;
          }
        }

        code_pc += static_cast<std::uint32_t>(encoded_size(op, dst_type, src_type));
      }
    } else if (current_section == Section::DATA) {
      std::string lower = to_lower(s);
      if (lower.rfind("db ", 0) != 0) {
        error_message = "only DB declarations allowed in _data (line " + std::to_string(line.line_number) + ")";
        return false;
      }
      std::string rest = trim(s.substr(3));
      std::size_t lb = rest.find('[');
      std::size_t rb = rest.find(']');
      if (lb == std::string::npos || rb == std::string::npos || rb <= lb + 1) {
        error_message = "malformed DB at line " + std::to_string(line.line_number);
        return false;
      }
      std::string name = trim(rest.substr(0, lb));
      std::string size_str = trim(rest.substr(lb + 1, rb - lb - 1));

      if (name.empty()) {
        error_message = "DB missing name at line " + std::to_string(line.line_number);
        return false;
      }

      std::uint32_t size_value = 0;
      if (!parse_number(size_str, size_value)) {
        error_message = "DB size must be a number at line " + std::to_string(line.line_number);
        return false;
      }
      if (data_symbols.count(name) != 0u || data_sizes.count(name) != 0u) {
        error_message = "duplicate DB name '" + name + "'";
        return false;
      }

      data_sizes[name] = size_value;
      data_decls.push_back({name, size_value});
    } else {
      error_message = "content outside of any section at line " + std::to_string(line.line_number);
      return false;
    }
  }

  std::uint32_t code_size_final = code_pc;
  std::uint32_t data_offset_base = code_size_final;
  std::uint32_t running_data_offset = 0;
  std::uint32_t total_data_size = 0;

  for (const auto& decl : data_decls) {
    const std::string& name = decl.first;
    std::uint32_t size_value = decl.second;
    data_symbols[name] = data_offset_base + running_data_offset;
    running_data_offset += size_value;
    total_data_size += size_value;
  }
  data_buffer.resize(total_data_size, 0);

  code_buffer.clear();
  code_buffer.reserve(code_size_final);

  current_section = Section::NONE;

  auto resolve_value = [&](const std::string& token, std::uint32_t& out_value) -> bool {
    if (parse_number(token, out_value)) {
      return true;
    }
    auto it_code = code_symbols.find(token);
    if (it_code != code_symbols.end()) {
      out_value = it_code->second;
      return true;
    }
    auto it_data = data_symbols.find(token);
    if (it_data != data_symbols.end()) {
      out_value = it_data->second;
      return true;
    }
    error_message = "unknown symbol: " + token;
    return false;
  };

  auto emit8 = [&](std::uint8_t v) {
    code_buffer.push_back(v);
  };
  auto emit32 = [&](std::uint32_t v) {
    std::size_t i = code_buffer.size();
    code_buffer.resize(i + 4);
    write_u32_le(&code_buffer[i], v);
  };

  for (const auto& line : lines) {
    const std::string& s = line.text;

    if (s == "_main:") {
      current_section = Section::MAIN;
      continue;
    }
    if (s == "_data:") {
      current_section = Section::DATA;
      continue;
    }
    if (current_section == Section::DATA) {
      continue;
    }
    if (!s.empty() && s.back() == ':') {
      continue;
    }

    std::string op_token;
    std::string operand_tail;
    {
      std::string tmp = s;
      std::size_t sp = tmp.find_first_of(" \t");
      if (sp == std::string::npos) {
        op_token = tmp;
      } else {
        op_token = tmp.substr(0, sp);
        operand_tail = trim(tmp.substr(sp + 1));
      }
    }

    Op op = parse_op(op_token);
    if (op == OP_NOP || op == OP_SYSCALL) {
      emit8(static_cast<std::uint8_t>(op));
      continue;
    }

    std::vector<std::string> operands = operand_tail.empty() ? std::vector<std::string>{}
                                                             : split_csv(operand_tail);

    auto operand_type_of = [](const std::string& tok) -> std::uint8_t {
      std::uint8_t r = 0;
      std::string inner;
      if (is_register_token(tok, r)) {
        return OT_REG;
      }
      if (is_mem_bracket(tok, inner)) {
        return OT_MEM;
      }
      return OT_IMM;
    };

    auto encode_reg = [&](const std::string& tok, std::uint8_t& out_reg) -> bool {
      if (!is_register_token(tok, out_reg)) {
        error_message = "expected register";
        return false;
      }
      return true;
    };

    auto encode_imm = [&](const std::string& tok, std::uint32_t& out_val) -> bool {
      if (!resolve_value(tok, out_val)) {
        error_message += " (line " + std::to_string(line.line_number) + ")";
        return false;
      }
      return true;
    };

    auto encode_mem = [&](const std::string& tok, std::uint32_t& out_addr) -> bool {
      std::string inner;
      if (!is_mem_bracket(tok, inner)) {
        error_message = "expected [mem]";
        return false;
      }
      if (!resolve_value(inner, out_addr)) {
        error_message += " (line " + std::to_string(line.line_number) + ")";
        return false;
      }
      return true;
    };

    if (op == OP_JMP || op == OP_JEQ || op == OP_JNEQ || op == OP_JLA || op == OP_JLE) {
      if (operands.size() != 1) {
        error_message = "branch needs 1 operand at line " + std::to_string(line.line_number);
        return false;
      }
      std::uint8_t src_type = operand_type_of(operands[0]);
      if (src_type == OT_MEM) {
        error_message = "branch target cannot be [mem] at line " + std::to_string(line.line_number);
        return false;
      }
      emit8(static_cast<std::uint8_t>(op));
      std::uint8_t mode = static_cast<std::uint8_t>((OT_NONE << 4) | src_type);
      emit8(mode);
      if (src_type == OT_IMM) {
        std::uint32_t v = 0;
        if (!encode_imm(operands[0], v)) {
          return false;
        }
        emit32(v);
      } else {
        std::uint8_t r = 0;
        if (!encode_reg(operands[0], r)) {
          return false;
        }
        emit8(r);
      }
      continue;
    }

    if (operands.size() != 2) {
      error_message = "expected 2 operands at line " + std::to_string(line.line_number);
      return false;
    }

    std::uint8_t dst_type = operand_type_of(operands[0]);
    std::uint8_t src_type = operand_type_of(operands[1]);

    if (op == OP_CMP) {
      if (dst_type != OT_REG) {
        error_message = "cmp lhs must be register at line " + std::to_string(line.line_number);
        return false;
      }
    } else if (op == OP_MOV) {
      if (!(dst_type == OT_REG || dst_type == OT_MEM)) {
        error_message = "mov dst must be reg or [mem] at line " + std::to_string(line.line_number);
        return false;
      }
      if (dst_type == OT_MEM && src_type == OT_MEM) {
        error_message = "mov [mem],[mem] not allowed at line " + std::to_string(line.line_number);
        return false;
      }
    } else {
      if (dst_type != OT_REG) {
        error_message = "arith dst must be register at line " + std::to_string(line.line_number);
        return false;
      }
    }

    emit8(static_cast<std::uint8_t>(op));
    std::uint8_t mode = static_cast<std::uint8_t>((dst_type << 4) | src_type);
    emit8(mode);

    if (dst_type == OT_REG) {
      std::uint8_t rd = 0;
      if (!encode_reg(operands[0], rd)) {
        return false;
      }
      emit8(rd);
    } else if (dst_type == OT_MEM) {
      std::uint32_t addr = 0;
      if (!encode_mem(operands[0], addr)) {
        return false;
      }
      emit32(addr);
    }

    if (src_type == OT_REG) {
      std::uint8_t rs = 0;
      if (!encode_reg(operands[1], rs)) {
        return false;
      }
      emit8(rs);
    } else if (src_type == OT_IMM) {
      std::uint32_t v = 0;
      if (!encode_imm(operands[1], v)) {
        return false;
      }
      emit32(v);
    } else if (src_type == OT_MEM) {
      std::uint32_t addr = 0;
      if (!encode_mem(operands[1], addr)) {
        return false;
      }
      emit32(addr);
    }
  }

  out_module.entry_point = 0;
  out_module.code_section = std::move(code_buffer);
  out_module.data_section = std::move(data_buffer);

  return true;
} 

/**
 * @brief Assemble a file on disk into a Module.
 *
 * Reads the entire file into memory and calls assemble_string().
 *
 * @param path           Input file path.
 * @param out_module     Output module (entry_point, code_section, data_section).
 * @param error_message  Populated with a human-readable error on failure.
 * @return true on success, false on error.
 */
bool Assembler::assemble_file(const std::string& path,
                              Module& out_module,
                              std::string& error_message) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error_message = "cannot open source file";
    return false;
  }
  std::string source_text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return assemble_string(source_text, out_module, error_message);
}

}  // namespace bc
