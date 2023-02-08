// 示例来自P0443 A Unified Executors Proposal for C++
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p0443r14.html
// 1.6.1 Algorithm then

#include <functional>
#include <type_traits>
#include <tuple>
#include <new>
#include <optional>
#include <exception>
extern "C" int printf(char const*, ...) noexcept;
extern "C" int puts(char const*) noexcept;
using std::invoke_result_t;

// C++20 concepts
template<class T, class... As>
concept constructible_from = std::is_constructible_v<T, As...>;

template<class T>
concept move_constructible = constructible_from<T, T>;

template<class T>
concept copy_constructible =
    move_constructible<T> &&
    constructible_from<T, T const&>;

template<class A, class B>
concept derived_from = std::is_base_of_v<B, A>;

template<class F, class... As>
concept invocable = requires {
    typename invoke_result_t<F, As...>;
};

// Generic sender/receiver customization points and concept
// definitions, simplified (not to spec).
inline namespace _receiver_cpos_ {
    inline constexpr struct _set_value_fn_ {
        template<class R, class... As>
            requires requires(R&& r, As&&... as) {
                ((R&&)r).set_value((As&&) as...);
            }
        void operator()(R&& r, As&&... as) const
            noexcept(noexcept(((R&&)r).set_value((As&&) as...))) {
            ((R&&)r).set_value((As&&) as...);
        }
    } set_value{};

    inline constexpr struct _set_error_fn_ {
        template<class R, class E>
            requires requires(R&& r, E&& e) {
                {((R&&)r).set_error((E&&) e)} noexcept;
            }
        void operator()(R&& r, E&& e) const noexcept {
            ((R&&)r).set_error((E&&) e);
        }
    } set_error{};

    inline constexpr struct _set_done_fn_ {
        template<class R>
            requires requires(R&& r) {
                {((R&&)r).set_done()} noexcept;
            }
        void operator()(R&& r) const noexcept {
            ((R&&)r).set_done();
        }
    } set_done{};
}

template<class R, class E = std::exception_ptr>
concept receiver =
    move_constructible<std::remove_cvref_t<R>> &&
    requires (R&& r, E&& e) {
        ::set_error((R&&) r, (E&&) e);
        ::set_done((R&&) r);
    };

template<class R,  class... As>
concept receiver_of =
    receiver<R> &&
    requires (R&& r, As&&... as) {
        ::set_value((R&&) r, (As&&) as...);
    };

template<class R, class... As>
inline constexpr bool is_nothrow_receiver_of_v =
    noexcept(::set_value(std::declval<R>(), std::declval<As>()...));

struct sender_base {};

template<class S>
struct sender_traits;

template<class S>
  requires derived_from<S, sender_base>
struct sender_traits<S> {};

template<class S>
concept sender =
    move_constructible<std::remove_cvref_t<S>> &&
    requires {
        // Opt-in to sender-ness by specializing sender_traits
        sizeof(sender_traits<std::remove_cvref_t<S>>);
    };

static_assert(!sender<int>);

inline namespace _sender_cpos_ {
    inline constexpr struct _start_fn_ {
        template<class O>
            requires requires(O&& o) {
                {((O&&)o).start()} noexcept;
            }
        void operator()(O&& o) const noexcept {
            ((O&&)o).start();
        }
    } start{};
}

template<class O>
concept operation_state =
    std::is_object_v<O> &&
    requires (O&& o) {
        ::start((O&&)o);
    };

inline namespace _sender_cpos_ {
    inline constexpr struct _connect_fn_ {
        template<sender S, receiver R>
            requires requires(S&& s, R&& r) {
                {((S&&)s).connect((R&&) r)} -> operation_state;
            }
        auto operator()(S&& s, R&& r) const
            noexcept(noexcept(((S&&)s).connect((R&&)r))) {
            return ((S&&)s).connect((R&&)r);
        }
    } connect{};
}

template<class S, class R>
concept sender_to =
    sender<S> &&
    receiver<R> &&
    requires(S&& s, R&& r) {
        ::connect((S&&)s, (R&&)r);
    };

template<class S, class R>
    requires sender_to<S, R>
using state_t =
    decltype(::connect(std::declval<S>(), std::declval<R>()));

/////////////////////////////////////////////////////////////////////////
// "then" algorithm implementation begins here
template<receiver R, class F>
struct _then_receiver {
    R r_;
    F f_;
    _then_receiver(R r, F f) : r_((R&&) r), f_((F&&) f) {}
    template<class... As, class Ret = invoke_result_t<F, As...>>
        requires receiver_of<R, Ret>
    void set_value(As&&... as) &&
        noexcept(std::is_nothrow_invocable_v<F, As...> &&
            is_nothrow_receiver_of_v<R, Ret>) {
        ::set_value((R&&) r_, std::invoke((F&&) f_, (As&&) as...));
    }
    template<class... As, class Ret = invoke_result_t<F, As...>>
        requires receiver_of<R> && std::is_void_v<Ret>
    void set_value(As&&... as) &&
        noexcept(std::is_nothrow_invocable_v<F, As...> &&
            is_nothrow_receiver_of_v<R>) {
        std::invoke((F&&) f_, (As&&) as...);
        ::set_value((R&&) r_);
    }
    template<class E>
        requires receiver<R, E>
    void set_error(E&& e) && noexcept {
        ::set_error((R&&) r_, (E&&) e);
    }
    void set_done() && noexcept {
        ::set_done((R&&) r_);
    }
};

template<sender S, class F>
struct _then_sender : sender_base {
    S s_;
    F f_;
    _then_sender(S s, F f) : s_((S&&)s), f_((F&&)f) {}
    template<receiver R>
        requires sender_to<S, _then_receiver<R, F>>
    auto connect(R r) && -> state_t<S, _then_receiver<R, F>> {
        return ::connect((S&&)s_, _then_receiver{(R&&)r, (F&&)f_});
    }
};

template<sender S, class F>
sender auto then(S s, F f) {
    return _then_sender{(S&&)s, (F&&)f};
}

// "then" algorithm implementation ends here
/////////////////////////////////////////////////////////////////////////

// A useful immediately executing multi-shot sender that sends some
// values through the value channel.
template<move_constructible... Ts>
struct just : sender_base {
    std::tuple<Ts...> ts_;
    explicit(sizeof...(Ts) == 1) constexpr just(Ts... ts):
        ts_{(Ts&&) ts...} {}
    template<receiver_of<Ts...> R>
    struct _op {
        R r_;
        std::tuple<Ts...> ts_;
        void start() && noexcept try {
            std::apply([this](Ts&&...ts) mutable {
                ::set_value((R&&)r_, (Ts&&)ts...);
            }, (std::tuple<Ts...>&&)ts_);
        } catch(...) {
            ::set_error((R&&) r_, std::current_exception());
        }
    };
    template<receiver_of<Ts...> R>
        requires (copy_constructible<Ts> &&...)
    auto connect(R r) const & -> _op<R> {
        return _op<R>{(R&&)r, ts_};
    }
    template<receiver_of<Ts...> R>
    auto connect(R r) && -> _op<R> {
        return _op<R>{(R&&)r, (std::tuple<Ts...>&&)ts_};
    }
};

inline constexpr struct _sink {
    void set_value(auto&&...) const noexcept {}
    [[noreturn]] void set_error(auto&&) const noexcept {
        std::terminate();
    }
    [[noreturn]] void set_done() const noexcept {
        std::terminate();
    }
} sink{};

int main() {
    auto j = just(1,2,3);
    auto f = [](int i, int j, int k) noexcept {
        printf("i=%d, j=%d, k=%d\n", i, j, k);
    };
    ::start(::connect(::then(j, f), sink));
}
