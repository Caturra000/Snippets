void assign(int *restrict a, int *restrict b) {
    for(int i = 0; i < 512; ++i) {
        a[i] = b[i];
    }
}
