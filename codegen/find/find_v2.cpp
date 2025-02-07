#include <cstddef>

bool find_v2(int* vect, size_t size, int val) {
    vect[size] = val;
    size_t i = 0;
    for(;;++i) {
        if(val == vect[i]) {
            if (i == size)
                return false;
            else
                return true;
        }
    }
    return false;
}