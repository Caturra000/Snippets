// case7是针对case3的另一种解决方案
// 这种方式通过智能指针来解决接口上的设计问题
// 相当于把ctor的过程提前了，这样我们可以控制是否异常安全
//
// Note: case4和case7是可以一起用的

#include <algorithm>
#include <stdexcept>
#include <memory>

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
    // 改为返回智能指针
    std::shared_ptr<T> pop();

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
std::shared_ptr<T> Stack<T>::pop() {
    if(_size == 0) {
        throw std::runtime_error("empty");
    }
    T &elem = _data[_size - 1];
    // 返回的是智能指针，那么外部获取该返回值时不会抛出异常
    // 注意不要std::move(elem)，shared_ptr的构造可能会fail
    //
    // Note:
    // 这里也许可以应用移动语义
    // 因为shared_ptr的构造过程，涉及到先分配内存再移动内部资源
    // 如果fail，那也是第一步的事情，此时elem实际上并没有move ctor
    // 但是也并不能说T类型的move过程是真的noexcept，虽然一般来说没问题
    auto ret = std::make_shared<T>(elem);
    _size--;
    return ret;
}

int main() {
    Stack<int> stk0;
    Stack<int> stk1(stk0);
    stk1 = stk0;

    size_t size = stk1.size();
    stk1.push(0b111);
    auto ret = stk1.pop();
}
