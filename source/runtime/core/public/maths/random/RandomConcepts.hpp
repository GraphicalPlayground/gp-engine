// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "maths/random/RandomEnums.hpp"
#include "maths/random/RandomForward.hpp"   // IWYU pragma: keep
#include <type_traits>

namespace gp::math
{

/// @brief Concept matching raw output word types produced by core PRNG engines (`gp::UInt32` or `gp::UInt64`).
/// @tparam InT Candidate word type.
template <typename InT>
concept IsRandomWord = gp::concepts::IsSameAs<std::remove_cvref_t<InT>, gp::UInt32> ||
                       gp::concepts::IsSameAs<std::remove_cvref_t<InT>, gp::UInt64>;

/// @brief Concept enforcing the unified deterministic PRNG engine contract across all `gp::math` generators.
/// @tparam InT Candidate engine type (`SplitMix64`, `Pcg32`, `Xoshiro128PlusPlus`, `Xoshiro256PlusPlus`, `Sfc64`).
template <typename InT>
concept IsRandomEngine = requires(
    InT engine,
    const InT constEngine,
    const gp::UInt64 seed64,
    const gp::UInt64 streamId64,
    const gp::UInt64 stepCount,
    const typename InT::StateType& stateSnapshot
) {
    typename InT::WordType;
    typename InT::StateType;
    requires IsRandomWord<typename InT::WordType>;
    requires std::is_trivially_copyable_v<typename InT::StateType>;
    {
        InT::kAlgorithm
    } -> gp::concepts::IsConvertibleTo<RandomAlgorithm>;
    {
        InT::kDefaultSeed
    } -> gp::concepts::IsConvertibleTo<gp::UInt64>;
    {
        engine.seed(seed64)
    } noexcept;
    {
        engine.seed(seed64, streamId64)
    } noexcept;
    {
        engine.reset()
    } noexcept;
    {
        engine.nextWord()
    } noexcept -> gp::concepts::IsSameAs<typename InT::WordType>;
    {
        engine.nextUInt32()
    } noexcept -> gp::concepts::IsSameAs<gp::UInt32>;
    {
        engine.nextUInt64()
    } noexcept -> gp::concepts::IsSameAs<gp::UInt64>;
    {
        engine.discard(stepCount)
    } noexcept;
    {
        constEngine.state()
    } noexcept -> gp::concepts::IsSameAs<const typename InT::StateType&>;
    {
        engine.setState(stateSnapshot)
    } noexcept;
    {
        constEngine.initialSeed()
    } noexcept -> gp::concepts::IsSameAs<gp::UInt64>;
    {
        constEngine.streamId()
    } noexcept -> gp::concepts::IsSameAs<gp::UInt64>;
    {
        constEngine.fork(streamId64)
    } noexcept -> gp::concepts::IsSameAs<InT>;
};

/// @brief Concept matching PRNG engines that support non-overlapping parallel `jump()` and `longJump()` sub-streams.
/// @tparam InT Candidate engine type.
template <typename InT>
concept IsJumpableRandomEngine = IsRandomEngine<InT> && requires(InT engine) {
    {
        engine.jump()
    } noexcept;
    {
        engine.longJump()
    } noexcept;
};

/// @brief Concept matching stateless counter-based PRNG evaluators such as `Philox4x32`.
/// @tparam InT Candidate counter-based evaluator type.
template <typename InT>
concept IsCounterRandomEngine = requires(const typename InT::KeyType& key, const typename InT::CounterType& counter) {
    typename InT::KeyType;
    typename InT::CounterType;
    typename InT::BlockType;
    {
        InT::evaluate(key, counter)
    } noexcept -> gp::concepts::IsSameAs<typename InT::BlockType>;
};

}   // namespace gp::math
