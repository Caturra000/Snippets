export void lookup_ispc(uniform uint8 src[], uniform const uint8 lut[], uniform int count) {
    foreach(i = 0 ... count) {
        uint8 index = src[i];
        src[i] = lut[index];
    }
}
