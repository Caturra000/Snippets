#include <string>
// #define unlikely(x) __builtin_expect((x), 0)
#define unlikely(x) (x)

int fb(const char *cursor)
{
    int len = 0;
    while (*cursor != '\"')
    {
        cursor++;
        if (unlikely(cursor[-1] == '\\'))
            cursor++;

        len++;
    }

    return len;
}

std::string s(4*1024*1024, 'a');
constexpr size_t testround = 4000;

int main() {
    s.back() = '\"';
    for(int i = 0; i < testround; ++i) {
        volatile int a = fb(s.c_str());
    }
}
