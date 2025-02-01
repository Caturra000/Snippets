int my_loop(int a[512]) {
    int r = 0;
    for(int i = 0; i < 512; ++i) {
        r += a[i];
    }
    return r;
}
