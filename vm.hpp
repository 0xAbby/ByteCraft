// vm.hpp:
// 
//

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#include "isa.hpp"

namespace bc {

class VM {
   public:
    VM(std::vector<std::uint8_t> memory,
        std::uint32_t entry_point,
        std::uint32_t code_size,
        std::uint32_t data_size);

   void run();

   private:
    std::vector<std::uint8_t> memory_image_;
    std::uint32_t registers_[REG_COUNT]{};
    std::uint32_t code_size_bytes_ = 0;
    std::uint32_t data_size_bytes_ = 0;
    bool is_running_ = false;

    std::uint8_t fetch8();
    std::uint32_t fetch32();

    bool oob_read(std::uint32_t address, std::size_t count = 1);
    bool oob_write(std::uint32_t address, std::size_t count = 1);
    std::uint32_t load32(std::uint32_t address);
    void store32(std::uint32_t address, std::uint32_t value);

    void step();
    void dump_registers(std::uint32_t ip_before, Op opcode);
    void handle_syscall();
   };

} 
