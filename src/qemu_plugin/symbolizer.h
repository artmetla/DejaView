#ifndef SRC_QEMU_PLUGIN_SYMBOLIZER_H_
#define SRC_QEMU_PLUGIN_SYMBOLIZER_H_

#include <string>
#include <memory>
#include <cstdint>
#include <unordered_map>

class ElfFile;

class Symbolizer {
public:
  Symbolizer() {}

  bool Init(ElfFile &elf);
  bool lookupAddress(uint64_t address, std::string& name, std::string& filename, int& line_number);
  uint64_t lookupSymbol(const std::string& symbol_name);

private:
  bool readSymbols(ElfFile &elf);

  std::unordered_map<std::string, uint64_t> m_symbol_to_address;
  std::unordered_map<uint64_t, std::string> m_address_to_symbol;
};

#endif  // SRC_QEMU_PLUGIN_SYMBOLIZER_H_
