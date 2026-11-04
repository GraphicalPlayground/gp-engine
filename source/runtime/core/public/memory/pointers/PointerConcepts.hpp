// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Fundamental.hpp"
#include "concepts/Relationship.hpp"
#include "CoreMinimal.hpp"
#include "memory/pointers/PointerTypes.hpp"

namespace gp::concepts
{

/// @brief Concept verifying that `InPolicyType` provides valid `noexcept` intrusive `addRef` and `release` hooks for
///        `InElementType*`.
/// @tparam InPolicyType Policy struct offering static `addRef` and `release` functions.
/// @tparam InElementType Pointee type managed by the policy.
template <typename InPolicyType, typename InElementType>
concept IsRefCountPolicy = !IsVoid<InElementType> && requires(InElementType* ptr) {
    {
        InPolicyType::addRef(ptr)
    } noexcept;
    {
        InPolicyType::release(ptr)
    } noexcept;
};

/// @brief Concept verifying that `InPolicyType` additionally supports querying the live reference count of
///        `const InElementType*`.
/// @tparam InPolicyType Policy struct to inspect.
/// @tparam InElementType Pointee type managed by the policy.
template <typename InPolicyType, typename InElementType>
concept HasRefCountQuery = IsRefCountPolicy<InPolicyType, InElementType> && requires(const InElementType* constPtr) {
    {
        InPolicyType::getRefCount(constPtr)
    } noexcept -> IsConvertibleTo<gp::UInt32>;
};

/// @brief Concept verifying that `InElementType` can be managed intrusively by `RefCountedPtr<InElementType,
///        InPolicyType>`.
/// @tparam InElementType Pointee type to validate.
/// @tparam InPolicyType Reference-counting policy (defaults to `gp::DefaultRefCountPolicy<InElementType>`).
template <typename InElementType, typename InPolicyType = gp::DefaultRefCountPolicy<InElementType>>
concept IsRefCounted = IsRefCountPolicy<InPolicyType, InElementType>;

/// @brief Concept detecting whether a derived class defines a custom `onZeroReferences()` hook to intercept
///        destruction when its intrusive reference count reaches zero.
/// @details Used by `IntrusiveRefCounted` at compile time to route zero-reference transitions to RHI deferred
///          deletion queues or custom `PoolAllocator` deallocation without incurring virtual call overhead.
/// @tparam InDerivedType Derived class type inheriting from `IntrusiveRefCounted`.
template <typename InDerivedType>
concept HasZeroReferencesHook = requires(InDerivedType& derived) {
    {
        derived.onZeroReferences()
    } noexcept;
};

}   // namespace gp::concepts
