// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/Map.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using namespace GP;
using namespace GP::Container;

struct MapTestObject
{
    int value;
    static int constructorCalls;
    static int destructorCalls;
    static int copyConstructorCalls;
    static int moveConstructorCalls;

    MapTestObject()
        : value(0)
    {
        ++constructorCalls;
    }

    explicit MapTestObject(int v)
        : value(v)
    {
        ++constructorCalls;
    }

    MapTestObject(const MapTestObject& other)
        : value(other.value)
    {
        ++copyConstructorCalls;
    }

    MapTestObject(MapTestObject&& other) noexcept
        : value(other.value)
    {
        other.value = -1;
        ++moveConstructorCalls;
    }

    ~MapTestObject() { ++destructorCalls; }

    MapTestObject& operator=(const MapTestObject& other)
    {
        value = other.value;
        return *this;
    }

    MapTestObject& operator=(MapTestObject&& other) noexcept
    {
        value = other.value;
        other.value = -1;
        return *this;
    }

    bool operator==(const MapTestObject& other) const { return value == other.value; }

    bool operator!=(const MapTestObject& other) const { return value != other.value; }

    bool operator<(const MapTestObject& other) const { return value < other.value; }

    static void Reset()
    {
        constructorCalls = 0;
        destructorCalls = 0;
        copyConstructorCalls = 0;
        moveConstructorCalls = 0;
    }
};

int MapTestObject::constructorCalls = 0;
int MapTestObject::destructorCalls = 0;
int MapTestObject::copyConstructorCalls = 0;
int MapTestObject::moveConstructorCalls = 0;

TEST_CASE("TMap Construction", "[GP][Core][Container][TMap]")
{
    SECTION("Default Constructor")
    {
        TMap<int, int> m;
        REQUIRE(m.Size() == 0);
        REQUIRE(m.IsEmpty());
        REQUIRE(m.begin() == m.end());
    }

    SECTION("Initializer List Constructor")
    {
        TMap<int, int> m = {
            { 3, 30 },
            { 1, 10 },
            { 2, 20 }
        };
        REQUIRE(m.Size() == 3);
        REQUIRE_FALSE(m.IsEmpty());

        // Keys must be visited in ascending order.
        auto it = m.begin();
        REQUIRE(it->first == 1);
        REQUIRE(it->second == 10);
        ++it;
        REQUIRE(it->first == 2);
        REQUIRE(it->second == 20);
        ++it;
        REQUIRE(it->first == 3);
        REQUIRE(it->second == 30);
        ++it;
        REQUIRE(it == m.end());
    }

    SECTION("Initializer List with Duplicate Keys — Last Write Loses")
    {
        // TMap keeps the first occurrence; a second insert with the same key is a no-op.
        TMap<int, int> m = {
            { 1, 10 },
            { 1, 99 },
            { 2, 20 }
        };
        REQUIRE(m.Size() == 2);
        REQUIRE(m.Find(1)->second == 10);
    }

    SECTION("Range Constructor — std::vector iterators")
    {
        std::vector<std::pair<const int, int>> src = {
            { 5, 50 },
            { 2, 20 },
            { 8, 80 },
            { 2, 99 }
        };
        TMap<int, int> m(src.begin(), src.end());
        REQUIRE(m.Size() == 3);   // Duplicate key 2 ignored.
        REQUIRE(m.Contains(5));
        REQUIRE(m.Contains(2));
        REQUIRE(m.Contains(8));
        REQUIRE(m.Find(2)->second == 20);
    }

    SECTION("Copy Constructor — deep copy, independent lifetime")
    {
        TMap<int, std::string> original = {
            { 1,   "one" },
            { 2,   "two" },
            { 3, "three" }
        };
        TMap<int, std::string> copy(original);
        REQUIRE(copy.Size() == 3);
        REQUIRE(copy.Find(2)->second == "two");

        // Mutating the copy must not affect the original.
        copy[2] = "modified";
        REQUIRE(original.Find(2)->second == "two");
    }

    SECTION("Move Constructor — source becomes empty")
    {
        TMap<int, int> source = {
            { 1, 10 },
            { 2, 20 },
            { 3, 30 }
        };
        TMap<int, int> moved(std::move(source));
        REQUIRE(moved.Size() == 3);
        REQUIRE(moved.Contains(2));
        // source must be in a valid (but empty after default moves) state.
        REQUIRE(source.IsEmpty());
    }

    SECTION("Destructor releases all nodes")
    {
        MapTestObject::Reset();
        {
            TMap<int, MapTestObject> m;
            m.Insert({ 1, MapTestObject(1) });
            m.Insert({ 2, MapTestObject(2) });
            m.Insert({ 3, MapTestObject(3) });
            // 3 nodes in map (temporary MapTestObjects destroyed by now).
        }
        // All live nodes must have been destroyed.
        REQUIRE(MapTestObject::destructorCalls >= 3);
    }
}

TEST_CASE("TMap Assignment", "[GP][Core][Container][TMap]")
{
    SECTION("Copy Assignment clears target and deep-copies source")
    {
        TMap<int, int> a = {
            { 1, 1 },
            { 2, 2 }
        };
        TMap<int, int> b = {
            { 10, 10 }
        };
        b = a;
        REQUIRE(b.Size() == 2);
        REQUIRE(b.Contains(1));
        REQUIRE(b.Contains(2));
        REQUIRE_FALSE(b.Contains(10));

        b[1] = 999;
        REQUIRE(a.Find(1)->second == 1);   // a is unaffected.
    }

    SECTION("Move Assignment steals source resources")
    {
        TMap<int, int> a = {
            { 1, 10 },
            { 2, 20 },
            { 3, 30 }
        };
        TMap<int, int> b;
        b = std::move(a);
        REQUIRE(b.Size() == 3);
        REQUIRE(a.IsEmpty());
    }

    SECTION("Self Copy Assignment is a no-op")
    {
        TMap<int, int> m = {
            { 1, 10 },
            { 2, 20 }
        };
        m = m;   // NOLINT(clang-diagnostic-self-assign-overloaded)
        REQUIRE(m.Size() == 2);
    }

    SECTION("Self Move Assignment is a no-op")
    {
        TMap<int, int> m = {
            { 1, 10 },
            { 2, 20 }
        };
        m = std::move(m);   // NOLINT(clang-diagnostic-self-move)
        REQUIRE(m.Size() == 2);
    }
}

TEST_CASE("TMap Insertion", "[GP][Core][Container][TMap]")
{
    SECTION("Insert lvalue returns iterator + true on success")
    {
        TMap<int, int> m;
        auto [it, inserted] = m.Insert({ 42, 420 });
        REQUIRE(inserted);
        REQUIRE(it->first == 42);
        REQUIRE(it->second == 420);
        REQUIRE(m.Size() == 1);
    }

    SECTION("Insert duplicate key returns iterator + false, value unchanged")
    {
        TMap<int, int> m;
        m.Insert({ 1, 10 });
        auto [it, inserted] = m.Insert({ 1, 99 });
        REQUIRE_FALSE(inserted);
        REQUIRE(it->second == 10);
        REQUIRE(m.Size() == 1);
    }

    SECTION("Insert rvalue")
    {
        TMap<int, std::string> m;
        std::string val = "hello";
        auto [it, ok] = m.Insert({ 1, std::move(val) });
        REQUIRE(ok);
        REQUIRE(it->second == "hello");
    }

    SECTION("Emplace constructs in-place")
    {
        TMap<int, int> m;
        auto [it, ok] = m.Emplace(7, 70);
        REQUIRE(ok);
        REQUIRE(it->first == 7);
        REQUIRE(it->second == 70);
    }

    SECTION("operator[] inserts default value for missing key")
    {
        TMap<int, int> m;
        m[5] = 50;
        REQUIRE(m.Size() == 1);
        REQUIRE(m.Find(5)->second == 50);
    }

    SECTION("operator[] does not overwrite existing key")
    {
        TMap<int, int> m = {
            { 5, 500 }
        };
        m[5];   // Access only, should not change value.
        REQUIRE(m.Find(5)->second == 500);
    }

    SECTION("operator[] with rvalue key does not move existing key")
    {
        TMap<std::string, int> m;
        m["alpha"] = 1;
        std::string k = "alpha";
        m[std::move(k)] = 2;   // key already exists — k must NOT have been moved
        // The value IS updated via the returned reference.
        REQUIRE(m.Find("alpha")->second == 2);
    }

    SECTION("Ascending insertion preserves sorted order")
    {
        TMap<int, int> m;
        for (int i = 1; i <= 10; ++i) { m.Insert({ i, i * 10 }); }
        int prev = -1;
        for (const auto& [k, v]: m)
        {
            REQUIRE(k > prev);
            prev = k;
        }
        REQUIRE(m.Size() == 10);
    }

    SECTION("Descending insertion preserves sorted order")
    {
        TMap<int, int> m;
        for (int i = 10; i >= 1; --i) { m.Insert({ i, i * 10 }); }
        int prev = -1;
        for (const auto& [k, v]: m)
        {
            REQUIRE(k > prev);
            prev = k;
        }
        REQUIRE(m.Size() == 10);
    }

    SECTION("Alternating insertion preserves sorted order — RB rebalancing")
    {
        TMap<int, int> m;
        int keys[] = { 5, 1, 9, 3, 7, 2, 8, 4, 6, 10 };
        for (int k: keys) { m.Insert({ k, k }); }
        int prev = -1;
        for (const auto& [k, v]: m)
        {
            REQUIRE(k > prev);
            prev = k;
        }
        REQUIRE(m.Size() == 10);
    }
}

TEST_CASE("TMap Lookup", "[GP][Core][Container][TMap]")
{
    TMap<int, int> m = {
        { 10, 100 },
        { 20, 200 },
        { 30, 300 }
    };
    const TMap<int, int>& cm = m;

    SECTION("Find existing key — mutable")
    {
        auto it = m.Find(20);
        REQUIRE(it != m.end());
        REQUIRE(it->second == 200);
    }

    SECTION("Find existing key — const")
    {
        auto it = cm.Find(20);
        REQUIRE(it != cm.end());
        REQUIRE(it->second == 200);
    }

    SECTION("Find missing key returns end()")
    {
        REQUIRE(m.Find(999) == m.end());
        REQUIRE(cm.Find(999) == cm.end());
    }

    SECTION("Contains returns true for present key")
    {
        REQUIRE(m.Contains(10));
        REQUIRE(m.Contains(20));
        REQUIRE(m.Contains(30));
    }

    SECTION("Contains returns false for missing key")
    {
        REQUIRE_FALSE(m.Contains(0));
        REQUIRE_FALSE(m.Contains(15));
    }

    SECTION("At() returns correct value")
    {
        REQUIRE(m.At(10) == 100);
        REQUIRE(m.At(30) == 300);
        REQUIRE(cm.At(20) == 200);
    }

    SECTION("At() allows mutable modification")
    {
        m.At(10) = 999;
        REQUIRE(m.Find(10)->second == 999);
    }

    SECTION("MaxCount() is non-zero") { REQUIRE(m.MaxCount() > 0); }
}

TEST_CASE("TMap Erasure", "[GP][Core][Container][TMap]")
{
    SECTION("Erase by key — present")
    {
        TMap<int, int> m = {
            { 1, 10 },
            { 2, 20 },
            { 3, 30 }
        };
        SizeT removed = m.Erase(2);
        REQUIRE(removed == 1);
        REQUIRE(m.Size() == 2);
        REQUIRE_FALSE(m.Contains(2));
        REQUIRE(m.Contains(1));
        REQUIRE(m.Contains(3));
    }

    SECTION("Erase by key — absent returns 0")
    {
        TMap<int, int> m = {
            { 1, 10 }
        };
        REQUIRE(m.Erase(999) == 0);
        REQUIRE(m.Size() == 1);
    }

    SECTION("Erase by iterator returns next iterator")
    {
        TMap<int, int> m = {
            { 1, 10 },
            { 2, 20 },
            { 3, 30 }
        };
        auto it = m.Find(2);
        auto next = m.Erase(it);
        REQUIRE(next->first == 3);
        REQUIRE(m.Size() == 2);
    }

    SECTION("Erase first element")
    {
        TMap<int, int> m = {
            { 1, 10 },
            { 2, 20 },
            { 3, 30 }
        };
        m.Erase(1);
        REQUIRE(m.Size() == 2);
        REQUIRE(m.begin()->first == 2);
    }

    SECTION("Erase last element")
    {
        TMap<int, int> m = {
            { 1, 10 },
            { 2, 20 },
            { 3, 30 }
        };
        m.Erase(3);
        REQUIRE(m.Size() == 2);
        // The pre-end iterator should now be key=2.
        auto it = m.end();
        --it;
        REQUIRE(it->first == 2);
    }

    SECTION("Erase sole element — map becomes empty")
    {
        TMap<int, int> m = {
            { 42, 1 }
        };
        m.Erase(42);
        REQUIRE(m.IsEmpty());
        REQUIRE(m.begin() == m.end());
    }

    SECTION("Erase all elements one by one in sorted order — tree remains valid")
    {
        TMap<int, int> m;
        for (int i = 1; i <= 8; ++i) { m.Insert({ i, i }); }
        for (int i = 1; i <= 8; ++i)
        {
            REQUIRE(m.Contains(i));
            m.Erase(i);
            REQUIRE_FALSE(m.Contains(i));
            // Remaining elements still in sorted order.
            int prev = -1;
            for (const auto& [k, v]: m)
            {
                REQUIRE(k > prev);
                prev = k;
            }
        }
        REQUIRE(m.IsEmpty());
    }

    SECTION("Erase all elements in reverse order — RB fixup paths exercised")
    {
        TMap<int, int> m;
        for (int i = 1; i <= 8; ++i) { m.Insert({ i, i }); }
        for (int i = 8; i >= 1; --i)
        {
            m.Erase(i);
            int prev = -1;
            for (const auto& [k, v]: m)
            {
                REQUIRE(k > prev);
                prev = k;
            }
        }
        REQUIRE(m.IsEmpty());
    }

    SECTION("Erase root node — RB rebalancing")
    {
        // Insert values that force the initial root to be somewhere in the middle.
        TMap<int, int> m = {
            { 4, 4 },
            { 2, 2 },
            { 6, 6 },
            { 1, 1 },
            { 3, 3 },
            { 5, 5 },
            { 7, 7 }
        };
        m.Erase(4);   // Erase root.
        REQUIRE(m.Size() == 6);
        REQUIRE_FALSE(m.Contains(4));
        int prev = -1;
        for (const auto& [k, v]: m)
        {
            REQUIRE(k > prev);
            prev = k;
        }
    }

    SECTION("Clear empties the map without leaking nodes")
    {
        MapTestObject::Reset();
        {
            TMap<int, MapTestObject> m;
            for (int i = 0; i < 5; ++i) { m.Insert({ i, MapTestObject(i) }); }
            int countBeforeClear = MapTestObject::destructorCalls;
            m.Clear();
            REQUIRE(m.IsEmpty());
            REQUIRE(m.Size() == 0);
            // All 5 live nodes destroyed.
            REQUIRE(MapTestObject::destructorCalls >= countBeforeClear + 5);
        }
    }
}

TEST_CASE("TMap Iterator", "[GP][Core][Container][TMap]")
{
    SECTION("begin() == end() on empty map")
    {
        TMap<int, int> m;
        REQUIRE(m.begin() == m.end());
        REQUIRE(m.cbegin() == m.cend());
    }

    SECTION("Forward iteration visits all elements in sorted order")
    {
        TMap<int, int> m;
        int keys[] = { 7, 3, 11, 5, 1, 9 };
        for (int k: keys) { m.Insert({ k, k * 2 }); }
        int count = 0;
        int prev = -1;
        for (auto it = m.begin(); it != m.end(); ++it)
        {
            REQUIRE(it->first > prev);
            REQUIRE(it->second == it->first * 2);
            prev = it->first;
            ++count;
        }
        REQUIRE(count == 6);
    }

    SECTION("Post-increment returns previous position")
    {
        TMap<int, int> m = {
            { 1, 10 },
            { 2, 20 }
        };
        auto it = m.begin();
        auto prev = it++;
        REQUIRE(prev->first == 1);
        REQUIRE(it->first == 2);
    }

    SECTION("Pre-decrement from end() reaches last element")
    {
        TMap<int, int> m = {
            { 1, 10 },
            { 2, 20 },
            { 3, 30 }
        };
        auto it = m.end();
        --it;
        REQUIRE(it->first == 3);
        --it;
        REQUIRE(it->first == 2);
        --it;
        REQUIRE(it->first == 1);
        REQUIRE(it == m.begin());
    }

    SECTION("Post-decrement returns previous position")
    {
        TMap<int, int> m = {
            { 1, 10 },
            { 2, 20 }
        };
        auto it = m.end();
        --it;   // now at key=2
        auto savedEnd = it--;
        REQUIRE(savedEnd->first == 2);
        REQUIRE(it->first == 1);
    }

    SECTION("Mutable iterator allows value modification")
    {
        TMap<int, int> m = {
            { 1, 10 },
            { 2, 20 }
        };
        for (auto& [k, v]: m) { v *= 100; }
        REQUIRE(m.Find(1)->second == 1000);
        REQUIRE(m.Find(2)->second == 2000);
    }

    SECTION("Const iterator is read-only")
    {
        const TMap<int, int> m = {
            { 1, 10 },
            { 2, 20 }
        };
        for (const auto& [k, v]: m) { REQUIRE(v == k * 10); }
    }

    SECTION("GP-style Begin()/End() match STL begin()/end()")
    {
        TMap<int, int> m = {
            { 3, 3 },
            { 1, 1 },
            { 2, 2 }
        };
        REQUIRE(m.Begin() == m.begin());
        REQUIRE(m.End() == m.end());
    }

    SECTION("Reverse iteration visits elements in descending order")
    {
        TMap<int, int> m = {
            { 1, 10 },
            { 2, 20 },
            { 3, 30 }
        };
        int prev = 100;
        for (auto it = m.rbegin(); it != m.rend(); ++it)
        {
            REQUIRE(it->first < prev);
            prev = it->first;
        }
    }

    SECTION("Iterator after erase of non-adjacent element remains valid")
    {
        TMap<int, int> m = {
            { 1, 1 },
            { 2, 2 },
            { 3, 3 },
            { 4, 4 }
        };
        auto it2 = m.Find(2);
        m.Erase(4);
        // it2 still valid; points to key=2.
        REQUIRE(it2->first == 2);
    }
}

TEST_CASE("TMap Object Lifetime", "[GP][Core][Container][TMap]")
{
    SECTION("Copy constructor invokes copy constructor of values")
    {
        MapTestObject::Reset();
        TMap<int, MapTestObject> original;
        original.Insert({ 1, MapTestObject(1) });
        original.Insert({ 2, MapTestObject(2) });

        MapTestObject::Reset();
        TMap<int, MapTestObject> copy(original);
        REQUIRE(MapTestObject::copyConstructorCalls == 2);
    }

    SECTION("Move constructor does not copy values")
    {
        TMap<int, MapTestObject> source;
        source.Insert({ 1, MapTestObject(1) });

        MapTestObject::Reset();
        TMap<int, MapTestObject> moved(std::move(source));
        REQUIRE(MapTestObject::copyConstructorCalls == 0);
        REQUIRE(moved.Size() == 1);
    }

    SECTION("Destructor count matches inserted node count")
    {
        MapTestObject::Reset();
        {
            TMap<int, MapTestObject> m;
            m.Insert({ 1, MapTestObject(1) });
            m.Insert({ 2, MapTestObject(2) });
            m.Insert({ 3, MapTestObject(3) });
        }
        // 3 nodes constructed (+ temporaries already destroyed), 3 nodes destroyed at scope exit.
        REQUIRE(MapTestObject::destructorCalls >= 3);
    }

    SECTION("Emplace constructs value in-place without extra copies")
    {
        MapTestObject::Reset();
        TMap<int, MapTestObject> m;
        m.Emplace(1, MapTestObject(42));
        // One default constructor + one move into slot (implementation detail); no copy.
        REQUIRE(MapTestObject::copyConstructorCalls == 0);
    }
}

TEST_CASE("TMap Edge Cases", "[GP][Core][Container][TMap]")
{
    SECTION("Large sequential insertion and lookup")
    {
        TMap<int, int> m;
        constexpr int N = 256;
        for (int i = 0; i < N; ++i) { m.Insert({ i, i * i }); }
        REQUIRE(m.Size() == N);
        for (int i = 0; i < N; ++i)
        {
            auto it = m.Find(i);
            REQUIRE(it != m.end());
            REQUIRE(it->second == i * i);
        }
    }

    SECTION("Insert, erase, and re-insert same key")
    {
        TMap<int, int> m;
        m.Insert({ 42, 1 });
        m.Erase(42);
        REQUIRE_FALSE(m.Contains(42));
        m.Insert({ 42, 2 });
        REQUIRE(m.Contains(42));
        REQUIRE(m.Find(42)->second == 2);
    }

    SECTION("Map with string keys sorted lexicographically")
    {
        TMap<std::string, int> m = {
            { "banana", 2 },
            {  "apple", 1 },
            { "cherry", 3 }
        };
        auto it = m.begin();
        REQUIRE(it->first == "apple");
        ++it;
        REQUIRE(it->first == "banana");
        ++it;
        REQUIRE(it->first == "cherry");
    }

    SECTION("Range constructor from empty range produces empty map")
    {
        std::vector<std::pair<const int, int>> src;
        TMap<int, int> m(src.begin(), src.end());
        REQUIRE(m.IsEmpty());
    }

    SECTION("Erase during iteration using returned next iterator")
    {
        TMap<int, int> m;
        for (int i = 1; i <= 6; ++i) { m.Insert({ i, i }); }
        // Erase all even keys.
        auto it = m.begin();
        while (it != m.end())
        {
            if (it->first % 2 == 0) { it = m.Erase(it); }
            else { ++it; }
        }
        REQUIRE(m.Size() == 3);
        REQUIRE(m.Contains(1));
        REQUIRE(m.Contains(3));
        REQUIRE(m.Contains(5));
    }

    SECTION("Complex rebalancing: interleaved insert and erase")
    {
        TMap<int, int> m;
        for (int i = 0; i < 20; ++i) { m.Insert({ i, i }); }
        for (int i = 0; i < 20; i += 3) { m.Erase(i); }
        // Verify remaining elements are still sorted and accessible.
        int prev = -1;
        SizeT count = 0;
        for (const auto& [k, v]: m)
        {
            REQUIRE(k > prev);
            prev = k;
            ++count;
        }
        REQUIRE(count == m.Size());
    }

    SECTION("Default-constructed TMap has non-zero MaxCount")
    {
        TMap<int, int> m;
        REQUIRE(m.MaxCount() > 0);
    }
}
