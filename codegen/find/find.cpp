#include <cstddef>

bool find(int* arr, size_t size, int val) {
    for(size_t i = 0; i < size; ++i) {
        if(val == arr[i]) return true;
    }
    return false;
}
