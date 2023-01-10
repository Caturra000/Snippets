// case4通过修改类的设计来完成异常安全

#include <algorithm>
#include <stdexcept>

template <typename T>
class Stack {
public:
    Stack();
    ~Stack();
    Stack(const Stack &);
    Stack& operator=(const Stack &);

public:
    size_t size();
    void push(const T &);
    // 修改过后的pop()接口
    // 在此前pop()存在隐晦的异常安全问题
    // 其实也存在着违反单一职责的原则（干了pop和top两件事情）
    // 不妨认为，职责越是简单，越容易做到异常安全
    void pop();
    T top();

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
    if(_size == _capacity) {
        T *data = copy_data(_data, _size, _capacity << 1);

        delete[] _data;
        _data = data;

        _capacity <<= 1;
    }

    _data[_size] = elem;
    _size++;
}

template <typename T>
void Stack<T>::pop() {
    if(_size == 0) {
        throw std::runtime_error("pop empty");
    }
    _size--;
}

template <typename T>
T Stack<T>::top() {
    if(_size == 0) {
        throw std::runtime_error("top empty");
    }
    return _data[_size - 1];
}

int main() {
    Stack<int> stk0;
    Stack<int> stk1(stk0);
    stk1 = stk0;

    size_t size = stk1.size();
    stk1.push(0b100);
    int v = stk1.top();
    stk1.pop();
}
