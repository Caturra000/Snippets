#include <iostream>

/// holy shit!

#define CO_VAR static
#define CO_BEGIN static int _state = 0; switch(_state) { case 0:
#define CO_YIELD(ret) do {_state = __LINE__; return ret; case __LINE__:;} while(0);
#define CO_END _state = -1; default:;}

int producer() {
    CO_VAR int i;

    CO_BEGIN
    for(i = 0; i < 10; ++i) {
        CO_YIELD(i)
    }
    CO_END

    return EOF;
}

int consumer() {
    using std::cout;
    using std::endl;

    int ret = producer();
    if(EOF != ret) {
        cout << "consumed: " << ret << endl;
    }
    return ret;
}

int main() {
    while(~consumer());
    return 0;
}
