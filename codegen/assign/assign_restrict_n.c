void assign(int *restrict a, int *restrict b, int N) {
    for(int i = 0; i < N; ++i) {
        a[i] = b[i];
    }
}