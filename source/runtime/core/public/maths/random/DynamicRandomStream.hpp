// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashingConcepts.hpp"
#include "hashing/HashingTypes.hpp"
#include "maths/random/Pcg32.hpp"
#include "maths/random/RandomConcepts.hpp"
#include "maths/random/RandomEnums.hpp"
#include "maths/random/RandomForward.hpp"   // IWYU pragma: keep
#include "maths/random/RandomStream.hpp"
#include "maths/random/RandomTypes.hpp"
#include "maths/random/Sfc64.hpp"
#include "maths/random/SplitMix64.hpp"
#include "maths/random/StatelessRandom.hpp"
#include "maths/random/Xoshiro128.hpp"
#include "maths/random/Xoshiro256.hpp"

namespace gp::math
{

/// @brief Zero-allocation runtime-dispatched random stream supporting any `RandomAlgorithm` enumerator.
/// @details Holds an inline tagged union of all built-in PRNG engines on the stack with no heap allocation or virtual
///          dispatch, mirroring `gp::hashing::DynamicHasher`. Exposes the complete `RandomStreamOperations` sampling
///          surface alongside runtime algorithm switching (`switchAlgorithm()`).
class DynamicRandomStream final : public RandomStreamOperations<DynamicRandomStream>
{
    friend class RandomStreamOperations<DynamicRandomStream>;

public:
    /// @brief Constructs a `DynamicRandomStream` using @p algorithm seeded with @p seedValue and @p streamIdValue.
    /// @param[in] algorithm Target PRNG algorithm (defaults to `RandomAlgorithm::Xoshiro256PlusPlus`).
    /// @param[in] seedValue Initial 64-bit seed (defaults to `kDefaultRandomSeed`).
    /// @param[in] streamIdValue Optional stream identifier (defaults to `0ULL`).
    [[nodiscard]] constexpr explicit DynamicRandomStream(
        const RandomAlgorithm algorithm = RandomAlgorithm::Xoshiro256PlusPlus,
        const gp::UInt64 seedValue = kDefaultRandomSeed,
        const gp::UInt64 streamIdValue = 0ULL
    ) noexcept
        : m_storage(makeStorage(sanitizeAlgorithm(algorithm), seedValue, streamIdValue))
        , m_algorithm(sanitizeAlgorithm(algorithm))
    {}

    /// @brief Creates a `DynamicRandomStream` seeded from runtime hardware entropy.
    /// @param[in] algorithm Target PRNG algorithm (defaults to `RandomAlgorithm::Xoshiro256PlusPlus`).
    /// @return Runtime-seeded `DynamicRandomStream`.
    [[nodiscard]] static inline DynamicRandomStream
        fromEntropy(const RandomAlgorithm algorithm = RandomAlgorithm::Xoshiro256PlusPlus) noexcept
    {
        return DynamicRandomStream(algorithm, makeEntropySeed(), 0ULL);
    }

public:
    /// @brief Returns the active `RandomAlgorithm` enumerator.
    /// @return Currently selected `RandomAlgorithm`.
    [[nodiscard]] constexpr RandomAlgorithm algorithm() const noexcept
    {
        return m_algorithm;
    }

    /// @brief Returns the native raw word width in bits (`32` or `64`) of the active algorithm.
    /// @return Word width in bits.
    [[nodiscard]] constexpr gp::UInt32 outputBits() const noexcept
    {
        return algorithmOutputBits(m_algorithm);
    }

    /// @brief Returns the state size in bytes of the active algorithm.
    /// @return State footprint in bytes.
    [[nodiscard]] constexpr gp::USize stateBytes() const noexcept
    {
        return algorithmStateBytes(m_algorithm);
    }

    /// @brief Reports whether the active algorithm provides jump-ahead (`jump()` / `longJump()`).
    /// @return True if fast jump-ahead is supported by the active algorithm.
    [[nodiscard]] constexpr bool supportsJump() const noexcept
    {
        return supportsJumpAhead(m_algorithm);
    }

    /// @brief Switches the active algorithm in place without allocation, seeded with @p seedValue and @p streamIdValue.
    /// @param[in] algorithm New PRNG algorithm to activate.
    /// @param[in] seedValue Initial 64-bit seed (defaults to `kDefaultRandomSeed`).
    /// @param[in] streamIdValue Optional stream identifier (defaults to `0ULL`).
    constexpr void switchAlgorithm(
        const RandomAlgorithm algorithm,
        const gp::UInt64 seedValue = kDefaultRandomSeed,
        const gp::UInt64 streamIdValue = 0ULL
    ) noexcept
    {
        m_algorithm = sanitizeAlgorithm(algorithm);
        m_storage = makeStorage(m_algorithm, seedValue, streamIdValue);
    }

    /// @brief Seeds the active algorithm with @p seedValue and optional @p streamIdValue.
    /// @param[in] seedValue Deterministic 64-bit seed.
    /// @param[in] streamIdValue Optional stream selector (defaults to `0ULL`).
    constexpr void seed(const gp::UInt64 seedValue, const gp::UInt64 streamIdValue = 0ULL) noexcept;

    /// @brief Seeds the active algorithm deterministically from a string label and optional numeric salt.
    /// @tparam InStrT String-like type satisfying `gp::hashing::concepts::IsCharacterSequence`.
    /// @param[in] seedLabel Descriptive seed string.
    /// @param[in] salt Optional 64-bit numeric salt (defaults to `0ULL`).
    template <gp::hashing::concepts::IsCharacterSequence InStrT>
    constexpr void seedFromString(const InStrT& seedLabel, const gp::UInt64 salt = 0ULL) noexcept
    {
        dispatch<void>(
            [&seedLabel, salt]<typename InEngine>(InEngine& eng) noexcept -> void
        {
            eng.seedFromString(seedLabel, salt);
        }
        );
    }

    /// @brief Seeds the active algorithm from a null-terminated C-string label.
    /// @param[in] seedLabel Null-terminated ASCII/UTF-8 string.
    /// @param[in] salt Optional 64-bit numeric salt (defaults to `0ULL`).
    constexpr void seedFromString(const char* const seedLabel, const gp::UInt64 salt = 0ULL) noexcept;

    /// @brief Seeds the active algorithm from a 128-bit digest (`Hash128`).
    /// @param[in] seedHash 128-bit digest seed.
    constexpr void seedFromHash(const gp::hashing::Hash128& seedHash) noexcept;

    /// @brief Seeds the active algorithm from a 256-bit digest (`Hash256`).
    /// @param[in] seedHash 256-bit digest seed.
    constexpr void seedFromHash(const gp::hashing::Hash256& seedHash) noexcept;

    /// @brief Resets the active engine back to its initial seed and stream identifier.
    constexpr void reset() noexcept;

    /// @brief Advances the active engine forward by @p steps raw output transitions.
    /// @param[in] steps Number of generator steps to discard.
    constexpr void discard(const gp::UInt64 steps) noexcept;

    /// @brief Advances the active engine using its native `jump()` operation, or returns `false` if unsupported.
    /// @return True if the active algorithm supports `jump()` (`SplitMix64`, `Pcg32`, `Xoshiro128PlusPlus`,
    ///         `Xoshiro256PlusPlus`), false for `Sfc64`.
    constexpr bool tryJump() noexcept;

    /// @brief Advances the active engine using its native `longJump()` operation, or returns `false` if unsupported.
    /// @return True if the active algorithm supports `longJump()`, false for `Sfc64`.
    constexpr bool tryLongJump() noexcept;

    /// @brief Derives a deterministic child `DynamicRandomStream` running the same algorithm without mutating `*this`.
    /// @param[in] subStreamId Numeric child stream identifier.
    /// @return Derived `DynamicRandomStream`.
    [[nodiscard]] constexpr DynamicRandomStream fork(const gp::UInt64 subStreamId) const noexcept;

    /// @brief Derives a deterministic child `DynamicRandomStream` keyed by @p label without mutating `*this`.
    /// @tparam InStrT String-like type satisfying `gp::hashing::concepts::IsCharacterSequence`.
    /// @param[in] label Descriptive child stream name.
    /// @return Derived `DynamicRandomStream`.
    template <gp::hashing::concepts::IsCharacterSequence InStrT>
    [[nodiscard]] constexpr DynamicRandomStream fork(const InStrT& label) const noexcept
    {
        return fork(gp::hashing::Fnv1a64::hash(label));
    }

    /// @brief Derives a deterministic child `DynamicRandomStream` keyed by a null-terminated C-string label.
    /// @param[in] label Null-terminated ASCII/UTF-8 label.
    /// @return Derived `DynamicRandomStream`.
    [[nodiscard]] constexpr DynamicRandomStream fork(const char* const label) const noexcept;

    /// @brief Returns the initial 64-bit seed of the active engine.
    /// @return Initial 64-bit seed.
    [[nodiscard]] constexpr gp::UInt64 initialSeed() const noexcept;

    /// @brief Returns the 64-bit stream identifier of the active engine.
    /// @return Initial 64-bit stream ID.
    [[nodiscard]] constexpr gp::UInt64 streamId() const noexcept;

    /// @brief Streams the active algorithm tag and engine state into any `gp::hashing` accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        dispatchConst<void>(
            [&hasher]<typename InEngine>(const InEngine& eng) noexcept -> void
        {
            eng.hashAppend(hasher);
        }
        );
    }

    /// @brief Compares two `DynamicRandomStream` instances by active algorithm and internal engine state.
    /// @param[in] other Stream to compare against.
    /// @return True if both streams run the same algorithm in the same state.
    [[nodiscard]] constexpr bool operator==(const DynamicRandomStream& other) const noexcept;

private:
    union EngineStorage
    {
        SplitMix64 splitMix64;
        Pcg32 pcg32;
        Xoshiro128PlusPlus xoshiro128;
        Xoshiro256PlusPlus xoshiro256;
        Sfc64 sfc64;

        constexpr explicit EngineStorage(const SplitMix64& value) noexcept
            : splitMix64(value)
        {}

        constexpr explicit EngineStorage(const Pcg32& value) noexcept
            : pcg32(value)
        {}

        constexpr explicit EngineStorage(const Xoshiro128PlusPlus& value) noexcept
            : xoshiro128(value)
        {}

        constexpr explicit EngineStorage(const Xoshiro256PlusPlus& value) noexcept
            : xoshiro256(value)
        {}

        constexpr explicit EngineStorage(const Sfc64& value) noexcept
            : sfc64(value)
        {}
    };

    template <typename InEngine>
    [[nodiscard]] constexpr explicit DynamicRandomStream(const InEngine& engine) noexcept
        : m_storage(engine)
        , m_algorithm(InEngine::kAlgorithm)
    {}

    [[nodiscard]] static constexpr RandomAlgorithm sanitizeAlgorithm(const RandomAlgorithm algorithm) noexcept
    {
        if (static_cast<gp::UInt8>(algorithm) >= static_cast<gp::UInt8>(RandomAlgorithm::Count))
        {
            return RandomAlgorithm::Xoshiro256PlusPlus;
        }
        return algorithm;
    }

    [[nodiscard]] static constexpr EngineStorage makeStorage(
        const RandomAlgorithm algorithm, const gp::UInt64 seedValue, const gp::UInt64 streamIdValue
    ) noexcept
    {
        switch (algorithm)
        {
        case RandomAlgorithm::SplitMix64:
            return EngineStorage(SplitMix64(seedValue, streamIdValue));
        case RandomAlgorithm::Pcg32:
            return EngineStorage(Pcg32(seedValue, streamIdValue));
        case RandomAlgorithm::Xoshiro128PlusPlus:
            return EngineStorage(Xoshiro128PlusPlus(seedValue, streamIdValue));
        case RandomAlgorithm::Xoshiro256PlusPlus:
            return EngineStorage(Xoshiro256PlusPlus(seedValue, streamIdValue));
        case RandomAlgorithm::Sfc64:
            return EngineStorage(Sfc64(seedValue, streamIdValue));
        case RandomAlgorithm::Count:
            // falls through
        default:
            return EngineStorage(Xoshiro256PlusPlus(seedValue, streamIdValue));
        }
    }

    template <typename InRetT, typename InVisitor>
    constexpr InRetT dispatch(InVisitor&& visitor) noexcept
    {
        switch (m_algorithm)
        {
        case RandomAlgorithm::SplitMix64:
            return visitor(m_storage.splitMix64);
        case RandomAlgorithm::Pcg32:
            return visitor(m_storage.pcg32);
        case RandomAlgorithm::Xoshiro128PlusPlus:
            return visitor(m_storage.xoshiro128);
        case RandomAlgorithm::Xoshiro256PlusPlus:
            return visitor(m_storage.xoshiro256);
        case RandomAlgorithm::Sfc64:
            return visitor(m_storage.sfc64);
        case RandomAlgorithm::Count:
            // falls through
        default:
            return visitor(m_storage.xoshiro256);
        }
    }

    template <typename InRetT, typename InVisitor>
    constexpr InRetT dispatchConst(InVisitor&& visitor) const noexcept
    {
        switch (m_algorithm)
        {
        case RandomAlgorithm::SplitMix64:
            return visitor(m_storage.splitMix64);
        case RandomAlgorithm::Pcg32:
            return visitor(m_storage.pcg32);
        case RandomAlgorithm::Xoshiro128PlusPlus:
            return visitor(m_storage.xoshiro128);
        case RandomAlgorithm::Xoshiro256PlusPlus:
            return visitor(m_storage.xoshiro256);
        case RandomAlgorithm::Sfc64:
            return visitor(m_storage.sfc64);
        case RandomAlgorithm::Count:
            // falls through
        default:
            return visitor(m_storage.xoshiro256);
        }
    }

    [[nodiscard]] constexpr gp::UInt32 engineNextUInt32() noexcept;
    [[nodiscard]] constexpr gp::UInt64 engineNextUInt64() noexcept;

private:
    EngineStorage m_storage;
    RandomAlgorithm m_algorithm{ RandomAlgorithm::Xoshiro256PlusPlus };
};

constexpr void DynamicRandomStream::seed(const gp::UInt64 seedValue, const gp::UInt64 streamIdValue) noexcept
{
    dispatch<void>(
        [seedValue, streamIdValue]<typename InEngine>(InEngine& eng) noexcept -> void
    {
        eng.seed(seedValue, streamIdValue);
    }
    );
}

constexpr void DynamicRandomStream::seedFromString(const char* const seedLabel, const gp::UInt64 salt) noexcept
{
    seedFromString(gp::StringView(seedLabel), salt);
}

constexpr void DynamicRandomStream::seedFromHash(const gp::hashing::Hash128& seedHash) noexcept
{
    dispatch<void>(
        [&seedHash]<typename InEngine>(InEngine& eng) noexcept -> void
    {
        eng.seedFromHash(seedHash);
    }
    );
}

constexpr void DynamicRandomStream::seedFromHash(const gp::hashing::Hash256& seedHash) noexcept
{
    dispatch<void>(
        [&seedHash]<typename InEngine>(InEngine& eng) noexcept -> void
    {
        eng.seedFromHash(seedHash);
    }
    );
}

constexpr void DynamicRandomStream::reset() noexcept
{
    dispatch<void>(
        []<typename InEngine>(InEngine& eng) noexcept -> void
    {
        eng.reset();
    }
    );
}

constexpr void DynamicRandomStream::discard(const gp::UInt64 steps) noexcept
{
    dispatch<void>(
        [steps]<typename InEngine>(InEngine& eng) noexcept -> void
    {
        eng.discard(steps);
    }
    );
}

constexpr bool DynamicRandomStream::tryJump() noexcept
{
    return dispatch<bool>(
        []<typename InEngine>(InEngine& eng) noexcept -> bool
    {
        if constexpr (IsJumpableRandomEngine<InEngine>)
        {
            eng.jump();
            return true;
        }
        else
        {
            return false;
        }
    }
    );
}

constexpr bool DynamicRandomStream::tryLongJump() noexcept
{
    return dispatch<bool>(
        []<typename InEngine>(InEngine& eng) noexcept -> bool
    {
        if constexpr (IsJumpableRandomEngine<InEngine>)
        {
            eng.longJump();
            return true;
        }
        else
        {
            return false;
        }
    }
    );
}

constexpr DynamicRandomStream DynamicRandomStream::fork(const gp::UInt64 subStreamId) const noexcept
{
    return dispatchConst<DynamicRandomStream>(
        [subStreamId]<typename InEngine>(const InEngine& eng) noexcept -> DynamicRandomStream
    {
        return DynamicRandomStream(eng.fork(subStreamId));
    }
    );
}

constexpr DynamicRandomStream DynamicRandomStream::fork(const char* const label) const noexcept
{
    return fork(gp::StringView(label));
}

constexpr gp::UInt64 DynamicRandomStream::initialSeed() const noexcept
{
    return dispatchConst<gp::UInt64>(
        []<typename InEngine>(const InEngine& eng) noexcept -> gp::UInt64
    {
        return eng.initialSeed();
    }
    );
}

constexpr gp::UInt64 DynamicRandomStream::streamId() const noexcept
{
    return dispatchConst<gp::UInt64>(
        []<typename InEngine>(const InEngine& eng) noexcept -> gp::UInt64
    {
        return eng.streamId();
    }
    );
}

constexpr bool DynamicRandomStream::operator==(const DynamicRandomStream& other) const noexcept
{
    if (m_algorithm != other.m_algorithm)
    {
        return false;
    }
    return dispatchConst<bool>(
        [&other]<typename InEngine>(const InEngine& eng) noexcept -> bool
    {
        return other.dispatchConst<bool>(
            [&eng]<typename InOtherEngine>(const InOtherEngine& rhs) noexcept -> bool
        {
            if constexpr (gp::concepts::IsSameAs<InEngine, InOtherEngine>)
            {
                return eng == rhs;
            }
            else
            {
                return false;
            }
        }
        );
    }
    );
}

constexpr gp::UInt32 DynamicRandomStream::engineNextUInt32() noexcept
{
    return dispatch<gp::UInt32>(
        []<typename InEngine>(InEngine& eng) noexcept -> gp::UInt32
    {
        return eng.nextUInt32();
    }
    );
}

constexpr gp::UInt64 DynamicRandomStream::engineNextUInt64() noexcept
{
    return dispatch<gp::UInt64>(
        []<typename InEngine>(InEngine& eng) noexcept -> gp::UInt64
    {
        return eng.nextUInt64();
    }
    );
}

}   // namespace gp::math
