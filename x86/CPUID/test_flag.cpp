#include <filesystem>
#include <string>
#include <string_view>
#include <charconv>
#include <iostream>
#include <fstream>
#include <ranges>

namespace stdf = std::filesystem;
namespace stdv = std::views;
namespace stdr = std::ranges;
using namespace std::literals;

int main(int argc, char **argv) {
    const stdf::path root {argc > 1 ? argv[1] : "."};
    std::cout << "Start: " << root << std::endl;
    auto cpuid_txt_path_view =
        stdv::filter([](auto &&dentry) { return dentry.is_regular_file(); })
      | stdv::transform([](auto &&dentry) { return dentry.path(); })
      | stdv::filter([](auto &&path) { return path.extension() == ".txt"; })
      | stdv::filter([](auto &&path) {
            auto name = path.filename().string();
            return name.find("CPUID"sv) != std::string::npos;
        });
    auto optimized_out = [](auto &&line) {
        return line.starts_with("CPUID 80000001");
    };
    auto contains_feature = [](auto &&line) {
        return line.starts_with("CPUID 00000007");
    };
    auto test_bit = [](auto &&line, auto bit_index) {
        constexpr size_t bias = 25;
        const char *features_hex {line.data() + bias};
        unsigned long long bitmap = 0;
        std::from_chars(features_hex, features_hex + 8, bitmap, 16);
        return bitmap >> bit_index & 1;
    };
    for(auto &&path : stdf::recursive_directory_iterator(root) | cpuid_txt_path_view) {
        std::ifstream file_stream {path};
        std::string line;

        while(std::getline(file_stream, line)) {
            if(optimized_out(line)) break;
            if(!contains_feature(line)) continue;
            if(test_bit(line, 9 /* ERMS */)) {
                std::cout << "OK: " << path.filename() << std::endl;
            }
            break;
        }
    }
}
