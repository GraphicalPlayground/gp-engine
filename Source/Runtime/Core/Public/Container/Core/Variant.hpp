// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Templates/Concepts/ComparisonConcepts.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Concepts/ObjectConcepts.hpp"
#include "Templates/Core/Conditional.hpp"
#include "Templates/Core/RemoveCVRef.hpp"
#include "Templates/Core/TemplateTypes.hpp"
#include "Templates/Core/Utility.hpp"
#include "Templates/Traits/TypeRelationships.hpp"
#include <compare>
#include <utility>

namespace GP
{

// Supress warnings about unreachable code in the visitor dispatch when the visitor's return type is void.
#ifdef GP_COMPILER_MSVC
    #pragma warning(push)
    #pragma warning(disable : 4702)   // unreachable code
#elif GP_COMPILER_CLANG || GP_COMPILER_GCC
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunreachable-code"
#endif

// clang-format off

/// @brief Empty monostate type. Enables default construction of a TVariant whose first
///        alternative is not itself default-constructible.
struct TMonostate
{};

GP_NODISCARD constexpr bool    operator==(TMonostate, TMonostate) noexcept { return true; }
GP_NODISCARD constexpr auto    operator<=>(TMonostate, TMonostate) noexcept { return std::strong_ordering::equal; }

/// @brief Tag type for constructing a TVariant in-place with a specific type T.
/// @tparam T Type to construct.
template <typename T>
struct TInPlaceTypeT  { explicit constexpr TInPlaceTypeT()  = default; };

/// @brief Tag type for constructing a TVariant in-place with the alternative at index I.
/// @tparam I Zero-based index of the alternative to construct.
template <SizeT I>
struct TInPlaceIndexT { explicit constexpr TInPlaceIndexT() = default; };

/// @brief Constant tag instance for TInPlaceTypeT.
template <typename T>
inline constexpr TInPlaceTypeT<T>  InPlaceType{};

/// @brief Constant tag instance for TInPlaceIndexT.
template <SizeT I>
inline constexpr TInPlaceIndexT<I> InPlaceIndex{};

// clang-format on

namespace Detail
{

/// @brief Retrieves the type at compile-time index I in the parameter pack.
/// @tparam I Zero-based index.
/// @tparam Ts Parameter pack to search.
template <SizeT I, typename... Ts>
struct TVariantTypeAt;

template <typename H, typename... Rest>
struct TVariantTypeAt<0, H, Rest...>
{
    using Type = H;
};

template <SizeT I, typename H, typename... Rest>
struct TVariantTypeAt<I, H, Rest...> : TVariantTypeAt<I - 1, Rest...>
{};

/// @brief Alias for TVariantTypeAt<I, Ts...>::Type.
template <SizeT I, typename... Ts>
using TVariantTypeAt_T = typename TVariantTypeAt<I, Ts...>::Type;

/// @brief Finds the zero-based index of the first occurrence of T in the pack.
///        Returns SizeT(-1) if T is not found.
template <typename T, SizeT I, typename... Ts>
struct TVariantIndexOf
{
    static constexpr SizeT Value = SizeT(-1);
};

template <typename T, SizeT I, typename U, typename... Rest>
struct TVariantIndexOf<T, I, U, Rest...>
{
    static constexpr SizeT Value = TIsSame_V<T, U> ? I : TVariantIndexOf<T, I + 1, Rest...>::Value;
};

/// @brief Finds the zero-based index of the first exact type match (TIsSame_V<T, U>).
///        Returns SizeT(-1) if T does not appear as any alternative.
template <typename T, SizeT I, typename... Ts>
struct TVariantExactIndex
{
    static constexpr SizeT Value = SizeT(-1);
};

template <typename T, SizeT I, typename U, typename... Rest>
struct TVariantExactIndex<T, I, U, Rest...>
{
    static constexpr SizeT Value = TIsSame_V<T, U> ? I : TVariantExactIndex<T, I + 1, Rest...>::Value;
};

/// @brief Finds the index of the first alternative constructible from type T.
///        Returns SizeT(-1) if no such alternative exists.
template <typename T, SizeT I, typename... Ts>
struct TVariantBestMatch
{
    static constexpr SizeT Value = SizeT(-1);
};

template <typename T, SizeT I, typename U, typename... Rest>
struct TVariantBestMatch<T, I, U, Rest...>
{
    static constexpr SizeT Value = TIsConstructible_V<U, T&&> ? I : TVariantBestMatch<T, I + 1, Rest...>::Value;
};

/// @brief Computes the maximum of a compile-time integer pack.
template <SizeT... Ns>
struct TVariantMaxOf;

template <SizeT N>
struct TVariantMaxOf<N> : TIntegralConstant<SizeT, N>
{};

template <SizeT N, SizeT... Rest>
struct TVariantMaxOf<N, Rest...>
    : TIntegralConstant<SizeT, (N > TVariantMaxOf<Rest...>::Value ? N : TVariantMaxOf<Rest...>::Value)>
{};

/// @brief Dispatches element-wise equality comparison for the active TVariant index.
template <SizeT I, typename... Ts>
struct TVariantEquals
{
    static bool Apply(SizeT /*index*/, const void* /*lhs*/, const void* /*rhs*/) noexcept
    {
        GP_UNREACHABLE();
        return false;
    }
};

template <SizeT I, typename T, typename... Rest>
struct TVariantEquals<I, T, Rest...>
{
    static bool Apply(SizeT index, const void* lhs, const void* rhs) noexcept
    {
        if (index == I) { return *reinterpret_cast<const T*>(lhs) == *reinterpret_cast<const T*>(rhs); }
        return TVariantEquals<I + 1, Rest...>::Apply(index, lhs, rhs);
    }
};

/// @brief Dispatches element-wise three-way comparison for the active TVariant index.
template <SizeT I, typename Cat, typename... Ts>
struct TVariantCompare
{
    static Cat Apply(SizeT /*index*/, const void* /*lhs*/, const void* /*rhs*/) noexcept
    {
        GP_UNREACHABLE();
        return Cat::equivalent;
    }
};

template <SizeT I, typename Cat, typename T, typename... Rest>
struct TVariantCompare<I, Cat, T, Rest...>
{
    static Cat Apply(SizeT index, const void* lhs, const void* rhs) noexcept
    {
        if (index == I) { return Cat(*reinterpret_cast<const T*>(lhs) <=> *reinterpret_cast<const T*>(rhs)); }
        return TVariantCompare<I + 1, Cat, Rest...>::Apply(index, lhs, rhs);
    }
};

/// @brief Recursive visitor dispatch; terminates at the end of the type list.
/// @tparam I   Current dispatch index.
/// @tparam RetT Return type deduced from the first alternative's invocation.
/// @tparam TVisitor Visitor callable type.
/// @tparam Ts  Remaining alternative types.
template <SizeT I, typename RetT, typename TVisitor, typename... Ts>
struct TVariantVisit
{
    GP_NORETURN static RetT Apply(SizeT, void*, TVisitor&&) { GP_UNREACHABLE(); }

    GP_NORETURN static RetT ApplyConst(SizeT, const void*, TVisitor&&) { GP_UNREACHABLE(); }
};

template <SizeT I, typename RetT, typename TVisitor, typename T, typename... Rest>
struct TVariantVisit<I, RetT, TVisitor, T, Rest...>
{
    static RetT Apply(SizeT index, void* storage, TVisitor&& visitor)
    {
        if (index == I) { return static_cast<RetT>(GP::Forward<TVisitor>(visitor)(*reinterpret_cast<T*>(storage))); }
        return TVariantVisit<I + 1, RetT, TVisitor, Rest...>::Apply(index, storage, GP::Forward<TVisitor>(visitor));
    }

    static RetT ApplyConst(SizeT index, const void* storage, TVisitor&& visitor)
    {
        if (index == I)
        {
            return static_cast<RetT>(GP::Forward<TVisitor>(visitor)(*reinterpret_cast<const T*>(storage)));
        }
        return TVariantVisit<I + 1, RetT, TVisitor, Rest...>::ApplyConst(
            index, storage, GP::Forward<TVisitor>(visitor)
        );
    }
};

}   // namespace Detail

/// @brief Type-safe tagged union that holds exactly one value from a fixed set of alternatives.
///        The active alternative is tracked by a zero-based index. A special kInvalidIndex
///        indicates the "valueless by exception" state.
/// @note  All access through Get<I>() or Get<T>() asserts that the requested alternative is
///        active. Non-asserting access is available via TryGet<I>() / TryGet<T>().
///        Visitation is available via the free function Visit().
/// @tparam Ts Alternative types. At least one type must be provided.
template <typename... Ts>
class TVariant
{
    static_assert(sizeof...(Ts) > 0, "TVariant requires at least one alternative type");

public:
    /// @brief Type at zero-based compile-time index I among the alternatives.
    template <SizeT I>
    using TTypeAt = Detail::TVariantTypeAt_T<I, Ts...>;

public:
    /// @brief Number of alternative types.
    static constexpr SizeT kSize = sizeof...(Ts);

    /// @brief Sentinel index returned when the variant is valueless by exception.
    static constexpr SizeT kInvalidIndex = SizeT(-1);

    /// @brief Zero-based index of type T among the alternatives, or kInvalidIndex if absent.
    template <typename T>
    static constexpr SizeT kIndexOf = Detail::TVariantIndexOf<T, 0, Ts...>::Value;

    /// @brief Index of the best-matching alternative for constructing from T.
    ///        Prefers exact type match (TIsSame_V<T, U>) over any constructible alternative.
    ///        Returns kInvalidIndex if no match exists.
    template <typename T>
    static constexpr SizeT kBestMatchFor = (Detail::TVariantExactIndex<T, 0, Ts...>::Value != kInvalidIndex)
                                               ? Detail::TVariantExactIndex<T, 0, Ts...>::Value
                                               : Detail::TVariantBestMatch<T, 0, Ts...>::Value;

private:
    static constexpr SizeT kBufferSize = Detail::TVariantMaxOf<sizeof(Ts)...>::Value;
    static constexpr SizeT kBufferAlign = Detail::TVariantMaxOf<alignof(Ts)...>::Value;

    /// @brief Per-type virtual dispatch table for type-erased operations.
    struct FVTable
    {
        void (*destroy)(void* dst) noexcept;
        void (*copyConstruct)(void* dst, const void* src);
        void (*moveConstruct)(void* dst, void* src) noexcept;
        void (*copyAssign)(void* dst, const void* src);
        void (*moveAssign)(void* dst, void* src) noexcept;
    };

    /// @brief Generates the FVTable entries for a single alternative type T.
    /// @tparam T Alternative type.
    template <typename T>
    struct TVariantOps
    {
        static void Destroy(void* p) noexcept { reinterpret_cast<T*>(p)->~T(); }

        static void CopyConstruct(void* dst, const void* src) { ::new (dst) T(*reinterpret_cast<const T*>(src)); }

        static void MoveConstruct(void* dst, void* src) noexcept
        {
            ::new (dst) T(GP::Move(*reinterpret_cast<T*>(src)));
        }

        static void CopyAssign(void* dst, const void* src)
        {
            *reinterpret_cast<T*>(dst) = *reinterpret_cast<const T*>(src);
        }

        static void MoveAssign(void* dst, void* src) noexcept
        {
            *reinterpret_cast<T*>(dst) = GP::Move(*reinterpret_cast<T*>(src));
        }

        static constexpr FVTable kVTable{ &Destroy, &CopyConstruct, &MoveConstruct, &CopyAssign, &MoveAssign };
    };

    static constexpr FVTable kVTables[kSize] = { TVariantOps<Ts>::kVTable... };

private:
    // clang-format off
    GP_ALIGN(kBufferAlign) Byte m_storage[kBufferSize] {};
    // clang-format on

    SizeT m_index{ kInvalidIndex };

private:
    template <SizeT I, typename... Us>
    friend constexpr auto& Get(TVariant<Us...>& v) noexcept;

    template <SizeT I, typename... Us>
    friend constexpr const auto& Get(const TVariant<Us...>& v) noexcept;

    template <SizeT I, typename... Us>
    friend constexpr auto&& Get(TVariant<Us...>&& v) noexcept;

    template <SizeT I, typename... Us>
    friend constexpr auto* TryGet(TVariant<Us...>* v) noexcept;

    template <SizeT I, typename... Us>
    friend constexpr const auto* TryGet(const TVariant<Us...>* v) noexcept;

    template <typename TVisitor, typename... Us>
    friend decltype(auto) Visit(TVisitor&& visitor, TVariant<Us...>& variant);

    template <typename TVisitor, typename... Us>
    friend decltype(auto) Visit(TVisitor&& visitor, const TVariant<Us...>& variant);

    template <typename TVisitor, typename... Us>
    friend decltype(auto) Visit(TVisitor&& visitor, TVariant<Us...>&& variant);

public:
    /// @brief Default-constructs the first alternative (index 0).
    /// @note  Only available when the first alternative is default-constructible.
    constexpr TVariant() requires CDefaultInitializable<TTypeAt<0>>
        : m_index(0)
    {
        ::new (m_storage) TTypeAt<0>();
    }

    /// @brief Converting constructor: constructs the first alternative that T is constructible from.
    /// @tparam T Source type. Must not be TVariant. The best-matching alternative is used.
    /// @param value Value to convert from, perfectly forwarded.
    template <typename T>
    requires(!CSameAs<TRemoveCVRef_T<T>, TVariant> && (kBestMatchFor<TRemoveCVRef_T<T>> != kInvalidIndex))
    TVariant(T&& value)
        : m_index(kBestMatchFor<TRemoveCVRef_T<T>>)
    {
        using U = TTypeAt<kBestMatchFor<TRemoveCVRef_T<T>>>;
        ::new (m_storage) U(GP::Forward<T>(value));
    }

    /// @brief In-place-by-type constructor: constructs alternative T from @p args.
    /// @tparam T Alternative type. Must be one of Ts.
    /// @tparam Args Constructor argument types.
    /// @param args Arguments forwarded to T's constructor.
    template <typename T, typename... Args>
    requires(CConstructibleFrom<T, Args && ...> && (kIndexOf<T> != kInvalidIndex))
    explicit TVariant(TInPlaceTypeT<T>, Args&&... args)
        : m_index(kIndexOf<T>)
    {
        ::new (m_storage) T(GP::Forward<Args>(args)...);
    }

    /// @brief In-place-by-index constructor: constructs the alternative at index I from @p args.
    /// @tparam I Zero-based index of the alternative.
    /// @tparam Args Constructor argument types.
    /// @param args Arguments forwarded to the alternative's constructor.
    template <SizeT I, typename... Args>
    requires(I < kSize && CConstructibleFrom<TTypeAt<I>, Args && ...>)
    explicit TVariant(TInPlaceIndexT<I>, Args&&... args)
        : m_index(I)
    {
        ::new (m_storage) TTypeAt<I>(GP::Forward<Args>(args)...);
    }

    /// @brief Copy-constructs from @p other.
    /// @param other TVariant to copy from. May be valueless.
    TVariant(const TVariant& other)
        : m_index(other.m_index)
    {
        if (m_index != kInvalidIndex) { kVTables[m_index].copyConstruct(m_storage, other.m_storage); }
    }

    /// @brief Move-constructs from @p other, leaving @p other valueless.
    /// @param other TVariant to move from.
    TVariant(TVariant&& other) noexcept
        : m_index(other.m_index)
    {
        if (m_index != kInvalidIndex)
        {
            kVTables[m_index].moveConstruct(m_storage, other.m_storage);
            kVTables[m_index].destroy(other.m_storage);
            other.m_index = kInvalidIndex;
        }
    }

    /// @brief Destroys the active alternative and the TVariant itself.
    ~TVariant() { DestroyActive(); }

public:
    /// @brief Copy-assigns from @p other, replacing the current alternative.
    /// @param other TVariant to copy from.
    /// @return Reference to this.
    TVariant& operator=(const TVariant& other)
    {
        if (this == &other) { return *this; }

        if (other.m_index == kInvalidIndex) { DestroyActive(); }
        else if (m_index == other.m_index) { kVTables[m_index].copyAssign(m_storage, other.m_storage); }
        else
        {
            DestroyActive();
            kVTables[other.m_index].copyConstruct(m_storage, other.m_storage);
            m_index = other.m_index;
        }
        return *this;
    }

    /// @brief Move-assigns from @p other, leaving @p other valueless.
    /// @param other TVariant to move from.
    /// @return Reference to this.
    TVariant& operator=(TVariant&& other) noexcept
    {
        if (this == &other) { return *this; }

        if (other.m_index == kInvalidIndex) { DestroyActive(); }
        else if (m_index == other.m_index)
        {
            kVTables[m_index].moveAssign(m_storage, other.m_storage);
            // GP stronger guarantee: destroy moved-from value and mark source as valueless.
            kVTables[m_index].destroy(other.m_storage);
            other.m_index = kInvalidIndex;
        }
        else
        {
            DestroyActive();
            kVTables[other.m_index].moveConstruct(m_storage, other.m_storage);
            m_index = other.m_index;
            // Destroy the moved-from alternative in source and mark it valueless.
            kVTables[m_index].destroy(other.m_storage);
            other.m_index = kInvalidIndex;
        }
        return *this;
    }

    /// @brief Converting assignment: assigns the best-matching alternative from @p value.
    /// @tparam T Value type. Must not be TVariant itself.
    /// @param value Value to assign.
    /// @return Reference to this.
    template <typename T>
    requires(!CSameAs<TRemoveCVRef_T<T>, TVariant> && (kBestMatchFor<TRemoveCVRef_T<T>> != kInvalidIndex))
    TVariant& operator=(T&& value)
    {
        constexpr SizeT I = kBestMatchFor<TRemoveCVRef_T<T>>;
        using U = TTypeAt<I>;
        if (m_index == I) { *reinterpret_cast<U*>(m_storage) = GP::Forward<T>(value); }
        else
        {
            DestroyActive();
            ::new (m_storage) U(GP::Forward<T>(value));
            m_index = I;
        }
        return *this;
    }

    /// @brief Equality operator; compares two variants element-wise.
    /// @param other TVariant to compare against.
    /// @return True if both are valueless, or hold the same active index with equal values.
    /// @note Requires all alternative types to satisfy CEqualityComparable.
    GP_NODISCARD bool operator==(const TVariant& other) const noexcept requires((CEqualityComparable<Ts> && ...))
    {
        if (m_index != other.m_index) { return false; }
        if (m_index == kInvalidIndex) { return true; }
        return Detail::TVariantEquals<0, Ts...>::Apply(m_index, m_storage, other.m_storage);
    }

    /// @brief Inequality operator; negates operator==.
    /// @param other TVariant to compare against.
    /// @return True if the variants differ.
    GP_NODISCARD bool operator!=(const TVariant& other) const noexcept requires((CEqualityComparable<Ts> && ...))
    {
        return !(*this == other);
    }

    /// @brief Three-way comparison operator.
    ///        Valueless variants compare less than non-valueless ones. Variants with different
    ///        active indices compare by index. Variants with the same active index compare by value.
    /// @param other TVariant to compare against.
    /// @return Comparison result in the common category of all alternative comparisons.
    GP_NODISCARD auto operator<=>(const TVariant& other) const noexcept requires((CThreeWayComparable<Ts> && ...))
    {
        using Cat = std::common_comparison_category_t<
            std::strong_ordering,
            decltype(GP::Declval<const Ts&>() <=> GP::Declval<const Ts&>())...>;

        // Valueless ordering
        if (m_index == kInvalidIndex && other.m_index == kInvalidIndex) { return Cat(std::strong_ordering::equal); }
        if (m_index == kInvalidIndex) { return Cat(std::strong_ordering::less); }
        if (other.m_index == kInvalidIndex) { return Cat(std::strong_ordering::greater); }

        // Different active alternative: compare by index
        if (m_index != other.m_index) { return Cat(m_index <=> other.m_index); }

        // Same active alternative: compare by value
        return Detail::TVariantCompare<0, Cat, Ts...>::Apply(m_index, m_storage, other.m_storage);
    }

public:
    /// @brief Returns the zero-based index of the active alternative.
    ///        Returns kInvalidIndex when the variant is valueless by exception.
    /// @return Active alternative index.
    GP_NODISCARD SizeT Index() const noexcept { return m_index; }

    /// @brief Checks whether the variant is in the valueless-by-exception state.
    /// @return True if no alternative is currently active.
    GP_NODISCARD bool IsValueless() const noexcept { return m_index == kInvalidIndex; }

    /// @brief Returns true if the alternative at compile-time index I is currently active.
    /// @tparam I Zero-based index to test.
    template <SizeT I>
    GP_NODISCARD bool HoldsAlternative() const noexcept
    {
        static_assert(I < kSize, "TVariant::HoldsAlternative<I>: index out of range");
        return m_index == I;
    }

    /// @brief Returns true if the alternative of type T is currently active.
    /// @tparam T Alternative type. Must appear in Ts.
    template <typename T>
    GP_NODISCARD bool HoldsAlternative() const noexcept
    {
        static_assert(kIndexOf<T> != kInvalidIndex, "TVariant::HoldsAlternative<T>: T is not an alternative");
        return m_index == kIndexOf<T>;
    }

    /// @brief Destroys the current alternative and constructs the alternative at index I in-place.
    /// @tparam I Zero-based index of the alternative to emplace.
    /// @tparam Args Constructor argument types.
    /// @param args Arguments forwarded to the alternative's constructor.
    /// @return Reference to the newly constructed value.
    template <SizeT I, typename... Args>
    requires(I < kSize && CConstructibleFrom<TTypeAt<I>, Args && ...>) TTypeAt<I>& Emplace(Args&&... args)
    {
        DestroyActive();
        ::new (m_storage) TTypeAt<I>(GP::Forward<Args>(args)...);
        m_index = I;
        return *reinterpret_cast<TTypeAt<I>*>(m_storage);
    }

    /// @brief Destroys the current alternative and constructs alternative T in-place.
    /// @tparam T Alternative type. Must appear in Ts and must be constructible from Args.
    /// @tparam Args Constructor argument types.
    /// @param args Arguments forwarded to T's constructor.
    /// @return Reference to the newly constructed value.
    template <typename T, typename... Args>
    requires(CConstructibleFrom<T, Args && ...> && (kIndexOf<T> != kInvalidIndex)) T& Emplace(Args&&... args)
    {
        return Emplace<kIndexOf<T>>(GP::Forward<Args>(args)...);
    }

    /// @brief Swaps the active alternatives of this variant and @p other.
    /// @param other TVariant to swap with.
    void Swap(TVariant& other) noexcept
    {
        if (this == &other) { return; }
        TVariant tmp(GP::Move(*this));
        *this = GP::Move(other);
        other = GP::Move(tmp);
    }

private:
    /// @brief Internal helper that destroys and clears the active alternative.
    void DestroyActive() noexcept
    {
        if (m_index != kInvalidIndex)
        {
            kVTables[m_index].destroy(m_storage);
            m_index = kInvalidIndex;
        }
    }
};

/// @brief Returns true if @p variant currently holds alternative T.
/// @tparam T Alternative type. Must appear as an alternative in @p variant.
/// @param variant TVariant to inspect.
/// @return True if T is the active alternative.
template <typename T, typename... Ts>
GP_NODISCARD constexpr bool HoldsAlternative(const TVariant<Ts...>& variant) noexcept
{
    return variant.template HoldsAlternative<T>();
}

/// @brief Returns a mutable reference to the alternative at index I.
/// @tparam I Zero-based index.
/// @tparam Ts Alternative types.
/// @param v Mutable variant reference.
/// @return Mutable reference to the alternative at I.
/// @warning Asserts that I matches the active index.
template <SizeT I, typename... Ts>
GP_NODISCARD constexpr auto& Get(TVariant<Ts...>& v) noexcept
{
    static_assert(I < TVariant<Ts...>::kSize, "Get<I>: index out of range");
    GP_ASSERT(v.m_index == I, "TVariant: Get<I> with wrong active index");
    return *reinterpret_cast<Detail::TVariantTypeAt_T<I, Ts...>*>(v.m_storage);
}

/// @brief Returns a const reference to the alternative at index I.
/// @tparam I Zero-based index.
/// @tparam Ts Alternative types.
/// @param v Const variant reference.
/// @return Const reference to the alternative at I.
/// @warning Asserts that I matches the active index.
template <SizeT I, typename... Ts>
GP_NODISCARD constexpr const auto& Get(const TVariant<Ts...>& v) noexcept
{
    static_assert(I < TVariant<Ts...>::kSize, "Get<I>: index out of range");
    GP_ASSERT(v.m_index == I, "TVariant: Get<I> with wrong active index");
    return *reinterpret_cast<const Detail::TVariantTypeAt_T<I, Ts...>*>(v.m_storage);
}

/// @brief Returns an rvalue reference to the alternative at index I, enabling moving it out.
/// @tparam I Zero-based index.
/// @tparam Ts Alternative types.
/// @param v Rvalue variant reference.
/// @return Rvalue reference to the alternative at I.
/// @warning Asserts that I matches the active index.
template <SizeT I, typename... Ts>
GP_NODISCARD constexpr auto&& Get(TVariant<Ts...>&& v) noexcept
{
    static_assert(I < TVariant<Ts...>::kSize, "Get<I>: index out of range");
    GP_ASSERT(v.m_index == I, "TVariant: Get<I> with wrong active index");
    return GP::Move(*reinterpret_cast<Detail::TVariantTypeAt_T<I, Ts...>*>(v.m_storage));
}

/// @brief Returns a mutable reference to alternative T.
/// @tparam T Alternative type. Must appear in @p v.
/// @param v Mutable variant reference.
/// @return Mutable reference to the T alternative.
/// @warning Asserts that T is the active alternative.
template <typename T, typename... Ts>
GP_NODISCARD constexpr T& Get(TVariant<Ts...>& v) noexcept
{
    return Get<TVariant<Ts...>::template kIndexOf<T>>(v);
}

/// @brief Returns a const reference to alternative T.
/// @tparam T Alternative type. Must appear in @p v.
/// @param v Const variant reference.
/// @return Const reference to the T alternative.
template <typename T, typename... Ts>
GP_NODISCARD constexpr const T& Get(const TVariant<Ts...>& v) noexcept
{
    return Get<TVariant<Ts...>::template kIndexOf<T>>(v);
}

/// @brief Moves the T alternative out of @p v.
/// @tparam T Alternative type. Must appear in @p v.
/// @param v Rvalue variant reference.
/// @return Rvalue reference to the T alternative.
template <typename T, typename... Ts>
GP_NODISCARD constexpr T&& Get(TVariant<Ts...>&& v) noexcept
{
    return Get<TVariant<Ts...>::template kIndexOf<T>>(GP::Move(v));
}

/// @brief Returns a pointer to alternative I if it is active, otherwise nullptr.
/// @tparam I Zero-based index.
/// @tparam Ts Alternative types.
/// @param v Pointer to a mutable variant. May be null.
/// @return Pointer to the alternative, or nullptr.
template <SizeT I, typename... Ts>
GP_NODISCARD constexpr auto* TryGet(TVariant<Ts...>* v) noexcept
{
    using T = Detail::TVariantTypeAt_T<I, Ts...>;
    if (v && v->m_index == I) { return reinterpret_cast<T*>(v->m_storage); }
    return static_cast<T*>(nullptr);
}

/// @brief Returns a const pointer to alternative I if it is active, otherwise nullptr.
/// @tparam I Zero-based index.
/// @tparam Ts Alternative types.
/// @param v Pointer to a const variant. May be null.
/// @return Const pointer to the alternative, or nullptr.
template <SizeT I, typename... Ts>
GP_NODISCARD constexpr const auto* TryGet(const TVariant<Ts...>* v) noexcept
{
    using T = Detail::TVariantTypeAt_T<I, Ts...>;
    if (v && v->m_index == I) { return reinterpret_cast<const T*>(v->m_storage); }
    return static_cast<const T*>(nullptr);
}

/// @brief Returns a pointer to alternative T if it is active, otherwise nullptr.
/// @tparam T Alternative type. Must appear in @p v.
/// @param v Pointer to a mutable variant.
/// @return Pointer to T or nullptr.
template <typename T, typename... Ts>
GP_NODISCARD constexpr T* TryGet(TVariant<Ts...>* v) noexcept
{
    return TryGet<TVariant<Ts...>::template kIndexOf<T>>(v);
}

/// @brief Returns a const pointer to alternative T if it is active, otherwise nullptr.
/// @tparam T Alternative type. Must appear in @p v.
/// @param v Pointer to a const variant.
/// @return Const pointer to T or nullptr.
template <typename T, typename... Ts>
GP_NODISCARD constexpr const T* TryGet(const TVariant<Ts...>* v) noexcept
{
    return TryGet<TVariant<Ts...>::template kIndexOf<T>>(v);
}

/// @brief Applies @p visitor to the active alternative of @p variant (mutable overload).
/// @tparam TVisitor Callable type. Must accept every alternative type.
/// @tparam Ts Alternative types.
/// @param visitor Callable to invoke with the active value.
/// @param variant Mutable TVariant reference.
/// @return The return value of the visitor invocation.
/// @warning Asserts that the variant is not valueless.
template <typename TVisitor, typename... Ts>
decltype(auto) Visit(TVisitor&& visitor, TVariant<Ts...>& variant)
{
    using FirstAlt = Detail::TVariantTypeAt_T<0, Ts...>;
    using RetT = decltype(GP::Declval<TVisitor&&>()(GP::Declval<FirstAlt&>()));
    GP_ASSERT(!variant.IsValueless(), "TVariant: Visit on a valueless variant");
    return Detail::TVariantVisit<0, RetT, TVisitor, Ts...>::Apply(
        variant.m_index, variant.m_storage, GP::Forward<TVisitor>(visitor)
    );
}

/// @brief Applies @p visitor to the active alternative of @p variant (const overload).
/// @tparam TVisitor Callable type. Must accept const references to every alternative type.
/// @tparam Ts Alternative types.
/// @param visitor Callable to invoke with the active value.
/// @param variant Const TVariant reference.
/// @return The return value of the visitor invocation.
/// @warning Asserts that the variant is not valueless.
template <typename TVisitor, typename... Ts>
decltype(auto) Visit(TVisitor&& visitor, const TVariant<Ts...>& variant)
{
    using FirstAlt = Detail::TVariantTypeAt_T<0, Ts...>;
    using RetT = decltype(GP::Declval<TVisitor&&>()(GP::Declval<const FirstAlt&>()));
    GP_ASSERT(!variant.IsValueless(), "TVariant: Visit on a valueless variant");
    return Detail::TVariantVisit<0, RetT, TVisitor, Ts...>::ApplyConst(
        variant.m_index, variant.m_storage, GP::Forward<TVisitor>(visitor)
    );
}

/// @brief Applies @p visitor to the active alternative of @p variant (rvalue overload).
///        The active alternative is moved into the visitor.
/// @tparam TVisitor Callable type. Must accept rvalue references to every alternative type.
/// @tparam Ts Alternative types.
/// @param visitor Callable to invoke with the active value.
/// @param variant Rvalue TVariant reference.
/// @return The return value of the visitor invocation.
/// @warning Asserts that the variant is not valueless.
template <typename TVisitor, typename... Ts>
decltype(auto) Visit(TVisitor&& visitor, TVariant<Ts...>&& variant)
{
    using FirstAlt = Detail::TVariantTypeAt_T<0, Ts...>;
    using RetT = decltype(GP::Declval<TVisitor&&>()(GP::Declval<FirstAlt>()));
    GP_ASSERT(!variant.IsValueless(), "TVariant: Visit on a valueless variant");
    return Detail::TVariantVisit<0, RetT, TVisitor, Ts...>::Apply(
        variant.m_index, variant.m_storage, GP::Forward<TVisitor>(visitor)
    );
}

// Restore warnings about unreachable code in visitor dispatch.
#ifdef GP_COMPILER_MSVC
    #pragma warning(pop)
#elif GP_COMPILER_CLANG || GP_COMPILER_GCC
    #pragma GCC diagnostic pop
#endif

}   // namespace GP
