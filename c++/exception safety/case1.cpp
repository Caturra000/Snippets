// code01展示一种传统上的异常安全容器的实现方式
// 不含任何技巧

#include <algorithm>

template <typename T>
class Stack {
public:
    Stack();
    ~Stack();
    Stack(const Stack &);
    Stack& operator=(const Stack &);

private:
    T *_data;
    size_t _size;
    size_t _capacity;
};

template <typename T>
Stack<T>::Stack()
    : _data(nullptr),
      _size(0),
      // 为了化简避免后续其他成员函数中要_data判空
      // 这里设定一个大于0的数
      _capacity(10)
{
    // 如果fail，那就抛出异常给上层（异常中立）
    // 并且此时new expression并不会泄露资源
    //
    // 当然也可以try-catch做一些需要现场处理的事情（如日志打印），但异常中立仍需要rethrow
    _data = new T[_capacity];

    // Note: 如果抛出异常，是否还需要把_capacity重新设为0？
    // 应该是不需要的，构造的失败本身就意味着对象不存在，不考虑invariant
}

template <typename T>
Stack<T>::~Stack() {
    // 不同于new，delete并不会抛出异常
    delete[] _data;
}

template <typename T>
Stack<T>::Stack(const Stack &rhs)
    : _size(rhs._size),
      _capacity(rhs._capacity)
{
    _data = new T[rhs._capacity];
    // new后的任何操作中，如果抛出异常的话就会泄露_data
    // 因此下面都需要捕获
    try {
        std::copy(rhs._data, rhs._data + rhs._size, _data);
    } catch(...) {
        delete[] _data;
        throw;
    }
}

template <typename T>
Stack<T>& Stack<T>::operator=(const Stack &rhs) {
    if(this != &rhs) {

        // 类似于copy ctor的操作
        // 但这个是临时变量，不能在没有安全完成前把原_data delete处理
        // Note: 显然这个代码片段是可以复用的
        T *data = new T[rhs._capacity];
        try {
            std::copy(rhs._data, rhs._data + rhs._size, data);
        } catch(...) {
            delete[] data;
            throw;
        }

        // 下面是“安全区”

        // 不可能抛出异常
        // Note: 不要在可能抛出异常的函数前调用，比如data的分配
        this->~Stack();

        _data = data;
        _size = rhs._size;
        _capacity = rhs._capacity;
    }
    return *this;
}

int main() {
    Stack<int> stk0;
    Stack<int> stk1(stk0);
    stk1 = stk0;
}
