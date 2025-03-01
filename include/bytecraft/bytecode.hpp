//  bytecode.hpp:
//
//

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace bc {

  struct Module {
    std::uint32_t entry_point = 0;
    std::vector<std::uint8_t> code_section;
    std::vector<std::uint8_t> data_section;
  };

  bool save_bvm(const std::string& path, const Module& module, std::string& error_message);
  bool load_bvm(const std::string& path, Module& module, std::string& error_message);

} 

