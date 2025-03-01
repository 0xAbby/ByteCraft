//  util.hpp:
//    helper functions.

#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

namespace bc {

  inline std::uint32_t read_u32_le(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0])
        | (static_cast<std::uint32_t>(data[1]) << 8)
        | (static_cast<std::uint32_t>(data[2]) << 16)
        | (static_cast<std::uint32_t>(data[3]) << 24);
  }

  inline void write_u32_le(std::uint8_t* data, std::uint32_t value) {
    data[0] = static_cast<std::uint8_t>(value & 0xFF);
    data[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    data[2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    data[3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
  }

  inline std::string trim(std::string text) {
    auto is_not_space = [](unsigned char c) {
      return !std::isspace(c);
    };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), is_not_space));
    text.erase(std::find_if(text.rbegin(), text.rend(), is_not_space).base(), text.end());
    return text;
  }

} 