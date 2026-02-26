// Copyright (c) - Graphical Playground. All rights reserved.

#include "Templates/Singleton.hpp"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <type_traits>
#include <vector>

namespace
{

struct LazyBaseType
{
    int value = 42;
};

struct LazyMutateType
{
    int counter = 0;

    void Increment() { ++counter; }
};

struct LazyPtrType
{
    float value = 1.5f;
};

struct LazyInitFlagType
{
    int value = 0;
};

struct LazyTSType
{
    int value = 7;
};

struct EagerBaseType
{
    int value = 99;
};

struct EagerIdempotentCountType
{
    static int s_constructCount;

    EagerIdempotentCountType() { ++s_constructCount; }
};

int EagerIdempotentCountType::s_constructCount = 0;

struct EagerGetType
{
    int value = 200;
};

struct EagerIsInitType
{
    int value = 0;
};

struct EagerTSType
{
    int value = 5;
};

/// \brief Used exclusively for the concurrent DCLP Init test so the construct
///        count starts at zero and is never polluted by other test sections.
struct EagerConcurrentType
{
    static std::atomic<int> s_constructCount;

    EagerConcurrentType() { s_constructCount.fetch_add(1, std::memory_order_relaxed); }
};

std::atomic<int> EagerConcurrentType::s_constructCount{ 0 };

struct ManualDefaultType
{
    int value = 0;
};

struct ManualLifecycleType
{
    static int s_ctorCount;
    static int s_dtorCount;

    ManualLifecycleType() { ++s_ctorCount; }

    ~ManualLifecycleType() { ++s_dtorCount; }

    static void ResetCounters()
    {
        s_ctorCount = 0;
        s_dtorCount = 0;
    }
};

int ManualLifecycleType::s_ctorCount = 0;
int ManualLifecycleType::s_dtorCount = 0;

struct ManualArgsType
{
    int x;
    float y;
    bool flag;

    ManualArgsType(int a, float b, bool c)
        : x(a)
        , y(b)
        , flag(c)
    {}
};

struct ManualRecreateType
{
    static int s_idCounter;
    int m_id;

    ManualRecreateType()
        : m_id(++s_idCounter)
    {}
};

int ManualRecreateType::s_idCounter = 0;

struct ManualGetType
{
    int value = 55;
};

struct ManualPtrType
{
    int value = 77;
};

struct ManualIsInitType
{
    int value = 88;
};

struct ManualTSType
{
    int value = 11;
};

struct ManualConcurrentType
{
    int value = 33;
};

}   // namespace

// ===========================================================================
// TLazySingleton
// ===========================================================================

TEST_CASE("TLazySingleton GetInstance", "[GP][Core][Templates][Singleton][Lazy]")
{
    using namespace GP::Template;

    SECTION("Returns a valid reference")
    {
        LazyBaseType& instance = TLazySingleton<LazyBaseType>::GetInstance();
        REQUIRE(instance.value == 42);
    }

    SECTION("Returns the same reference on consecutive calls")
    {
        LazyBaseType& a = TLazySingleton<LazyBaseType>::GetInstance();
        LazyBaseType& b = TLazySingleton<LazyBaseType>::GetInstance();
        LazyBaseType& c = TLazySingleton<LazyBaseType>::GetInstance();

        REQUIRE(&a == &b);
        REQUIRE(&b == &c);
    }

    SECTION("Mutations made through the reference persist across calls")
    {
        TLazySingleton<LazyMutateType>::GetInstance().Increment();
        TLazySingleton<LazyMutateType>::GetInstance().Increment();

        REQUIRE(TLazySingleton<LazyMutateType>::GetInstance().counter == 2);
    }
}

TEST_CASE("TLazySingleton GetInstancePtr", "[GP][Core][Templates][Singleton][Lazy]")
{
    using namespace GP::Template;

    SECTION("Returns a non-null pointer") { REQUIRE(TLazySingleton<LazyPtrType>::GetInstancePtr() != nullptr); }

    SECTION("Pointer and reference address the same object")
    {
        LazyPtrType* ptr = TLazySingleton<LazyPtrType>::GetInstancePtr();
        LazyPtrType& ref = TLazySingleton<LazyPtrType>::GetInstance();

        REQUIRE(ptr == &ref);
    }

    SECTION("Consecutive calls return the same pointer")
    {
        LazyPtrType* p1 = TLazySingleton<LazyPtrType>::GetInstancePtr();
        LazyPtrType* p2 = TLazySingleton<LazyPtrType>::GetInstancePtr();

        REQUIRE(p1 == p2);
    }
}

TEST_CASE("TLazySingleton IsInitialized", "[GP][Core][Templates][Singleton][Lazy]")
{
    using namespace GP::Template;

    SECTION("Returns true after GetInstance has been called")
    {
        (void)TLazySingleton<LazyBaseType>::GetInstance();

        REQUIRE(TLazySingleton<LazyBaseType>::IsInitialized());
    }

    SECTION("Returns true when called directly (triggers construction internally)")
    {
        // The Lazy policy's IsInitialized uses a second magic-static whose
        // initializer calls GetInstance() as a side-effect, guaranteeing true.
        REQUIRE(TLazySingleton<LazyInitFlagType>::IsInitialized());
    }
}

TEST_CASE("TLazySingleton InstanceType alias", "[GP][Core][Templates][Singleton][Lazy]")
{
    using namespace GP::Template;

    SECTION("InstanceType matches the template parameter")
    {
        REQUIRE(std::is_same_v<TLazySingleton<LazyBaseType>::InstanceType, LazyBaseType>);
    }
}

TEST_CASE("TLazySingleton ThreadSafe variant", "[GP][Core][Templates][Singleton][Lazy][ThreadSafe]")
{
    using namespace GP::Template;

    SECTION("GetInstance with ThreadSafe=true returns a valid reference")
    {
        LazyTSType& instance = TLazySingleton<LazyTSType, true>::GetInstance();

        REQUIRE(instance.value == 7);
    }

    SECTION("GetInstancePtr with ThreadSafe=true returns a non-null pointer")
    {
        REQUIRE(TLazySingleton<LazyTSType, true>::GetInstancePtr() != nullptr);
    }

    SECTION("Same instance is returned on every call")
    {
        LazyTSType& a = TLazySingleton<LazyTSType, true>::GetInstance();
        LazyTSType& b = TLazySingleton<LazyTSType, true>::GetInstance();

        REQUIRE(&a == &b);
    }

    SECTION("IsInitialized returns true after GetInstance")
    {
        (void)TLazySingleton<LazyTSType, true>::GetInstance();

        REQUIRE(TLazySingleton<LazyTSType, true>::IsInitialized());
    }

    SECTION("Concurrent access from multiple threads returns the same pointer")
    {
        constexpr int ThreadCount = 8;

        LazyTSType* basePtr = TLazySingleton<LazyTSType, true>::GetInstancePtr();
        std::atomic<int> successCount{ 0 };

        std::vector<std::thread> threads;
        threads.reserve(ThreadCount);

        for (int i = 0; i < ThreadCount; ++i)
        {
            threads.emplace_back(
                [&]
                {
                    LazyTSType* ptr = TLazySingleton<LazyTSType, true>::GetInstancePtr();
                    if (ptr == basePtr) { successCount.fetch_add(1, std::memory_order_relaxed); }
                }
            );
        }

        for (auto& t: threads) { t.join(); }

        REQUIRE(successCount.load() == ThreadCount);
    }
}

// ===========================================================================
// TEagerSingleton
// ===========================================================================

TEST_CASE("TEagerSingleton Init", "[GP][Core][Templates][Singleton][Eager]")
{
    using namespace GP::Template;

    SECTION("Returns a valid reference on first call")
    {
        EagerBaseType& instance = TEagerSingleton<EagerBaseType>::Init();

        REQUIRE(instance.value == 99);
    }

    SECTION("Constructs the instance exactly once across multiple Init calls")
    {
        GP_UNUSED(TEagerSingleton<EagerIdempotentCountType>::Init());
        GP_UNUSED(TEagerSingleton<EagerIdempotentCountType>::Init());
        GP_UNUSED(TEagerSingleton<EagerIdempotentCountType>::Init());

        REQUIRE(EagerIdempotentCountType::s_constructCount == 1);
    }

    SECTION("Repeated calls return the same instance")
    {
        EagerBaseType& a = TEagerSingleton<EagerBaseType>::Init();
        EagerBaseType& b = TEagerSingleton<EagerBaseType>::Init();

        REQUIRE(&a == &b);
    }
}

TEST_CASE("TEagerSingleton GetInstance", "[GP][Core][Templates][Singleton][Eager]")
{
    using namespace GP::Template;

    SECTION("Returns a valid reference")
    {
        EagerGetType& instance = TEagerSingleton<EagerGetType>::GetInstance();

        REQUIRE(instance.value == 200);
    }

    SECTION("GetInstance and Init return the same object")
    {
        EagerGetType& fromInit = TEagerSingleton<EagerGetType>::Init();
        EagerGetType& fromGet = TEagerSingleton<EagerGetType>::GetInstance();

        REQUIRE(&fromInit == &fromGet);
    }

    SECTION("Consecutive calls return the same instance")
    {
        EagerGetType& a = TEagerSingleton<EagerGetType>::GetInstance();
        EagerGetType& b = TEagerSingleton<EagerGetType>::GetInstance();

        REQUIRE(&a == &b);
    }
}

TEST_CASE("TEagerSingleton GetInstancePtr", "[GP][Core][Templates][Singleton][Eager]")
{
    using namespace GP::Template;

    SECTION("Returns a non-null pointer after Init")
    {
        GP_UNUSED(TEagerSingleton<EagerGetType>::Init());

        REQUIRE(TEagerSingleton<EagerGetType>::GetInstancePtr() != nullptr);
    }

    SECTION("Pointer and reference address the same object")
    {
        EagerGetType& ref = TEagerSingleton<EagerGetType>::GetInstance();
        EagerGetType* ptr = TEagerSingleton<EagerGetType>::GetInstancePtr();

        REQUIRE(ptr == &ref);
    }

    SECTION("Consecutive calls return the same pointer")
    {
        EagerGetType* p1 = TEagerSingleton<EagerGetType>::GetInstancePtr();
        EagerGetType* p2 = TEagerSingleton<EagerGetType>::GetInstancePtr();

        REQUIRE(p1 == p2);
    }
}

TEST_CASE("TEagerSingleton IsInitialized", "[GP][Core][Templates][Singleton][Eager]")
{
    using namespace GP::Template;

    SECTION("Returns false before Init is called") { REQUIRE_FALSE(TEagerSingleton<EagerIsInitType>::IsInitialized()); }

    SECTION("Returns true after Init is called")
    {
        GP_UNUSED(TEagerSingleton<EagerIsInitType>::Init());

        REQUIRE(TEagerSingleton<EagerIsInitType>::IsInitialized());
    }
}

TEST_CASE("TEagerSingleton InstanceType alias", "[GP][Core][Templates][Singleton][Eager]")
{
    using namespace GP::Template;

    SECTION("InstanceType matches the template parameter")
    {
        REQUIRE(std::is_same_v<TEagerSingleton<EagerBaseType>::InstanceType, EagerBaseType>);
    }
}

TEST_CASE("TEagerSingleton ThreadSafe variant", "[GP][Core][Templates][Singleton][Eager][ThreadSafe]")
{
    using namespace GP::Template;

    SECTION("Init with ThreadSafe=true returns a valid reference")
    {
        EagerTSType& instance = TEagerSingleton<EagerTSType, true>::Init();

        REQUIRE(instance.value == 5);
    }

    SECTION("Repeated Init calls return the same instance")
    {
        EagerTSType& a = TEagerSingleton<EagerTSType, true>::Init();
        EagerTSType& b = TEagerSingleton<EagerTSType, true>::Init();

        REQUIRE(&a == &b);
    }

    SECTION("IsInitialized returns true after Init")
    {
        (void)TEagerSingleton<EagerTSType, true>::Init();

        REQUIRE(TEagerSingleton<EagerTSType, true>::IsInitialized());
    }

    SECTION("Concurrent Init calls construct the instance exactly once")
    {
        constexpr int ThreadCount = 8;

        std::vector<std::thread> threads;
        threads.reserve(ThreadCount);

        for (int i = 0; i < ThreadCount; ++i)
        {
            threads.emplace_back([] { (void)TEagerSingleton<EagerConcurrentType, true>::Init(); });
        }

        for (auto& t: threads) { t.join(); }

        REQUIRE(EagerConcurrentType::s_constructCount.load() == 1);
    }

    SECTION("Concurrent GetInstance calls return the same pointer")
    {
        constexpr int ThreadCount = 8;

        EagerTSType* basePtr = TEagerSingleton<EagerTSType, true>::GetInstancePtr();
        std::atomic<int> matchCount{ 0 };

        std::vector<std::thread> threads;
        threads.reserve(ThreadCount);

        for (int i = 0; i < ThreadCount; ++i)
        {
            threads.emplace_back(
                [&]
                {
                    EagerTSType* ptr = TEagerSingleton<EagerTSType, true>::GetInstancePtr();
                    if (ptr == basePtr) { matchCount.fetch_add(1, std::memory_order_relaxed); }
                }
            );
        }

        for (auto& t: threads) { t.join(); }

        REQUIRE(matchCount.load() == ThreadCount);
    }
}

// ===========================================================================
// TManualSingleton
// ===========================================================================

TEST_CASE("TManualSingleton Create", "[GP][Core][Templates][Singleton][Manual]")
{
    using namespace GP::Template;

    SECTION("Create with default constructor returns a valid reference")
    {
        ManualDefaultType& instance = TManualSingleton<ManualDefaultType>::Create();

        REQUIRE(instance.value == 0);

        TManualSingleton<ManualDefaultType>::Destroy();
    }

    SECTION("Create with constructor arguments initializes the instance correctly")
    {
        ManualArgsType& instance = TManualSingleton<ManualArgsType>::Create(10, 2.5f, true);

        REQUIRE(instance.x == 10);
        REQUIRE(instance.y == 2.5f);
        REQUIRE(instance.flag == true);

        TManualSingleton<ManualArgsType>::Destroy();
    }

    SECTION("Create invokes the constructor exactly once")
    {
        ManualLifecycleType::ResetCounters();
        TManualSingleton<ManualLifecycleType>::Create();

        REQUIRE(ManualLifecycleType::s_ctorCount == 1);

        TManualSingleton<ManualLifecycleType>::Destroy();
    }
}

TEST_CASE("TManualSingleton Destroy", "[GP][Core][Templates][Singleton][Manual]")
{
    using namespace GP::Template;

    SECTION("Destroy invokes the destructor exactly once")
    {
        ManualLifecycleType::ResetCounters();
        TManualSingleton<ManualLifecycleType>::Create();

        REQUIRE(ManualLifecycleType::s_dtorCount == 0);

        TManualSingleton<ManualLifecycleType>::Destroy();

        REQUIRE(ManualLifecycleType::s_dtorCount == 1);
    }

    SECTION("Create and Destroy cycle can be repeated")
    {
        ManualLifecycleType::ResetCounters();

        TManualSingleton<ManualLifecycleType>::Create();
        TManualSingleton<ManualLifecycleType>::Destroy();
        TManualSingleton<ManualLifecycleType>::Create();
        TManualSingleton<ManualLifecycleType>::Destroy();

        REQUIRE(ManualLifecycleType::s_ctorCount == 2);
        REQUIRE(ManualLifecycleType::s_dtorCount == 2);
    }
}

TEST_CASE("TManualSingleton GetInstance", "[GP][Core][Templates][Singleton][Manual]")
{
    using namespace GP::Template;

    SECTION("Returns the constructed instance")
    {
        TManualSingleton<ManualGetType>::Create();
        ManualGetType& instance = TManualSingleton<ManualGetType>::GetInstance();

        REQUIRE(instance.value == 55);

        TManualSingleton<ManualGetType>::Destroy();
    }

    SECTION("Consecutive calls return the same instance")
    {
        TManualSingleton<ManualGetType>::Create();

        ManualGetType& a = TManualSingleton<ManualGetType>::GetInstance();
        ManualGetType& b = TManualSingleton<ManualGetType>::GetInstance();

        REQUIRE(&a == &b);

        TManualSingleton<ManualGetType>::Destroy();
    }

    SECTION("Mutations made through the reference persist across calls")
    {
        TManualSingleton<ManualGetType>::Create();
        TManualSingleton<ManualGetType>::GetInstance().value = 999;

        REQUIRE(TManualSingleton<ManualGetType>::GetInstance().value == 999);

        TManualSingleton<ManualGetType>::Destroy();
    }
}

TEST_CASE("TManualSingleton GetInstancePtr", "[GP][Core][Templates][Singleton][Manual]")
{
    using namespace GP::Template;

    SECTION("Returns nullptr before Create") { REQUIRE(TManualSingleton<ManualPtrType>::GetInstancePtr() == nullptr); }

    SECTION("Returns a non-null pointer after Create")
    {
        TManualSingleton<ManualPtrType>::Create();

        REQUIRE(TManualSingleton<ManualPtrType>::GetInstancePtr() != nullptr);

        TManualSingleton<ManualPtrType>::Destroy();
    }

    SECTION("Returns nullptr after Destroy")
    {
        TManualSingleton<ManualPtrType>::Create();
        TManualSingleton<ManualPtrType>::Destroy();

        REQUIRE(TManualSingleton<ManualPtrType>::GetInstancePtr() == nullptr);
    }

    SECTION("Pointer and reference address the same object")
    {
        TManualSingleton<ManualPtrType>::Create();

        ManualPtrType* ptr = TManualSingleton<ManualPtrType>::GetInstancePtr();
        ManualPtrType& ref = TManualSingleton<ManualPtrType>::GetInstance();

        REQUIRE(ptr == &ref);

        TManualSingleton<ManualPtrType>::Destroy();
    }
}

TEST_CASE("TManualSingleton IsInitialized", "[GP][Core][Templates][Singleton][Manual]")
{
    using namespace GP::Template;

    SECTION("Returns false before Create") { REQUIRE_FALSE(TManualSingleton<ManualIsInitType>::IsInitialized()); }

    SECTION("Returns true after Create")
    {
        TManualSingleton<ManualIsInitType>::Create();

        REQUIRE(TManualSingleton<ManualIsInitType>::IsInitialized());

        TManualSingleton<ManualIsInitType>::Destroy();
    }

    SECTION("Returns false after Destroy")
    {
        TManualSingleton<ManualIsInitType>::Create();
        TManualSingleton<ManualIsInitType>::Destroy();

        REQUIRE_FALSE(TManualSingleton<ManualIsInitType>::IsInitialized());
    }
}

TEST_CASE("TManualSingleton Custom Constructor Arguments", "[GP][Core][Templates][Singleton][Manual]")
{
    using namespace GP::Template;

    SECTION("All argument types are forwarded correctly")
    {
        ManualArgsType& instance = TManualSingleton<ManualArgsType>::Create(-7, 0.5f, false);

        REQUIRE(instance.x == -7);
        REQUIRE(instance.y == 0.5f);
        REQUIRE(instance.flag == false);

        TManualSingleton<ManualArgsType>::Destroy();
    }

    SECTION("Different argument values produce a correctly initialized instance")
    {
        ManualArgsType& instance = TManualSingleton<ManualArgsType>::Create(100, 3.14f, true);

        REQUIRE(instance.x == 100);
        REQUIRE(instance.y == 3.14f);
        REQUIRE(instance.flag == true);

        TManualSingleton<ManualArgsType>::Destroy();
    }
}

TEST_CASE("TManualSingleton Recreate After Destroy", "[GP][Core][Templates][Singleton][Manual]")
{
    using namespace GP::Template;

    SECTION("Each Create produces a distinct new instance")
    {
        TManualSingleton<ManualRecreateType>::Create();
        int firstId = TManualSingleton<ManualRecreateType>::GetInstance().m_id;
        TManualSingleton<ManualRecreateType>::Destroy();

        TManualSingleton<ManualRecreateType>::Create();
        int secondId = TManualSingleton<ManualRecreateType>::GetInstance().m_id;
        TManualSingleton<ManualRecreateType>::Destroy();

        REQUIRE(secondId == firstId + 1);
    }

    SECTION("IsInitialized correctly reflects state across multiple lifecycle cycles")
    {
        REQUIRE_FALSE(TManualSingleton<ManualRecreateType>::IsInitialized());

        TManualSingleton<ManualRecreateType>::Create();
        REQUIRE(TManualSingleton<ManualRecreateType>::IsInitialized());

        TManualSingleton<ManualRecreateType>::Destroy();
        REQUIRE_FALSE(TManualSingleton<ManualRecreateType>::IsInitialized());

        TManualSingleton<ManualRecreateType>::Create();
        REQUIRE(TManualSingleton<ManualRecreateType>::IsInitialized());

        TManualSingleton<ManualRecreateType>::Destroy();
        REQUIRE_FALSE(TManualSingleton<ManualRecreateType>::IsInitialized());
    }
}

TEST_CASE("TManualSingleton InstanceType alias", "[GP][Core][Templates][Singleton][Manual]")
{
    using namespace GP::Template;

    SECTION("InstanceType matches the template parameter")
    {
        REQUIRE(std::is_same_v<TManualSingleton<ManualDefaultType>::InstanceType, ManualDefaultType>);
    }
}

TEST_CASE("TManualSingleton ThreadSafe variant", "[GP][Core][Templates][Singleton][Manual][ThreadSafe]")
{
    using namespace GP::Template;

    SECTION("Create with ThreadSafe=true returns a valid reference")
    {
        ManualTSType& instance = TManualSingleton<ManualTSType, true>::Create();

        REQUIRE(instance.value == 11);

        TManualSingleton<ManualTSType, true>::Destroy();
    }

    SECTION("IsInitialized correctly reflects the full lifecycle")
    {
        REQUIRE_FALSE(TManualSingleton<ManualTSType, true>::IsInitialized());

        TManualSingleton<ManualTSType, true>::Create();
        REQUIRE(TManualSingleton<ManualTSType, true>::IsInitialized());

        TManualSingleton<ManualTSType, true>::Destroy();
        REQUIRE_FALSE(TManualSingleton<ManualTSType, true>::IsInitialized());
    }

    SECTION("Consecutive GetInstance calls with ThreadSafe=true return the same instance")
    {
        TManualSingleton<ManualTSType, true>::Create();

        ManualTSType& a = TManualSingleton<ManualTSType, true>::GetInstance();
        ManualTSType& b = TManualSingleton<ManualTSType, true>::GetInstance();

        REQUIRE(&a == &b);

        TManualSingleton<ManualTSType, true>::Destroy();
    }

    SECTION("Concurrent GetInstance calls return the same pointer")
    {
        constexpr int ThreadCount = 8;

        TManualSingleton<ManualConcurrentType, true>::Create();
        ManualConcurrentType* basePtr = TManualSingleton<ManualConcurrentType, true>::GetInstancePtr();
        std::atomic<int> matchCount{ 0 };

        std::vector<std::thread> threads;
        threads.reserve(ThreadCount);

        for (int i = 0; i < ThreadCount; ++i)
        {
            threads.emplace_back(
                [&]
                {
                    ManualConcurrentType& ref = TManualSingleton<ManualConcurrentType, true>::GetInstance();
                    if (&ref == basePtr) { matchCount.fetch_add(1, std::memory_order_relaxed); }
                }
            );
        }

        for (auto& t: threads) { t.join(); }

        REQUIRE(matchCount.load() == ThreadCount);

        TManualSingleton<ManualConcurrentType, true>::Destroy();
    }
}

// ===========================================================================
// TSingleton Policy Alias Tests
// ===========================================================================

TEST_CASE("TSingleton policy aliases", "[GP][Core][Templates][Singleton][Aliases]")
{
    using namespace GP::Template;

    SECTION("TLazySingleton aliases TSingleton with Lazy policy and ThreadSafe=false")
    {
        REQUIRE(std::is_same_v<
                TLazySingleton<LazyBaseType>,
                TSingleton<LazyBaseType, ESingletonCreationPolicy::Lazy, false>>);
    }

    SECTION("TEagerSingleton aliases TSingleton with Eager policy and ThreadSafe=false")
    {
        REQUIRE(std::is_same_v<
                TEagerSingleton<EagerBaseType>,
                TSingleton<EagerBaseType, ESingletonCreationPolicy::Eager, false>>);
    }

    SECTION("TManualSingleton aliases TSingleton with Manual policy and ThreadSafe=false")
    {
        REQUIRE(std::is_same_v<
                TManualSingleton<ManualDefaultType>,
                TSingleton<ManualDefaultType, ESingletonCreationPolicy::Manual, false>>);
    }

    SECTION("Thread-safe aliases resolve correctly for all three policies")
    {
        REQUIRE(std::is_same_v<
                TLazySingleton<LazyBaseType, true>,
                TSingleton<LazyBaseType, ESingletonCreationPolicy::Lazy, true>>);

        REQUIRE(std::is_same_v<
                TEagerSingleton<EagerBaseType, true>,
                TSingleton<EagerBaseType, ESingletonCreationPolicy::Eager, true>>);

        REQUIRE(std::is_same_v<
                TManualSingleton<ManualDefaultType, true>,
                TSingleton<ManualDefaultType, ESingletonCreationPolicy::Manual, true>>);
    }
}
