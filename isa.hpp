//  isa.hpp:
//
//

#include <cstdint>
#include <string_view>

namespace bc {
  enum Register : std::uint8_t {
    R1 = 0,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,
    R8,
    IP,
    RF,
    RS,
    REG_COUNT
  };

  inline std::string_view register_name(std::uint8_t index) {
    static constexpr std::string_view names[] = {
      "r1",
      "r2",
      "r3",
      "r4",
      "r5",
      "r6",
      "r7",
      "r8",
      "IP",
      "rF",
      "rS"
    };
    return (index < REG_COUNT) ? names[index] : std::string_view{"??"};
  }

  enum FlagBits : std::uint8_t {
    F_EQ        = 1u << 0,
    F_GT        = 1u << 1,
    F_LT        = 1u << 2,
    F_TEST_TRUE = 1u << 3,
    F_BAD_INSTR = 1u << 4,
    F_IP_OOB    = 1u << 5,
    F_READ_OOB  = 1u << 6,
    F_WRITE_OOB = 1u << 7
  };

  enum Op : std::uint8_t {
    OP_NOP = 0,
    OP_MOV,
    OP_ADD,
    OP_SUB,
    OP_XOR,
    OP_CMP,
    OP_JMP,
    OP_JEQ,
    OP_JNEQ,
    OP_JLA,
    OP_JLE,
    OP_SYSCALL
  };

  enum OperandType : std::uint8_t {
    OT_NONE = 0,
    OT_REG  = 1,
    OT_IMM  = 2,
    OT_MEM  = 3
  };

  enum SysId : std::uint32_t {
    SC_EXIT  = 0,
    SC_WRITE = 1,
    SC_READ  = 2,
    SC_OPEN  = 3
  };

}  
