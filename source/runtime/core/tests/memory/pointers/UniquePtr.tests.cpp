// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/pointers/UniquePtr.hpp"
#include "concepts/Comparison.hpp"
#include "concepts/Construction.hpp"
#include "concepts/Memory.hpp"
#include "CoreMinimal.hpp"
#include "templates/ScopeExit.hpp"
#include <compare>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

namespace gp::memory::tests
{

namespace
{

/// @brief Forward-declared incomplete types used to verify PImpl member declarations compile prior to completion.
struct IncompleteSingle;
struct IncompleteArrayElem;

struct IncompleteHolder
{
    UniquePtr<IncompleteSingle> single{};
    UniquePtr<IncompleteArrayElem[]> array{};

    constexpr IncompleteHolder() noexcept = default;
    constexpr IncompleteHolder(IncompleteHolder&&) noexcept = default;
    ~IncompleteHolder() noexcept;
};

struct IncompleteSingle
{
    static inline gp::Int32 s_activeCount{ 0 };
    gp::Int32 id{ 0 };

    explicit IncompleteSingle(const gp::Int32 inId = 1) noexcept
        : id(inId)
    {
        ++s_activeCount;
    }

    ~IncompleteSingle() noexcept
    {
        --s_activeCount;
    }
};

struct IncompleteArrayElem
{
    static inline gp::Int32 s_activeCount{ 0 };
    gp::Int32 value{ 0 };

    IncompleteArrayElem() noexcept
    {
        ++s_activeCount;
    }

    ~IncompleteArrayElem() noexcept
    {
        --s_activeCount;
    }
};

IncompleteHolder::~IncompleteHolder() noexcept = default;

/// @brief Tracks constructor and destructor counts for single objects and dynamic arrays.
struct LifetimeTracker
{
    static inline gp::Int32 s_constructCount{ 0 };
    static inline gp::Int32 s_destructCount{ 0 };
    static inline gp::Int32 s_activeInstances{ 0 };

    gp::Int32 value{ 0 };

    static void resetCounters() noexcept
    {
        s_constructCount = 0;
        s_destructCount = 0;
        s_activeInstances = 0;
    }

    LifetimeTracker() noexcept
        : value(0)
    {
        ++s_constructCount;
        ++s_activeInstances;
    }

    explicit LifetimeTracker(const gp::Int32 inValue) noexcept
        : value(inValue)
    {
        ++s_constructCount;
        ++s_activeInstances;
    }

    LifetimeTracker(const LifetimeTracker&) = delete;
    LifetimeTracker& operator=(const LifetimeTracker&) = delete;

    ~LifetimeTracker() noexcept
    {
        ++s_destructCount;
        --s_activeInstances;
    }
};

struct PolymorphicBase
{
    static inline gp::Int32 s_activeBases{ 0 };

    PolymorphicBase() noexcept
    {
        ++s_activeBases;
    }

    virtual ~PolymorphicBase() noexcept
    {
        --s_activeBases;
    }

    [[nodiscard]] virtual gp::Int32 getValue() const noexcept
    {
        return 1;
    }
};

struct PolymorphicDerived final : public PolymorphicBase
{
    static inline gp::Int32 s_activeDerived{ 0 };
    gp::Int32 extra{ 77 };

    PolymorphicDerived() noexcept
    {
        ++s_activeDerived;
    }

    virtual ~PolymorphicDerived() noexcept override
    {
        --s_activeDerived;
    }

    [[nodiscard]] virtual gp::Int32 getValue() const noexcept override
    {
        return 2;
    }
};

/// @brief Aggregate type used to confirm parenthesized construction inside `makeUnique`.
struct AggregatePoint
{
    gp::Int32 x;
    gp::Int32 y;
};

/// @brief Empty `final` deleter proving EBO / `GP_NO_UNIQUE_ADDRESS` works without inheritance.
struct FinalEmptyDeleter final
{
    template <typename T>
    constexpr void operator()(T* ptr) const noexcept
    {
        delete ptr;
    }
};

/// @brief Empty `final` array deleter proving zero byte overhead for `UniquePtr<T[], FinalEmptyDeleter>`.
struct FinalEmptyArrayDeleter final
{
    template <typename T>
    constexpr void operator()(T* ptr) const noexcept
    {
        delete[] ptr;
    }
};

/// @brief Deleter with an explicit default constructor, checking non-aggregate construction of storage.
struct ExplicitDefaultDeleter
{
    gp::Int32 tag{ 9 };

    explicit constexpr ExplicitDefaultDeleter() noexcept = default;

    constexpr void operator()(gp::Int32* ptr) const noexcept
    {
        delete ptr;
    }
};

/// @brief Stateful deleter that counts calls and fails if `UniquePtr` passes a null pointer.
struct StrictStatefulDeleter
{
    gp::Int32* deleteCount{ nullptr };
    gp::Int32* nullViolationCount{ nullptr };
    gp::Int32 id{ 0 };

    constexpr explicit StrictStatefulDeleter(
        gp::Int32* const inDeleteCount, gp::Int32* const inNullViolationCount = nullptr, const gp::Int32 inId = 0
    ) noexcept
        : deleteCount(inDeleteCount)
        , nullViolationCount(inNullViolationCount)
        , id(inId)
    {}

    template <typename T>
    constexpr void operator()(T* const ptr) const noexcept
    {
        if (ptr == nullptr)
        {
            if (nullViolationCount != nullptr)
            {
                ++(*nullViolationCount);
            }
            return;
        }
        if (deleteCount != nullptr)
        {
            ++(*deleteCount);
        }
        delete ptr;
    }
};

/// @brief Stateful array deleter tracking `delete[]` invocations and flagging null pointer invocations.
struct StrictArrayDeleter
{
    gp::Int32* deleteCount{ nullptr };
    gp::Int32* nullViolationCount{ nullptr };
    gp::Int32 id{ 0 };

    constexpr explicit StrictArrayDeleter(
        gp::Int32* const inDeleteCount, gp::Int32* const inNullViolationCount = nullptr, const gp::Int32 inId = 0
    ) noexcept
        : deleteCount(inDeleteCount)
        , nullViolationCount(inNullViolationCount)
        , id(inId)
    {}

    template <typename T>
    constexpr void operator()(T* const ptr) const noexcept
    {
        if (ptr == nullptr)
        {
            if (nullViolationCount != nullptr)
            {
                ++(*nullViolationCount);
            }
            return;
        }
        if (deleteCount != nullptr)
        {
            ++(*deleteCount);
        }
        delete[] ptr;
    }
};

/// @brief Non-copyable move-only deleter for verifying move-only deleter support.
struct MoveOnlyDeleter
{
    gp::Int32* deleteCount{ nullptr };
    bool isArray{ false };

    constexpr MoveOnlyDeleter(gp::Int32* const inDeleteCount, const bool inIsArray = false) noexcept
        : deleteCount(inDeleteCount)
        , isArray(inIsArray)
    {}

    MoveOnlyDeleter(const MoveOnlyDeleter&) = delete;
    MoveOnlyDeleter& operator=(const MoveOnlyDeleter&) = delete;

    constexpr MoveOnlyDeleter(MoveOnlyDeleter&& other) noexcept
        : deleteCount(other.deleteCount)
        , isArray(other.isArray)
    {
        other.deleteCount = nullptr;
    }

    constexpr MoveOnlyDeleter& operator=(MoveOnlyDeleter&& other) noexcept
    {
        deleteCount = other.deleteCount;
        isArray = other.isArray;
        other.deleteCount = nullptr;
        return *this;
    }

    template <typename T>
    constexpr void operator()(T* const ptr) const noexcept
    {
        if (deleteCount != nullptr)
        {
            ++(*deleteCount);
        }
        if (isArray)
        {
            delete[] ptr;
        }
        else
        {
            delete ptr;
        }
    }
};

/// @brief Non-movable deleter for testing move-constraint propagation.
struct NonMovableDeleter
{
    gp::Int32 tag{ 0 };

    NonMovableDeleter() noexcept = default;
    NonMovableDeleter(const NonMovableDeleter&) = delete;
    NonMovableDeleter& operator=(const NonMovableDeleter&) = delete;
    NonMovableDeleter(NonMovableDeleter&&) = delete;
    NonMovableDeleter& operator=(NonMovableDeleter&&) = delete;

    void operator()(gp::Int32* const ptr) const noexcept
    {
        delete ptr;
    }
};

/// @brief Non-trivially-copyable deleter for verifying `!concepts::IsRelocatable` propagation.
struct NonRelocatableDeleter
{
    gp::Int32 tag{ 0 };

    NonRelocatableDeleter() noexcept = default;

    NonRelocatableDeleter(NonRelocatableDeleter&& other) noexcept
        : tag(other.tag)
    {
        other.tag = -1;
    }

    NonRelocatableDeleter& operator=(NonRelocatableDeleter&& other) noexcept
    {
        tag = other.tag;
        other.tag = -1;
        return *this;
    }

    ~NonRelocatableDeleter() noexcept
    {
        tag = 0;
    }

    void operator()(gp::Int32* const ptr) const noexcept
    {
        delete ptr;
    }
};

// Templated concept probes so negative `!requires` assertions evaluate in dependent contexts.
template <typename PtrType, typename ArgType>
concept CanResetWith = requires(PtrType ptr, ArgType arg) { ptr.reset(arg); };

template <typename DeleterType, typename ArgType>
concept CanInvokeDeleterWith = requires(DeleterType deleter, ArgType arg) { deleter(arg); };

template <typename PtrType>
concept HasDereference = requires(const PtrType& ptr) { *ptr; };

template <typename PtrType>
concept HasMemberArrow = requires(const PtrType& ptr) { ptr.operator->(); };

template <typename PtrType>
concept HasSubscript = requires(const PtrType& ptr) { ptr[0u]; };

template <typename T, typename... Args>
concept CanMakeUnique = requires(Args&&... args) { makeUnique<T>(std::forward<Args>(args)...); };

template <typename T, typename... Args>
concept CanMakeUniqueOverwrite = requires(Args&&... args) { makeUniqueForOverwrite<T>(std::forward<Args>(args)...); };

template <typename LhsType, typename RhsType>
concept CanEqualityCompare = requires(const LhsType& lhs, const RhsType& rhs) {
    lhs == rhs;
    lhs != rhs;
};

template <typename LhsType, typename RhsType>
concept CanThreeWayCompare = requires(const LhsType& lhs, const RhsType& rhs) {
    lhs <=> rhs;
    lhs < rhs;
};

/// @brief Base and derived deleter pair verifying converting move construction between compatible deleters.
struct BaseCustomDeleter
{
    gp::Int32* deleteCount{ nullptr };

    constexpr explicit BaseCustomDeleter(gp::Int32* const inCount = nullptr) noexcept
        : deleteCount(inCount)
    {}

    template <typename T>
    constexpr void operator()(T* const ptr) const noexcept
    {
        if (deleteCount != nullptr)
        {
            ++(*deleteCount);
        }
        delete ptr;
    }
};

struct DerivedCustomDeleter : public BaseCustomDeleter
{
    using BaseCustomDeleter::BaseCustomDeleter;
};

struct UnrelatedDeleter
{
    template <typename T>
    constexpr void operator()(T* const ptr) const noexcept
    {
        delete ptr;
    }
};

/// @brief Self-observing single object verifying `UniquePtr` clears its pointer before calling the deleter.
struct ReentrantSingleNode
{
    UniquePtr<ReentrantSingleNode>* owner{ nullptr };
    bool observedNullOwnerInDestructor{ false };
    bool* outFlag{ nullptr };

    ReentrantSingleNode(UniquePtr<ReentrantSingleNode>* const inOwner, bool* const inOutFlag) noexcept
        : owner(inOwner)
        , outFlag(inOutFlag)
    {}

    ~ReentrantSingleNode() noexcept
    {
        if (owner != nullptr && outFlag != nullptr)
        {
            observedNullOwnerInDestructor = (!owner->isValid() && owner->get() == nullptr);
            *outFlag = observedNullOwnerInDestructor;
        }
    }
};

/// @brief Self-observing array element verifying `UniquePtr<T[]>` clears its pointer before deleting elements.
struct ReentrantArrayElement
{
    static inline UniquePtr<ReentrantArrayElement[]>* s_observedOwner{ nullptr };
    static inline gp::Int32 s_nullObservedCount{ 0 };

    ~ReentrantArrayElement() noexcept
    {
        if (s_observedOwner != nullptr && !s_observedOwner->isValid() && s_observedOwner->get() == nullptr)
        {
            ++s_nullObservedCount;
        }
    }
};

gp::Int32 g_fnPtrDeleteCount = 0;
gp::Int32 g_fnPtrArrayDeleteCount = 0;

void deleteIntViaFunctionPointer(gp::Int32* const ptr) noexcept
{
    ++g_fnPtrDeleteCount;
    delete ptr;
}

void deleteIntArrayViaFunctionPointer(gp::Int32* const ptr) noexcept
{
    ++g_fnPtrArrayDeleteCount;
    delete[] ptr;
}

/// @brief Compile-time verification for single-object `UniquePtr<T, Deleter>`.
constexpr bool testConstexprSingleUniquePtr()
{
    UniquePtr<gp::Int32> empty;
    if (empty.isValid() || static_cast<bool>(empty) || empty != nullptr || nullptr != empty)
    {
        return false;
    }

    UniquePtr<gp::Int32> ptr1(new gp::Int32(42));
    if (!ptr1.isValid() || !static_cast<bool>(ptr1) || *ptr1 != 42 || ptr1 == nullptr)
    {
        return false;
    }

    ptr1.reset(new gp::Int32(100));
    if (*ptr1 != 100)
    {
        return false;
    }

    UniquePtr<gp::Int32> ptr2(std::move(ptr1));
    if (ptr1.isValid() || *ptr2 != 100)
    {
        return false;
    }

    gp::Int32* const raw = ptr2.release();
    if (ptr2.isValid() || raw == nullptr || *raw != 100)
    {
        delete raw;
        return false;
    }
    delete raw;

    UniquePtr<gp::Int32> made = makeUnique<gp::Int32>(77);
    UniquePtr<gp::Int32> madeAlt = makeUnique<gp::Int32>(88);
    made.swap(madeAlt);
    if (*made != 88 || *madeAlt != 77)
    {
        return false;
    }

    swap(made, madeAlt);
    if (*made != 77 || *madeAlt != 88)
    {
        return false;
    }

    UniquePtr<gp::Int32> overwrite = makeUniqueForOverwrite<gp::Int32>();
    *overwrite = 123;
    if (*overwrite != 123)
    {
        return false;
    }

    // Generalized equality across distinct custom deleters (`<=>` on unrelated heap allocations is runtime-only).
    gp::Int32 sharedDeleteCount = 0;
    UniquePtr<gp::Int32, StrictStatefulDeleter> customPtr(
        new gp::Int32(9), StrictStatefulDeleter(&sharedDeleteCount, nullptr, 1)
    );
    const UniquePtr<const gp::Int32> constView = makeUnique<const gp::Int32>(9);
    if (customPtr == constView || !(customPtr != constView))
    {
        return false;
    }
    if ((customPtr <=> customPtr) != std::strong_ordering::equal || (empty <=> nullptr) != std::strong_ordering::equal)
    {
        return false;
    }
    if (customPtr.getDeleter().id != 1)
    {
        return false;
    }

    customPtr = nullptr;
    if (customPtr.isValid() || sharedDeleteCount != 1)
    {
        return false;
    }

    return true;
}

/// @brief Compile-time verification for array `UniquePtr<T[], Deleter>` and `makeUnique<T[]>`.
constexpr bool testConstexprArrayUniquePtr()
{
    UniquePtr<gp::Int32[]> empty;
    if (empty.isValid() || static_cast<bool>(empty) || empty != nullptr || !(empty == nullptr))
    {
        return false;
    }

    UniquePtr<gp::Int32[]> arr = makeUnique<gp::Int32[]>(4u);
    if (!arr.isValid() || !static_cast<bool>(arr) || arr == nullptr)
    {
        return false;
    }
    if (arr[0] != 0 || arr[1] != 0 || arr[2] != 0 || arr[3] != 0)
    {
        return false;
    }

    arr[0] = 10;
    arr[3] = 40;

    // Qualification conversion: UniquePtr<T[]> -> UniquePtr<const T[]>.
    UniquePtr<const gp::Int32[]> constArr(std::move(arr));
    if (arr.isValid() || !constArr.isValid() || constArr[0] != 10 || constArr[3] != 40)
    {
        return false;
    }

    const gp::Int32* const released = constArr.release();
    if (constArr.isValid() || released == nullptr || released[0] != 10)
    {
        delete[] released;
        return false;
    }
    delete[] released;

    // Overwrite factory skips zero-initialization; write before reading to stay valid in constexpr.
    UniquePtr<gp::Int32[]> overwriteArr = makeUniqueForOverwrite<gp::Int32[]>(3u);
    overwriteArr[0] = 5;
    overwriteArr[1] = 6;
    overwriteArr[2] = 7;

    gp::Int32 arrayDeletes = 0;
    gp::Int32 nullViolations = 0;
    UniquePtr<gp::Int32[], StrictArrayDeleter> customArr(
        new gp::Int32[2]{ 1, 2 }, StrictArrayDeleter(&arrayDeletes, &nullViolations, 7)
    );
    UniquePtr<gp::Int32[], StrictArrayDeleter> otherCustomArr(
        nullptr, StrictArrayDeleter(&arrayDeletes, &nullViolations, 8)
    );

    if (customArr == overwriteArr || !(customArr != overwriteArr))
    {
        return false;
    }
    if (customArr == otherCustomArr || otherCustomArr != nullptr)
    {
        return false;
    }

    customArr.swap(otherCustomArr);
    if (customArr.isValid() || !otherCustomArr.isValid() || otherCustomArr[1] != 2 || customArr.getDeleter().id != 8 ||
        otherCustomArr.getDeleter().id != 7)
    {
        return false;
    }

    otherCustomArr.reset(new gp::Int32[2]{ 3, 4 });
    if (arrayDeletes != 1 || nullViolations != 0 || otherCustomArr[0] != 3)
    {
        return false;
    }

    otherCustomArr.reset();
    if (otherCustomArr.isValid() || arrayDeletes != 2 || nullViolations != 0)
    {
        return false;
    }

    return true;
}

static_assert(testConstexprSingleUniquePtr(), "Constexpr single-object UniquePtr evaluation failed.");
static_assert(testConstexprArrayUniquePtr(), "Constexpr array UniquePtr evaluation failed.");

// Layout, size, and alignment invariants.
using StatelessLambdaDeleter = decltype([](gp::Int32* const ptr) noexcept
{
    delete ptr;
});

static_assert(sizeof(UniquePtr<gp::Int32>) == sizeof(gp::Int32*));
static_assert(alignof(UniquePtr<gp::Int32>) == alignof(gp::Int32*));
static_assert(sizeof(UniquePtr<gp::Int32[]>) == sizeof(gp::Int32*));
static_assert(alignof(UniquePtr<gp::Int32[]>) == alignof(gp::Int32*));
static_assert(sizeof(UniquePtr<gp::Int32, FinalEmptyDeleter>) == sizeof(gp::Int32*));
static_assert(sizeof(UniquePtr<gp::Int32[], FinalEmptyArrayDeleter>) == sizeof(gp::Int32*));
static_assert(sizeof(UniquePtr<gp::Int32, StatelessLambdaDeleter>) == sizeof(gp::Int32*));

// Member type alias invariants.
static_assert(concepts::IsSameAs<UniquePtr<gp::Int32>::ElementType, gp::Int32>);
static_assert(concepts::IsSameAs<UniquePtr<gp::Int32>::PointerType, gp::Int32*>);
static_assert(concepts::IsSameAs<UniquePtr<gp::Int32>::DeleterType, DefaultDelete<gp::Int32>>);
static_assert(concepts::IsSameAs<UniquePtr<gp::Int32[]>::ElementType, gp::Int32>);
static_assert(concepts::IsSameAs<UniquePtr<gp::Int32[]>::PointerType, gp::Int32*>);
static_assert(concepts::IsSameAs<UniquePtr<gp::Int32[]>::DeleterType, DefaultDelete<gp::Int32[]>>);

// Move-only and relocatability invariants.
static_assert(!concepts::IsCopyConstructible<UniquePtr<gp::Int32>>);
static_assert(!concepts::IsCopyAssignable<UniquePtr<gp::Int32>>);
static_assert(concepts::IsMoveConstructible<UniquePtr<gp::Int32>>);
static_assert(concepts::IsMoveAssignable<UniquePtr<gp::Int32>>);
static_assert(!concepts::IsCopyConstructible<UniquePtr<gp::Int32[]>>);
static_assert(!concepts::IsCopyAssignable<UniquePtr<gp::Int32[]>>);
static_assert(concepts::IsMoveConstructible<UniquePtr<gp::Int32[]>>);
static_assert(concepts::IsMoveAssignable<UniquePtr<gp::Int32[]>>);

static_assert(concepts::IsRelocatable<UniquePtr<gp::Int32>>);
static_assert(concepts::IsRelocatable<UniquePtr<gp::Int32[]>>);
static_assert(concepts::IsRelocatable<UniquePtr<gp::Int32, StrictStatefulDeleter>>);
static_assert(!concepts::IsRelocatable<UniquePtr<gp::Int32, StrictStatefulDeleter&>>);
static_assert(!concepts::IsRelocatable<UniquePtr<gp::Int32, NonRelocatableDeleter>>);

// Array qualification safety: derived-to-base array conversion and cross-form conversion must be rejected.
static_assert(std::is_constructible_v<UniquePtr<PolymorphicBase>, PolymorphicDerived*>);
static_assert(std::is_constructible_v<UniquePtr<PolymorphicBase>, UniquePtr<PolymorphicDerived>&&>);
static_assert(!std::is_constructible_v<UniquePtr<PolymorphicBase[]>, PolymorphicDerived*>);
static_assert(!std::is_constructible_v<UniquePtr<PolymorphicBase[]>, UniquePtr<PolymorphicDerived[]>&&>);
static_assert(!std::is_assignable_v<UniquePtr<PolymorphicBase[]>&, UniquePtr<PolymorphicDerived[]>&&>);
static_assert(!std::is_constructible_v<UniquePtr<gp::Int32[]>, const gp::Int32*>);
static_assert(!std::is_constructible_v<UniquePtr<gp::Int32[]>, UniquePtr<const gp::Int32[]>&&>);
static_assert(!std::is_assignable_v<UniquePtr<gp::Int32[]>&, UniquePtr<const gp::Int32[]>&&>);
static_assert(!std::is_constructible_v<UniquePtr<gp::Int32[]>, void*>);
static_assert(!std::is_constructible_v<UniquePtr<gp::Int32>, UniquePtr<gp::Int32[]>&&>);
static_assert(!std::is_constructible_v<UniquePtr<gp::Int32[]>, UniquePtr<gp::Int32>&&>);
static_assert(!std::is_assignable_v<UniquePtr<gp::Int32>&, UniquePtr<gp::Int32[]>&&>);
static_assert(!std::is_assignable_v<UniquePtr<gp::Int32[]>&, UniquePtr<gp::Int32>&&>);

// Allowed array qualification conversions (`T[]` -> `const T[]`).
static_assert(std::is_constructible_v<UniquePtr<const gp::Int32[]>, gp::Int32*>);
static_assert(std::is_constructible_v<UniquePtr<const gp::Int32[]>, UniquePtr<gp::Int32[]>&&>);
static_assert(std::is_assignable_v<UniquePtr<const gp::Int32[]>&, UniquePtr<gp::Int32[]>&&>);
static_assert(std::is_convertible_v<DefaultDelete<gp::Int32[]>, DefaultDelete<const gp::Int32[]>>);
static_assert(!std::is_convertible_v<DefaultDelete<PolymorphicDerived[]>, DefaultDelete<PolymorphicBase[]>>);

// Raw pointer and reset safety on arrays.
static_assert(!CanResetWith<UniquePtr<PolymorphicBase[]>, PolymorphicDerived*>);
static_assert(!CanResetWith<UniquePtr<gp::Int32[]>, const gp::Int32*>);
static_assert(!CanResetWith<UniquePtr<gp::Int32[]>, void*>);
static_assert(CanResetWith<UniquePtr<const gp::Int32[]>, gp::Int32*>);
static_assert(!CanInvokeDeleterWith<DefaultDelete<PolymorphicBase[]>, PolymorphicDerived*>);

// Operator separation: single object has `*`/`->` (except `void`), array has `[]`.
static_assert(HasDereference<UniquePtr<gp::Int32>>);
static_assert(HasMemberArrow<UniquePtr<gp::Int32>>);
static_assert(!HasSubscript<UniquePtr<gp::Int32>>);
static_assert(!HasDereference<UniquePtr<void, UnrelatedDeleter>>);
static_assert(HasSubscript<UniquePtr<gp::Int32[]>>);
static_assert(!HasDereference<UniquePtr<gp::Int32[]>>);
static_assert(!HasMemberArrow<UniquePtr<gp::Int32[]>>);

// Function pointer and reference deleter safety constraints.
static_assert(!std::is_default_constructible_v<UniquePtr<gp::Int32, void (*)(gp::Int32*)>>);
static_assert(!std::is_constructible_v<UniquePtr<gp::Int32, void (*)(gp::Int32*)>, gp::NullPtrT>);
static_assert(!std::is_default_constructible_v<UniquePtr<gp::Int32[], void (*)(gp::Int32*)>>);
static_assert(!std::is_constructible_v<UniquePtr<gp::Int32[], void (*)(gp::Int32*)>, gp::NullPtrT>);
static_assert(!std::is_default_constructible_v<UniquePtr<gp::Int32, StrictStatefulDeleter&>>);
static_assert(
    !std::is_constructible_v<UniquePtr<gp::Int32, StrictStatefulDeleter&>, gp::Int32*, StrictStatefulDeleter&&>
);
static_assert(
    !std::is_constructible_v<UniquePtr<gp::Int32, const StrictStatefulDeleter&>, gp::Int32*, StrictStatefulDeleter&&>
);
static_assert(!std::is_move_assignable_v<UniquePtr<gp::Int32, const StrictStatefulDeleter&>>);
static_assert(!std::is_constructible_v<UniquePtr<gp::Int32[], StrictArrayDeleter&>, gp::Int32*, StrictArrayDeleter&&>);
static_assert(
    !std::is_constructible_v<UniquePtr<gp::Int32[], const StrictArrayDeleter&>, gp::Int32*, StrictArrayDeleter&&>
);
static_assert(
    !std::is_constructible_v<UniquePtr<gp::Int32[], StrictArrayDeleter&>, gp::NullPtrT, StrictArrayDeleter&&>
);
static_assert(!std::is_move_assignable_v<UniquePtr<gp::Int32[], const StrictArrayDeleter&>>);
static_assert(!std::is_move_constructible_v<UniquePtr<gp::Int32, NonMovableDeleter>>);

// Incompatible deleter conversion rejection.
static_assert(!std::is_constructible_v<UniquePtr<gp::Int32, UnrelatedDeleter>, UniquePtr<gp::Int32>&&>);
static_assert(!std::is_assignable_v<UniquePtr<gp::Int32, UnrelatedDeleter>&, UniquePtr<gp::Int32>&&>);

// Bounded array rejection in factory helpers.
static_assert(!CanMakeUnique<gp::Int32[4]>);
static_assert(!CanMakeUnique<gp::Int32[4], gp::USize>);
static_assert(!CanMakeUniqueOverwrite<gp::Int32[4]>);
static_assert(!CanMakeUniqueOverwrite<gp::Int32[4], gp::USize>);
static_assert(!CanMakeUniqueOverwrite<const gp::Int32>);
static_assert(!CanMakeUniqueOverwrite<const gp::Int32[], gp::USize>);
static_assert(CanMakeUnique<const gp::Int32[], gp::USize>);

// Equality and three-way comparison concept checks across deleters and rejection of unrelated types.
static_assert(concepts::IsEqualityComparable<UniquePtr<gp::Int32>>);
static_assert(concepts::IsThreeWayComparable<UniquePtr<gp::Int32>>);
static_assert(concepts::IsEqualityComparable<UniquePtr<gp::Int32[]>>);
static_assert(concepts::IsThreeWayComparable<UniquePtr<gp::Int32[]>>);
static_assert(CanEqualityCompare<UniquePtr<gp::Int32, StrictStatefulDeleter>, UniquePtr<gp::Int32, UnrelatedDeleter>>);
static_assert(CanThreeWayCompare<UniquePtr<gp::Int32, StrictStatefulDeleter>, UniquePtr<gp::Int32, UnrelatedDeleter>>);
static_assert(CanEqualityCompare<UniquePtr<gp::Int32[], StrictArrayDeleter>, UniquePtr<const gp::Int32[]>>);
static_assert(CanThreeWayCompare<UniquePtr<gp::Int32[], StrictArrayDeleter>, UniquePtr<const gp::Int32[]>>);
static_assert(!CanEqualityCompare<UniquePtr<gp::Int32>, UniquePtr<gp::Float32>>);
static_assert(!CanThreeWayCompare<UniquePtr<gp::Int32>, UniquePtr<gp::Float32>>);

}   // namespace

class UniquePtrTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        IncompleteSingle::s_activeCount = 0;
        IncompleteArrayElem::s_activeCount = 0;
        LifetimeTracker::resetCounters();
        PolymorphicBase::s_activeBases = 0;
        PolymorphicDerived::s_activeDerived = 0;
        ReentrantArrayElement::s_observedOwner = nullptr;
        ReentrantArrayElement::s_nullObservedCount = 0;
        g_fnPtrDeleteCount = 0;
        g_fnPtrArrayDeleteCount = 0;
    }

    virtual void TearDown() override
    {
        EXPECT_EQ(IncompleteSingle::s_activeCount, 0) << "IncompleteSingle leaked!";
        EXPECT_EQ(IncompleteArrayElem::s_activeCount, 0) << "IncompleteArrayElem leaked!";
        EXPECT_EQ(LifetimeTracker::s_activeInstances, 0) << "LifetimeTracker leaked!";
        EXPECT_EQ(PolymorphicBase::s_activeBases, 0) << "PolymorphicBase leaked!";
        EXPECT_EQ(PolymorphicDerived::s_activeDerived, 0) << "PolymorphicDerived leaked!";
    }
};

TEST_F(UniquePtrTest, DefaultDeleteSingleAndArrayMethodsWork)
{
    DefaultDelete<PolymorphicDerived> derivedDeleter;
    DefaultDelete<PolymorphicBase> baseDeleter(derivedDeleter);
    baseDeleter = derivedDeleter;

    PolymorphicBase* const obj = new PolymorphicDerived();
    EXPECT_EQ(PolymorphicDerived::s_activeDerived, 1);
    baseDeleter(obj);
    baseDeleter(nullptr);
    EXPECT_EQ(PolymorphicDerived::s_activeDerived, 0);

    DefaultDelete<LifetimeTracker[]> mutableArrDeleter;
    DefaultDelete<const LifetimeTracker[]> constArrDeleter(mutableArrDeleter);
    constArrDeleter = mutableArrDeleter;

    LifetimeTracker* const rawArray = new LifetimeTracker[3];
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 3);
    constArrDeleter(rawArray);
    constArrDeleter(static_cast<LifetimeTracker*>(nullptr));
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 0);
}

TEST_F(UniquePtrTest, SingleDefaultAndNullptrConstruction)
{
    const UniquePtr<LifetimeTracker> defaultPtr;
    const UniquePtr<LifetimeTracker> nullPtr(nullptr);
    const UniquePtr<LifetimeTracker> implicitNullPtr = nullptr;

    EXPECT_FALSE(defaultPtr.isValid());
    EXPECT_FALSE(static_cast<bool>(defaultPtr));
    EXPECT_EQ(defaultPtr.get(), nullptr);

    EXPECT_FALSE(nullPtr.isValid());
    EXPECT_FALSE(static_cast<bool>(nullPtr));
    EXPECT_EQ(nullPtr.get(), nullptr);

    EXPECT_FALSE(implicitNullPtr.isValid());
    EXPECT_EQ(implicitNullPtr.get(), nullptr);
}

TEST_F(UniquePtrTest, SinglePointerConstructionDereferenceAndDestruction)
{
    {
        UniquePtr<LifetimeTracker> ptr(new LifetimeTracker(42));
        EXPECT_TRUE(ptr.isValid());
        EXPECT_TRUE(static_cast<bool>(ptr));
        EXPECT_NE(ptr.get(), nullptr);
        EXPECT_EQ(LifetimeTracker::s_activeInstances, 1);
        EXPECT_EQ((*ptr).value, 42);
        EXPECT_EQ(ptr->value, 42);

        ptr->value = 99;
        EXPECT_EQ((*ptr).value, 99);
    }

    EXPECT_EQ(LifetimeTracker::s_activeInstances, 0);
    EXPECT_EQ(LifetimeTracker::s_destructCount, 1);
}

TEST_F(UniquePtrTest, SingleMoveConstructionAndAssignments)
{
    UniquePtr<LifetimeTracker> ptr1(new LifetimeTracker(10));
    UniquePtr<LifetimeTracker> ptr2(std::move(ptr1));

    EXPECT_FALSE(ptr1.isValid());
    EXPECT_EQ(ptr1.get(), nullptr);
    EXPECT_TRUE(ptr2.isValid());
    EXPECT_EQ(ptr2->value, 10);
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 1);

    // Move assignment replaces an existing object and destroys the old one first.
    UniquePtr<LifetimeTracker> ptr3(new LifetimeTracker(20));
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 2);

    ptr3 = std::move(ptr2);
    EXPECT_FALSE(ptr2.isValid());
    EXPECT_TRUE(ptr3.isValid());
    EXPECT_EQ(ptr3->value, 10);
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 1);
    EXPECT_EQ(LifetimeTracker::s_destructCount, 1);

    // Self-move assignment must leave ownership intact.
    UniquePtr<LifetimeTracker>& selfRef = ptr3;
    ptr3 = std::move(selfRef);
    EXPECT_TRUE(ptr3.isValid());
    EXPECT_EQ(ptr3->value, 10);
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 1);

    // Nullptr assignment deletes the owned object and resets to empty.
    ptr3 = nullptr;
    EXPECT_FALSE(ptr3.isValid());
    EXPECT_EQ(ptr3.get(), nullptr);
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 0);
    EXPECT_EQ(LifetimeTracker::s_destructCount, 2);
}

TEST_F(UniquePtrTest, SingleReleaseResetAndSwap)
{
    UniquePtr<LifetimeTracker> ptr(new LifetimeTracker(99));
    LifetimeTracker* const raw = ptr.release();

    EXPECT_FALSE(ptr.isValid());
    EXPECT_EQ(ptr.get(), nullptr);
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->value, 99);
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 1);
    delete raw;
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 0);

    // Reset transitions: null -> live -> live -> self-reset -> null.
    ptr.reset(new LifetimeTracker(1));
    EXPECT_TRUE(ptr.isValid());
    EXPECT_EQ(ptr->value, 1);
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 1);

    ptr.reset(new LifetimeTracker(2));
    EXPECT_EQ(ptr->value, 2);
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 1);

    ptr.reset(ptr.get());
    EXPECT_TRUE(ptr.isValid());
    EXPECT_EQ(ptr->value, 2);
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 1);

    UniquePtr<LifetimeTracker> other(new LifetimeTracker(5));
    ptr.swap(other);
    EXPECT_EQ(ptr->value, 5);
    EXPECT_EQ(other->value, 2);

    swap(ptr, other);
    EXPECT_EQ(ptr->value, 2);
    EXPECT_EQ(other->value, 5);

    ptr.reset();
    other.reset(nullptr);
    EXPECT_FALSE(ptr.isValid());
    EXPECT_FALSE(other.isValid());
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 0);
}

TEST_F(UniquePtrTest, SinglePolymorphismAndCompatibleDeleterMove)
{
    UniquePtr<PolymorphicBase> poly(new PolymorphicDerived());
    EXPECT_EQ(poly->getValue(), 2);
    EXPECT_EQ(PolymorphicDerived::s_activeDerived, 1);

    UniquePtr<PolymorphicDerived> derived = makeUnique<PolymorphicDerived>();
    EXPECT_EQ(PolymorphicDerived::s_activeDerived, 2);

    // Converting move assignment replaces existing base object cleanly.
    poly = std::move(derived);
    EXPECT_FALSE(derived.isValid());
    EXPECT_EQ(poly->getValue(), 2);
    EXPECT_EQ(PolymorphicDerived::s_activeDerived, 1);

    UniquePtr<PolymorphicBase> movedBase(std::move(poly));
    EXPECT_FALSE(poly.isValid());
    EXPECT_EQ(movedBase->getValue(), 2);

    // Converting move construction across compatible custom deleters.
    gp::Int32 customDeleteCount = 0;
    UniquePtr<PolymorphicDerived, DerivedCustomDeleter> customDerived(
        new PolymorphicDerived(), DerivedCustomDeleter(&customDeleteCount)
    );
    UniquePtr<PolymorphicBase, BaseCustomDeleter> customBase(std::move(customDerived));
    EXPECT_FALSE(customDerived.isValid());
    EXPECT_TRUE(customBase.isValid());
    customBase.reset();
    EXPECT_EQ(customDeleteCount, 1);
}

TEST_F(UniquePtrTest, SingleCustomDeletersReferenceMoveOnlyAndFunctionPointer)
{
    gp::Int32 deleteCount = 0;
    gp::Int32 nullViolations = 0;

    // Empty and nullptr UniquePtr must never invoke the deleter on destruction.
    {
        const StrictStatefulDeleter deleter(&deleteCount, &nullViolations, 1);
        UniquePtr<gp::Int32, StrictStatefulDeleter> emptyCustom(nullptr, deleter);
        EXPECT_FALSE(emptyCustom.isValid());
        EXPECT_EQ(emptyCustom.getDeleter().id, 1);
    }
    EXPECT_EQ(deleteCount, 0);
    EXPECT_EQ(nullViolations, 0);

    // Move assignment uses the destination's old deleter before adopting the source's deleter.
    gp::Int32 oldDeletes = 0;
    gp::Int32 newDeletes = 0;
    {
        UniquePtr<gp::Int32, StrictStatefulDeleter> dst(
            new gp::Int32(1), StrictStatefulDeleter(&oldDeletes, &nullViolations, 10)
        );
        UniquePtr<gp::Int32, StrictStatefulDeleter> src(
            new gp::Int32(2), StrictStatefulDeleter(&newDeletes, &nullViolations, 20)
        );
        dst = std::move(src);
        EXPECT_EQ(oldDeletes, 1);
        EXPECT_EQ(newDeletes, 0);
        EXPECT_EQ(dst.getDeleter().id, 20);
        EXPECT_EQ(*dst, 2);
    }
    EXPECT_EQ(oldDeletes, 1);
    EXPECT_EQ(newDeletes, 1);
    EXPECT_EQ(nullViolations, 0);

    // Move-only deleter.
    gp::Int32 moveOnlyDeletes = 0;
    {
        UniquePtr<gp::Int32, MoveOnlyDeleter> p1(new gp::Int32(12), MoveOnlyDeleter(&moveOnlyDeletes));
        UniquePtr<gp::Int32, MoveOnlyDeleter> p2(std::move(p1));
        EXPECT_FALSE(p1.isValid());
        EXPECT_EQ(*p2, 12);
    }
    EXPECT_EQ(moveOnlyDeletes, 1);

    // Function pointer deleter.
    {
        const UniquePtr<gp::Int32, void (*)(gp::Int32*)> fnPtr(new gp::Int32(33), &deleteIntViaFunctionPointer);
        EXPECT_TRUE(fnPtr.isValid());
        EXPECT_EQ(*fnPtr, 33);
        EXPECT_EQ(fnPtr.getDeleter(), &deleteIntViaFunctionPointer);
    }
    EXPECT_EQ(g_fnPtrDeleteCount, 1);

    // Reference deleters (`D&` and `const D&`).
    gp::Int32 refDeletes = 0;
    StrictStatefulDeleter externalDeleter(&refDeletes, &nullViolations, 42);
    {
        UniquePtr<gp::Int32, StrictStatefulDeleter&> refPtr(new gp::Int32(7), externalDeleter);
        const UniquePtr<gp::Int32, const StrictStatefulDeleter&> constRefPtr(new gp::Int32(8), externalDeleter);
        EXPECT_EQ(&refPtr.getDeleter(), &externalDeleter);
        EXPECT_EQ(&constRefPtr.getDeleter(), &externalDeleter);
    }
    EXPECT_EQ(refDeletes, 2);
    EXPECT_EQ(nullViolations, 0);

    // Explicit default constructor deleter and final empty deleter.
    const UniquePtr<gp::Int32, ExplicitDefaultDeleter> explicitPtr(new gp::Int32(4));
    EXPECT_EQ(explicitPtr.getDeleter().tag, 9);
    const UniquePtr<gp::Int32, FinalEmptyDeleter> finalPtr(new gp::Int32(5));
    EXPECT_EQ(*finalPtr, 5);
}

TEST_F(UniquePtrTest, ArrayConstructionIndexingAndDestruction)
{
    const UniquePtr<LifetimeTracker[]> defaultArr;
    const UniquePtr<LifetimeTracker[]> nullArr(nullptr);
    const UniquePtr<LifetimeTracker[]> implicitNullArr = nullptr;

    EXPECT_FALSE(defaultArr.isValid());
    EXPECT_FALSE(static_cast<bool>(defaultArr));
    EXPECT_EQ(defaultArr.get(), nullptr);
    EXPECT_FALSE(nullArr.isValid());
    EXPECT_EQ(implicitNullArr.get(), nullptr);

    constexpr gp::USize kCount = 5u;
    {
        UniquePtr<LifetimeTracker[]> arr(new LifetimeTracker[kCount]);
        EXPECT_TRUE(arr.isValid());
        EXPECT_TRUE(static_cast<bool>(arr));
        EXPECT_NE(arr.get(), nullptr);
        EXPECT_EQ(LifetimeTracker::s_activeInstances, static_cast<gp::Int32>(kCount));

        for (gp::USize i = 0u; i < kCount; ++i)
        {
            arr[i].value = static_cast<gp::Int32>(i * 10u);
        }

        const UniquePtr<LifetimeTracker[]>& constView = arr;
        for (gp::USize i = 0u; i < kCount; ++i)
        {
            EXPECT_EQ(constView[i].value, static_cast<gp::Int32>(i * 10u));
        }
    }

    EXPECT_EQ(LifetimeTracker::s_activeInstances, 0);
    EXPECT_EQ(LifetimeTracker::s_destructCount, static_cast<gp::Int32>(kCount));
}

TEST_F(UniquePtrTest, ArrayMoveQualificationConversionsReleaseResetAndSwap)
{
    UniquePtr<LifetimeTracker[]> arr1 = makeUnique<LifetimeTracker[]>(3u);
    arr1[0].value = 11;
    arr1[1].value = 22;
    arr1[2].value = 33;
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 3);

    UniquePtr<LifetimeTracker[]> arr2(std::move(arr1));
    EXPECT_FALSE(arr1.isValid());
    EXPECT_TRUE(arr2.isValid());
    EXPECT_EQ(arr2[1].value, 22);

    // Self-move assignment must preserve the array.
    UniquePtr<LifetimeTracker[]>& selfRef = arr2;
    arr2 = std::move(selfRef);
    EXPECT_TRUE(arr2.isValid());
    EXPECT_EQ(arr2[2].value, 33);

    // Converting move construction & assignment (`T[]` -> `const T[]`).
    UniquePtr<const LifetimeTracker[]> constArr(std::move(arr2));
    EXPECT_FALSE(arr2.isValid());
    EXPECT_TRUE(constArr.isValid());
    EXPECT_EQ(constArr[0].value, 11);

    UniquePtr<LifetimeTracker[]> arr3 = makeUnique<LifetimeTracker[]>(2u);
    arr3[0].value = 77;
    constArr = std::move(arr3);
    EXPECT_FALSE(arr3.isValid());
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 2);
    EXPECT_EQ(constArr[0].value, 77);

    // Release transfers raw ownership back to caller.
    const LifetimeTracker* const rawArr = constArr.release();
    EXPECT_FALSE(constArr.isValid());
    EXPECT_EQ(constArr.get(), nullptr);
    ASSERT_NE(rawArr, nullptr);
    EXPECT_EQ(rawArr[0].value, 77);
    delete[] rawArr;
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 0);

    // Reset with qualification conversion (`T*` into `UniquePtr<const T[]>`), self-reset, and nullptr reset.
    constArr.reset(new LifetimeTracker[2]);
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 2);

    UniquePtr<const LifetimeTracker[]> otherConstArr(new LifetimeTracker[1]);
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 3);

    constArr.swap(otherConstArr);
    swap(constArr, otherConstArr);
    EXPECT_TRUE(constArr.isValid());
    EXPECT_TRUE(otherConstArr.isValid());

    constArr = nullptr;
    otherConstArr.reset();
    EXPECT_FALSE(constArr.isValid());
    EXPECT_FALSE(otherConstArr.isValid());
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 0);
}

TEST_F(UniquePtrTest, ArrayCustomDeletersReferenceMoveOnlyAndFunctionPointer)
{
    gp::Int32 deleteCount = 0;
    gp::Int32 nullViolations = 0;

    // Empty array UniquePtr with custom deleter must skip deleter invocation.
    {
        const StrictArrayDeleter deleter(&deleteCount, &nullViolations, 5);
        const UniquePtr<gp::Int32[], StrictArrayDeleter> emptyArr(nullptr, deleter);
        EXPECT_FALSE(emptyArr.isValid());
        EXPECT_EQ(emptyArr.getDeleter().id, 5);
    }
    EXPECT_EQ(deleteCount, 0);
    EXPECT_EQ(nullViolations, 0);

    // Copyable and rvalue custom array deleters, including `T*` -> `const T[]`.
    {
        const StrictArrayDeleter deleter(&deleteCount, &nullViolations, 1);
        const UniquePtr<const gp::Int32[], StrictArrayDeleter> constArr(new gp::Int32[3]{ 1, 2, 3 }, deleter);
        EXPECT_EQ(constArr[2], 3);

        UniquePtr<gp::Int32[], StrictArrayDeleter> movedArr(
            new gp::Int32[2]{ 4, 5 }, StrictArrayDeleter(&deleteCount, &nullViolations, 2)
        );
        EXPECT_EQ(movedArr.getDeleter().id, 2);
        movedArr.reset(movedArr.get());
        EXPECT_EQ(deleteCount, 0);
    }
    EXPECT_EQ(deleteCount, 2);
    EXPECT_EQ(nullViolations, 0);

    // Move-only array deleter.
    gp::Int32 moveOnlyDeletes = 0;
    {
        UniquePtr<gp::Int32[], MoveOnlyDeleter> a1(new gp::Int32[2]{ 8, 9 }, MoveOnlyDeleter(&moveOnlyDeletes, true));
        UniquePtr<gp::Int32[], MoveOnlyDeleter> a2(std::move(a1));
        EXPECT_FALSE(a1.isValid());
        EXPECT_EQ(a2[0], 8);
    }
    EXPECT_EQ(moveOnlyDeletes, 1);

    // Function pointer array deleter (both pointer and nullptr construction).
    {
        const UniquePtr<gp::Int32[], void (*)(gp::Int32*)> fnNull(nullptr, &deleteIntArrayViaFunctionPointer);
        EXPECT_FALSE(fnNull.isValid());
        const UniquePtr<gp::Int32[], void (*)(gp::Int32*)> fnArr(
            new gp::Int32[3]{ 1, 2, 3 }, &deleteIntArrayViaFunctionPointer
        );
        EXPECT_EQ(fnArr[1], 2);
    }
    EXPECT_EQ(g_fnPtrArrayDeleteCount, 1);

    // Reference array deleters (`D&` and `const D&`, pointer and nullptr).
    gp::Int32 refDeletes = 0;
    StrictArrayDeleter externalDeleter(&refDeletes, &nullViolations, 77);
    {
        UniquePtr<gp::Int32[], StrictArrayDeleter&> refArr(new gp::Int32[2]{ 6, 7 }, externalDeleter);
        UniquePtr<gp::Int32[], StrictArrayDeleter&> refNullArr(nullptr, externalDeleter);
        const UniquePtr<gp::Int32[], const StrictArrayDeleter&> constRefArr(new gp::Int32[1]{ 9 }, externalDeleter);
        EXPECT_FALSE(refNullArr.isValid());
        EXPECT_EQ(&refArr.getDeleter(), &externalDeleter);
        EXPECT_EQ(&constRefArr.getDeleter(), &externalDeleter);
    }
    EXPECT_EQ(refDeletes, 2);
    EXPECT_EQ(nullViolations, 0);
}

TEST_F(UniquePtrTest, GeneralizedComparisonsAcrossDeletersTypesAndArrays)
{
    gp::Int32 deleteCountA = 0;
    gp::Int32 deleteCountB = 0;

    UniquePtr<gp::Int32> defaultInt = makeUnique<gp::Int32>(1);
    UniquePtr<gp::Int32, StrictStatefulDeleter> customInt(
        new gp::Int32(1), StrictStatefulDeleter(&deleteCountA, nullptr, 1)
    );
    UniquePtr<const gp::Int32, FinalEmptyDeleter> constCustomInt(new gp::Int32(2));
    UniquePtr<gp::Int32, UnrelatedDeleter> nullCustom;

    // Nullptr comparisons (`==`, `!=`, `<=>`, `<`, `<=`, `>`, `>=`) for single objects.
    EXPECT_TRUE(nullCustom == nullptr);
    EXPECT_TRUE(nullptr == nullCustom);
    EXPECT_FALSE(nullCustom != nullptr);
    EXPECT_TRUE(defaultInt != nullptr);
    EXPECT_TRUE(nullptr != defaultInt);
    EXPECT_EQ(nullCustom <=> nullptr, std::strong_ordering::equal);
    EXPECT_EQ(defaultInt <=> nullptr, std::strong_ordering::greater);
    EXPECT_EQ(nullptr <=> defaultInt, std::strong_ordering::less);
    EXPECT_GT(defaultInt, nullptr);
    EXPECT_LT(nullptr, defaultInt);

    // Cross-deleter equality and ordering for single objects.
    EXPECT_NE(defaultInt, customInt);
    EXPECT_FALSE(defaultInt == customInt);
    EXPECT_NE(customInt, constCustomInt);

    // Two null UniquePtrs with completely different types and deleters compare equal.
    const UniquePtr<PolymorphicBase> nullBase;
    const UniquePtr<PolymorphicDerived, BaseCustomDeleter> nullDerived(nullptr, BaseCustomDeleter(&deleteCountB));
    EXPECT_EQ(nullBase, nullDerived);
    EXPECT_EQ(nullBase <=> nullDerived, std::strong_ordering::equal);

    // Polymorphic non-null comparison across different deleters.
    const UniquePtr<PolymorphicBase> liveBase = makeUnique<PolymorphicBase>();
    const UniquePtr<PolymorphicDerived, BaseCustomDeleter> liveDerived(
        new PolymorphicDerived(), BaseCustomDeleter(&deleteCountB)
    );
    EXPECT_NE(liveBase, liveDerived);
    if (liveBase.get() < static_cast<const PolymorphicBase*>(liveDerived.get()))
    {
        EXPECT_EQ(liveBase <=> liveDerived, std::strong_ordering::less);
        EXPECT_LT(liveBase, liveDerived);
    }
    else
    {
        EXPECT_EQ(liveBase <=> liveDerived, std::strong_ordering::greater);
        EXPECT_GT(liveBase, liveDerived);
    }

    // Cross-deleter array equality and ordering (`T[]` vs `const T[]` and distinct deleters).
    const UniquePtr<gp::Int32[]> defaultArr = makeUnique<gp::Int32[]>(2u);
    const UniquePtr<gp::Int32[], StrictArrayDeleter> customArr(
        new gp::Int32[2]{ 3, 4 }, StrictArrayDeleter(&deleteCountA, nullptr, 3)
    );
    const UniquePtr<const gp::Int32[], FinalEmptyArrayDeleter> constArr(new gp::Int32[2]{ 5, 6 });
    const UniquePtr<gp::Int32[], StrictArrayDeleter> nullArr(nullptr, StrictArrayDeleter(&deleteCountA, nullptr, 4));

    EXPECT_EQ(nullArr, nullptr);
    EXPECT_EQ(nullptr, nullArr);
    EXPECT_NE(customArr, nullptr);
    EXPECT_EQ(customArr <=> nullptr, std::strong_ordering::greater);
    EXPECT_EQ(nullArr <=> nullptr, std::strong_ordering::equal);
    EXPECT_NE(defaultArr, customArr);
    EXPECT_NE(customArr, constArr);

    if (customArr.get() < constArr.get())
    {
        EXPECT_EQ(customArr <=> constArr, std::strong_ordering::less);
        EXPECT_LT(customArr, constArr);
    }
    else
    {
        EXPECT_EQ(customArr <=> constArr, std::strong_ordering::greater);
        EXPECT_GT(customArr, constArr);
    }

    // Releasing and aliasing two UniquePtrs briefly to verify equal non-null cross-deleter comparison.
    gp::Int32* const sharedRaw = defaultInt.release();
    customInt.reset();
    UniquePtr<gp::Int32, StrictStatefulDeleter> aliasA(sharedRaw, StrictStatefulDeleter(&deleteCountA));
    UniquePtr<const gp::Int32, UnrelatedDeleter> aliasB(sharedRaw);
    EXPECT_EQ(aliasA, aliasB);
    EXPECT_EQ(aliasA <=> aliasB, std::strong_ordering::equal);
    (void)aliasB.release();
}

TEST_F(UniquePtrTest, FactoriesSingleAndArrayBehavior)
{
    {
        const UniquePtr<LifetimeTracker> singleDefault = makeUnique<LifetimeTracker>();
        EXPECT_TRUE(singleDefault.isValid());
        EXPECT_EQ(singleDefault->value, 0);

        const UniquePtr<LifetimeTracker> singleArg = makeUnique<LifetimeTracker>(7);
        EXPECT_EQ(singleArg->value, 7);
        EXPECT_EQ(LifetimeTracker::s_activeInstances, 2);
    }
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 0);

    const UniquePtr<AggregatePoint> aggregate = makeUnique<AggregatePoint>(3, 4);
    EXPECT_EQ(aggregate->x, 3);
    EXPECT_EQ(aggregate->y, 4);

    const UniquePtr<gp::Int32> overwriteSingle = makeUniqueForOverwrite<gp::Int32>();
    EXPECT_TRUE(overwriteSingle.isValid());

    // `makeUnique<T[]>(count)` value-initializes trivial primitives to zero.
    constexpr gp::USize kPrimitiveCount = 16u;
    const UniquePtr<gp::UInt32[]> zeroed = makeUnique<gp::UInt32[]>(kPrimitiveCount);
    ASSERT_TRUE(zeroed.isValid());
    for (gp::USize i = 0u; i < kPrimitiveCount; ++i)
    {
        EXPECT_EQ(zeroed[i], 0u);
    }

    // `makeUnique<T[]>(count)` constructs non-trivial elements and destroys all of them on exit.
    {
        const UniquePtr<LifetimeTracker[]> objects = makeUnique<LifetimeTracker[]>(4u);
        ASSERT_TRUE(objects.isValid());
        EXPECT_EQ(LifetimeTracker::s_activeInstances, 4);
        EXPECT_EQ(objects[0].value, 0);
        EXPECT_EQ(objects[3].value, 0);
    }
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 0);

    // Zero-length arrays must allocate and free cleanly.
    const UniquePtr<gp::Int32[]> zeroLen = makeUnique<gp::Int32[]>(0u);
    const UniquePtr<gp::Int32[]> zeroLenOverwrite = makeUniqueForOverwrite<gp::Int32[]>(0u);
    EXPECT_TRUE(zeroLen.isValid());
    EXPECT_TRUE(zeroLenOverwrite.isValid());

    // Array of fixed-size rows (`T[][3]`).
    const UniquePtr<gp::Int32[][3]> rows = makeUnique<gp::Int32[][3]>(2u);
    ASSERT_TRUE(rows.isValid());
    EXPECT_EQ(rows[0][0], 0);
    EXPECT_EQ(rows[1][2], 0);
    rows[1][2] = 42;
    EXPECT_EQ(rows[1][2], 42);

    // `makeUniqueForOverwrite<T[]>(count)` leaves trivial elements ready to overwrite and constructs class elements.
    const UniquePtr<gp::UInt8[]> scratchBuffer = makeUniqueForOverwrite<gp::UInt8[]>(8u);
    ASSERT_TRUE(scratchBuffer.isValid());
    for (gp::USize i = 0u; i < 8u; ++i)
    {
        scratchBuffer[i] = static_cast<gp::UInt8>(i + 1u);
        EXPECT_EQ(scratchBuffer[i], static_cast<gp::UInt8>(i + 1u));
    }

    {
        const UniquePtr<LifetimeTracker[]> overwriteObjects = makeUniqueForOverwrite<LifetimeTracker[]>(3u);
        ASSERT_TRUE(overwriteObjects.isValid());
        EXPECT_EQ(LifetimeTracker::s_activeInstances, 3);
    }
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 0);
}

TEST_F(UniquePtrTest, IncompleteTypeMembersAndReentrantDestructionSafety)
{
    {
        IncompleteHolder holder;
        holder.single = makeUnique<IncompleteSingle>(15);
        holder.array = makeUnique<IncompleteArrayElem[]>(3u);
        EXPECT_EQ(IncompleteSingle::s_activeCount, 1);
        EXPECT_EQ(IncompleteArrayElem::s_activeCount, 3);
        EXPECT_EQ(holder.single->id, 15);
    }
    EXPECT_EQ(IncompleteSingle::s_activeCount, 0);
    EXPECT_EQ(IncompleteArrayElem::s_activeCount, 0);

    // Reentrancy: owning pointer must already read null while the destructor executes.
    bool observedNullOnDestroy = false;
    UniquePtr<ReentrantSingleNode> singleOwner;
    singleOwner = makeUnique<ReentrantSingleNode>(&singleOwner, &observedNullOnDestroy);
    singleOwner.reset();
    EXPECT_TRUE(observedNullOnDestroy);

    UniquePtr<ReentrantArrayElement[]> arrayOwner = makeUnique<ReentrantArrayElement[]>(3u);
    ReentrantArrayElement::s_observedOwner = &arrayOwner;
    arrayOwner.reset();
    EXPECT_EQ(ReentrantArrayElement::s_nullObservedCount, 3);
}

#if GP_DO_CHECK
TEST_F(UniquePtrTest, CheckedBuildAssertsOnNullDereference)
{
    static gp::UInt32 s_assertHitCount = 0u;
    s_assertHitCount = 0u;

    const gp::diagnostics::AssertHandlerFn previous = gp::diagnostics::setAssertHandler(
        [](const gp::diagnostics::AssertContext&) noexcept -> gp::diagnostics::AssertAction
    {
        ++s_assertHitCount;
        return gp::diagnostics::AssertAction::Continue;
    }
    );
    // clang-format off
    GP_ON_SCOPE_EXIT
    {
        gp::diagnostics::setAssertHandler(previous);
    };
    // clang-format on

    const UniquePtr<LifetimeTracker> nullSingle;
    EXPECT_EQ(nullSingle.operator->(), nullptr);
    EXPECT_EQ(s_assertHitCount, 1u);
}
#endif

#if GP_DO_CHECK && defined(GTEST_HAS_DEATH_TEST) && GTEST_HAS_DEATH_TEST
TEST_F(UniquePtrTest, CheckedBuildDeathOnNullDereferenceAndIndexing)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");

    EXPECT_DEATH(
        {
            gp::diagnostics::setAssertHandler(nullptr);
            const UniquePtr<gp::Int32> nullPtr;
            (void)*nullPtr;
        },
        "Attempting to dereference a null UniquePtr"
    );

    EXPECT_DEATH(
        {
            gp::diagnostics::setAssertHandler(nullptr);
            const UniquePtr<gp::Int32[]> nullArr;
            (void)nullArr[0u];
        },
        "Attempting to index a null UniquePtr<T\\[\\]>"
    );
}
#endif

}   // namespace gp::memory::tests
