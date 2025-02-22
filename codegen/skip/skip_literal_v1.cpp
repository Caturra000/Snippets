#include <cstring>
#include <cstdint>

// ~std::bit_cast
bool EqBytes4(const char *src, uint32_t target) {
    uint32_t val;
    std::memcpy(&val, src, sizeof(uint32_t));
    return val == target;
}

bool skip_literal_v1(const char *data, size_t &pos,
                  size_t len, uint8_t token) {
    static constexpr uint32_t kNullBin = 0x6c6c756e;
    static constexpr uint32_t kTrueBin = 0x65757274;
    static constexpr uint32_t kFalseBin =
        0x65736c61;  // the binary of 'alse' in false
    auto start = data + pos;
    auto end = data + len + 1;
    switch (token) {
      case 't':
        if (start + 4 < end && EqBytes4(start, kTrueBin)) {
          pos += 4;
          return true;
        };
        break;
      case 'n':
        if (start + 4 < end && EqBytes4(start, kNullBin)) {
          pos += 4;
          return true;
        };
        break;
      case 'f':
        if (start + 5 < end && EqBytes4(start + 1, kFalseBin)) {
          pos += 5;
          return true;
        }
    }
    return false;
}
