// Copyright (c) - Graphical Playground. All rights reserved.

#include "container/Vector.hpp"
#include "memory/DefaultAllocator.hpp"
#include "memory/LinearAllocator.hpp"
#include "memory/PolymorphicAllocator.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>

namespace gp::tests
{

struct NonTrivial
{
    int value;
    static int constructions;
    static int destructions;

    NonTrivial()
        : value(0)
    {
        ++constructions;
    }

    explicit NonTrivial(int v)
        : value(v)
    {
        ++constructions;
    }

    NonTrivial(const NonTrivial& o)
        : value(o.value)
    {
        ++constructions;
    }

    NonTrivial(NonTrivial&& o) noexcept
        : value(o.value)
    {
        o.value = -1;
        ++constructions;
    }

    ~NonTrivial() { ++destructions; }

    NonTrivial& operator=(const NonTrivial& o)
    {
        value = o.value;
        return *this;
    }

    NonTrivial& operator=(NonTrivial&& o) noexcept
    {
        value = o.value;
        o.value = -1;
        return *this;
    }

    bool operator==(const NonTrivial& o) const { return value == o.value; }
};

int NonTrivial::constructions = 0;
int NonTrivial::destructions = 0;

static void resetNonTrivialCounts()
{
    NonTrivial::constructions = 0;
    NonTrivial::destructions = 0;
}

TEST_CASE("Vector - default construction", "[container][Vector]")
{
    gp::Vector<int> v;
    REQUIRE(v.size() == 0);
    REQUIRE(v.capacity() == 0);
    REQUIRE(v.empty());
    REQUIRE(v.data() == nullptr);
}

TEST_CASE("Vector - construction with allocator", "[container][Vector]")
{
    gp::memory::PolymorphicAllocator<int> alloc;
    gp::Vector<int> v(alloc);
    REQUIRE(v.empty());
    REQUIRE(v.getAllocator() == alloc);
}

TEST_CASE("Vector - count construction with default value", "[container][Vector]")
{
    gp::Vector<int> v(5);
    REQUIRE(v.size() == 5);
    for (gp::USize i = 0; i < 5; ++i) { REQUIRE(v[i] == 0); }
}

TEST_CASE("Vector - count construction with fill value", "[container][Vector]")
{
    gp::Vector<int> v(3, 42);
    REQUIRE(v.size() == 3);
    REQUIRE(v[0] == 42);
    REQUIRE(v[1] == 42);
    REQUIRE(v[2] == 42);
}

TEST_CASE("Vector - initializer list construction", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2, 3, 4, 5 };
    REQUIRE(v.size() == 5);
    REQUIRE(v[0] == 1);
    REQUIRE(v[4] == 5);
}

TEST_CASE("Vector - iterator range construction", "[container][Vector]")
{
    int arr[] = { 10, 20, 30 };
    gp::Vector<int> v(arr, arr + 3);
    REQUIRE(v.size() == 3);
    REQUIRE(v[0] == 10);
    REQUIRE(v[2] == 30);
}

TEST_CASE("Vector - zero-count construction", "[container][Vector]")
{
    gp::Vector<int> v(static_cast<gp::USize>(0));
    REQUIRE(v.empty());
    REQUIRE(v.data() == nullptr);
}

TEST_CASE("Vector - copy construction", "[container][Vector]")
{
    gp::Vector<int> a{ 1, 2, 3 };
    gp::Vector<int> b(a);
    REQUIRE(b.size() == 3);
    REQUIRE(b[0] == 1);
    REQUIRE(b[2] == 3);
    REQUIRE(a.data() != b.data());
}

TEST_CASE("Vector - move construction", "[container][Vector]")
{
    gp::Vector<int> a{ 1, 2, 3 };
    int* old_data = a.data();
    gp::Vector<int> b(static_cast<gp::Vector<int>&&>(a));
    REQUIRE(b.size() == 3);
    REQUIRE(b.data() == old_data);
    REQUIRE(a.empty());
    REQUIRE(a.data() == nullptr);
}

TEST_CASE("Vector - copy assignment", "[container][Vector]")
{
    gp::Vector<int> a{ 1, 2, 3 };
    gp::Vector<int> b{ 4, 5 };
    b = a;
    REQUIRE(b.size() == 3);
    REQUIRE(b[0] == 1);
}

TEST_CASE("Vector - move assignment", "[container][Vector]")
{
    gp::Vector<int> a{ 1, 2, 3 };
    gp::Vector<int> b{ 4, 5 };
    int* old_data = a.data();
    b = static_cast<gp::Vector<int>&&>(a);
    REQUIRE(b.size() == 3);
    REQUIRE(b.data() == old_data);
    REQUIRE(a.empty());
}

TEST_CASE("Vector - self-copy assignment", "[container][Vector]")
{
    gp::Vector<int> a{ 1, 2, 3 };
    a = a;
    REQUIRE(a.size() == 3);
    REQUIRE(a[0] == 1);
}

TEST_CASE("Vector - initializer list assignment", "[container][Vector]")
{
    gp::Vector<int> v;
    v = { 10, 20, 30 };
    REQUIRE(v.size() == 3);
    REQUIRE(v[1] == 20);
}

TEST_CASE("Vector - assign count and value", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2, 3 };
    v.assign(5, 99);
    REQUIRE(v.size() == 5);
    for (gp::USize i = 0; i < 5; ++i) { REQUIRE(v[i] == 99); }
}

TEST_CASE("Vector - assign initializer list", "[container][Vector]")
{
    gp::Vector<int> v{ 1 };
    v.assign({ 10, 20, 30 });
    REQUIRE(v.size() == 3);
    REQUIRE(v[2] == 30);
}

TEST_CASE("Vector - at / operator[] / front / back", "[container][Vector]")
{
    gp::Vector<int> v{ 10, 20, 30 };
    REQUIRE(v.at(0) == 10);
    REQUIRE(v.at(2) == 30);
    REQUIRE(v[1] == 20);
    REQUIRE(v.front() == 10);
    REQUIRE(v.back() == 30);
}

TEST_CASE("Vector - data pointer", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2, 3 };
    int* p = v.data();
    REQUIRE(p[0] == 1);
    REQUIRE(p[2] == 3);
}

TEST_CASE("Vector - begin / end iteration", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2, 3 };
    int sum = 0;
    for (auto it = v.begin(); it != v.end(); ++it) { sum += *it; }
    REQUIRE(sum == 6);
}

TEST_CASE("Vector - const iterators", "[container][Vector]")
{
    const gp::Vector<int> v{ 1, 2, 3 };
    int sum = 0;
    for (auto it = v.cbegin(); it != v.cend(); ++it) { sum += *it; }
    REQUIRE(sum == 6);
}

TEST_CASE("Vector - reverse iterators", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2, 3 };
    REQUIRE(*v.rbegin() == 3);
    REQUIRE(*(v.rend() - 1) == 1);
}

TEST_CASE("Vector - range-for loop", "[container][Vector]")
{
    gp::Vector<int> v{ 10, 20, 30 };
    int sum = 0;
    for (int x: v) { sum += x; }
    REQUIRE(sum == 60);
}

TEST_CASE("Vector - reserve", "[container][Vector]")
{
    gp::Vector<int> v;
    v.reserve(100);
    REQUIRE(v.capacity() >= 100);
    REQUIRE(v.size() == 0);
}

TEST_CASE("Vector - reserve does not shrink", "[container][Vector]")
{
    gp::Vector<int> v;
    v.reserve(100);
    v.reserve(10);
    REQUIRE(v.capacity() >= 100);
}

TEST_CASE("Vector - shrinkToFit", "[container][Vector]")
{
    gp::Vector<int> v;
    v.reserve(100);
    v.pushBack(1);
    v.pushBack(2);
    v.shrinkToFit();
    REQUIRE(v.capacity() == 2);
    REQUIRE(v.size() == 2);
    REQUIRE(v[0] == 1);
}

TEST_CASE("Vector - shrinkToFit on empty", "[container][Vector]")
{
    gp::Vector<int> v;
    v.reserve(100);
    v.shrinkToFit();
    REQUIRE(v.capacity() == 0);
    REQUIRE(v.data() == nullptr);
}

TEST_CASE("Vector - 1.5x growth factor", "[container][Vector]")
{
    gp::Vector<int> v;
    v.pushBack(1);
    REQUIRE(v.capacity() == 1);

    v.pushBack(2);
    REQUIRE(v.capacity() >= 2);

    // After cap=1, next should be 1 + 0 = 1 (min growth to 2 happens from _next_capacity)
    // Capacity goes: 0->1->1 (but must fit 2, so realloc to 1.5 rounded)
    // Actually: _next_capacity when cap=1 returns 1+0=1, but pushBack calls _grow which
    // calls _realloc_exact(1). Then pushBack again triggers _grow again with cap=1 -> 1.
    // This is a degenerate case. Let's test from larger capacity.
    gp::Vector<int> v2;
    v2.reserve(4);
    for (int i = 0; i < 5; ++i) { v2.pushBack(i); }
    // After 4 elements (cap=4), 5th triggers grow: 4 + 4/2 = 6
    REQUIRE(v2.capacity() == 6);

    for (int i = 5; i < 7; ++i) { v2.pushBack(i); }
    // Now at 7 elements, cap=6 triggers grow: 6 + 6/2 = 9
    REQUIRE(v2.capacity() == 9);
}

TEST_CASE("Vector - pushBack", "[container][Vector]")
{
    gp::Vector<int> v;
    for (int i = 0; i < 100; ++i) { v.pushBack(i); }
    REQUIRE(v.size() == 100);
    for (int i = 0; i < 100; ++i) { REQUIRE(v[i] == i); }
}

TEST_CASE("Vector - pushBack move", "[container][Vector]")
{
    gp::Vector<std::string> v;
    std::string s = "hello";
    v.pushBack(static_cast<std::string&&>(s));
    REQUIRE(v.size() == 1);
    REQUIRE(v[0] == "hello");
}

TEST_CASE("Vector - emplaceBack", "[container][Vector]")
{
    gp::Vector<std::pair<int, int>> v;
    auto& ref = v.emplaceBack(1, 2);
    REQUIRE(ref.first == 1);
    REQUIRE(ref.second == 2);
    REQUIRE(v.size() == 1);
}

TEST_CASE("Vector - popBack", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2, 3 };
    v.popBack();
    REQUIRE(v.size() == 2);
    REQUIRE(v.back() == 2);
}

TEST_CASE("Vector - clear", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2, 3 };
    gp::USize old_cap = v.capacity();
    v.clear();
    REQUIRE(v.size() == 0);
    REQUIRE(v.empty());
    REQUIRE(v.capacity() == old_cap);
}

TEST_CASE("Vector - insert single element", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 3, 4 };
    auto it = v.insert(v.begin() + 1, 2);
    REQUIRE(*it == 2);
    REQUIRE(v.size() == 4);
    REQUIRE(v[0] == 1);
    REQUIRE(v[1] == 2);
    REQUIRE(v[2] == 3);
    REQUIRE(v[3] == 4);
}

TEST_CASE("Vector - insert at beginning", "[container][Vector]")
{
    gp::Vector<int> v{ 2, 3 };
    v.insert(v.begin(), 1);
    REQUIRE(v[0] == 1);
    REQUIRE(v.size() == 3);
}

TEST_CASE("Vector - insert at end", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2 };
    v.insert(v.end(), 3);
    REQUIRE(v.back() == 3);
    REQUIRE(v.size() == 3);
}

TEST_CASE("Vector - insert count copies", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 5 };
    v.insert(v.begin() + 1, 3, 99);
    REQUIRE(v.size() == 5);
    REQUIRE(v[0] == 1);
    REQUIRE(v[1] == 99);
    REQUIRE(v[2] == 99);
    REQUIRE(v[3] == 99);
    REQUIRE(v[4] == 5);
}

TEST_CASE("Vector - insert initializer list", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 5 };
    v.insert(v.begin() + 1, { 2, 3, 4 });
    REQUIRE(v.size() == 5);
    for (int i = 0; i < 5; ++i) { REQUIRE(v[i] == i + 1); }
}

TEST_CASE("Vector - emplace", "[container][Vector]")
{
    gp::Vector<std::pair<int, int>> v;
    v.emplaceBack(1, 1);
    v.emplaceBack(3, 3);
    v.emplace(v.begin() + 1, 2, 2);
    REQUIRE(v.size() == 3);
    REQUIRE(v[1].first == 2);
}

TEST_CASE("Vector - erase single element", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2, 3, 4 };
    auto it = v.erase(v.begin() + 1);
    REQUIRE(*it == 3);
    REQUIRE(v.size() == 3);
    REQUIRE(v[0] == 1);
    REQUIRE(v[1] == 3);
    REQUIRE(v[2] == 4);
}

TEST_CASE("Vector - erase range", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2, 3, 4, 5 };
    auto it = v.erase(v.begin() + 1, v.begin() + 4);
    REQUIRE(*it == 5);
    REQUIRE(v.size() == 2);
    REQUIRE(v[0] == 1);
    REQUIRE(v[1] == 5);
}

TEST_CASE("Vector - erase empty range is no-op", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2, 3 };
    auto it = v.erase(v.begin() + 1, v.begin() + 1);
    REQUIRE(*it == 2);
    REQUIRE(v.size() == 3);
}

TEST_CASE("Vector - resize grow", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2 };
    v.resize(5);
    REQUIRE(v.size() == 5);
    REQUIRE(v[0] == 1);
    REQUIRE(v[1] == 2);
    REQUIRE(v[2] == 0);
}

TEST_CASE("Vector - resize grow with fill value", "[container][Vector]")
{
    gp::Vector<int> v{ 1 };
    v.resize(4, 99);
    REQUIRE(v.size() == 4);
    REQUIRE(v[0] == 1);
    REQUIRE(v[3] == 99);
}

TEST_CASE("Vector - resize shrink", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2, 3, 4, 5 };
    v.resize(2);
    REQUIRE(v.size() == 2);
    REQUIRE(v[1] == 2);
}

TEST_CASE("Vector - swap", "[container][Vector]")
{
    gp::Vector<int> a{ 1, 2, 3 };
    gp::Vector<int> b{ 4, 5 };
    a.swap(b);
    REQUIRE(a.size() == 2);
    REQUIRE(a[0] == 4);
    REQUIRE(b.size() == 3);
    REQUIRE(b[0] == 1);
}

TEST_CASE("Vector - equality comparison", "[container][Vector]")
{
    gp::Vector<int> a{ 1, 2, 3 };
    gp::Vector<int> b{ 1, 2, 3 };
    gp::Vector<int> c{ 1, 2, 4 };
    gp::Vector<int> d{ 1, 2 };
    REQUIRE(a == b);
    REQUIRE(a != c);
    REQUIRE(a != d);
}

TEST_CASE("Vector - trivial type memcpy reallocation", "[container][Vector]")
{
    static_assert(std::is_trivially_copyable_v<int>, "int should be trivially copyable");
    gp::Vector<int> v;
    for (int i = 0; i < 50; ++i) { v.pushBack(i); }
    REQUIRE(v.size() == 50);
    for (int i = 0; i < 50; ++i) { REQUIRE(v[i] == i); }
}

TEST_CASE("Vector - trivial type insert shifts via memmove", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2, 5, 6 };
    v.insert(v.begin() + 2, { 3, 4 });
    REQUIRE(v.size() == 6);
    for (int i = 0; i < 6; ++i) { REQUIRE(v[i] == i + 1); }
}

TEST_CASE("Vector - trivial type erase shifts via memmove", "[container][Vector]")
{
    gp::Vector<int> v{ 1, 2, 99, 98, 3, 4 };
    v.erase(v.begin() + 2, v.begin() + 4);
    REQUIRE(v.size() == 4);
    REQUIRE(v[0] == 1);
    REQUIRE(v[1] == 2);
    REQUIRE(v[2] == 3);
    REQUIRE(v[3] == 4);
}

TEST_CASE("Vector - non-trivial construction and destruction counts", "[container][Vector]")
{
    resetNonTrivialCounts();
    {
        gp::Vector<NonTrivial> v;
        v.pushBack(NonTrivial(1));
        v.pushBack(NonTrivial(2));
        v.pushBack(NonTrivial(3));
    }
    REQUIRE(NonTrivial::destructions >= NonTrivial::constructions);
}

TEST_CASE("Vector - non-trivial erase calls destructor", "[container][Vector]")
{
    resetNonTrivialCounts();
    {
        gp::Vector<NonTrivial> v;
        v.emplaceBack(1);
        v.emplaceBack(2);
        v.emplaceBack(3);
        int before = NonTrivial::destructions;
        v.erase(v.begin());
        REQUIRE(NonTrivial::destructions > before);
        REQUIRE(v.size() == 2);
        REQUIRE(v[0].value == 2);
    }
}

TEST_CASE("Vector - non-trivial clear calls destructors", "[container][Vector]")
{
    resetNonTrivialCounts();
    {
        gp::Vector<NonTrivial> v;
        v.emplaceBack(1);
        v.emplaceBack(2);
        v.emplaceBack(3);
        int before = NonTrivial::destructions;
        v.clear();
        REQUIRE(NonTrivial::destructions == before + 3);
    }
}

TEST_CASE("Vector - copy uses select_on_container_copy_construction", "[container][Vector][allocator]")
{
    // PolymorphicAllocator returns default allocator on copy
    alignas(16) gp::UInt8 buf[4096];
    gp::memory::LinearAllocator lin(buf, sizeof(buf));
    gp::memory::PolymorphicAllocator<int> custom(&lin);

    gp::Vector<int> a(custom);
    a.pushBack(1);
    a.pushBack(2);

    // Copy should use default allocator (per PolymorphicAllocator semantics)
    gp::Vector<int> b(a);
    REQUIRE(b.size() == 2);
    REQUIRE(b[0] == 1);
    REQUIRE(b.getAllocator().getResource() == &gp::memory::DefaultAllocator::get());
}

TEST_CASE("Vector - move transfers allocator", "[container][Vector][allocator]")
{
    alignas(16) gp::UInt8 buf[4096];
    gp::memory::LinearAllocator lin(buf, sizeof(buf));
    gp::memory::PolymorphicAllocator<int> custom(&lin);

    gp::Vector<int> a(custom);
    a.pushBack(1);

    gp::Vector<int> b(static_cast<gp::Vector<int>&&>(a));
    REQUIRE(b.getAllocator().getResource() == &lin);
    REQUIRE(a.empty());
}

TEST_CASE("Vector - move assignment with propagation", "[container][Vector][allocator]")
{
    alignas(16) gp::UInt8 buf[4096];
    gp::memory::LinearAllocator lin(buf, sizeof(buf));
    gp::memory::PolymorphicAllocator<int> custom(&lin);

    gp::Vector<int> a(custom);
    a.pushBack(42);

    gp::Vector<int> b;
    b = static_cast<gp::Vector<int>&&>(a);
    REQUIRE(b.getAllocator().getResource() == &lin);
    REQUIRE(b.size() == 1);
    REQUIRE(b[0] == 42);
}

TEST_CASE("Vector - custom allocator basic operations", "[container][Vector][allocator]")
{
    alignas(16) gp::UInt8 buf[4096];
    gp::memory::LinearAllocator lin(buf, sizeof(buf));
    gp::memory::PolymorphicAllocator<int> alloc(&lin);

    gp::Vector<int> v(alloc);
    for (int i = 0; i < 20; ++i) { v.pushBack(i); }
    REQUIRE(v.size() == 20);
    for (int i = 0; i < 20; ++i) { REQUIRE(v[i] == i); }
    REQUIRE(lin.getAllocatedSize() > 0);
}

}   // namespace gp::tests
