//  bytecode.cpp:
//
//

#include "bytecraft/bytecode.hpp"
#include <cstring>
#include <fstream>

namespace bc {

static constexpr char MAGIC_BVM[4] = {'B', 'V', 'M', '\0'};

/**
 * @brief Write a ByteCraft module to disk in BVM format.
 *
 * File layout:
 *   - Magic bytes: "BVM\0" (4 bytes).
 *   - Header: entry_point (u32), code_section_size (u32), data_section_size (u32).
 *   - Payload: code_section bytes followed by data_section bytes.
 *
 * On failure, this function sets @p error_message and returns false.
 *
 * @param path           Filesystem path of the output .bvm file to create.
 * @param module         Module containing entry_point, code_section, and data_section.
 * @param error_message  Output parameter populated with a human-readable error on failure.
 * @return true on success, false on failure.
 */
bool save_bvm(const std::string& path, const Module& module, std::string& error_message) {
  std::ofstream output_file(path, std::ios::binary);
  if (!output_file) {
    error_message = "cannot open output file";
    return false;
  }

  output_file.write(MAGIC_BVM, 4);

  std::uint32_t entry_point_value = module.entry_point;
  std::uint32_t code_section_size = static_cast<std::uint32_t>(module.code_section.size());
  std::uint32_t data_section_size = static_cast<std::uint32_t>(module.data_section.size());

  output_file.write(reinterpret_cast<char*>(&entry_point_value), 4);
  output_file.write(reinterpret_cast<char*>(&code_section_size), 4);
  output_file.write(reinterpret_cast<char*>(&data_section_size), 4);

  if (code_section_size > 0) {
    output_file.write(reinterpret_cast<const char*>(module.code_section.data()), code_section_size);
  }

  if (data_section_size > 0) {
    output_file.write(reinterpret_cast<const char*>(module.data_section.data()), data_section_size);
  }

  return true;
}

/**
 * @brief Read a ByteCraft module from a BVM file on disk.
 *
 * Expects the same layout produced by save_bvm():
 *   - Magic "BVM\0".
 *   - Header (entry_point, code_section_size, data_section_size).
 *   - Code bytes followed by data bytes.
 *
 * On malformed or truncated files, this function sets @p error_message and returns false.
 *
 * @param path           Filesystem path of the input .bvm file to read.
 * @param module         Output parameter populated with entry_point, code_section, and data_section.
 * @param error_message  Output parameter populated with a human-readable error on failure.
 * @return true on success, false on failure.
 */
bool load_bvm(const std::string& path, Module& module, std::string& error_message) {
  std::ifstream input_file(path, std::ios::binary);
  if (!input_file) {
    error_message = "cannot open program file";
    return false;
  }

  char magic_buffer[4];
  input_file.read(magic_buffer, 4);
  if (input_file.gcount() != 4 || std::memcmp(magic_buffer, MAGIC_BVM, 4) != 0) {
    error_message = "bad magic";
    return false;
  }

  std::uint32_t entry_point_value = 0;
  std::uint32_t code_section_size = 0;
  std::uint32_t data_section_size = 0;

  input_file.read(reinterpret_cast<char*>(&entry_point_value), 4);
  input_file.read(reinterpret_cast<char*>(&code_section_size), 4);
  input_file.read(reinterpret_cast<char*>(&data_section_size), 4);
  if (!input_file) {
    error_message = "truncated header";
    return false;
  }

  module.entry_point = entry_point_value;
  module.code_section.resize(code_section_size);
  module.data_section.resize(data_section_size);

  if (code_section_size > 0) {
    input_file.read(reinterpret_cast<char*>(module.code_section.data()), code_section_size);
  }

  if (data_section_size > 0) {
    input_file.read(reinterpret_cast<char*>(module.data_section.data()), data_section_size);
  }

  if (!input_file) {
    error_message = "truncated payload";
    return false;
  }

  return true;
}

}  // namespace bc
