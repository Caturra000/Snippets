// 基于 cppcon2022 幻灯片：
// https://github.com/CppCon/CppCon2022/blob/main/Presentations/Type-Erasure-The-Implementation-Details-Klaus-Iglberger-CppCon-2022.pdf
// 让 gemini 写出对应的 benchmark。纯氛围跑分，我也没看它怎么写的
//
/*
clang++-20 -O3
--------------------------------------------------------------
Benchmark                    Time             CPU   Iterations
--------------------------------------------------------------
BM_ClassicOO             55666 ns        55649 ns        12054
BM_BasicTypeErasure      55788 ns        55770 ns        12192
BM_TypeErasure_SBO       48273 ns        48257 ns        13920
BM_SBO_MVD               44551 ns        44540 ns        16020
*/

#include <benchmark/benchmark.h>
#include <vector>
#include <memory>
#include <array>
#include <iostream>
#include <random>
#include <new>
#include <algorithm>

// ==========================================
// 0. 具体形状类 (Concrete Shapes)
// ==========================================
// 为了模拟 Slide 52，我们需要4种形状。
// 为了防止编译器优化掉空函数调用，我们做一些简单的计算模拟 translate()。

struct Circle {
    double x = 0, y = 0;
    void draw() { x += 1.0; y += 1.0; benchmark::DoNotOptimize(x); }
};

struct Square {
    double x = 0, y = 0;
    void draw() { x += 2.0; y += 2.0; benchmark::DoNotOptimize(y); }
};

struct Triangle {
    double x = 0, y = 0;
    void draw() { x -= 1.0; y -= 1.0; benchmark::DoNotOptimize(x); }
};

struct Rectangle {
    double x = 0, y = 0;
    void draw() { x -= 2.0; y -= 2.0; benchmark::DoNotOptimize(y); }
};

// ==========================================
// 1. Classic OO Solution (Inheritance)
// ==========================================

struct ShapeOO {
    virtual void draw() = 0;
    virtual ~ShapeOO() = default;
};

struct CircleOO : ShapeOO {
    Circle c;
    void draw() override { c.draw(); }
};

struct SquareOO : ShapeOO {
    Square s;
    void draw() override { s.draw(); }
};

struct TriangleOO : ShapeOO {
    Triangle t;
    void draw() override { t.draw(); }
};

struct RectangleOO : ShapeOO {
    Rectangle r;
    void draw() override { r.draw(); }
};

// ==========================================
// 2. Basic Type Erasure (std::unique_ptr)
// Reference: Slide 14-31
// ==========================================

class ShapeTE {
private:
    struct ShapeConcept {
        virtual ~ShapeConcept() = default;
        virtual void do_draw() = 0;
        virtual std::unique_ptr<ShapeConcept> clone() const = 0;
    };

    template<typename ShapeT>
    struct ShapeModel : public ShapeConcept {
        ShapeModel(ShapeT s) : shape(std::move(s)) {}
        void do_draw() override { shape.draw(); }
        std::unique_ptr<ShapeConcept> clone() const override {
            return std::make_unique<ShapeModel>(*this);
        }
        ShapeT shape;
    };

    std::unique_ptr<ShapeConcept> pimpl;

public:
    template<typename ShapeT>
    ShapeTE(ShapeT shape) : pimpl(std::make_unique<ShapeModel<ShapeT>>(std::move(shape))) {}

    // Copy operations
    ShapeTE(const ShapeTE& other) : pimpl(other.pimpl ? other.pimpl->clone() : nullptr) {}
    ShapeTE& operator=(const ShapeTE& other) {
        if (other.pimpl) pimpl = other.pimpl->clone();
        return *this;
    }

    // Move operations (default)
    ShapeTE(ShapeTE&&) = default;
    ShapeTE& operator=(ShapeTE&&) = default;

    void draw() { pimpl->do_draw(); }
};

// ==========================================
// 3. Type Erasure with SBO
// Reference: Slide 56-66
// ==========================================

class ShapeSBO {
private:
    static constexpr size_t buffersize = 64UL; // 稍微调大一点以容纳 shape
    static constexpr size_t alignment = 16UL;

    struct ShapeConcept {
        virtual ~ShapeConcept() = default;
        virtual void do_draw() = 0;
        virtual void move(void* buffer) = 0;
        // 省略了 clone 和 serialize 以简化 benchmark
    };

    template<typename ShapeT>
    struct ShapeModel : public ShapeConcept {
        ShapeModel(ShapeT s) : shape(std::move(s)) {}
        void do_draw() override { shape.draw(); }
        void move(void* buffer) override {
            ::new (buffer) ShapeModel(std::move(*this));
        }
        ShapeT shape;
    };

    alignas(alignment) std::array<std::byte, buffersize> buffer;

    ShapeConcept* pimpl() { return reinterpret_cast<ShapeConcept*>(buffer.data()); }
    const ShapeConcept* pimpl() const { return reinterpret_cast<const ShapeConcept*>(buffer.data()); }

public:
    template<typename ShapeT>
    ShapeSBO(ShapeT shape) {
        using Model = ShapeModel<ShapeT>;
        static_assert(sizeof(Model) <= buffersize, "Shape too large for SBO");
        ::new (buffer.data()) Model(std::move(shape));
    }

    ~ShapeSBO() { pimpl()->~ShapeConcept(); }
    
    // 简化：Benchmark主要测试 draw 性能，move构造用于 vector 初始化
    ShapeSBO(ShapeSBO&& other) noexcept {
        other.pimpl()->move(buffer.data());
    }

    void draw() { pimpl()->do_draw(); }
};

// ==========================================
// 4. SBO & Manual Virtual Dispatch (MVD)
// Reference: Slide 77-82 + Value Semantics
// 幻灯片展示的是 ShapeConstRef，这里为了 Benchmark 
// 适配为拥有所有权的值类型 (SBO + Function Ptr)
// ==========================================

class ShapeMVD {
private:
    static constexpr size_t buffersize = 64UL;
    static constexpr size_t alignment = 16UL;
    alignas(alignment) std::array<std::byte, buffersize> buffer;

    using DrawOp = void(*)(void*);
    using MoveOp = void(*)(void* src, void* dst);
    using DtorOp = void(*)(void*);

    DrawOp draw_op = nullptr;
    MoveOp move_op = nullptr;
    DtorOp dtor_op = nullptr;

public:
    template<typename ShapeT>
    ShapeMVD(ShapeT shape) {
        // 1. Store Data (SBO)
        static_assert(sizeof(ShapeT) <= buffersize);
        ::new (buffer.data()) ShapeT(std::move(shape));

        // 2. Setup VTable (Function Pointers)
        draw_op = [](void* ptr) {
            static_cast<ShapeT*>(ptr)->draw();
        };
        move_op = [](void* src, void* dst) {
            ::new (dst) ShapeT(std::move(*static_cast<ShapeT*>(src)));
        };
        dtor_op = [](void* ptr) {
            static_cast<ShapeT*>(ptr)->~ShapeT();
        };
    }

    ~ShapeMVD() {
        if (dtor_op) dtor_op(buffer.data());
    }

    ShapeMVD(ShapeMVD&& other) noexcept {
        if (other.move_op) {
            other.move_op(other.buffer.data(), buffer.data());
            draw_op = other.draw_op;
            move_op = other.move_op;
            dtor_op = other.dtor_op;
        }
    }

    void draw() {
        draw_op(buffer.data());
    }
};

// ==========================================
// Benchmark Setup
// Reference: Slide 52
// - 10,000 randomly generated shapes
// - Circles, Squares, Ellipses(Triangle), Rectangles
// ==========================================

constexpr int NUM_SHAPES = 10000;

template<typename T>
void generate_shapes(std::vector<T>& container) {
    container.reserve(NUM_SHAPES);
    std::mt19937 rng(42); // 固定种子
    std::uniform_int_distribution<int> dist(0, 3);

    for (int i = 0; i < NUM_SHAPES; ++i) {
        int type = dist(rng);
        switch (type) {
            case 0: container.emplace_back(Circle{}); break;
            case 1: container.emplace_back(Square{}); break;
            case 2: container.emplace_back(Triangle{}); break;
            case 3: container.emplace_back(Rectangle{}); break;
        }
    }
}

// OO 特化版 generator
void generate_oo_shapes(std::vector<std::unique_ptr<ShapeOO>>& container) {
    container.reserve(NUM_SHAPES);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 3);

    for (int i = 0; i < NUM_SHAPES; ++i) {
        int type = dist(rng);
        switch (type) {
            case 0: container.push_back(std::make_unique<CircleOO>()); break;
            case 1: container.push_back(std::make_unique<SquareOO>()); break;
            case 2: container.push_back(std::make_unique<TriangleOO>()); break;
            case 3: container.push_back(std::make_unique<RectangleOO>()); break;
        }
    }
}

// ---------------------------------------------------

static void BM_ClassicOO(benchmark::State& state) {
    std::vector<std::unique_ptr<ShapeOO>> shapes;
    generate_oo_shapes(shapes);

    for (auto _ : state) {
        for (auto& s : shapes) {
            s->draw();
        }
    }
}
BENCHMARK(BM_ClassicOO);

static void BM_BasicTypeErasure(benchmark::State& state) {
    std::vector<ShapeTE> shapes;
    generate_shapes(shapes);

    for (auto _ : state) {
        for (auto& s : shapes) {
            s.draw();
        }
    }
}
BENCHMARK(BM_BasicTypeErasure);

static void BM_TypeErasure_SBO(benchmark::State& state) {
    std::vector<ShapeSBO> shapes;
    generate_shapes(shapes);

    for (auto _ : state) {
        for (auto& s : shapes) {
            s.draw();
        }
    }
}
BENCHMARK(BM_TypeErasure_SBO);

static void BM_SBO_MVD(benchmark::State& state) {
    std::vector<ShapeMVD> shapes;
    generate_shapes(shapes);

    for (auto _ : state) {
        for (auto& s : shapes) {
            s.draw();
        }
    }
}
BENCHMARK(BM_SBO_MVD);

BENCHMARK_MAIN();
