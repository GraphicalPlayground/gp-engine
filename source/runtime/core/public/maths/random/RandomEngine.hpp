// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"             // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashingConcepts.hpp"
#include "hashing/HashingTypes.hpp"
#include "hashing/HashMix.hpp"
#include "maths/random/RandomEnums.hpp"
#include "maths/random/RandomForward.hpp"   // IWYU pragma: keep
#include "maths/random/RandomTypes.hpp"
#include <type_traits>                      // IWYU pragma: keep

namespace gp::math
{

/// @brief Shared CRTP base giving every stateful PRNG engine one uniform seeding, stepping, and state API.
/// @details Each concrete generator (`SplitMix64`, `Pcg32`, `Xoshiro128PlusPlus`, `Xoshiro256PlusPlus`, `Sfc64`)
///          implements `seedImpl`, `nextWordImpl`, and `discardImpl`. This base provides unified 32-bit and 64-bit
///          raw word extraction, string and digest seeding, deterministic child `fork()` derivation, state snapshots,
///          and `gp::hashing` integration without duplicating boilerplate across engines.
/// @tparam InDerived Concrete CRTP PRNG engine class.
/// @tparam InWordType Native raw unsigned word produced by the engine (`gp::UInt32` or `gp::UInt64`).
/// @tparam InStateType Trivially copyable POD state snapshot struct for the engine.
/// @tparam InAlgorithm Algorithm enumerator identifying this generator.
template <typename InDerived, typename InWordType, typename InStateType, RandomAlgorithm InAlgorithm>
class RandomEngineBase
{
public:
    /// @brief Native unsigned output word type (`gp::UInt32` or `gp::UInt64`).
    using WordType = InWordType;

    /// @brief Trivially copyable POD struct storing the generator's full internal state.
    using StateType = InStateType;

    /// @brief Enumerator identifying this algorithm for metadata queries and runtime dispatch.
    static constexpr RandomAlgorithm kAlgorithm = InAlgorithm;

    /// @brief Canonical default 64-bit seed used when constructing without explicit arguments.
    static constexpr gp::UInt64 kDefaultSeed = kDefaultRandomSeed;

    /// @brief Native raw output width in bits (`32` or `64`).
    static constexpr gp::USize kOutputBits = sizeof(WordType) * 8u;

public:
    /// @brief Seeds the generator with a 64-bit seed and default stream index `0`.
    /// @param[in] seedValue Deterministic 64-bit seed.
    constexpr void seed(const gp::UInt64 seedValue) noexcept
    {
        seed(seedValue, 0ULL);
    }

    /// @brief Seeds the generator with an explicit 64-bit seed and stream identifier.
    /// @param[in] seedValue Deterministic 64-bit seed.
    /// @param[in] streamIdValue Stream selector isolating independent sequences from the same base seed.
    constexpr void seed(const gp::UInt64 seedValue, const gp::UInt64 streamIdValue) noexcept
    {
        m_initialSeed = seedValue;
        m_streamId = streamIdValue;
        derived().seedImpl(seedValue, streamIdValue);
    }

    /// @brief Seeds the generator deterministically from an ASCII/UTF-8 label (such as a subsystem or biome name).
    /// @details Hashes @p label using `gp::hashing::Fnv1a64` and mixes @p salt into the stream identifier, following
    ///          the string-seeded stream pattern used in Ubisoft Anvil and Guerrilla Decima.
    /// @tparam InStrT String-like type satisfying `gp::hashing::concepts::IsCharacterSequence`.
    /// @param[in] label Descriptive stream identifier string.
    /// @param[in] salt Optional 64-bit numeric salt (defaults to `0ULL`).
    template <gp::hashing::concepts::IsCharacterSequence InStrT>
    constexpr void seedFromString(const InStrT& label, const gp::UInt64 salt = 0ULL) noexcept
    {
        const gp::UInt64 labelHash = gp::hashing::Fnv1a64::hash(label);
        seed(gp::hashing::splitMix64(labelHash ^ salt), labelHash);
    }

    /// @brief Seeds the generator from a null-terminated C-string label.
    /// @param[in] label Null-terminated ASCII/UTF-8 string (safe when `nullptr`).
    /// @param[in] salt Optional 64-bit numeric salt (defaults to `0ULL`).
    constexpr void seedFromString(const char* const label, const gp::UInt64 salt = 0ULL) noexcept
    {
        seedFromString(gp::StringView(label), salt);
    }

    /// @brief Seeds the generator from a 128-bit digest (`low` as seed, `high` as stream identifier).
    /// @param[in] hash 128-bit digest value.
    constexpr void seedFromHash(const gp::hashing::Hash128& hash) noexcept
    {
        seed(hash.low, hash.high);
    }

    /// @brief Seeds the generator from a 256-bit digest by folding its upper and lower 128-bit halves.
    /// @param[in] hash 256-bit digest value.
    constexpr void seedFromHash(const gp::hashing::Hash256& hash) noexcept
    {
        const gp::hashing::Hash128 folded = hash.foldTo128();
        seed(folded.low, folded.high);
    }

    /// @brief Restores the generator to the exact state produced by its most recent `seed()` call.
    constexpr void reset() noexcept
    {
        derived().seedImpl(m_initialSeed, m_streamId);
    }

    /// @brief Advances the engine and returns its native unsigned output word (`gp::UInt32` or `gp::UInt64`).
    /// @return Uniformly distributed raw word of width `kOutputBits`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr WordType nextWord() noexcept
    {
        return derived().nextWordImpl();
    }

    /// @brief Advances the engine and returns a uniform 32-bit unsigned integer.
    /// @details 64-bit engines return the upper 32 bits of `nextWord()`, where linear complexity is strongest in
    ///          Xoshiro and SplitMix generators.
    /// @return Uniformly distributed 32-bit unsigned integer.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 nextUInt32() noexcept
    {
        if constexpr (sizeof(WordType) == 8u)
        {
            return static_cast<gp::UInt32>(derived().nextWordImpl() >> 32u);
        }
        else
        {
            return derived().nextWordImpl();
        }
    }

    /// @brief Advances the engine and returns a uniform 64-bit unsigned integer.
    /// @details 32-bit engines pack two consecutive 32-bit draws (`(first << 32) | second`).
    /// @return Uniformly distributed 64-bit unsigned integer.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 nextUInt64() noexcept
    {
        if constexpr (sizeof(WordType) == 8u)
        {
            return derived().nextWordImpl();
        }
        else
        {
            const gp::UInt64 high = static_cast<gp::UInt64>(derived().nextWordImpl());
            const gp::UInt64 low = static_cast<gp::UInt64>(derived().nextWordImpl());
            return (high << 32u) | low;
        }
    }

    /// @brief Advances the engine forward by @p steps raw `nextWord()` transitions.
    /// @param[in] steps Number of native generator steps to skip.
    constexpr void discard(const gp::UInt64 steps) noexcept
    {
        derived().discardImpl(steps);
    }

    /// @brief Returns a read-only reference to the generator's serializable state snapshot.
    /// @return Const reference to `StateType`.
    [[nodiscard]] constexpr const StateType& state() const noexcept
    {
        return m_state;
    }

    /// @brief Restores a previously captured `StateType` snapshot (for savegames, replays, or rollback).
    /// @param[in] snapshot State snapshot obtained from `state()`.
    constexpr void setState(const StateType& snapshot) noexcept
    {
        m_state = snapshot;
    }

    /// @brief Returns the initial 64-bit seed supplied at construction or the last `seed()` call.
    /// @return Initial 64-bit seed value.
    [[nodiscard]] constexpr gp::UInt64 initialSeed() const noexcept
    {
        return m_initialSeed;
    }

    /// @brief Returns the 64-bit stream identifier supplied at construction or the last `seed()` call.
    /// @return Initial 64-bit stream ID.
    [[nodiscard]] constexpr gp::UInt64 streamId() const noexcept
    {
        return m_streamId;
    }

    /// @brief Derives a deterministic, independent child engine from the current state and @p subStreamId.
    /// @details Does not mutate the parent generator. Mixing `m_initialSeed`, `m_streamId`, `m_state`, and
    ///          @p subStreamId prevents sibling streams from sharing correlated prefixes.
    /// @param[in] subStreamId Numeric identifier for the forked child stream.
    /// @return Newly seeded child engine instance.
    [[nodiscard]] constexpr InDerived fork(const gp::UInt64 subStreamId) const noexcept
    {
        gp::hashing::Fnv1a64 hasher(m_initialSeed ^ 0x9E3779B97F4A7C15ULL);
        hasher.append(m_streamId);
        hasher.append(m_state);
        hasher.append(subStreamId);
        const gp::UInt64 childSeed = gp::hashing::splitMix64(hasher.finalize());
        const gp::UInt64 childStream = gp::hashing::cityHash128To64(m_streamId ^ subStreamId, childSeed);

        InDerived child;
        child.seed(childSeed, childStream);
        return child;
    }

    /// @brief Derives a deterministic child engine keyed by a human-readable subsystem or entity label.
    /// @tparam InStrT String-like type satisfying `gp::hashing::concepts::IsCharacterSequence`.
    /// @param[in] label Descriptive child stream name.
    /// @return Newly seeded child engine instance.
    template <gp::hashing::concepts::IsCharacterSequence InStrT>
    [[nodiscard]] constexpr InDerived fork(const InStrT& label) const noexcept
    {
        return fork(gp::hashing::Fnv1a64::hash(label));
    }

    /// @brief Derives a deterministic child engine keyed by a null-terminated C-string label.
    /// @param[in] label Null-terminated ASCII/UTF-8 string (safe when `nullptr`).
    /// @return Newly seeded child engine instance.
    [[nodiscard]] constexpr InDerived fork(const char* const label) const noexcept
    {
        return fork(gp::StringView(label));
    }

    /// @brief Streams algorithm tag, seed metadata, and internal state into any `gp::hashing` accumulator.
    /// @details Enables zero-allocation determinism checksums for lockstep simulation and replay validation.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target hash accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(kAlgorithm);
        hasher.append(m_initialSeed);
        hasher.append(m_streamId);
        hasher.append(m_state);
    }

    /// @brief Compares two engines of the same type by their internal PRNG state.
    /// @param[in] other Engine instance to compare against.
    /// @return True if both generators hold identical internal state.
    [[nodiscard]] constexpr bool operator==(const RandomEngineBase& other) const noexcept
    {
        return m_state == other.m_state;
    }

protected:
    /// @brief Protected default constructor restricted to CRTP derived engines.
    constexpr RandomEngineBase() noexcept = default;

    /// @brief Derives a 64-bit starting state from `(seedValue, streamIdValue)` for engines that seed via SplitMix64.
    /// @param[in] seedValue Base 64-bit seed.
    /// @param[in] streamIdValue Stream selector.
    /// @return Mixed 64-bit seed word.
    [[nodiscard]] static constexpr gp::UInt64
        mixSeedAndStream(const gp::UInt64 seedValue, const gp::UInt64 streamIdValue) noexcept
    {
        if (streamIdValue == 0ULL)
        {
            return seedValue;
        }
        return gp::hashing::cityHash128To64(seedValue, streamIdValue ^ 0x9E3779B97F4A7C15ULL);
    }

private:
    [[nodiscard]] constexpr InDerived& derived() noexcept
    {
        return static_cast<InDerived&>(*this);
    }

    [[nodiscard]] constexpr const InDerived& derived() const noexcept
    {
        return static_cast<const InDerived&>(*this);
    }

protected:
    StateType m_state{};
    gp::UInt64 m_initialSeed{ kDefaultRandomSeed };
    gp::UInt64 m_streamId{ 0ULL };
};

}   // namespace gp::math
