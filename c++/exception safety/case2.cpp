// case2对比case1只增加了封装性

#include <algorithm>

template <typename T>
class Stack {
public:
    Stack();
    ~Stack();
    Stack(const Stack &);
    Stack& operator=(const Stack &);

private:
    // 封装一个处理好异常的copy函数
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
    // 通过封装简化
    : _data(copy_data(rhs._data, rhs._size, rhs._capacity)),
      _size(rhs._size),
      _capacity(rhs._capacity)
{}

template <typename T>
Stack<T>& Stack<T>::operator=(const Stack &rhs) {
    if(this != &rhs) {

        // 通过封装简化
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

int main() {
    Stack<int> stk0;
    Stack<int> stk1(stk0);
    stk1 = stk0;
}
