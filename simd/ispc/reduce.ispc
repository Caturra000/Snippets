export uniform int sum_ispc(uniform const int ptr[], uniform const int count) {
    int sum = 0;

    foreach(i = 0 ... count) {
        sum += ptr[i];
    }

    return reduce_add(sum);
}

export uniform int sum_ispc_unroll4(uniform const int ptr[], uniform const int count) {
    int sum = 0;

    #pragma unroll 4
    foreach(i = 0 ... count) {
        sum += ptr[i];
    }

    return reduce_add(sum);
}
