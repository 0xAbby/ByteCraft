//  vm.cpp:
//
//

#include "bytecraft/vm.hpp"
#include "bytecraft/util.hpp"
#include <cstring>

namespace bc {

/**
 * @brief Construct a VM instance with a memory image and layout metadata.
 *
 * @param memory       Flat memory image containing [code][data].
 * @param entry_point  Initial instruction pointer (offset into code region).
 * @param code_size    Size in bytes of the code region at the beginning of memory.
 * @param data_size    Size in bytes of the data region following code.
 * @return void
 */
VM::VM(std::vector<std::uint8_t> memory,
       std::uint32_t entry_point,
       std::uint32_t code_size,
       std::uint32_t data_size)
  : memory_image_(std::move(memory)),
    code_size_bytes_(code_size),
    data_size_bytes_(data_size) {
  std::memset(registers_, 0, sizeof(registers_));
  registers_[IP] = entry_point;
  is_running_ = true;
}

/**
 * @brief Check for out-of-bounds read access over a byte range.
 *
 * Sets READ_OOB flag and stops the VM when an OOB read is detected.
 *
 * @param address  Starting address in the VM memory space.
 * @param count    Number of bytes intended to be read.
 * @return true if the range is out-of-bounds, false otherwise.
 */
bool VM::oob_read(std::uint32_t address, std::size_t count) {
  std::uint32_t memory_size = static_cast<std::uint32_t>(memory_image_.size());
  bool is_out_of_bounds = (address > memory_size) || (count > memory_size) || (address + count > memory_size);
  if (is_out_of_bounds) {
    registers_[RF] |= F_READ_OOB;
    is_running_ = false;
  }
  return is_out_of_bounds;
}

/**
 * @brief Check for out-of-bounds write access over a byte range.
 *
 * Sets WRITE_OOB flag and stops the VM when an OOB write is detected.
 *
 * @param address  Starting address in the VM memory space.
 * @param count    Number of bytes intended to be written.
 * @return true if the range is out-of-bounds, false otherwise.
 */
bool VM::oob_write(std::uint32_t address, std::size_t count) {
  std::uint32_t memory_size = static_cast<std::uint32_t>(memory_image_.size());
  bool is_out_of_bounds = (address > memory_size) || (count > memory_size) || (address + count > memory_size);
  if (is_out_of_bounds) {
    registers_[RF] |= F_WRITE_OOB;
    is_running_ = false;
  }
  return is_out_of_bounds;
}

/**
 * @brief Fetch a single byte from the code stream at IP and advance IP.
 *
 * Sets IP_OOB flag and stops the VM if IP is outside the code region.
 *
 * @return The fetched byte, or 0 if out-of-bounds.
 */
std::uint8_t VM::fetch8() {
  if (registers_[IP] >= code_size_bytes_) {
    registers_[RF] |= F_IP_OOB;
    is_running_ = false;
    return 0;
  }
  std::uint8_t value = memory_image_[registers_[IP]];
  registers_[IP] += 1;
  return value;
}

/**
 * @brief Fetch a 32-bit little-endian value from the code stream and advance IP by 4.
 *
 * Sets IP_OOB flag and stops the VM if there are not enough bytes remaining in code.
 *
 * @return The fetched 32-bit value, or 0 if out-of-bounds.
 */
std::uint32_t VM::fetch32() {
  if (registers_[IP] + 4 > code_size_bytes_) {
    registers_[RF] |= F_IP_OOB;
    is_running_ = false;
    return 0;
  }
  std::uint32_t value = read_u32_le(&memory_image_[registers_[IP]]);
  registers_[IP] += 4;
  return value;
}

/**
 * @brief Read a 32-bit little-endian value from absolute memory.
 *
 * Performs bounds checking and sets READ_OOB on failure.
 *
 * @param address  Absolute address inside the VM memory space.
 * @return The 32-bit value read, or 0 if out-of-bounds.
 */
std::uint32_t VM::load32(std::uint32_t address) {
  if (oob_read(address, 4)) {
    return 0;
  }
  return read_u32_le(&memory_image_[address]);
}

/**
 * @brief Write a 32-bit value to absolute memory in little-endian order.
 *
 * Performs bounds checking and sets WRITE_OOB on failure.
 *
 * @param address  Absolute address inside the VM memory space.
 * @param value    The 32-bit value to write.
 * @return void
 */
void VM::store32(std::uint32_t address, std::uint32_t value) {
  if (oob_write(address, 4)) {
    return;
  }
  write_u32_le(&memory_image_[address], value);
}

/**
 * @brief Update comparison flags (EQ, GT, LT) based on rS signedness.
 *
 * If rS bit0 is set, values are compared as signed int32.
 * Otherwise, values are compared as unsigned.
 *
 * @param lhs  Left-hand value.
 * @param rhs  Right-hand value.
 * @return void
 */
void VM::set_compare_flags(std::uint32_t lhs, std::uint32_t rhs) {
  registers_[RF] &= ~static_cast<std::uint32_t>(F_EQ | F_GT | F_LT);
  bool signed_mode = (registers_[RS] & 1u) != 0u;
  if (signed_mode) {
    std::int32_t a = static_cast<std::int32_t>(lhs);
    std::int32_t b = static_cast<std::int32_t>(rhs);
    if (a == b) {
      registers_[RF] |= F_EQ;
    } else if (a > b) {
      registers_[RF] |= F_GT;
    } else {
      registers_[RF] |= F_LT;
    }
  } else {
    if (lhs == rhs) {
      registers_[RF] |= F_EQ;
    } else if (lhs > rhs) {
      registers_[RF] |= F_GT;
    } else {
      registers_[RF] |= F_LT;
    }
  }
}

/**
 * @brief Print a single-step diagnostic line with registers and flags.
 *
 * Intended for tracing program execution after each instruction.
 *
 * @param ip_before  The IP value before executing the current instruction.
 * @param opcode     The opcode that was executed.
 * @return void
 */
void VM::dump_registers(std::uint32_t ip_before, Op opcode) {
  std::uint32_t flags_value = registers_[RF];

  std::cout << std::hex << std::uppercase << std::setfill('0');
  std::cout << "IP:" << std::setw(8) << ip_before << " OP:" << std::setw(2) << static_cast<unsigned>(opcode) << " | ";

  for (int reg_index = R1; reg_index <= R8; reg_index += 1) {
    std::cout << register_name(static_cast<std::uint8_t>(reg_index)) << ":" << std::setw(8) << registers_[reg_index] << " ";
  }

  std::cout << "IP:" << std::setw(8) << registers_[IP] << " ";
  std::cout << "rF:" << std::setw(8) << registers_[RF] << " ";
  std::cout << "rS:" << (registers_[RS] & 1u) << " ";

  std::cout << "["
            << ((flags_value & F_EQ) ? "EQ " : "")
            << ((flags_value & F_GT) ? "GT " : "")
            << ((flags_value & F_LT) ? "LT " : "")
            << ((flags_value & F_TEST_TRUE) ? "TEST " : "")
            << ((flags_value & F_BAD_INSTR) ? "BAD " : "")
            << ((flags_value & F_IP_OOB) ? "IP_OOB " : "")
            << ((flags_value & F_READ_OOB) ? "R_OOB " : "")
            << ((flags_value & F_WRITE_OOB) ? "W_OOB " : "")
            << "]\n";

  std::cout << std::dec << std::nouppercase;
}

/**
 * @brief Handle the SYSCALL instruction.
 *
 * Interprets syscall ID in r1 and parameters in r2+.
 * Sets return values in r1 when applicable.
 *
 * @return void
 */
void VM::handle_syscall() {
  std::uint32_t syscall_id = registers_[R1];

  switch (syscall_id) {
    case SC_EXIT: {
      is_running_ = false;
      break;
    }
    case SC_WRITE: {
      std::uint32_t file_descriptor = registers_[R2];
      std::uint32_t buffer_address = registers_[R3];
      std::uint32_t byte_count = registers_[R4];

      if (oob_read(buffer_address, byte_count)) {
        break;
      }

      std::string text(byte_count, '\0');
      std::memcpy(text.data(), &memory_image_[buffer_address], byte_count);

      if (file_descriptor == 2) {
        std::cerr << text << std::flush;
      } else {
        std::cout << text << std::flush;
      }

      registers_[R1] = byte_count;
      break;
    }
    case SC_READ: {
      std::uint32_t file_descriptor = registers_[R2];
      std::uint32_t buffer_address = registers_[R3];
      std::uint32_t byte_count = registers_[R4];

      if (oob_write(buffer_address, byte_count)) {
        break;
      }

      std::string input_text;

      if (file_descriptor == 0) {
        input_text.reserve(byte_count);
        for (std::uint32_t i = 0; i < byte_count; i += 1) {
          int ch = std::cin.get();
          if (ch == EOF) {
            break;
          }
          input_text.push_back(static_cast<char>(ch));
        }
      }

      std::memcpy(&memory_image_[buffer_address], input_text.data(), input_text.size());
      registers_[R1] = static_cast<std::uint32_t>(input_text.size());
      break;
    }
    case SC_OPEN: {
      registers_[R1] = 0xFFFFFFFFu;
      break;
    }
    default: {
      registers_[RF] |= F_BAD_INSTR;
      is_running_ = false;
      break;
    }
  }
}

/**
 * @brief Execute a single instruction at IP and update machine state.
 *
 * Handles fetch/decode/execute for the implemented instruction set.
 * On error or OOB conditions, sets flags and transitions VM to a stopped state.
 *
 * @return void
 */
void VM::step() {
  if (registers_[IP] >= code_size_bytes_) {
    registers_[RF] |= F_IP_OOB;
    is_running_ = false;
    return;
  }

  std::uint32_t ip_before = registers_[IP];
  Op opcode = static_cast<Op>(fetch8());

  auto read_mode = [&]() -> std::uint8_t {
    return fetch8();
  };

  auto dst_type_of = [](std::uint8_t mode) -> std::uint8_t {
    return static_cast<std::uint8_t>((mode >> 4) & 0xF);
  };

  auto src_type_of = [](std::uint8_t mode) -> std::uint8_t {
    return static_cast<std::uint8_t>(mode & 0xF);
  };

  switch (opcode) {
    case OP_NOP: {
      break;
    }

    case OP_MOV: {
      std::uint8_t mode_byte = read_mode();
      std::uint8_t dst_type = dst_type_of(mode_byte);
      std::uint8_t src_type = src_type_of(mode_byte);

      if (dst_type == OT_REG) {
        std::uint8_t dst_reg = fetch8();
        if (dst_reg >= REG_COUNT) {
          registers_[RF] |= F_BAD_INSTR;
          is_running_ = false;
          break;
        }
        std::uint32_t value = 0;
        if (src_type == OT_REG) {
          std::uint8_t src_reg = fetch8();
          if (src_reg >= REG_COUNT) {
            registers_[RF] |= F_BAD_INSTR;
            is_running_ = false;
            break;
          }
          value = registers_[src_reg];
        } else if (src_type == OT_IMM) {
          value = fetch32();
        } else if (src_type == OT_MEM) {
          std::uint32_t addr = fetch32();
          value = load32(addr);
          if (!is_running_) {
            break;
          }
        } else {
          registers_[RF] |= F_BAD_INSTR;
          is_running_ = false;
          break;
        }

        if (dst_reg == RS) {
          registers_[RS] = (value & 1u);
        } else {
          registers_[dst_reg] = value;
        }
      } else if (dst_type == OT_MEM) {
        std::uint32_t addr = fetch32();
        std::uint32_t value = 0;
        if (src_type == OT_REG) {
          std::uint8_t src_reg = fetch8();
          if (src_reg >= REG_COUNT) {
            registers_[RF] |= F_BAD_INSTR;
            is_running_ = false;
            break;
          }
          value = registers_[src_reg];
        } else if (src_type == OT_IMM) {
          value = fetch32();
        } else {
          registers_[RF] |= F_BAD_INSTR;
          is_running_ = false;
          break;
        }
        store32(addr, value);
      } else {
        registers_[RF] |= F_BAD_INSTR;
        is_running_ = false;
      }
      break;
    }

    case OP_ADD:
    case OP_SUB:
    case OP_XOR: {
      std::uint8_t mode_byte = read_mode();
      std::uint8_t dst_type = dst_type_of(mode_byte);
      std::uint8_t src_type = src_type_of(mode_byte);

      if (dst_type != OT_REG) {
        registers_[RF] |= F_BAD_INSTR;
        is_running_ = false;
        break;
      }

      std::uint8_t dst_reg = fetch8();
      if (dst_reg >= REG_COUNT) {
        registers_[RF] |= F_BAD_INSTR;
        is_running_ = false;
        break;
      }

      std::uint32_t rhs = 0;
      if (src_type == OT_REG) {
        std::uint8_t src_reg = fetch8();
        if (src_reg >= REG_COUNT) {
          registers_[RF] |= F_BAD_INSTR;
          is_running_ = false;
          break;
        }
        rhs = registers_[src_reg];
      } else if (src_type == OT_IMM) {
        rhs = fetch32();
      } else if (src_type == OT_MEM) {
        std::uint32_t addr = fetch32();
        rhs = load32(addr);
        if (!is_running_) {
          break;
        }
      } else {
        registers_[RF] |= F_BAD_INSTR;
        is_running_ = false;
        break;
      }

      if (opcode == OP_ADD) {
        registers_[dst_reg] = registers_[dst_reg] + rhs;
      } else if (opcode == OP_SUB) {
        registers_[dst_reg] = registers_[dst_reg] - rhs;
      } else {
        registers_[dst_reg] = registers_[dst_reg] ^ rhs;
      }
      break;
    }

    case OP_CMP: {
      std::uint8_t mode_byte = read_mode();
      std::uint8_t dst_type = dst_type_of(mode_byte);
      std::uint8_t src_type = src_type_of(mode_byte);

      if (dst_type != OT_REG) {
        registers_[RF] |= F_BAD_INSTR;
        is_running_ = false;
        break;
      }

      std::uint8_t lhs_reg = fetch8();
      if (lhs_reg >= REG_COUNT) {
        registers_[RF] |= F_BAD_INSTR;
        is_running_ = false;
        break;
      }

      std::uint32_t lhs = registers_[lhs_reg];
      std::uint32_t rhs = 0;

      if (src_type == OT_REG) {
        std::uint8_t src_reg = fetch8();
        if (src_reg >= REG_COUNT) {
          registers_[RF] |= F_BAD_INSTR;
          is_running_ = false;
          break;
        }
        rhs = registers_[src_reg];
      } else if (src_type == OT_IMM) {
        rhs = fetch32();
      } else if (src_type == OT_MEM) {
        std::uint32_t addr = fetch32();
        rhs = load32(addr);
        if (!is_running_) {
          break;
        }
      } else {
        registers_[RF] |= F_BAD_INSTR;
        is_running_ = false;
        break;
      }

      set_compare_flags(lhs, rhs);
      break;
    }

    case OP_JMP:
    case OP_JEQ:
    case OP_JNEQ:
    case OP_JLA:
    case OP_JLE: {
      std::uint8_t mode_byte = read_mode();
      std::uint8_t src_type = src_type_of(mode_byte);

      std::uint32_t target = 0;
      if (src_type == OT_IMM) {
        target = fetch32();
      } else if (src_type == OT_REG) {
        std::uint8_t r = fetch8();
        if (r >= REG_COUNT) {
          registers_[RF] |= F_BAD_INSTR;
          is_running_ = false;
          break;
        }
        target = registers_[r];
      } else {
        registers_[RF] |= F_BAD_INSTR;
        is_running_ = false;
        break;
      }

      bool take = false;
      if (opcode == OP_JMP) {
        take = true;
      } else if (opcode == OP_JEQ) {
        take = (registers_[RF] & F_EQ) != 0u;
      } else if (opcode == OP_JNEQ) {
        take = (registers_[RF] & F_EQ) == 0u;
      } else if (opcode == OP_JLA) {
        take = (registers_[RF] & F_GT) != 0u;
      } else if (opcode == OP_JLE) {
        take = (registers_[RF] & (F_LT | F_EQ)) != 0u;
      }

      if (take) {
        registers_[RF] |= F_TEST_TRUE;
        registers_[IP] = target;
      } else {
        registers_[RF] &= ~static_cast<std::uint32_t>(F_TEST_TRUE);
      }
      break;
    }

    case OP_SYSCALL: {
      handle_syscall();
      break;
    }

    default: {
      registers_[RF] |= F_BAD_INSTR;
      is_running_ = false;
      break;
    }
  }

  dump_registers(ip_before, opcode);
}

/**
 * @brief Run the VM until it halts or an error condition occurs.
 *
 * Repeatedly calls step() while the VM is in a running state.
 *
 * @return void
 */
void VM::run() {
  while (is_running_) {
    step();
  }
}

}  // namespace bc
