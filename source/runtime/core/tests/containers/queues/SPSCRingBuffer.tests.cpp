// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "containers/queues/SPSCRingBuffer.hpp"
#include <gtest/gtest.h>
#include <numeric>   // IWYU pragma: keep
#include <string>
#include <thread>
#include <vector>

namespace gp::tests
{

namespace
{

struct LifecycleTracker
{
    static inline int s_constructCount{ 0 };
    static inline int s_copyCount{ 0 };
    static inline int s_moveCount{ 0 };
    static inline int s_destructCount{ 0 };

    static void reset()
    {
        s_constructCount = 0;
        s_copyCount = 0;
        s_moveCount = 0;
        s_destructCount = 0;
    }

    int id{ 0 };

    LifecycleTracker() noexcept
        : id(0)
    {
        ++s_constructCount;
    }

    explicit LifecycleTracker(int inId) noexcept
        : id(inId)
    {
        ++s_constructCount;
    }

    LifecycleTracker(const LifecycleTracker& other) noexcept
        : id(other.id)
    {
        ++s_copyCount;
    }

    LifecycleTracker(LifecycleTracker&& other) noexcept
        : id(other.id)
    {
        ++s_moveCount;
    }

    LifecycleTracker& operator=(const LifecycleTracker& other) noexcept
    {
        id = other.id;
        ++s_copyCount;
        return *this;
    }

    LifecycleTracker& operator=(LifecycleTracker&& other) noexcept
    {
        id = other.id;
        ++s_moveCount;
        return *this;
    }

    ~LifecycleTracker() noexcept
    {
        ++s_destructCount;
    }
};

struct TestPacketHeader
{
    gp::UInt16 packetId{ 0u };
    gp::UInt16 payloadSize{ 0u };
    gp::UInt32 sequenceNumber{ 0u };
};

}   // namespace

TEST(SPSCRingBufferInitializationTest, DefaultUninitializedState)
{
    SPSCRingBuffer<int> queue;

    EXPECT_FALSE(queue.isValid());
    EXPECT_EQ(queue.capacity(), 0u);
    EXPECT_EQ(queue.sizeApprox(), 0u);
    EXPECT_TRUE(queue.isEmptyApprox());
    EXPECT_FALSE(queue.canWrite(1u));
    EXPECT_FALSE(queue.canRead(1u));
    EXPECT_EQ(queue.peek(), nullptr);

    int poppedValue = 0;
    EXPECT_FALSE(queue.tryPop(poppedValue));
    EXPECT_FALSE(queue.tryPush(42));
}

TEST(SPSCRingBufferInitializationTest, CapacityRoundingAndPowerOfTwo)
{
    SPSCRingBuffer<int> queue;

    EXPECT_TRUE(queue.initialize(5u));
    EXPECT_TRUE(queue.isValid());
    EXPECT_EQ(queue.capacity(), 8u);

    EXPECT_TRUE(queue.initialize(1u));
    EXPECT_EQ(queue.capacity(), 2u);

    EXPECT_TRUE(queue.initialize(0u));
    EXPECT_EQ(queue.capacity(), 2u);

    EXPECT_TRUE(queue.initialize(16u));
    EXPECT_EQ(queue.capacity(), 16u);
}

TEST(SPSCRingBufferInitializationTest, ExternalBufferAttachment)
{
    alignas(alignof(int)) int buffer[8];
    SPSCRingBuffer<int> queue;

    EXPECT_FALSE(queue.initializeWithExternalBuffer(nullptr, 8u));
    EXPECT_FALSE(queue.initializeWithExternalBuffer(buffer, 7u));
    EXPECT_FALSE(queue.initializeWithExternalBuffer(buffer, 1u));

    EXPECT_TRUE(queue.initializeWithExternalBuffer(buffer, 8u));
    EXPECT_TRUE(queue.isValid());
    EXPECT_EQ(queue.capacity(), 8u);

    for (int i = 0; i < 8; ++i)
    {
        EXPECT_TRUE(queue.tryPush(i * 10));
    }
    EXPECT_FALSE(queue.tryPush(999));

    for (int i = 0; i < 8; ++i)
    {
        int val = 0;
        EXPECT_TRUE(queue.tryPop(val));
        EXPECT_EQ(val, i * 10);
    }
    EXPECT_FALSE(queue.tryPop(buffer[0]));
}

TEST(SPSCRingBufferInitializationTest, ShutdownAndReinitialize)
{
    SPSCRingBuffer<int> queue(4u);
    EXPECT_TRUE(queue.isValid());
    EXPECT_EQ(queue.capacity(), 4u);

    EXPECT_TRUE(queue.tryPush(1));
    EXPECT_TRUE(queue.tryPush(2));
    EXPECT_EQ(queue.sizeApprox(), 2u);

    queue.shutdown();
    EXPECT_FALSE(queue.isValid());
    EXPECT_EQ(queue.capacity(), 0u);
    EXPECT_EQ(queue.sizeApprox(), 0u);

    EXPECT_TRUE(queue.initialize(8u));
    EXPECT_TRUE(queue.isValid());
    EXPECT_EQ(queue.capacity(), 8u);
    EXPECT_EQ(queue.sizeApprox(), 0u);
}

TEST(SPSCRingBufferMoveTest, MoveConstructorAndAssignment)
{
    SPSCRingBuffer<int> queueA(8u);
    EXPECT_TRUE(queueA.tryPush(10));
    EXPECT_TRUE(queueA.tryPush(20));
    EXPECT_TRUE(queueA.tryPush(30));

    SPSCRingBuffer<int> queueB(std::move(queueA));
    EXPECT_FALSE(queueA.isValid());
    EXPECT_EQ(queueA.capacity(), 0u);

    EXPECT_TRUE(queueB.isValid());
    EXPECT_EQ(queueB.capacity(), 8u);
    EXPECT_EQ(queueB.sizeApprox(), 3u);

    SPSCRingBuffer<int> queueC;
    queueC = std::move(queueB);
    EXPECT_FALSE(queueB.isValid());
    EXPECT_TRUE(queueC.isValid());
    EXPECT_EQ(queueC.capacity(), 8u);
    EXPECT_EQ(queueC.sizeApprox(), 3u);

    int val = 0;
    EXPECT_TRUE(queueC.tryPop(val));
    EXPECT_EQ(val, 10);
    EXPECT_TRUE(queueC.tryPop(val));
    EXPECT_EQ(val, 20);
    EXPECT_TRUE(queueC.tryPop(val));
    EXPECT_EQ(val, 30);
    EXPECT_FALSE(queueC.tryPop(val));
}

TEST(SPSCRingBufferBasicOperationsTest, PushPopAndFullEmptyInvariants)
{
    SPSCRingBuffer<int> queue(4u);
    EXPECT_EQ(queue.capacity(), 4u);

    EXPECT_TRUE(queue.isEmptyApprox());
    EXPECT_TRUE(queue.canWrite(4u));
    EXPECT_FALSE(queue.canWrite(5u));
    EXPECT_FALSE(queue.canRead(1u));

    EXPECT_TRUE(queue.tryPush(1));
    EXPECT_TRUE(queue.tryPush(2));
    EXPECT_TRUE(queue.tryPush(3));
    EXPECT_TRUE(queue.tryPush(4));

    EXPECT_FALSE(queue.canWrite(1u));
    EXPECT_FALSE(queue.tryPush(5));
    EXPECT_FALSE(queue.tryEmplace(6));
    EXPECT_EQ(queue.sizeApprox(), 4u);

    int popped = 0;
    EXPECT_TRUE(queue.tryPop(popped));
    EXPECT_EQ(popped, 1);
    EXPECT_TRUE(queue.canWrite(1u));

    EXPECT_TRUE(queue.tryPush(5));
    EXPECT_FALSE(queue.canWrite(1u));

    for (int expected: { 2, 3, 4, 5 })
    {
        EXPECT_TRUE(queue.tryPop(popped));
        EXPECT_EQ(popped, expected);
    }

    EXPECT_TRUE(queue.isEmptyApprox());
    EXPECT_FALSE(queue.tryPop(popped));
}

TEST(SPSCRingBufferBasicOperationsTest, PeekAndInPlaceMutation)
{
    struct MutableItem
    {
        int id{ 0 };
        int value{ 0 };
    };

    SPSCRingBuffer<MutableItem> queue(4u);
    EXPECT_EQ(queue.peek(), nullptr);

    EXPECT_TRUE(queue.tryPush(MutableItem{ 1, 100 }));
    EXPECT_TRUE(queue.tryPush(MutableItem{ 2, 200 }));

    MutableItem* front = queue.peek();
    ASSERT_NE(front, nullptr);
    EXPECT_EQ(front->id, 1);
    EXPECT_EQ(front->value, 100);

    front->value = 999;

    MutableItem popped{};
    EXPECT_TRUE(queue.tryPop(popped));
    EXPECT_EQ(popped.id, 1);
    EXPECT_EQ(popped.value, 999);

    front = queue.peek();
    ASSERT_NE(front, nullptr);
    EXPECT_EQ(front->id, 2);
    EXPECT_EQ(front->value, 200);
}

TEST(SPSCRingBufferBasicOperationsTest, CircularWrapAroundSequence)
{
    SPSCRingBuffer<int> queue(4u);

    for (int cycle = 0; cycle < 100; ++cycle)
    {
        for (int i = 0; i < 3; ++i)
        {
            const int item = cycle * 10 + i;
            EXPECT_TRUE(queue.tryPush(item));
        }

        for (int i = 0; i < 3; ++i)
        {
            int outVal = 0;
            EXPECT_TRUE(queue.tryPop(outVal));
            EXPECT_EQ(outVal, cycle * 10 + i);
        }
    }

    EXPECT_TRUE(queue.isEmptyApprox());
}

TEST(SPSCRingBufferLifecycleTest, ConstructionAndDestructionCount)
{
    LifecycleTracker::reset();

    {
        SPSCRingBuffer<LifecycleTracker> queue(4u);

        EXPECT_TRUE(queue.tryEmplace(10));
        EXPECT_TRUE(queue.tryEmplace(20));
        EXPECT_TRUE(queue.tryEmplace(30));

        EXPECT_EQ(LifecycleTracker::s_constructCount, 3);
        EXPECT_EQ(LifecycleTracker::s_destructCount, 0);

        LifecycleTracker item;
        EXPECT_TRUE(queue.tryPop(item));
        EXPECT_EQ(item.id, 10);

        EXPECT_EQ(LifecycleTracker::s_destructCount, 1);

        queue.clear();
        EXPECT_EQ(LifecycleTracker::s_destructCount, 3);
        EXPECT_TRUE(queue.isEmptyApprox());
    }

    EXPECT_EQ(LifecycleTracker::s_destructCount, 4);
}

TEST(SPSCRingBufferLifecycleTest, AdvanceConsumerDestroysElements)
{
    LifecycleTracker::reset();

    SPSCRingBuffer<LifecycleTracker> queue(4u);
    EXPECT_TRUE(queue.tryEmplace(1));
    EXPECT_TRUE(queue.tryEmplace(2));
    EXPECT_TRUE(queue.tryEmplace(3));

    EXPECT_EQ(LifecycleTracker::s_constructCount, 3);
    EXPECT_EQ(LifecycleTracker::s_destructCount, 0);

    queue.advanceConsumer(2u);
    EXPECT_EQ(LifecycleTracker::s_destructCount, 2);
    EXPECT_EQ(queue.sizeApprox(), 1u);

    LifecycleTracker remaining;
    EXPECT_TRUE(queue.tryPop(remaining));
    EXPECT_EQ(remaining.id, 3);
    EXPECT_EQ(queue.sizeApprox(), 0u);
}

TEST(SPSCRingBufferRangeTest, PushRangeAndPopRangeWithBoundaryWrap)
{
    SPSCRingBuffer<int> queue(8u);

    EXPECT_TRUE(queue.tryPushRange(nullptr, 0u));
    EXPECT_TRUE(queue.tryPopRange(nullptr, 0u));

    int inputData[6] = { 10, 20, 30, 40, 50, 60 };
    EXPECT_TRUE(queue.tryPushRange(inputData, 6u));
    EXPECT_EQ(queue.sizeApprox(), 6u);

    int outputData[6] = { 0 };
    EXPECT_TRUE(queue.tryPopRange(outputData, 4u));
    EXPECT_EQ(outputData[0], 10);
    EXPECT_EQ(outputData[1], 20);
    EXPECT_EQ(outputData[2], 30);
    EXPECT_EQ(outputData[3], 40);
    EXPECT_EQ(queue.sizeApprox(), 2u);

    int wrapData[5] = { 70, 80, 90, 100, 110 };
    EXPECT_TRUE(queue.tryPushRange(wrapData, 5u));
    EXPECT_EQ(queue.sizeApprox(), 7u);

    int popRemaining[7] = { 0 };
    EXPECT_TRUE(queue.tryPopRange(popRemaining, 7u));
    EXPECT_EQ(popRemaining[0], 50);
    EXPECT_EQ(popRemaining[1], 60);
    EXPECT_EQ(popRemaining[2], 70);
    EXPECT_EQ(popRemaining[3], 80);
    EXPECT_EQ(popRemaining[4], 90);
    EXPECT_EQ(popRemaining[5], 100);
    EXPECT_EQ(popRemaining[6], 110);
    EXPECT_TRUE(queue.isEmptyApprox());
}

TEST(SPSCRingBufferRangeTest, InsufficientSpaceAndNullRejection)
{
    SPSCRingBuffer<int> queue(4u);
    int data[5] = { 1, 2, 3, 4, 5 };

    EXPECT_FALSE(queue.tryPushRange(nullptr, 2u));
    EXPECT_FALSE(queue.tryPushRange(data, 5u));

    EXPECT_TRUE(queue.tryPushRange(data, 3u));
    EXPECT_FALSE(queue.tryPushRange(data, 2u));

    int outData[4] = { 0 };
    EXPECT_FALSE(queue.tryPopRange(nullptr, 2u));
    EXPECT_FALSE(queue.tryPopRange(outData, 4u));
    EXPECT_TRUE(queue.tryPopRange(outData, 3u));
}

TEST(SPSCRingBufferPeekAtTest, OffsetPeekWithoutConsuming)
{
    SPSCRingBuffer<int> queue(8u);
    int items[5] = { 100, 200, 300, 400, 500 };
    EXPECT_TRUE(queue.tryPushRange(items, 5u));

    int peekedVal = 0;
    EXPECT_TRUE(queue.tryPeekAt(0u, &peekedVal, 1u));
    EXPECT_EQ(peekedVal, 100);

    EXPECT_TRUE(queue.tryPeekAt(2u, &peekedVal, 1u));
    EXPECT_EQ(peekedVal, 300);

    int peekedRange[3] = { 0 };
    EXPECT_TRUE(queue.tryPeekAt(1u, peekedRange, 3u));
    EXPECT_EQ(peekedRange[0], 200);
    EXPECT_EQ(peekedRange[1], 300);
    EXPECT_EQ(peekedRange[2], 400);

    EXPECT_FALSE(queue.tryPeekAt(4u, peekedRange, 2u));
    EXPECT_FALSE(queue.tryPeekAt(5u, &peekedVal, 1u));

    EXPECT_EQ(queue.sizeApprox(), 5u);
    queue.advanceConsumer(5u);
    EXPECT_TRUE(queue.isEmptyApprox());
}

TEST(SPSCByteRingBufferPacketTest, WriteAndReadPacketStream)
{
    SPSCByteRingBuffer byteQueue(64u);

    TestPacketHeader header1{ 101u, 5u, 1u };
    const char* payload1 = "Hello";
    EXPECT_TRUE(byteQueue.tryWritePacket(header1, payload1, 5u));

    TestPacketHeader header2{ 102u, 0u, 2u };
    EXPECT_TRUE(byteQueue.tryWritePacket(header2, nullptr, 0u));

    TestPacketHeader readHeader1{};
    EXPECT_TRUE(byteQueue.tryPeekAt(0u, &readHeader1, sizeof(readHeader1)));
    EXPECT_EQ(readHeader1.packetId, 101u);
    EXPECT_EQ(readHeader1.payloadSize, 5u);
    EXPECT_EQ(readHeader1.sequenceNumber, 1u);

    char readPayload1[6] = { 0 };
    EXPECT_TRUE(byteQueue.tryPeekAt(sizeof(TestPacketHeader), readPayload1, readHeader1.payloadSize));
    EXPECT_STREQ(readPayload1, "Hello");

    byteQueue.advanceConsumer(sizeof(TestPacketHeader) + readHeader1.payloadSize);

    TestPacketHeader readHeader2{};
    EXPECT_TRUE(byteQueue.tryPeekAt(0u, &readHeader2, sizeof(readHeader2)));
    EXPECT_EQ(readHeader2.packetId, 102u);
    EXPECT_EQ(readHeader2.payloadSize, 0u);
    EXPECT_EQ(readHeader2.sequenceNumber, 2u);

    byteQueue.advanceConsumer(sizeof(TestPacketHeader));
    EXPECT_TRUE(byteQueue.isEmptyApprox());
}

TEST(SPSCByteRingBufferPacketTest, PacketBoundaryWrapAround)
{
    SPSCByteRingBuffer byteQueue(32u);

    gp::UInt8 padding[24];
    std::memset(padding, 0xAA, sizeof(padding));
    EXPECT_TRUE(byteQueue.tryPushRange(padding, 24u));
    byteQueue.advanceConsumer(24u);
    EXPECT_TRUE(byteQueue.isEmptyApprox());

    TestPacketHeader header{ 777u, 8u, 42u };
    const char* payload = "12345678";
    const gp::USize totalPacketSize = sizeof(header) + 8u;

    EXPECT_TRUE(byteQueue.tryWritePacket(header, payload, 8u));

    TestPacketHeader outHeader{};
    EXPECT_TRUE(byteQueue.tryPeekAt(0u, &outHeader, sizeof(outHeader)));
    EXPECT_EQ(outHeader.packetId, 777u);
    EXPECT_EQ(outHeader.payloadSize, 8u);

    char outPayload[9] = { 0 };
    EXPECT_TRUE(byteQueue.tryPeekAt(sizeof(TestPacketHeader), outPayload, 8u));
    EXPECT_STREQ(outPayload, "12345678");

    byteQueue.advanceConsumer(totalPacketSize);
    EXPECT_TRUE(byteQueue.isEmptyApprox());
}

TEST(SPSCRingBufferConcurrencyTest, SingleProducerSingleConsumerHighContention)
{
    constexpr gp::UInt64 kTotalItems = 200000ULL;
    constexpr gp::USize kQueueCapacity = 64u;

    SPSCRingBuffer<gp::UInt64> queue(kQueueCapacity);
    ASSERT_TRUE(queue.isValid());

    std::thread producer([&queue]()
    {
        for (gp::UInt64 i = 0ULL; i < kTotalItems; ++i)
        {
            while (!queue.tryPush(i))
            {
            }
        }
    });

    std::vector<gp::UInt64> receivedItems;
    receivedItems.reserve(kTotalItems);

    std::thread consumer([&queue, &receivedItems]()
    {
        gp::UInt64 item = 0ULL;
        while (receivedItems.size() < kTotalItems)
        {
            if (queue.tryPop(item))
            {
                receivedItems.push_back(item);
            }
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(receivedItems.size(), kTotalItems);
    for (gp::UInt64 i = 0ULL; i < kTotalItems; ++i)
    {
        ASSERT_EQ(receivedItems[i], i);
    }
}

TEST(SPSCRingBufferConcurrencyTest, ConcurrentPacketStreamProcessing)
{
    constexpr gp::UInt32 kPacketCount = 10000u;
    constexpr gp::USize kBufferSize = 1024u;

    SPSCByteRingBuffer queue(kBufferSize);
    ASSERT_TRUE(queue.isValid());

    std::thread producer([&queue]()
    {
        for (gp::UInt32 i = 0u; i < kPacketCount; ++i)
        {
            const gp::UInt16 payloadLen = static_cast<gp::UInt16>((i % 32u) + 1u);
            TestPacketHeader header;
            header.packetId = static_cast<gp::UInt16>(i & 0xFFFFu);
            header.payloadSize = payloadLen;
            header.sequenceNumber = i;

            std::vector<gp::UInt8> payload(payloadLen, static_cast<gp::UInt8>(i & 0xFFu));

            while (!queue.tryWritePacket(header, payload.data(), payloadLen))
            {
            }
        }
    });

    std::thread consumer([&queue]()
    {
        for (gp::UInt32 i = 0u; i < kPacketCount; ++i)
        {
            TestPacketHeader header{};
            while (!queue.tryPeekAt(0u, &header, sizeof(header)))
            {
            }

            ASSERT_EQ(header.sequenceNumber, i);
            const gp::UInt16 expectedLen = static_cast<gp::UInt16>((i % 32u) + 1u);
            ASSERT_EQ(header.payloadSize, expectedLen);

            std::vector<gp::UInt8> payload(header.payloadSize);
            while (!queue.tryPeekAt(sizeof(TestPacketHeader), payload.data(), header.payloadSize))
            {
            }

            for (gp::UInt8 byte: payload)
            {
                ASSERT_EQ(byte, static_cast<gp::UInt8>(i & 0xFFu));
            }

            queue.advanceConsumer(sizeof(TestPacketHeader) + header.payloadSize);
        }
    });

    producer.join();
    consumer.join();

    EXPECT_TRUE(queue.isEmptyApprox());
}

}   // namespace gp::tests
