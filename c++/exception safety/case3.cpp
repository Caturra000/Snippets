// case3增加user API

#include <algorithm>
#include <stdexcept>

template <typename T>
class Stack {
public:
    Stack();
    ~Stack();
    Stack(const Stack &);
    Stack& operator=(const Stack &);

// 新增以下API
public:
    // 毫无争议的函数
    size_t size();
    // 复习一下前面的做法
    void push(const T &);
    // 这是一个看似没有问题的坑
    // 可以用来说明当前代码没问题不意味着异常安全
    // 正确实现版本见case4
    T pop();

private:
    T* copy_data(T *from, size_t size, size_t capacity);

private:
    T *_data;
    size_t _size;
    size_t _capacity;
};

template <typename T>
Stack<T>::Stack()
    : _data(nullptr),
      _size(0),
      _capacity(10)
{
    _data = new T[_capacity];
}

template <typename T>
Stack<T>::~Stack() {
    delete[] _data;
}

template <typename T>
Stack<T>::Stack(const Stack &rhs)
    : _data(copy_data(rhs._data, rhs._size, rhs._capacity)),
      _size(rhs._size),
      _capacity(rhs._capacity)
{}

template <typename T>
Stack<T>& Stack<T>::operator=(const Stack &rhs) {
    if(this != &rhs) {

        T *data = copy_data(rhs._data, rhs._size, rhs._capacity);

        this->~Stack();

        _data = data;
        _size = rhs._size;
        _capacity = rhs._capacity;
    }
    return *this;
}

template <typename T>
T* Stack<T>::copy_data(T *from, size_t size, size_t capacity) {
    T *data = new T[capacity];
    try {
        std::copy(from, from + size, data);
    } catch(...) {
        delete[] data;
        throw;
    }
    return data;
}

template <typename T>
size_t Stack<T>::size() {
    return _size;
}

template <typename T>
void Stack<T>::push(const T &elem) {
    // 扩容操作，可能引发异常
    if(_size == _capacity) {
        // 同case1，即使发生异常也不影响类的正确性
        T *data = copy_data(_data, _size, _capacity << 1);

        delete[] _data;
        _data = data;

        _capacity <<= 1;
    }

    _data[_size] = elem;
    // elem copy发生问题也不影响_size
    _size++;
}

template <typename T>
T Stack<T>::pop() {
    // 这一整段看似没有问题，
    // 但是如果有这样的代码：
    // auto ret = stk.pop();
    // pop本身确保了stk的不变式
    // 但是ret的copy ctor过程可能会fail
    // 此时stk栈上最顶端的elem已经弹出，无法恢复
    // 因此pop()在接口的设计上就已经是错误的
    //
    // Note1: 不要只看critical line
    // Note2: 简单点可以认为异常发生在关键点return处，此时没法再划出一条critical line

    if(_size == 0) {
        throw std::runtime_error("empty");
    }
    T &elem = _data[_size - 1];
    T ret = elem;
    elem.~T();
    _size--;
    return ret;
}

int main() {
    Stack<int> stk0;
    Stack<int> stk1(stk0);
    stk1 = stk0;

    size_t size = stk1.size();
    stk1.push(0b11);
    auto ret = stk1.pop();
}
