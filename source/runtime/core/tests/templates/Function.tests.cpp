// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "templates/Function.hpp"
#include "CoreMinimal.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "templates/Delegate.hpp"
#include "templates/Optional.hpp"
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

namespace gp::tests
{

namespace
{

gp::Int32 g_functionCallSum = 0;

void addToFunctionGlobal(const gp::Int32 delta) noexcept
{
    g_functionCallSum += delta;
}

gp::Int32 computeWeightedSum(const gp::Int32 a, const gp::Int32 b) noexcept
{
    return (a * 10) + b;
}

struct SampleReceiver final
{
    gp::Int32 total{ 100 };

    void accumulate(const gp::Int32 delta) noexcept
    {
        total += delta;
    }

    [[nodiscard]] gp::Int32 computeWithBonus(const gp::Int32 bonus) const noexcept
    {
        return total + bonus;
    }
};

struct SmallLifecycleCallable final
{
    static inline gp::Int32 s_constructCount{ 0 };
    static inline gp::Int32 s_copyCount{ 0 };
    static inline gp::Int32 s_moveCount{ 0 };
    static inline gp::Int32 s_destructCount{ 0 };

    static void resetCounts() noexcept
    {
        s_constructCount = 0;
        s_copyCount = 0;
        s_moveCount = 0;
        s_destructCount = 0;
    }

    gp::Int32 offset{ 0 };

    explicit SmallLifecycleCallable(const gp::Int32 inOffset) noexcept
        : offset(inOffset)
    {
        ++s_constructCount;
    }

    SmallLifecycleCallable(const SmallLifecycleCallable& other) noexcept
        : offset(other.offset)
    {
        ++s_copyCount;
    }

    SmallLifecycleCallable(SmallLifecycleCallable&& other) noexcept
        : offset(other.offset)
    {
        other.offset = -1;
        ++s_moveCount;
    }

    ~SmallLifecycleCallable() noexcept
    {
        ++s_destructCount;
    }

    [[nodiscard]] gp::Int32 operator()(const gp::Int32 x) const noexcept
    {
        return offset + x;
    }
};

/// @brief Large 64-byte callable that exceeds the default 32-byte inline SBO buffer and spills to HeapAllocator.
struct LargeHeapCallable final
{
    static inline gp::Int32 s_constructCount{ 0 };
    static inline gp::Int32 s_copyCount{ 0 };
    static inline gp::Int32 s_moveCount{ 0 };
    static inline gp::Int32 s_destructCount{ 0 };

    static void resetCounts() noexcept
    {
        s_constructCount = 0;
        s_copyCount = 0;
        s_moveCount = 0;
        s_destructCount = 0;
    }

    gp::Int64 weights[8]{};

    explicit LargeHeapCallable(const gp::Int64 base) noexcept
    {
        for (gp::USize i = 0u; i < 8u; ++i)
        {
            weights[i] = base + static_cast<gp::Int64>(i);
        }
        ++s_constructCount;
    }

    LargeHeapCallable(const LargeHeapCallable& other) noexcept
    {
        for (gp::USize i = 0u; i < 8u; ++i)
        {
            weights[i] = other.weights[i];
        }
        ++s_copyCount;
    }

    LargeHeapCallable(LargeHeapCallable&& other) noexcept
    {
        for (gp::USize i = 0u; i < 8u; ++i)
        {
            weights[i] = other.weights[i];
            other.weights[i] = 0;
        }
        ++s_moveCount;
    }

    ~LargeHeapCallable() noexcept
    {
        ++s_destructCount;
    }

    [[nodiscard]] gp::Int64 operator()(const gp::Int64 multiplier) const noexcept
    {
        gp::Int64 sum = 0;
        for (gp::USize i = 0u; i < 8u; ++i)
        {
            sum += weights[i];
        }
        return sum * multiplier;
    }
};

/// @brief Large 64-byte move-only callable for verifying heap spillover inside `UniqueFunction`.
struct LargeMoveOnlyCallable final
{
    static inline gp::Int32 s_constructCount{ 0 };
    static inline gp::Int32 s_moveCount{ 0 };
    static inline gp::Int32 s_destructCount{ 0 };

    static void resetCounts() noexcept
    {
        s_constructCount = 0;
        s_moveCount = 0;
        s_destructCount = 0;
    }

    UniquePtr<gp::Int32> ownedValue{};
    gp::Int64 padding[7]{};

    explicit LargeMoveOnlyCallable(const gp::Int32 initialValue) noexcept
        : ownedValue(new gp::Int32(initialValue))
    {
        for (gp::USize i = 0u; i < 7u; ++i)
        {
            padding[i] = static_cast<gp::Int64>(i + 1u);
        }
        ++s_constructCount;
    }

    LargeMoveOnlyCallable(const LargeMoveOnlyCallable&) = delete;
    LargeMoveOnlyCallable& operator=(const LargeMoveOnlyCallable&) = delete;

    LargeMoveOnlyCallable(LargeMoveOnlyCallable&& other) noexcept
        : ownedValue(std::move(other.ownedValue))
    {
        for (gp::USize i = 0u; i < 7u; ++i)
        {
            padding[i] = other.padding[i];
            other.padding[i] = 0;
        }
        ++s_moveCount;
    }

    ~LargeMoveOnlyCallable() noexcept
    {
        ++s_destructCount;
    }

    [[nodiscard]] gp::Int32 operator()(const gp::Int32 delta) const noexcept
    {
        if (!ownedValue.isValid())
        {
            return -1;
        }
        return *ownedValue + delta + static_cast<gp::Int32>(padding[0]);
    }
};

/// @brief Over-aligned (64-byte cache-line aligned) small callable that fits in 32 bytes by `sizeof` if unpacked,
///        or forces heap spillover because `alignof(OverAlignedCallable) > kInlineStorageAlignment`.
struct alignas(64) OverAlignedCallable final
{
    static inline gp::Int32 s_constructCount{ 0 };
    static inline gp::Int32 s_copyCount{ 0 };
    static inline gp::Int32 s_moveCount{ 0 };
    static inline gp::Int32 s_destructCount{ 0 };

    static void resetCounts() noexcept
    {
        s_constructCount = 0;
        s_copyCount = 0;
        s_moveCount = 0;
        s_destructCount = 0;
    }

    gp::Int32 factor{ 1 };

    explicit OverAlignedCallable(const gp::Int32 inFactor) noexcept
        : factor(inFactor)
    {
        ++s_constructCount;
    }

    OverAlignedCallable(const OverAlignedCallable& other) noexcept
        : factor(other.factor)
    {
        ++s_copyCount;
    }

    OverAlignedCallable(OverAlignedCallable&& other) noexcept
        : factor(other.factor)
    {
        other.factor = 0;
        ++s_moveCount;
    }

    ~OverAlignedCallable() noexcept
    {
        ++s_destructCount;
    }

    [[nodiscard]] gp::Int32 operator()(const gp::Int32 input) const noexcept
    {
        // Verify at runtime that the allocated object address satisfies 64-byte alignment.
        const gp::UIntPtr address = reinterpret_cast<gp::UIntPtr>(this);
        if ((address % 64u) != 0u)
        {
            return -999;
        }
        return input * factor;
    }
};

gp::Int32 applyVisitor(const gp::Int32 value, const FunctionRef<gp::Int32(gp::Int32)> visitor)
{
    return visitor(value);
}

}   // namespace

constexpr gp::USize kExpectedFunctionUnpaddedSize = Function<void()>::kInlineStorageSize + (3u * sizeof(void*));
constexpr gp::USize kExpectedFunctionAlign = Function<void()>::kInlineStorageAlignment;
constexpr gp::USize kExpectedFunctionSize =
    ((kExpectedFunctionUnpaddedSize + kExpectedFunctionAlign - 1u) / kExpectedFunctionAlign) * kExpectedFunctionAlign;

static_assert(sizeof(FunctionRef<void()>) == 2u * sizeof(void*), "FunctionRef must be a 2-pointer 16-byte view.");
static_assert(
    sizeof(Function<void()>) == kExpectedFunctionSize,
    "Default 32-byte SBO gp::Function must occupy (32B + 3 pointers) rounded up to alignof(std::max_align_t) " "(56B on Win64, 64B on System V x64/ARM64)."
);
static_assert(
    sizeof(UniqueFunction<void()>) == kExpectedFunctionSize,
    "Default 32-byte SBO gp::UniqueFunction must occupy (32B + 3 pointers) rounded up to alignof(std::max_align_t) " "(56B on Win64, 64B on System V x64/ARM64)."
);
static_assert(!std::is_default_constructible_v<FunctionRef<void()>>, "FunctionRef must not be default-constructible.");
static_assert(
    !std::is_constructible_v<FunctionRef<void()>, gp::NullPtrT>, "FunctionRef must not be constructible from nullptr."
);
static_assert(
    !std::is_copy_constructible_v<UniqueFunction<void()>>,
    "UniqueFunction must be move-only and reject copy construction."
);
static_assert(
    !std::is_copy_assignable_v<UniqueFunction<void()>>, "UniqueFunction must be move-only and reject copy assignment."
);
static_assert(
    std::is_copy_constructible_v<Function<void()>>, "gp::Function must be copy-constructible and move-constructible."
);
static_assert(
    !std::is_constructible_v<Function<void()>, UniqueFunction<void()>>,
    "Copyable gp::Function must not be constructible from move-only gp::UniqueFunction."
);

TEST(FunctionRefTest, InvokesLvalueTemporaryFunctionPointersAndOwningWrappersWithoutCopying)
{
    SmallLifecycleCallable::resetCounts();
    {
        SmallLifecycleCallable callable(50);
        EXPECT_EQ(SmallLifecycleCallable::s_constructCount, 1);
        EXPECT_EQ(SmallLifecycleCallable::s_copyCount, 0);

        const FunctionRef<gp::Int32(gp::Int32)> ref(callable);
        EXPECT_EQ(SmallLifecycleCallable::s_copyCount, 0);
        EXPECT_EQ(SmallLifecycleCallable::s_moveCount, 0);
        EXPECT_EQ(ref(12), 62);
        EXPECT_EQ(ref.execute(8), 58);

        // Copy constructor and copy assignment of FunctionRef view.
        FunctionRef<gp::Int32(gp::Int32)> copiedRef(ref);
        EXPECT_EQ(copiedRef(2), 52);

        SmallLifecycleCallable otherCallable(100);
        const FunctionRef<gp::Int32(gp::Int32)> otherRef(otherCallable);
        copiedRef = otherRef;
        EXPECT_EQ(copiedRef(5), 105);
        EXPECT_EQ(SmallLifecycleCallable::s_copyCount, 0);

        // Passing temporary lambda directly to a FunctionRef parameter.
        const gp::Int32 factor = 4;
        const gp::Int32 result = applyVisitor(
            9,
            [factor](const gp::Int32 v) noexcept -> gp::Int32
        {
            return v * factor;
        }
        );
        EXPECT_EQ(result, 36);

        // Mutable stateful lambda referenced via FunctionRef mutates caller state in place.
        gp::Int32 runningTotal = 10;
        auto statefulLambda = [&runningTotal](const gp::Int32 step) noexcept -> gp::Int32
        {
            runningTotal += step;
            return runningTotal;
        };
        const FunctionRef<gp::Int32(gp::Int32)> mutatingRef(statefulLambda);
        EXPECT_EQ(mutatingRef(5), 15);
        EXPECT_EQ(mutatingRef(7), 22);
        EXPECT_EQ(runningTotal, 22);

        // Function pointer binding into FunctionRef.
        g_functionCallSum = 0;
        const FunctionRef<void(gp::Int32)> fnPtrRef(&addToFunctionGlobal);
        fnPtrRef(15);
        fnPtrRef.execute(5);
        EXPECT_EQ(g_functionCallSum, 20);

        const FunctionRef<gp::Int32(gp::Int32, gp::Int32)> calcRef(&computeWeightedSum);
        EXPECT_EQ(calcRef(3, 7), 37);

        // Binding FunctionRef to bound gp::Function, gp::UniqueFunction, and gp::Delegate.
        const gp::Int32 addend = 11;
        Function<gp::Int32(gp::Int32)> boundFn = [addend](const gp::Int32 v) noexcept -> gp::Int32
        {
            return v + addend;
        };
        UniqueFunction<gp::Int32(gp::Int32)> boundUnique = [addend](const gp::Int32 v) noexcept -> gp::Int32
        {
            return v * addend;
        };
        Delegate<gp::Int32(gp::Int32)> boundDelegate = [addend](const gp::Int32 v) noexcept -> gp::Int32
        {
            return v - addend;
        };

        EXPECT_EQ(applyVisitor(9, boundFn), 20);
        EXPECT_EQ(applyVisitor(3, boundUnique), 33);
        EXPECT_EQ(applyVisitor(25, boundDelegate), 14);
    }
    EXPECT_EQ(SmallLifecycleCallable::s_destructCount, 2);
}

TEST(FunctionTest, UnboundNullptrAndStaticMemberBindings)
{
    Function<void(gp::Int32)> voidFunc;
    EXPECT_FALSE(voidFunc.isBound());
    EXPECT_FALSE(voidFunc.isHeapAllocated());
    EXPECT_FALSE(static_cast<bool>(voidFunc));
    EXPECT_TRUE(voidFunc == nullptr);
    EXPECT_TRUE(nullptr == voidFunc);
    EXPECT_FALSE(voidFunc != nullptr);
    EXPECT_FALSE(voidFunc.executeIfBound(10));

    Function<gp::Int32(gp::Int32, gp::Int32)> valFunc(nullptr);
    EXPECT_FALSE(valFunc.isBound());
    EXPECT_FALSE(valFunc.executeIfBound(1, 2).hasValue());

    // Compile-time static function binding via createStatic and bindStatic.
    g_functionCallSum = 0;
    voidFunc = Function<void(gp::Int32)>::createStatic<&addToFunctionGlobal>();
    ASSERT_TRUE(voidFunc.isBound());
    EXPECT_FALSE(voidFunc.isHeapAllocated());
    EXPECT_TRUE(voidFunc != nullptr);
    voidFunc(6);
    EXPECT_TRUE(voidFunc.executeIfBound(9));
    EXPECT_EQ(g_functionCallSum, 15);

    voidFunc.reset();
    EXPECT_FALSE(voidFunc.isBound());
    voidFunc.bindStatic<&addToFunctionGlobal>();
    ASSERT_TRUE(voidFunc.isBound());
    voidFunc.execute(5);
    EXPECT_EQ(g_functionCallSum, 20);

    // Runtime static function binding via constructor, createStatic, bindStatic, and bindLambda with function pointer.
    Function<gp::Int32(gp::Int32, gp::Int32)> ctorStatic(&computeWeightedSum);
    ASSERT_TRUE(ctorStatic.isBound());
    EXPECT_EQ(ctorStatic(1, 9), 19);

    valFunc = Function<gp::Int32(gp::Int32, gp::Int32)>::createStatic(&computeWeightedSum);
    ASSERT_TRUE(valFunc.isBound());
    EXPECT_EQ(valFunc(4, 5), 45);
    const Optional<gp::Int32> optVal = valFunc.executeIfBound(2, 8);
    ASSERT_TRUE(optVal.hasValue());
    EXPECT_EQ(*optVal, 28);

    valFunc = static_cast<gp::Int32 (*)(gp::Int32, gp::Int32)>(nullptr);
    EXPECT_FALSE(valFunc.isBound());

    valFunc.bindLambda(&computeWeightedSum);
    ASSERT_TRUE(valFunc.isBound());
    EXPECT_EQ(valFunc(3, 4), 34);
    valFunc.bindLambda(static_cast<gp::Int32 (*)(gp::Int32, gp::Int32)>(nullptr));
    EXPECT_FALSE(valFunc.isBound());

    // Raw member function bindings (const and non-const).
    SampleReceiver receiver;
    Function<void(gp::Int32)> memberMutator =
        Function<void(gp::Int32)>::createRaw<&SampleReceiver::accumulate>(&receiver);
    ASSERT_TRUE(memberMutator.isBound());
    EXPECT_FALSE(memberMutator.isHeapAllocated());
    memberMutator(25);
    EXPECT_EQ(receiver.total, 125);

    memberMutator.bindRaw<&SampleReceiver::accumulate>(static_cast<SampleReceiver*>(nullptr));
    EXPECT_FALSE(memberMutator.isBound());
    memberMutator.bindRaw<&SampleReceiver::accumulate>(&receiver);
    ASSERT_TRUE(memberMutator.isBound());
    memberMutator(5);
    EXPECT_EQ(receiver.total, 130);

    const SampleReceiver& constReceiver = receiver;
    Function<gp::Int32(gp::Int32)> memberReader =
        Function<gp::Int32(gp::Int32)>::createRaw<&SampleReceiver::computeWithBonus>(&constReceiver);
    ASSERT_TRUE(memberReader.isBound());
    EXPECT_EQ(memberReader(5), 135);

    memberReader.bindRaw<&SampleReceiver::computeWithBonus>(static_cast<const SampleReceiver*>(nullptr));
    EXPECT_FALSE(memberReader.isBound());
}

TEST(FunctionTest, InlineSBOLifecycleSelfAssignmentAndDelegateInterop)
{
    SmallLifecycleCallable::resetCounts();
    {
        Function<gp::Int32(gp::Int32)> f1 = Function<gp::Int32(gp::Int32)>::createLambda(SmallLifecycleCallable(30));
        ASSERT_TRUE(f1.isBound());
        EXPECT_FALSE(f1.isHeapAllocated());
        EXPECT_EQ(f1(12), 42);

        Function<gp::Int32(gp::Int32)> f2(f1);
        ASSERT_TRUE(f2.isBound());
        EXPECT_FALSE(f2.isHeapAllocated());
        EXPECT_EQ(f2(5), 35);

        Function<gp::Int32(gp::Int32)> f3(std::move(f1));
        EXPECT_FALSE(f1.isBound());
        ASSERT_TRUE(f3.isBound());
        EXPECT_EQ(f3(10), 40);

        f1 = f2;
        ASSERT_TRUE(f1.isBound());
        EXPECT_EQ(f1(1), 31);

        f2 = std::move(f3);
        EXPECT_FALSE(f3.isBound());
        ASSERT_TRUE(f2.isBound());
        EXPECT_EQ(f2(2), 32);

        // Self-copy, self-move, and self-swap preserve invariants.
        const Function<gp::Int32(gp::Int32)>& selfCopyRef = f2;
        f2 = selfCopyRef;
        ASSERT_TRUE(f2.isBound());
        EXPECT_EQ(f2(3), 33);

        Function<gp::Int32(gp::Int32)>& selfMoveRef = f2;
        f2 = std::move(selfMoveRef);
        ASSERT_TRUE(f2.isBound());
        EXPECT_EQ(f2(4), 34);

        f2.swap(f2);
        ASSERT_TRUE(f2.isBound());
        EXPECT_EQ(f2(5), 35);

        // Unbound copy and move construction/assignment.
        const Function<gp::Int32(gp::Int32)> unboundConst{};
        Function<gp::Int32(gp::Int32)> unboundCopied(unboundConst);
        EXPECT_FALSE(unboundCopied.isBound());
        Function<gp::Int32(gp::Int32)> unboundMoved(std::move(unboundCopied));
        EXPECT_FALSE(unboundMoved.isBound());

        f1 = unboundConst;
        EXPECT_FALSE(f1.isBound());
        f2 = std::move(unboundMoved);
        EXPECT_FALSE(f2.isBound());
    }
    EXPECT_EQ(
        SmallLifecycleCallable::s_constructCount + SmallLifecycleCallable::s_copyCount +
            SmallLifecycleCallable::s_moveCount,
        SmallLifecycleCallable::s_destructCount
    );

    // Constructing and assigning gp::Function from lvalue/rvalue gp::Delegate (bound and unbound).
    const gp::Int32 bias = 77;
    Delegate<gp::Int32(gp::Int32)> boundDelegate = [bias](const gp::Int32 x) noexcept -> gp::Int32
    {
        return x + bias;
    };
    Function<gp::Int32(gp::Int32)> fromDelegateLvalue(boundDelegate);
    ASSERT_TRUE(fromDelegateLvalue.isBound());
    EXPECT_EQ(fromDelegateLvalue(23), 100);

    Function<gp::Int32(gp::Int32)> fromDelegateRvalue(std::move(boundDelegate));
    EXPECT_FALSE(boundDelegate.isBound());
    ASSERT_TRUE(fromDelegateRvalue.isBound());
    EXPECT_EQ(fromDelegateRvalue(3), 80);

    Delegate<gp::Int32(gp::Int32)> unboundDelegate{};
    Function<gp::Int32(gp::Int32)> fromUnboundDelegate(unboundDelegate);
    EXPECT_FALSE(fromUnboundDelegate.isBound());

    fromDelegateRvalue = unboundDelegate;
    EXPECT_FALSE(fromDelegateRvalue.isBound());

    Delegate<gp::Int32(gp::Int32)> reassignDelegate = [](const gp::Int32 x) noexcept -> gp::Int32
    {
        return x * 2;
    };
    fromDelegateRvalue = reassignDelegate;
    ASSERT_TRUE(fromDelegateRvalue.isBound());
    EXPECT_EQ(fromDelegateRvalue(21), 42);
}

TEST(FunctionTest, LargeClosureAndOverAlignedHeapSpilloverCopyMoveAndSwap)
{
    static_assert(sizeof(LargeHeapCallable) > Function<gp::Int64(gp::Int64)>::kInlineStorageSize);
    static_assert(alignof(OverAlignedCallable) > Function<gp::Int32(gp::Int32)>::kInlineStorageAlignment);

    LargeHeapCallable::resetCounts();
    {
        Function<gp::Int64(gp::Int64)> largeFn = LargeHeapCallable(10);
        ASSERT_TRUE(largeFn.isBound());
        EXPECT_TRUE(largeFn.isHeapAllocated());
        // weights = [10, 11, 12, 13, 14, 15, 16, 17] -> sum = 108.
        EXPECT_EQ(largeFn(2), 216);

        // Copying a heap-spilled gp::Function deep-clones the heap closure.
        const gp::Int32 copiesBefore = LargeHeapCallable::s_copyCount;
        Function<gp::Int64(gp::Int64)> clonedFn(largeFn);
        EXPECT_EQ(LargeHeapCallable::s_copyCount, copiesBefore + 1);
        ASSERT_TRUE(clonedFn.isBound());
        EXPECT_TRUE(clonedFn.isHeapAllocated());
        EXPECT_EQ(clonedFn(3), 324);

        // Moving a heap-spilled gp::Function transfers the heap pointer in O(1) with zero extra element moves/copies.
        const gp::Int32 movesBefore = LargeHeapCallable::s_moveCount;
        const gp::Int32 copiesBeforeMove = LargeHeapCallable::s_copyCount;
        Function<gp::Int64(gp::Int64)> movedFn(std::move(largeFn));
        EXPECT_EQ(LargeHeapCallable::s_moveCount, movesBefore);
        EXPECT_EQ(LargeHeapCallable::s_copyCount, copiesBeforeMove);
        EXPECT_FALSE(largeFn.isBound());
        EXPECT_FALSE(largeFn.isHeapAllocated());
        ASSERT_TRUE(movedFn.isBound());
        EXPECT_TRUE(movedFn.isHeapAllocated());
        EXPECT_EQ(movedFn(1), 108);

        // Swapping a heap function and an inline function preserves both targets cleanly.
        Function<gp::Int64(gp::Int64)> inlineFn = [](const gp::Int64 v) noexcept -> gp::Int64
        {
            return v * 5;
        };
        EXPECT_FALSE(inlineFn.isHeapAllocated());

        swap(movedFn, inlineFn);
        EXPECT_FALSE(movedFn.isHeapAllocated());
        EXPECT_TRUE(inlineFn.isHeapAllocated());
        EXPECT_EQ(movedFn(9), 45);
        EXPECT_EQ(inlineFn(2), 216);

        // Swapping a heap function with an unbound function transfers ownership cleanly.
        Function<gp::Int64(gp::Int64)> emptyFn{};
        swap(inlineFn, emptyFn);
        EXPECT_FALSE(inlineFn.isBound());
        EXPECT_FALSE(inlineFn.isHeapAllocated());
        ASSERT_TRUE(emptyFn.isBound());
        EXPECT_TRUE(emptyFn.isHeapAllocated());
        EXPECT_EQ(emptyFn(1), 108);
    }
    EXPECT_EQ(
        LargeHeapCallable::s_constructCount + LargeHeapCallable::s_copyCount + LargeHeapCallable::s_moveCount,
        LargeHeapCallable::s_destructCount
    );

    // Over-aligned callable spills to heap with proper 64-byte alignment via HeapAllocator.
    OverAlignedCallable::resetCounts();
    {
        Function<gp::Int32(gp::Int32)> alignedFn(OverAlignedCallable(7));
        ASSERT_TRUE(alignedFn.isBound());
        EXPECT_TRUE(alignedFn.isHeapAllocated());
        EXPECT_EQ(alignedFn(6), 42);

        Function<gp::Int32(gp::Int32)> alignedCopy(alignedFn);
        ASSERT_TRUE(alignedCopy.isBound());
        EXPECT_TRUE(alignedCopy.isHeapAllocated());
        EXPECT_EQ(alignedCopy(5), 35);
    }
    EXPECT_EQ(
        OverAlignedCallable::s_constructCount + OverAlignedCallable::s_copyCount + OverAlignedCallable::s_moveCount,
        OverAlignedCallable::s_destructCount
    );
}

TEST(UniqueFunctionTest, MoveOnlyInlineHeapSpilloverAndZeroAllocationConversionFromFunction)
{
    // 1. Unbound and static/member bindings on UniqueFunction.
    UniqueFunction<void(gp::Int32)> voidUnique{};
    EXPECT_FALSE(voidUnique.isBound());
    EXPECT_FALSE(static_cast<bool>(voidUnique));
    EXPECT_FALSE(voidUnique.executeIfBound(1));

    g_functionCallSum = 0;
    voidUnique = UniqueFunction<void(gp::Int32)>::createStatic<&addToFunctionGlobal>();
    ASSERT_TRUE(voidUnique.isBound());
    EXPECT_TRUE(voidUnique.executeIfBound(12));
    EXPECT_EQ(g_functionCallSum, 12);

    SampleReceiver receiver;
    UniqueFunction<gp::Int32(gp::Int32)> memberUnique =
        UniqueFunction<gp::Int32(gp::Int32)>::createRaw<&SampleReceiver::computeWithBonus>(&receiver);
    ASSERT_TRUE(memberUnique.isBound());
    const Optional<gp::Int32> bonusOpt = memberUnique.executeIfBound(23);
    ASSERT_TRUE(bonusOpt.hasValue());
    EXPECT_EQ(*bonusOpt, 123);

    // 2. Inline SBO move-only lambda capturing gp::UniquePtr<gp::Int32>.
    UniquePtr<gp::Int32> uniquePayload(new gp::Int32(40));
    UniqueFunction<gp::Int32(gp::Int32)> uniqueInline =
        [ptr = std::move(uniquePayload)](const gp::Int32 addend) noexcept -> gp::Int32
    {
        return *ptr + addend;
    };
    EXPECT_FALSE(uniquePayload.isValid());
    ASSERT_TRUE(uniqueInline.isBound());
    EXPECT_FALSE(uniqueInline.isHeapAllocated());
    EXPECT_EQ(uniqueInline(2), 42);

    UniqueFunction<gp::Int32(gp::Int32)> movedInline(std::move(uniqueInline));
    EXPECT_FALSE(uniqueInline.isBound());
    ASSERT_TRUE(movedInline.isBound());
    EXPECT_EQ(movedInline(10), 50);

    // 3. Heap-spilled move-only callable exceeding 32-byte inline capacity.
    static_assert(sizeof(LargeMoveOnlyCallable) > UniqueFunction<gp::Int32(gp::Int32)>::kInlineStorageSize);
    LargeMoveOnlyCallable::resetCounts();
    {
        UniqueFunction<gp::Int32(gp::Int32)> largeUnique(LargeMoveOnlyCallable(200));
        ASSERT_TRUE(largeUnique.isBound());
        EXPECT_TRUE(largeUnique.isHeapAllocated());
        EXPECT_EQ(largeUnique(10), 211);

        const gp::Int32 movesBefore = LargeMoveOnlyCallable::s_moveCount;
        UniqueFunction<gp::Int32(gp::Int32)> transferredUnique(std::move(largeUnique));
        // Heap move transfers pointer in O(1) without invoking element move constructor.
        EXPECT_EQ(LargeMoveOnlyCallable::s_moveCount, movesBefore);
        EXPECT_FALSE(largeUnique.isBound());
        ASSERT_TRUE(transferredUnique.isBound());
        EXPECT_TRUE(transferredUnique.isHeapAllocated());
        EXPECT_EQ(transferredUnique(20), 221);

        swap(transferredUnique, movedInline);
        EXPECT_FALSE(transferredUnique.isHeapAllocated());
        EXPECT_TRUE(movedInline.isHeapAllocated());
        EXPECT_EQ(transferredUnique(5), 45);
        EXPECT_EQ(movedInline(30), 231);

        transferredUnique = nullptr;
        movedInline.reset();
        EXPECT_FALSE(transferredUnique.isBound());
        EXPECT_FALSE(movedInline.isBound());
    }
    EXPECT_EQ(
        LargeMoveOnlyCallable::s_constructCount + LargeMoveOnlyCallable::s_moveCount,
        LargeMoveOnlyCallable::s_destructCount
    );

    // 4. Converting an inline copyable gp::Function rvalue into a gp::UniqueFunction MUST stay inline (zero heap
    // spillover!) and leave the source gp::Function unbound.
    SmallLifecycleCallable::resetCounts();
    {
        Function<gp::Int32(gp::Int32)> inlineCopyable(SmallLifecycleCallable(100));
        ASSERT_TRUE(inlineCopyable.isBound());
        EXPECT_FALSE(inlineCopyable.isHeapAllocated());

        UniqueFunction<gp::Int32(gp::Int32)> adoptedUnique(std::move(inlineCopyable));
        EXPECT_FALSE(inlineCopyable.isBound());
        ASSERT_TRUE(adoptedUnique.isBound());
        // Crucial AAA invariant: moving a 32B inline Function into a 32B UniqueFunction steals the SBO buffer directly
        // rather than wrapping a 56B Function on the heap!
        EXPECT_FALSE(adoptedUnique.isHeapAllocated());
        EXPECT_EQ(SmallLifecycleCallable::s_copyCount, 0);
        EXPECT_EQ(adoptedUnique(23), 123);

        // Also test move-assignment of a heap-spilled gp::Function into UniqueFunction: O(1) pointer steal.
        LargeHeapCallable::resetCounts();
        Function<gp::Int64(gp::Int64)> heapCopyable(LargeHeapCallable(5));
        ASSERT_TRUE(heapCopyable.isHeapAllocated());
        const gp::Int32 heapMovesBefore = LargeHeapCallable::s_moveCount;

        UniqueFunction<gp::Int64(gp::Int64)> heapAdopted{};
        heapAdopted = std::move(heapCopyable);
        EXPECT_FALSE(heapCopyable.isBound());
        ASSERT_TRUE(heapAdopted.isBound());
        EXPECT_TRUE(heapAdopted.isHeapAllocated());
        EXPECT_EQ(LargeHeapCallable::s_moveCount, heapMovesBefore);
        EXPECT_EQ(LargeHeapCallable::s_copyCount, 0);
        // weights = [5..12] -> sum = 68.
        EXPECT_EQ(heapAdopted(2), 136);
    }
    EXPECT_EQ(
        SmallLifecycleCallable::s_constructCount + SmallLifecycleCallable::s_copyCount +
            SmallLifecycleCallable::s_moveCount,
        SmallLifecycleCallable::s_destructCount
    );
    EXPECT_EQ(
        LargeHeapCallable::s_constructCount + LargeHeapCallable::s_copyCount + LargeHeapCallable::s_moveCount,
        LargeHeapCallable::s_destructCount
    );
}

TEST(FunctionTest, CustomInlineCapacityAndReferenceParameters)
{
    // A 64-byte inline capacity stores LargeHeapCallable inline without touching the heap.
    LargeHeapCallable::resetCounts();
    {
        Function<gp::Int64(gp::Int64), 64u> customInlineFn(LargeHeapCallable(1));
        ASSERT_TRUE(customInlineFn.isBound());
        EXPECT_FALSE(customInlineFn.isHeapAllocated());
        // weights = [1..8] -> sum = 36.
        EXPECT_EQ(customInlineFn(2), 72);
    }
    EXPECT_EQ(
        LargeHeapCallable::s_constructCount + LargeHeapCallable::s_copyCount + LargeHeapCallable::s_moveCount,
        LargeHeapCallable::s_destructCount
    );

    // Reference and move-only value parameter forwarding without decay.
    UniqueFunction<void(gp::Int32&, UniquePtr<gp::Int32>)> refAndMoveMutator =
        [](gp::Int32& target, UniquePtr<gp::Int32> bonusPtr) noexcept -> void
    {
        if (bonusPtr.isValid())
        {
            target += *bonusPtr;
        }
    };
    gp::Int32 counter = 23;
    refAndMoveMutator(counter, UniquePtr<gp::Int32>(new gp::Int32(19)));
    EXPECT_EQ(counter, 42);
}

}   // namespace gp::tests
