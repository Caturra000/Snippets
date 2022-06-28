// from purecpp.org

#include <memory>
#include <coroutine>
#include <iostream>
namespace test {
template<typename T>
struct sync {
  std::shared_ptr<T> value;
  sync(std::shared_ptr<T> p)
    : value(p) {
    std::cout << "Construct a sync object" << std::endl;
  }
  sync(const sync& s)
    : value(s.value) {
    std::cout << "Copied a sync object" << std::endl;
  }
  ~sync() {
    std::cout << "Destruct a sync object " << std::endl;
  }
  T get() {
    std::cout << "We got the return value..." << std::endl;
    return *value;
  }
  struct promise_type {
    std::shared_ptr<T> ptr;
    promise_type()
      : ptr(std::make_shared<T>()) {
      std::cout << "Promise created" << std::endl;
    }
    ~promise_type() {
      std::cout << "Promise died" << std::endl;
    }
    sync get_return_object() {
      std::cout << "get_return_object a sync object to the caller" << std::endl;
      return ptr;
    }
    auto initial_suspend() {
      std::cout << "Started the coroutine" << std::endl;
      return std::suspend_never{};
    }
    void return_value(T v) {
      std::cout << "Got coroutine result " << v << std::endl;
      *ptr = v;
    }
    auto final_suspend() noexcept {
      std::cout << "Finished the coroutine" << std::endl;
      return std::suspend_never{};
    }
    void unhandled_exception() {
      std::exit(1);
    }
  };
};
sync<int> my_coroutine() {
  std::cout << "Execute the coroutine function body" << std::endl;
  co_return 42;
}
} // test
int main() {
  auto coro = test::my_coroutine();
  std::cout << "Created a coroutine" << std::endl;
  auto result = coro.get();
  std::cout << "The coroutine result: " << result << std::endl;
}


/*
Promise created
get_return_object a sync object to the caller
Construct a sync object
Started the coroutine
Execute the coroutine function body
Got coroutine result 42
Finished the coroutine
Promise died
Created a coroutine
We got the return value...
The coroutine result: 42
Destruct a sync object
*/