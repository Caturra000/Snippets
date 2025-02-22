#include <cstring>
#include <cstdint>

// ~std::bit_cast
bool EqBytes4(const char *src, uint32_t target) {
    uint32_t val;
    std::memcpy(&val, src, sizeof(uint32_t));
    return val == target;
}

bool skip_literal_v2(const char *data, size_t &pos,
                     size_t len, uint8_t token) {
  (void) token;
  static constexpr uint32_t kNullBin = 0x6c6c756e;
  static constexpr uint32_t kTrueBin = 0x65757274;
  static constexpr uint32_t kAlseBin = 0x65736c61;  // the binary of 'alse' in false
  static constexpr uint32_t kFalsBin = 0x736c6166;  // the binary of 'fals' in false
  
  auto start = data + pos;
  auto end = data + len + 1;
  if (start + 5 < end) {
      if (EqBytes4(start, kNullBin) || EqBytes4(start, kTrueBin)) {
          pos += 4;
          return true;
      }
      if (EqBytes4(start, kFalsBin) && EqBytes4(start + 1, kAlseBin)) {
          pos += 5;
          return true;
      }
  }
  // slow path
  if (start + 4 < end) {
      if (EqBytes4(start, kNullBin) || EqBytes4(start, kTrueBin)) {
          pos += 4;
          return true;
      }
  }
  return false;
}
