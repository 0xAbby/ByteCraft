// test_vm_registers.cpp:
//    
//

#include <gtest/gtest.h>

#include "bytecraft/asm.hpp"
#include "bytecraft/vm.hpp"

/**
 * @brief Assemble and run a program that writes a known value into r3.
 *
 * Program:
 *   mov r3, 0xDEADBEEF
 *   mov r1, 0          ; SC_EXIT
 *   mov r2, 0
 *   syscall
 *
 * After execution, r3 must equal 0xDEADBEEF.
 */
TEST(VMRegisters, SetAndReadR3ViaAssembly) {
  const char* source =
    "_main:\n"
    "  mov r3, 0xDEADBEEF\n"
    "  mov r1, 0\n"
    "  mov r2, 0\n"
    "  syscall\n"
    "\n"
    "_data:\n"
    "  DB buf[4]\n";

  bc::Assembler assembler;
  bc::Module module;
  std::string error_message;

  ASSERT_TRUE(assembler.assemble_string(source, module, error_message))
      << "Assembly failed: " << error_message;

  std::vector<std::uint8_t> memory_image;
  memory_image.reserve(module.code_section.size() + module.data_section.size());
  memory_image.insert(memory_image.end(), module.code_section.begin(), module.code_section.end());
  memory_image.insert(memory_image.end(), module.data_section.begin(), module.data_section.end());

  bc::VM vm(std::move(memory_image),
            module.entry_point,
            static_cast<std::uint32_t>(module.code_section.size()),
            static_cast<std::uint32_t>(module.data_section.size()));

  vm.run();

  EXPECT_EQ(vm.get_register(bc::R3), 0xDEADBEEFu);
}

/**
 * @brief Directly set a register using the VM accessor and read it back.
 */
TEST(VMRegisters, DirectSetAndGet) {
  std::vector<std::uint8_t> memory_image;
  memory_image.resize(0);

  bc::VM vm(std::move(memory_image),
            0,
            0,
            0);

  vm.set_register(bc::R5, 0xCAFEBABEu);

  EXPECT_EQ(vm.get_register(bc::R5), 0xCAFEBABEu);
}
