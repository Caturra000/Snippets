#include <cstddef>

bool find_v3(int* vect, size_t size, int val) {
    vect[size] = val;
    for(size_t i = 0; ; ++i) {
        if(val == vect[i]) return i != size;
    }
    return false;
}
