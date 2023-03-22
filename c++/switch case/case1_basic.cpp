#include <iostream>

using std::cout;
using std::endl;

// case实际是label
// 既switch-case本身就可以设计出“飞线”一样的代码
int main() {
    int a = 0;
    switch (a) case 0: for(;;) {
        cout<<"hello"<<endl;
        case 1:
        cout<<"world"<<endl;
        return 0;
    }
}
