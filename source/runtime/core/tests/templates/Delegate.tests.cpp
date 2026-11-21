// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "templates/Delegate.hpp"
#include "CoreMinimal.hpp"
#include "templates/Optional.hpp"
#include <gtest/gtest.h>
#include <utility>

namespace gp::tests
{

namespace
{

gp::Int32 g_freeCallSum = 0;

void addToGlobal(const gp::Int32 delta) noexcept
{
    g_freeCallSum += delta;
}

gp::Int32 multiplyValues(const gp::Int32 lhs, const gp::Int32 rhs)
{
    return lhs * rhs;
}

struct TargetObject final
{
    gp::Int32 accumulator{ 10 };

    void add(const gp::Int32 value) noexcept
    {
        accumulator += value;
    }

    [[nodiscard]] gp::Int32 readWithOffset(const gp::Int32 offset) const noexcept
    {
        return accumulator + offset;
    }
};

struct NonTrivialCallable final
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

    gp::Int32 bias{ 0 };

    explicit NonTrivialCallable(const gp::Int32 inBias) noexcept
        : bias(inBias)
    {
        ++s_constructCount;
    }

    NonTrivialCallable(const NonTrivialCallable& other) noexcept
        : bias(other.bias)
    {
        ++s_copyCount;
    }

    NonTrivialCallable(NonTrivialCallable&& other) noexcept
        : bias(other.bias)
    {
        other.bias = 0;
        ++s_moveCount;
    }

    ~NonTrivialCallable() noexcept
    {
        ++s_destructCount;
    }

    [[nodiscard]] gp::Int32 operator()(const gp::Int32 input) const noexcept
    {
        return bias + input;
    }
};

}   // namespace

TEST(DelegateHandleTest, DefaultAndExplicitStates)
{
    DelegateHandle invalidHandle;
    EXPECT_FALSE(invalidHandle.isValid());
    EXPECT_FALSE(static_cast<bool>(invalidHandle));
    EXPECT_EQ(invalidHandle.rawValue(), 0ULL);

    DelegateHandle validHandle(42ULL);
    EXPECT_TRUE(validHandle.isValid());
    EXPECT_TRUE(static_cast<bool>(validHandle));
    EXPECT_EQ(validHandle.rawValue(), 42ULL);
    EXPECT_NE(invalidHandle, validHandle);

    DelegateHandle sameHandle(42ULL);
    EXPECT_EQ(validHandle, sameHandle);

    validHandle.reset();
    EXPECT_FALSE(validHandle.isValid());
    EXPECT_EQ(validHandle, invalidHandle);
}

TEST(DelegateTest, UnboundBehaviorAndExecuteIfBound)
{
    Delegate<void(gp::Int32)> voidDelegate;
    EXPECT_FALSE(voidDelegate.isBound());
    EXPECT_FALSE(static_cast<bool>(voidDelegate));
    EXPECT_FALSE(voidDelegate.executeIfBound(5));

    Delegate<gp::Int32(gp::Int32, gp::Int32)> valueDelegate(nullptr);
    EXPECT_FALSE(valueDelegate.isBound());
    const Optional<gp::Int32> emptyResult = valueDelegate.executeIfBound(2, 3);
    EXPECT_FALSE(emptyResult.hasValue());
}

TEST(DelegateTest, StaticCompileTimeAndRuntimeBindings)
{
    g_freeCallSum = 0;

    Delegate<void(gp::Int32)> compileTimeStatic = Delegate<void(gp::Int32)>::createStatic<&addToGlobal>();
    ASSERT_TRUE(compileTimeStatic.isBound());
    compileTimeStatic.execute(7);
    compileTimeStatic(3);
    EXPECT_EQ(g_freeCallSum, 10);
    EXPECT_TRUE(compileTimeStatic.executeIfBound(5));
    EXPECT_EQ(g_freeCallSum, 15);

    Delegate<gp::Int32(gp::Int32, gp::Int32)> runtimeStatic(&multiplyValues);
    ASSERT_TRUE(runtimeStatic.isBound());
    EXPECT_EQ(runtimeStatic.execute(6, 7), 42);
    const Optional<gp::Int32> maybeProduct = runtimeStatic.executeIfBound(3, 9);
    ASSERT_TRUE(maybeProduct.hasValue());
    EXPECT_EQ(*maybeProduct, 27);

    runtimeStatic.bindStatic(nullptr);
    EXPECT_FALSE(runtimeStatic.isBound());

    runtimeStatic = Delegate<gp::Int32(gp::Int32, gp::Int32)>::createStatic(&multiplyValues);
    ASSERT_TRUE(runtimeStatic.isBound());
    runtimeStatic = nullptr;
    EXPECT_FALSE(runtimeStatic.isBound());
}

TEST(DelegateTest, MemberRawBindingsConstAndNonConst)
{
    TargetObject target;

    Delegate<void(gp::Int32)> mutator = Delegate<void(gp::Int32)>::createRaw<&TargetObject::add>(&target);
    ASSERT_TRUE(mutator.isBound());
    mutator(15);
    EXPECT_EQ(target.accumulator, 25);

    const TargetObject& constRef = target;
    Delegate<gp::Int32(gp::Int32)> reader;
    reader.bindRaw<&TargetObject::readWithOffset>(&constRef);
    ASSERT_TRUE(reader.isBound());
    EXPECT_EQ(reader.execute(5), 30);

    reader.bindRaw<&TargetObject::readWithOffset>(static_cast<const TargetObject*>(nullptr));
    EXPECT_FALSE(reader.isBound());
}

TEST(DelegateTest, LambdaAndNonTrivialLifecycleTracking)
{
    NonTrivialCallable::resetCounts();
    {
        Delegate<gp::Int32(gp::Int32)> first = Delegate<gp::Int32(gp::Int32)>::createLambda(NonTrivialCallable(100));
        ASSERT_TRUE(first.isBound());
        EXPECT_EQ(first(23), 123);

        Delegate<gp::Int32(gp::Int32)> copied(first);
        EXPECT_EQ(copied(7), 107);

        Delegate<gp::Int32(gp::Int32)> moved(std::move(first));
        EXPECT_FALSE(first.isBound());
        ASSERT_TRUE(moved.isBound());
        EXPECT_EQ(moved(11), 111);

        first = copied;
        ASSERT_TRUE(first.isBound());
        EXPECT_EQ(first(1), 101);

        copied = std::move(moved);
        EXPECT_FALSE(moved.isBound());
        ASSERT_TRUE(copied.isBound());
        EXPECT_EQ(copied(2), 102);

        // Self-copy, self-move, and unbound copy assignment preserve clean invariants.
        const Delegate<gp::Int32(gp::Int32)>& selfRef = copied;
        copied = selfRef;
        ASSERT_TRUE(copied.isBound());
        EXPECT_EQ(copied(3), 103);

        Delegate<gp::Int32(gp::Int32)>& selfMoveRef = copied;
        copied = std::move(selfMoveRef);
        ASSERT_TRUE(copied.isBound());
        EXPECT_EQ(copied(4), 104);

        const Delegate<gp::Int32(gp::Int32)> unboundSource{};
        Delegate<gp::Int32(gp::Int32)> unboundCopy(unboundSource);
        EXPECT_FALSE(unboundCopy.isBound());
        copied = unboundSource;
        EXPECT_FALSE(copied.isBound());
    }

    EXPECT_EQ(
        NonTrivialCallable::s_constructCount + NonTrivialCallable::s_copyCount + NonTrivialCallable::s_moveCount,
        NonTrivialCallable::s_destructCount
    );
}

TEST(DelegateTest, CapturingLambdaMutation)
{
    gp::Int32 hitCount = 0;
    gp::Int32 lastValue = 0;

    Delegate<void(gp::Int32)> callback = [&hitCount, &lastValue](const gp::Int32 v) noexcept -> void
    {
        ++hitCount;
        lastValue = v;
    };

    ASSERT_TRUE(callback.isBound());
    callback.execute(99);
    EXPECT_EQ(hitCount, 1);
    EXPECT_EQ(lastValue, 99);

    callback.unbind();
    EXPECT_FALSE(callback.isBound());
    // Re-calling unbind on an already unbound delegate is a safe no-op.
    callback.unbind();
    EXPECT_FALSE(callback.isBound());

    // Capturing value-returning lambda validates inline storage with execute() and executeIfBound().
    const gp::Int32 offset = 15;
    Delegate<gp::Int32(gp::Int32)> adder = [offset](const gp::Int32 input) noexcept -> gp::Int32
    {
        return input + offset;
    };
    ASSERT_TRUE(adder.isBound());
    EXPECT_EQ(adder.execute(27), 42);
    const Optional<gp::Int32> sumOpt = adder.executeIfBound(5);
    ASSERT_TRUE(sumOpt.hasValue());
    EXPECT_EQ(*sumOpt, 20);
}

TEST(FixedMulticastDelegateTest, AddBroadcastRemoveClearAndReentrantRemoval)
{
    FixedMulticastDelegate<4u, gp::Int32> multicast;
    EXPECT_EQ(multicast.getCapacity(), 4u);
    EXPECT_EQ(multicast.getBoundCount(), 0u);
    EXPECT_FALSE(multicast.isBound());

    // Adding an unbound delegate returns an invalid handle.
    const DelegateHandle invalidSub = multicast.add(Delegate<void(gp::Int32)>{});
    EXPECT_FALSE(invalidSub.isValid());
    EXPECT_EQ(multicast.getBoundCount(), 0u);

    gp::Int32 sumA = 0;
    gp::Int32 sumB = 0;
    gp::Int32 sumC = 0;

    const DelegateHandle handleA = multicast.add(
        [&sumA](const gp::Int32 v) noexcept -> void
    {
        sumA += v;
    }
    );
    DelegateHandle handleB{};
    handleB = multicast.add(
        [&multicast, &handleB, &sumB](const gp::Int32 v) noexcept -> void
    {
        sumB += v;
        // Re-entrantly remove self during broadcast.
        multicast.remove(handleB);
    }
    );
    const DelegateHandle handleC = multicast.add(
        [&sumC](const gp::Int32 v) noexcept -> void
    {
        sumC += v;
    }
    );

    EXPECT_TRUE(handleA.isValid());
    EXPECT_TRUE(handleB.isValid());
    EXPECT_TRUE(handleC.isValid());
    EXPECT_EQ(multicast.getBoundCount(), 3u);
    EXPECT_TRUE(multicast.isBound());

    multicast.broadcast(10);
    EXPECT_EQ(sumA, 10);
    EXPECT_EQ(sumB, 10);
    EXPECT_EQ(sumC, 10);
    EXPECT_EQ(multicast.getBoundCount(), 2u);

    // Re-removing stale handle fails safely due to generation mismatch.
    EXPECT_FALSE(multicast.remove(handleB));

    multicast.broadcast(5);
    EXPECT_EQ(sumA, 15);
    EXPECT_EQ(sumB, 10);
    EXPECT_EQ(sumC, 15);

    // Fill to capacity and verify overflow returns invalid handle.
    const DelegateHandle handleD = multicast.add(
        [](gp::Int32) noexcept -> void
    {
    }
    );
    const DelegateHandle handleE = multicast.add(
        [](gp::Int32) noexcept -> void
    {
    }
    );
    const DelegateHandle handleOverflow = multicast.add(
        [](gp::Int32) noexcept -> void
    {
    }
    );
    EXPECT_TRUE(handleD.isValid());
    EXPECT_TRUE(handleE.isValid());
    EXPECT_FALSE(handleOverflow.isValid());
    EXPECT_EQ(multicast.getBoundCount(), 4u);

    multicast.clear();
    EXPECT_EQ(multicast.getBoundCount(), 0u);
    EXPECT_FALSE(multicast.isBound());
    EXPECT_FALSE(multicast.remove(handleA));
    EXPECT_FALSE(multicast.remove(DelegateHandle{}));
    EXPECT_FALSE(multicast.remove(DelegateHandle(999ULL)));

    // Empty broadcast is a safe no-op.
    multicast.broadcast(123);

    // Downstream re-entrant removal: listener 1 unsubscribes listener 2 mid-broadcast so listener 2 skips.
    gp::Int32 firstHits = 0;
    gp::Int32 secondHits = 0;
    DelegateHandle secondHandle{};
    const DelegateHandle firstHandle = multicast.add(
        [&multicast, &firstHits, &secondHandle](const gp::Int32 v) noexcept -> void
    {
        firstHits += v;
        EXPECT_TRUE(multicast.remove(secondHandle));
    }
    );
    secondHandle = multicast.add(
        [&secondHits](const gp::Int32 v) noexcept -> void
    {
        secondHits += v;
    }
    );
    EXPECT_TRUE(firstHandle.isValid());
    EXPECT_TRUE(secondHandle.isValid());
    multicast.broadcast(4);
    EXPECT_EQ(firstHits, 4);
    EXPECT_EQ(secondHits, 0);

    // Reference-parameter multicast forwards lvalue mutations in place across subscribers.
    FixedMulticastDelegate<2u, gp::Int32&> refMulticast;
    EXPECT_TRUE(refMulticast
                    .add(
                        [](gp::Int32& value) noexcept -> void
    {
        value += 3;
    }
                    )
                    .isValid());
    EXPECT_TRUE(refMulticast
                    .add(
                        [](gp::Int32& value) noexcept -> void
    {
        value *= 2;
    }
                    )
                    .isValid());
    gp::Int32 accum = 5;
    refMulticast.broadcast(accum);
    EXPECT_EQ(accum, 16);
}

TEST(DelegateTest, TrivialCopyMoveConstInvokeReferenceArgsAndHandleReset)
{
    g_freeCallSum = 0;

    // Static function delegate exercises trivial (m_ops == nullptr) copy/move ctor & assignment.
    Delegate<void(gp::Int32)> srcStatic = Delegate<void(gp::Int32)>::createStatic<&addToGlobal>();
    Delegate<void(gp::Int32)> copyStatic(srcStatic);
    Delegate<void(gp::Int32)> moveStatic(std::move(copyStatic));
    EXPECT_FALSE(copyStatic.isBound());
    EXPECT_TRUE(moveStatic.isBound());

    Delegate<void(gp::Int32)> copyAssignStatic;
    copyAssignStatic = srcStatic;
    Delegate<void(gp::Int32)> moveAssignStatic;
    moveAssignStatic = std::move(moveStatic);
    EXPECT_FALSE(moveStatic.isBound());

    const Delegate<void(gp::Int32)>& constView = moveAssignStatic;
    constView(5);
    constView.executeIfBound(7);
    copyAssignStatic.execute(3);
    EXPECT_EQ(g_freeCallSum, 15);

    // Raw const member + trivially-copyable stateless lambda copy/move & executeIfBound on const Delegate.
    const TargetObject target{ 20 };
    Delegate<gp::Int32(gp::Int32)> constMember =
        Delegate<gp::Int32(gp::Int32)>::createRaw<&TargetObject::readWithOffset>(&target);
    const Delegate<gp::Int32(gp::Int32)> constMemberCopy(constMember);
    const Optional<gp::Int32> memberOpt = constMemberCopy.executeIfBound(4);
    ASSERT_TRUE(memberOpt.hasValue());
    EXPECT_EQ(*memberOpt, 24);
    EXPECT_EQ(constMemberCopy(1), 21);

    Delegate<gp::Int32(gp::Int32)> statelessLambda = [](const gp::Int32 x) noexcept -> gp::Int32
    {
        return x * 3;
    };
    Delegate<gp::Int32(gp::Int32)> statelessMoved;
    statelessMoved = std::move(statelessLambda);
    EXPECT_FALSE(statelessLambda.isBound());
    EXPECT_EQ(statelessMoved(6), 18);

    // Reference-parameter single-cast delegate mutates caller argument without decaying by value.
    Delegate<void(gp::Int32&)> refMutator = [](gp::Int32& value) noexcept -> void
    {
        value += 9;
    };
    gp::Int32 mutatedValue = 11;
    refMutator(mutatedValue);
    EXPECT_EQ(mutatedValue, 20);

    // DelegateHandle reset, explicit operator bool, and equality semantics.
    DelegateHandle handleA(42ULL);
    DelegateHandle handleB(42ULL);
    DelegateHandle handleC(99ULL);
    EXPECT_TRUE(static_cast<bool>(handleA));
    EXPECT_TRUE(handleA == handleB);
    EXPECT_TRUE(handleA != handleC);
    handleA.reset();
    EXPECT_FALSE(handleA.isValid());
    EXPECT_FALSE(static_cast<bool>(handleA));
    EXPECT_EQ(handleA.rawValue(), 0ULL);
}

}   // namespace gp::tests
