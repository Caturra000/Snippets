// 使用swap idiom来支持异常安全
// （但是写起来还是痛苦）

#include <algorithm>
#include <stdexcept>
#include <memory>

// 只管理资源本身，只作为内部类使用
// TODO 放到class Stack内部
template <typename T>
struct Stack_impl {
    Stack_impl(size_t capacity);
    ~Stack_impl();
    Stack_impl(const Stack_impl &);
    Stack_impl& operator=(const Stack_impl &);

    // 诀窍是swap肯定不抛异常
    void swap(Stack_impl &);

    T *data;
    size_t size;
    size_t capacity;
};

template <typename T>
class Stack {
public:
    Stack();
    ~Stack() = default;
    Stack(const Stack &);
    Stack& operator=(const Stack &);

public:
    void swap(Stack &);

public:
    size_t size();
    void push(const T &);
    void pop();
    T top();

private:
    Stack_impl<T> _impl;
};

template <typename T>
Stack_impl<T>::Stack_impl(size_t c)
    : data(new T[c]),
      size(0),
      capacity(c)
{}

template <typename T>
Stack_impl<T>::~Stack_impl() {
    delete[] data;
}

template <typename T>
Stack_impl<T>::Stack_impl(const Stack_impl &rhs)
    : data(new T[rhs.size]),
      size(rhs.size),
      capacity(rhs.capacity)
{
    std::copy(rhs.data, rhs.data + size, data);
}

template <typename T>
Stack_impl<T>& Stack_impl<T>::operator=(const Stack_impl &rhs) {
    Stack_impl{rhs}.swap(*this);
    return *this;
}

template <typename T>
void Stack_impl<T>::swap(Stack_impl &rhs) {
    std::swap(data, rhs.data);
    std::swap(size, rhs.size);
    std::swap(capacity, rhs.capacity);
}

template <typename T>
Stack<T>::Stack()
    : _impl(10)
{}

template <typename T>
Stack<T>::Stack(const Stack &rhs)
    : _impl(rhs._impl)
{}

template <typename T>
Stack<T>& Stack<T>::operator=(const Stack &rhs) {
    Stack tmp {rhs};
    this->swap(tmp);
    return *this;
}

template <typename T>
void Stack<T>::swap(Stack &rhs) {
    _impl.swap(rhs._impl);
}

template <typename T>
size_t Stack<T>::size() {
    return _impl.size;
}

template <typename T>
void Stack<T>::push(const T &elem) {
    auto copy_data = [](const Stack_impl<T> &from, Stack_impl<T> &to) {
        std::copy(from.data, from.data + from.size, to.data);
        to.size = from.size;
    };

    if(_impl.size == _impl.capacity) {
        Stack_impl<T> impl(_impl.capacity << 1);
        copy_data(_impl, impl);
        // 需要获取资源的地方只需要swap即可
        // 与case4不同的是
        // 从`impl`构造（含）到`impl.swap`前都不需要小心翼翼处理异常
        // 因为并不影响this
        impl.swap(_impl);
    }
    _impl.data[_impl.size] = elem;
    _impl.size++;
}

template <typename T>
void Stack<T>::pop() {
    if(_impl.size == 0) {
        throw std::runtime_error("pop empty");
    }
    --_impl.size;
}

template <typename T>
T Stack<T>::top() {
    if(_impl.size == 0) {
        throw std::runtime_error("pop empty");
    }
    return _impl.data[_impl.size - 1];
}

int main() {
    Stack<int> stk0;
    Stack<int> stk1(stk0);
    stk1 = stk0;

    size_t size = stk1.size();
    stk1.push(0b101);
    int v = stk1.top();
    stk1.pop();
}
