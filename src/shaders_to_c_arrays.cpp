#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

int main() {
  std::ofstream out_file(OUT_DIR "/im3d_sdl3_gpu_shaders.h");
  if (!out_file) {
    std::cerr << "Failed to open output file\n";
    return 1;
  }

  out_file << "// clang-format off\n\n";

  out_file << "#pragma once\n\n";
  out_file << "#include <cstdint>\n\n";

  for (const auto& entry : std::filesystem::directory_iterator(".shaders")) {
    if (!entry.is_regular_file()) continue;

    std::ifstream in_file(entry.path(), std::ios::binary);
    if (!in_file) {
      std::cerr << "Failed to open: " << entry.path() << "\n";
      return 1;
    }

    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(in_file)),
        std::istreambuf_iterator<char>());
    in_file.close();

    std::string var_name = entry.path().filename().string();
    std::replace(var_name.begin(), var_name.end(), '.', '_');

    out_file << "constexpr uint8_t " << var_name << "[] = {\n";
    for (size_t i = 0; i < data.size(); ++i) {
      if (i % 16 == 0) out_file << "  ";
      out_file << "0x" << std::hex << std::setfill('0') << std::setw(2)
               << static_cast<int>(data[i]);
      if (i + 1 < data.size()) out_file << ",";
      if (i % 16 == 15 || i + 1 == data.size())
        out_file << "\n";
      else
        out_file << " ";
    }
    out_file << "};\n\n";
  }

  out_file << "// clang-format on\n";

  out_file.close();

  return 0;
}
