#include <iostream>
#include <cassert>

/// holy shit!

#define CO_VAR static
#define CO_BEGIN static int _state = 0; switch(_state) { case 0:
#define CO_YIELD(ret) do {_state = __LINE__; return ret; case __LINE__:;} while(0);
#define CO_RETURN(ret) do {_state = -1; return ret;} while(0);
#define CO_END _state = -1; default:;}

static constexpr int NONE = 0;
// acts like a pipe
static int g_pipe {NONE};

using std::cout;
using std::endl;

int producer() {
    CO_VAR int i;

    CO_BEGIN
    for(i = NONE + 1; i < NONE + 10; ++i) {
        g_pipe = i;
        cout << "[producer] generates: " << i << endl;
        CO_YIELD(i)
    }
    CO_END

    return g_pipe = EOF;
}

void consumer() {
    CO_BEGIN
    for(;;) {
        if(g_pipe == NONE) {
            cout << "[consumer] none, yield." << endl;
            CO_YIELD();
            cout << "[consumer] wakeups and checks again." << endl;
        }
        if(g_pipe == EOF) {
            cout << "[consumer] end-of-file, return." << endl;
            CO_RETURN();
            // unreachable
            cout << "[wtf] jintianxiaomidaobila" << endl;
        }
        cout << "[consumer] consumes:  " << g_pipe << endl;
        g_pipe = 0;
    }
    CO_END
    cout << "cannot consume" << endl;
}

int main() {
    while(~g_pipe) {
        int debug = producer();
        assert(debug == g_pipe);
        consumer();
    }
    cout << "==================" << endl;
    for(auto _{3}; _--;) {
        consumer();
    }
    return 0;
}
