export void scan_ispc(uniform int data[], uniform int count) {
    uniform int carry = 0;
    uniform int width = programCount;
    for(uniform int i = 0; i < count; i += width) {
        bool active = (i + programIndex) < count;
        int val = active ? data[i + programIndex] : 0;
        int scanned = exclusive_scan_add(val) + val + carry;
        if(active) data[i + programIndex] = scanned;
        carry = extract(scanned, width - 1);
    }
}
