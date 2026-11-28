// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "templates/Delegate.hpp"
#include "templates/Optional.hpp"
#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

namespace gp
{

template <typename InSignature>
class FunctionRef;

template <typename InSignature, gp::USize InInlineStorageSize = 32u>
class UniqueFunction;

template <typename InSignature, gp::USize InInlineStorageSize = 32u>
class Function;

namespace detail
{

/// @brief Lifecycle operation codes dispatched by `UniqueFunction` and `Function` storage handlers.
enum class FunctionStorageOp : gp::UInt8
{
    Copy = 0u,
    Move = 1u,
    Destroy = 2u
};

/// @brief Wrapper storing a runtime function pointer as a callable object inside inline storage.
/// @tparam InFuncPtrT Function pointer type (`InRet (*)(InArgs...)`).
template <typename InFuncPtrT>
struct FunctionPointerBinding final
{
    InFuncPtrT func{ nullptr };
};

/// @brief Trait detecting whether `InT` is an instantiation of `gp::FunctionRef`.
template <typename InT>
struct IsFunctionRef final : std::false_type
{};

template <typename InSignature>
struct IsFunctionRef<FunctionRef<InSignature>> final : std::true_type
{};

/// @brief Trait detecting whether `InT` is an instantiation of `gp::UniqueFunction`.
template <typename InT>
struct IsUniqueFunction final : std::false_type
{};

template <typename InSignature, gp::USize InInlineSize>
struct IsUniqueFunction<UniqueFunction<InSignature, InInlineSize>> final : std::true_type
{};

template <typename InRet, typename... InArgs, gp::USize InInlineSize>
struct IsUniqueFunction<UniqueFunction<InRet(InArgs...), InInlineSize>> final : std::true_type
{};

/// @brief Trait detecting whether `InT` is an instantiation of `gp::Function`.
template <typename InT>
struct IsFunction final : std::false_type
{};

template <typename InSignature, gp::USize InInlineSize>
struct IsFunction<Function<InSignature, InInlineSize>> final : std::true_type
{};

template <typename InRet, typename... InArgs, gp::USize InInlineSize>
struct IsFunction<Function<InRet(InArgs...), InInlineSize>> final : std::true_type
{};

/// @brief Shared SBO + heap-spillover polymorphic callable storage core modeled on `UE::Core::Private::Function`.
/// @details Deconstructive Architecture Notes:
///          1. **Zero-Branch Hot-Path Dispatch:** `m_callablePtr` always caches the active payload address (`m_storage`
///             when stored inline, or the allocated heap block when spilled). Invoking `operator()` or `execute()`
///             loads `m_invoke` and `m_callablePtr` directly with zero inline/heap branching on the execution path.
///          2. **Compact Footprint (Default 32B SBO):** By replacing a boolean heap flag with `m_callablePtr` and
///             eliminating C++ virtual tables (`vptr` inside every heap block), `FunctionStorageCore<..., 32>` stores
///             only `32 B` inline storage + `3` pointers (`56 B` unpadded: `56 B` on Win64 where
///             `alignof(std::max_align_t) == 8`, and `64 B` — one cache line — on System V x64/ARM64 where
///             `alignof(std::max_align_t) == 16`). `isHeapAllocated()` is computed via pointer comparison
///             `m_callablePtr != m_storage`.
///          3. **Trivial Inline Fast-Path:** Trivially copyable and trivially destructible inline closures set
///             `m_ops = nullptr`, allowing copy and move operations to execute a single fixed-size `std::memcpy` with
///             zero indirect lifecycle call overhead.
///          4. **O(1) Cross-Type Conversion:** Moving a `gp::Function<Sig, N>` into a `gp::UniqueFunction<Sig, N>`
///             steals the underlying SBO buffer or heap pointer directly (`adoptMovedFrom`) instead of wrapping a
///             48-byte `Function` inside another `UniqueFunction` (avoiding double-indirection and secondary heap
///             spillover).
/// @tparam InIsUnique When `true`, allows move-only closures (`UniqueFunction`); when `false`, enforces copyability.
/// @tparam InInlineStorageSize Embedded inline buffer size in bytes (must be `>= sizeof(void*)`).
/// @tparam InRet Return type of the callable signature.
/// @tparam InArgs Parameter types of the callable signature.
template <bool InIsUnique, gp::USize InInlineStorageSize, typename InRet, typename... InArgs>
class FunctionStorageCore
{
    static_assert(
        InInlineStorageSize >= sizeof(void*),
        "FunctionStorageCore inline storage size must be at least sizeof(void*) to hold a pointer."
    );

    template <bool OtherIsUnique, gp::USize OtherInlineSize, typename OtherRet, typename... OtherArgs>
    friend class FunctionStorageCore;

public:
    /// @brief Inline storage capacity in bytes reserved inside each instance.
    static constexpr gp::USize kInlineStorageSize = InInlineStorageSize;

    /// @brief Alignment of the inline storage buffer.
    static constexpr gp::USize kInlineStorageAlignment = alignof(std::max_align_t);

protected:
    using ErasureCore = FunctionStorageCore<true, InInlineStorageSize, InRet, InArgs...>;
    using InvokeFn = InRet (*)(void*, InArgs...);
    using OpsFn = void (*)(FunctionStorageOp, ErasureCore*, const ErasureCore*);

public:
    /// @brief Default-constructs an unbound storage core.
    constexpr FunctionStorageCore() noexcept = default;

    /// @brief Copy constructor (enabled only when `InIsUnique == false`).
    /// @param[in] other Source storage core to copy from.
    FunctionStorageCore(const FunctionStorageCore& other) noexcept requires(!InIsUnique)
    {
        copyFrom(other);
    }

    /// @brief Deleted copy constructor for move-only `UniqueFunction` storage.
    FunctionStorageCore(const FunctionStorageCore&) requires(InIsUnique) = delete;

    /// @brief Move constructor. Transfers inline or heap ownership and leaves @p other unbound.
    /// @param[in,out] other Source storage core to move from.
    FunctionStorageCore(FunctionStorageCore&& other) noexcept
    {
        moveFrom(std::move(other));
    }

    /// @brief Destructor. Destroys any bound callable and releases any heap allocation.
    ~FunctionStorageCore() noexcept
    {
        unbind();
    }

    /// @brief Copy assignment operator (enabled only when `InIsUnique == false`).
    /// @param[in] other Source storage core to copy from.
    /// @return Reference to `*this`.
    FunctionStorageCore& operator=(const FunctionStorageCore& other) noexcept requires(!InIsUnique)
    {
        if (this != &other)
        {
            unbind();
            copyFrom(other);
        }
        return *this;
    }

    /// @brief Deleted copy assignment operator for move-only `UniqueFunction` storage.
    FunctionStorageCore& operator=(const FunctionStorageCore&) requires(InIsUnique) = delete;

    /// @brief Move assignment operator.
    /// @param[in,out] other Source storage core to move from.
    /// @return Reference to `*this`.
    FunctionStorageCore& operator=(FunctionStorageCore&& other) noexcept
    {
        if (this != &other)
        {
            unbind();
            moveFrom(std::move(other));
        }
        return *this;
    }

public:
    /// @brief Reports whether a callable is currently bound.
    /// @return `true` if bound, `false` otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isBound() const noexcept
    {
        return m_invoke != nullptr;
    }

    /// @brief Reports whether the bound callable is currently stored in a heap block rather than the inline SBO buffer.
    /// @details Determined in $O(1)$ without storing an extra boolean field by comparing `m_callablePtr` against the
    ///          address of `m_storage`, keeping the unpadded layout to `kInlineStorageSize + 3 * sizeof(void*)` bytes.
    /// @return `true` if spilled to `gp::memory::HeapAllocator`, `false` if inline or unbound.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isHeapAllocated() const noexcept
    {
        return (m_callablePtr != nullptr) && (m_callablePtr != static_cast<const void*>(m_storage));
    }

    /// @brief Destroys any bound callable, frees any heap allocation, and resets to the unbound state.
    void unbind() noexcept
    {
        if (m_ops != nullptr)
        {
            m_ops(FunctionStorageOp::Destroy, asErasureCore(), nullptr);
        }
        m_callablePtr = nullptr;
        m_invoke = nullptr;
        m_ops = nullptr;
    }

protected:
    /// @brief Reinterprets `this` as the canonical `ErasureCore` layout so `Function` and `UniqueFunction` share
    ///        identical `OpsFn` signatures and can transfer ownership in $O(1)$ without thunk adapters.
    /// @return Pointer to the canonical erasure core representation.
    [[nodiscard]] GP_FORCEINLINE_HINT ErasureCore* asErasureCore() noexcept
    {
        return reinterpret_cast<ErasureCore*>(this);
    }

    /// @brief Reinterprets `this` as a const `ErasureCore` pointer.
    /// @return Const pointer to the canonical erasure core representation.
    [[nodiscard]] GP_FORCEINLINE_HINT const ErasureCore* asErasureCore() const noexcept
    {
        return reinterpret_cast<const ErasureCore*>(this);
    }

    /// @brief Binds a compile-time function pointer with zero payload storage overhead.
    /// @tparam InFuncPtr Compile-time function pointer matching `InRet(InArgs...)`.
    template <auto InFuncPtr>
    void bindStatic() noexcept
    {
        static_assert(
            std::is_invocable_r_v<InRet, decltype(InFuncPtr), InArgs...>,
            "Compile-time function pointer signature does not match gp::Function."
        );
        unbind();
        m_callablePtr = static_cast<void*>(m_storage);
        m_invoke = []([[maybe_unused]] void* const objectPtr, InArgs... args) -> InRet
        {
            if constexpr (std::is_void_v<InRet>)
            {
                InFuncPtr(std::forward<InArgs>(args)...);
            }
            else
            {
                return InFuncPtr(std::forward<InArgs>(args)...);
            }
        };
        m_ops = nullptr;
    }

    /// @brief Binds a runtime function pointer (`nullptr` leaves the wrapper unbound).
    /// @param[in] funcPtr Runtime function pointer to bind.
    void bindStatic(InRet (*const funcPtr)(InArgs...)) noexcept
    {
        unbind();
        if (funcPtr == nullptr)
        {
            return;
        }

        using BindingT = FunctionPointerBinding<InRet (*)(InArgs...)>;
        std::construct_at(reinterpret_cast<BindingT*>(m_storage), BindingT{ funcPtr });
        m_callablePtr = static_cast<void*>(m_storage);
        m_invoke = [](void* const objectPtr, InArgs... args) -> InRet
        {
            const BindingT* const binding = static_cast<const BindingT*>(objectPtr);
            if constexpr (std::is_void_v<InRet>)
            {
                binding->func(std::forward<InArgs>(args)...);
            }
            else
            {
                return binding->func(std::forward<InArgs>(args)...);
            }
        };
        m_ops = nullptr;
    }

    /// @brief Binds a member function on a raw object pointer (`nullptr` leaves the wrapper unbound).
    /// @tparam InMethodPtr Compile-time pointer-to-member-function (`&Class::method`).
    /// @tparam InObjectT Target class type (const or non-const).
    /// @param[in] object Non-owning pointer to the target instance.
    template <auto InMethodPtr, typename InObjectT>
    void bindRaw(InObjectT* const object) noexcept
    {
        static_assert(
            std::is_invocable_r_v<InRet, decltype(InMethodPtr), InObjectT*, InArgs...>,
            "Incompatible member function signature for target object type."
        );
        unbind();
        if (object == nullptr)
        {
            return;
        }

        std::construct_at(reinterpret_cast<InObjectT**>(m_storage), object);
        m_callablePtr = static_cast<void*>(m_storage);
        m_invoke = [](void* const objectPtr, InArgs... args) -> InRet
        {
            InObjectT* const target = *static_cast<InObjectT* const*>(objectPtr);
            if constexpr (std::is_void_v<InRet>)
            {
                (target->*InMethodPtr)(std::forward<InArgs>(args)...);
            }
            else
            {
                return (target->*InMethodPtr)(std::forward<InArgs>(args)...);
            }
        };
        m_ops = nullptr;
    }

    /// @brief Binds a callable object or lambda, placing it in inline SBO storage if it fits or allocating from
    ///        `gp::memory::HeapAllocator` when larger or over-aligned.
    /// @tparam InCallableT Forwarded callable type.
    /// @param[in] callable Callable object to store.
    template <typename InCallableT>
    void bindCallable(
        InCallableT&& callable
    ) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT>)
    {
        using DecayedT = std::remove_cvref_t<InCallableT>;
        static_assert(
            std::is_invocable_r_v<InRet, DecayedT&, InArgs...>,
            "Callable signature is not invocable with the target gp::Function parameters and return type."
        );
        static_assert(
            std::is_move_constructible_v<DecayedT>,
            "Callable stored in gp::Function or gp::UniqueFunction must be move-constructible."
        );
        if constexpr (!InIsUnique)
        {
            static_assert(
                std::is_copy_constructible_v<DecayedT>,
                "Callable stored in copyable gp::Function must be copy-constructible; use gp::UniqueFunction for " "move-only captures."
            );
        }

        // Dynamic function pointer passed through bindLambda / bindCallable: route directly to bindStatic.
        if constexpr (std::is_pointer_v<DecayedT> && std::is_function_v<std::remove_pointer_t<DecayedT>>)
        {
            bindStatic(callable);
            return;
        }
        else if constexpr (std::is_same_v<DecayedT, Delegate<InRet(InArgs...)>>)
        {
            unbind();
            if (!callable.isBound())
            {
                return;
            }
        }
        else
        {
            unbind();
        }

        constexpr bool kFitsInline =
            (sizeof(DecayedT) <= kInlineStorageSize) && (alignof(DecayedT) <= kInlineStorageAlignment);

        if constexpr (kFitsInline)
        {
            std::construct_at(reinterpret_cast<DecayedT*>(m_storage), std::forward<InCallableT>(callable));
            m_callablePtr = static_cast<void*>(m_storage);

            if constexpr (std::is_trivially_copyable_v<DecayedT> && std::is_trivially_destructible_v<DecayedT>)
            {
                m_ops = nullptr;
            }
            else
            {
                m_ops = [](const FunctionStorageOp op,
                           ErasureCore* const dst,
                           const ErasureCore* const src) noexcept -> void
                {
                    DecayedT* const dstObj = reinterpret_cast<DecayedT*>(dst->m_storage);
                    switch (op)
                    {
                    case FunctionStorageOp::Copy:
                        if constexpr (!InIsUnique)
                        {
                            const DecayedT* const srcObj = reinterpret_cast<const DecayedT*>(src->m_storage);
                            std::construct_at(dstObj, *srcObj);
                            dst->m_callablePtr = static_cast<void*>(dst->m_storage);
                        }
                        break;
                    case FunctionStorageOp::Move:
                    {
                        DecayedT* const srcObj = reinterpret_cast<DecayedT*>(const_cast<gp::Byte*>(src->m_storage));
                        std::construct_at(dstObj, std::move(*srcObj));
                        std::destroy_at(srcObj);
                        dst->m_callablePtr = static_cast<void*>(dst->m_storage);
                        break;
                    }
                    case FunctionStorageOp::Destroy:
                        std::destroy_at(dstObj);
                        break;
                    default:
                        break;
                    }
                };
            }
        }
        else
        {
            gp::memory::HeapAllocator allocator{};
            DecayedT* const heapObj = allocator.allocateElements<DecayedT>(1u);
            GP_ASSERT(heapObj != nullptr, "gp::Function failed to allocate heap storage for large callable.");
            std::construct_at(heapObj, std::forward<InCallableT>(callable));
            m_callablePtr = static_cast<void*>(heapObj);

            m_ops =
                [](const FunctionStorageOp op, ErasureCore* const dst, const ErasureCore* const src) noexcept -> void
            {
                gp::memory::HeapAllocator heapAlloc{};
                switch (op)
                {
                case FunctionStorageOp::Copy:
                    if constexpr (!InIsUnique)
                    {
                        const DecayedT* const srcObj = static_cast<const DecayedT*>(src->m_callablePtr);
                        DecayedT* const clonedObj = heapAlloc.allocateElements<DecayedT>(1u);
                        GP_ASSERT(
                            clonedObj != nullptr, "gp::Function failed to allocate heap storage during callable copy."
                        );
                        std::construct_at(clonedObj, *srcObj);
                        dst->m_callablePtr = static_cast<void*>(clonedObj);
                    }
                    break;
                case FunctionStorageOp::Move:
                {
                    // O(1) pointer transfer for heap-allocated closures: zero element reallocation or move constructor.
                    dst->m_callablePtr = src->m_callablePtr;
                    break;
                }
                case FunctionStorageOp::Destroy:
                {
                    DecayedT* const targetObj = static_cast<DecayedT*>(dst->m_callablePtr);
                    if (targetObj != nullptr)
                    {
                        std::destroy_at(targetObj);
                        heapAlloc.deallocateElements<DecayedT>(targetObj, 1u);
                    }
                    dst->m_callablePtr = nullptr;
                    break;
                }
                default:
                    break;
                }
            };
        }

        m_invoke = [](void* const objectPtr, InArgs... args) -> InRet
        {
            DecayedT* const target = static_cast<DecayedT*>(objectPtr);
            if constexpr (std::is_void_v<InRet>)
            {
                (*target)(std::forward<InArgs>(args)...);
            }
            else
            {
                return (*target)(std::forward<InArgs>(args)...);
            }
        };
    }

    /// @brief Copies state from another `FunctionStorageCore` (only callable when `InIsUnique == false`).
    /// @param[in] other Source storage core.
    void copyFrom(const FunctionStorageCore& other) noexcept requires(!InIsUnique)
    {
        m_invoke = other.m_invoke;
        m_ops = other.m_ops;
        if (other.m_invoke == nullptr)
        {
            m_callablePtr = nullptr;
            return;
        }

        if (other.m_ops != nullptr)
        {
            other.m_ops(FunctionStorageOp::Copy, asErasureCore(), other.asErasureCore());
        }
        else
        {
            std::memcpy(m_storage, other.m_storage, kInlineStorageSize);
            m_callablePtr = static_cast<void*>(m_storage);
        }
    }

    /// @brief Moves state from another `FunctionStorageCore` (including converting `Function` -> `UniqueFunction` with
    ///        matching `InInlineStorageSize`) and leaves @p other unbound.
    /// @tparam OtherIsUnique Source uniqueness flag (`false` or `InIsUnique`).
    /// @param[in,out] other Source storage core.
    template <bool OtherIsUnique>
    void moveFrom(FunctionStorageCore<OtherIsUnique, InInlineStorageSize, InRet, InArgs...>&& other) noexcept
    {
        m_invoke = other.m_invoke;
        m_ops = other.m_ops;
        if (other.m_invoke == nullptr)
        {
            m_callablePtr = nullptr;
            return;
        }

        if (other.m_ops != nullptr)
        {
            other.m_ops(FunctionStorageOp::Move, asErasureCore(), other.asErasureCore());
        }
        else
        {
            std::memcpy(m_storage, other.m_storage, kInlineStorageSize);
            m_callablePtr = static_cast<void*>(m_storage);
        }

        other.m_callablePtr = nullptr;
        other.m_invoke = nullptr;
        other.m_ops = nullptr;
    }

    /// @brief Swaps two `FunctionStorageCore` instances cleanly.
    /// @param[in,out] other Storage core to exchange with.
    void swapStorage(FunctionStorageCore& other) noexcept
    {
        if (this == &other)
        {
            return;
        }

        FunctionStorageCore temp(std::move(*this));
        *this = std::move(other);
        other = std::move(temp);
    }

protected:
    alignas(kInlineStorageAlignment) mutable gp::Byte m_storage[kInlineStorageSize]{};
    void* m_callablePtr{ nullptr };
    InvokeFn m_invoke{ nullptr };
    OpsFn m_ops{ nullptr };
};

}   // namespace detail

/// @brief Non-owning, 16-byte lightweight callable reference modeled after `UE::TFunctionRef<InRet(InArgs...)>`.
/// @details `FunctionRef` never allocates memory and never copies the referenced callable. It stores a non-null pointer
///          to an external callable alongside a static thunk function pointer.
/// @note Because `FunctionRef` does not own its target, it must only be used for synchronous function parameters (such
///       as visitors, predicates, and `forEach` callbacks) and must never be stored past the lifetime of the bound
///       expression.
/// @tparam InRet Return type of the callable signature.
/// @tparam InArgs Parameter types of the callable signature.
template <typename InRet, typename... InArgs>
class FunctionRef<InRet(InArgs...)> final
{
private:
    using InvokeFn = InRet (*)(void*, InArgs...);

public:
    /// @brief Default construction is deleted because `FunctionRef` is a non-nullable reference wrapper (like
    ///        `UE::TFunctionRef`).
    FunctionRef() = delete;
    FunctionRef(gp::NullPtrT) = delete;

    /// @brief Constructs a `FunctionRef` referencing a callable object, lambda, or functor.
    /// @note If @p callable is an existing `gp::Delegate`, `gp::Function`, or `gp::UniqueFunction`, asserts via
    ///       `GP_ASSERT` that the wrapper is currently bound (`isBound() == true`).
    /// @tparam InCallableT Invocable object type (lvalue or temporary living for the full call expression).
    /// @param[in] callable Target callable referenced by this view.
    template <typename InCallableT>
    requires(!std::is_same_v<std::remove_cvref_t<InCallableT>, FunctionRef>) &&
                (!std::is_same_v<std::remove_cvref_t<InCallableT>, gp::NullPtrT>) &&
                (!std::is_pointer_v<std::decay_t<InCallableT>>) &&
                (!std::is_function_v<std::remove_cvref_t<InCallableT>>) &&
                std::is_invocable_r_v<InRet, std::remove_reference_t<InCallableT>&, InArgs...>
    [[nodiscard]] constexpr explicit(false) FunctionRef(InCallableT&& callable) noexcept
        : m_callablePtr(const_cast<void*>(static_cast<const void*>(std::addressof(callable))))
        , m_invoke(
              [](void* const objectPtr, InArgs... args) -> InRet
    {
        using TargetT = std::remove_reference_t<InCallableT>;
        TargetT* const target = static_cast<TargetT*>(objectPtr);
        if constexpr (std::is_void_v<InRet>)
        {
            (*target)(std::forward<InArgs>(args)...);
        }
        else
        {
            return (*target)(std::forward<InArgs>(args)...);
        }
    }
          )
    {
        using DecayedT = std::remove_cvref_t<InCallableT>;
        if constexpr (
            detail::IsFunction<DecayedT>::value || detail::IsUniqueFunction<DecayedT>::value ||
            std::is_same_v<DecayedT, Delegate<InRet(InArgs...)>>
        )
        {
            GP_ASSERT(
                callable.isBound(),
                "gp::FunctionRef cannot bind to an unbound gp::Function, gp::UniqueFunction, or gp::Delegate."
            );
        }
    }

    /// @brief Constructs a `FunctionRef` from a non-null free or static function pointer.
    /// @note Triggers `GP_ASSERT` if @p funcPtr is `nullptr`.
    /// @param[in] funcPtr Non-null function pointer matching `InRet(InArgs...)`.
    [[nodiscard]] explicit(false) FunctionRef(InRet (*const funcPtr)(InArgs...)) noexcept
        : m_callablePtr(reinterpret_cast<void*>(funcPtr))
        , m_invoke(
              [](void* const objectPtr, InArgs... args) -> InRet
    {
        using FuncPtrT = InRet (*)(InArgs...);
        const FuncPtrT fn = reinterpret_cast<FuncPtrT>(objectPtr);
        if constexpr (std::is_void_v<InRet>)
        {
            fn(std::forward<InArgs>(args)...);
        }
        else
        {
            return fn(std::forward<InArgs>(args)...);
        }
    }
          )
    {
        GP_ASSERT(funcPtr != nullptr, "gp::FunctionRef cannot be constructed from a null function pointer.");
    }

    /// @brief Trivially copyable and copy-assignable view.
    [[nodiscard]] constexpr FunctionRef(const FunctionRef&) noexcept = default;
    constexpr FunctionRef& operator=(const FunctionRef&) noexcept = default;
    constexpr ~FunctionRef() noexcept = default;

public:
    /// @brief Invokes the referenced callable with @p args.
    /// @param[in] args Arguments forwarded to the callable.
    /// @return Return value produced by the callable.
    InRet operator()(InArgs... args) const
    {
        GP_ASSERT(m_invoke != nullptr && m_callablePtr != nullptr, "Invalid gp::FunctionRef invocation.");
        return m_invoke(m_callablePtr, std::forward<InArgs>(args)...);
    }

    /// @brief Named alias for `operator()` matching `gp::Delegate::execute()`.
    /// @param[in] args Arguments forwarded to the callable.
    /// @return Return value produced by the callable.
    InRet execute(InArgs... args) const
    {
        return (*this)(std::forward<InArgs>(args)...);
    }

private:
    void* m_callablePtr{ nullptr };
    InvokeFn m_invoke{ nullptr };
};

/// @brief Move-only owning polymorphic callable wrapper modeled after `UE::TUniqueFunction<InRet(InArgs...)>`.
/// @details Supports move-only captures (such as `gp::UniquePtr<T>`) alongside regular lambdas, functors, member
///          bindings, and function pointers. Closures up to `InInlineStorageSize` bytes (default 32 bytes) live inside
///          the inline SBO buffer with zero heap allocations; larger closures spill over to
///          `gp::memory::HeapAllocator`.
/// @tparam InRet Return type of the callable signature.
/// @tparam InArgs Parameter types forwarded when invoking the function.
/// @tparam InInlineStorageSize Inline SBO buffer size in bytes (defaults to `32`).
template <typename InRet, typename... InArgs, gp::USize InInlineStorageSize>
class UniqueFunction<InRet(InArgs...), InInlineStorageSize> final
    : private detail::FunctionStorageCore<true, InInlineStorageSize, InRet, InArgs...>
{
private:
    using Base = detail::FunctionStorageCore<true, InInlineStorageSize, InRet, InArgs...>;

public:
    using Base::isBound;
    using Base::isHeapAllocated;
    using Base::kInlineStorageAlignment;
    using Base::kInlineStorageSize;
    using Base::unbind;

public:
    /// @brief Default-constructs an unbound `UniqueFunction` (`isBound() == false`).
    [[nodiscard]] constexpr UniqueFunction() noexcept = default;

    /// @brief Constructs an unbound `UniqueFunction` from `nullptr`.
    /// @param[in] nullValue Null pointer literal.
    [[nodiscard]] constexpr explicit(false) UniqueFunction([[maybe_unused]] gp::NullPtrT nullValue) noexcept
    {}

    /// @brief Constructs a `UniqueFunction` from a free or static function pointer (`nullptr` yields unbound).
    /// @param[in] funcPtr Function pointer to bind.
    [[nodiscard]] explicit(false) UniqueFunction(InRet (*const funcPtr)(InArgs...)) noexcept
    {
        this->bindStatic(funcPtr);
    }

    /// @brief Constructs a `UniqueFunction` from a `gp::Delegate` with matching signature.
    /// @param[in] delegate Source delegate (`!delegate.isBound()` produces an unbound `UniqueFunction`).
    [[nodiscard]] explicit(false) UniqueFunction(const Delegate<InRet(InArgs...)>& delegate) noexcept
    {
        if (delegate.isBound())
        {
            this->bindCallable(delegate);
        }
    }

    /// @brief Constructs a `UniqueFunction` by moving a `gp::Delegate` with matching signature.
    /// @param[in,out] delegate Source delegate (`!delegate.isBound()` produces an unbound `UniqueFunction`).
    [[nodiscard]] explicit(false) UniqueFunction(Delegate<InRet(InArgs...)>&& delegate) noexcept
    {
        if (delegate.isBound())
        {
            this->bindCallable(std::move(delegate));
        }
    }

    /// @brief Constructs a `UniqueFunction` by moving from a copyable `gp::Function` of matching signature and inline
    ///        capacity in $O(1)$ without double-wrapping or extra heap allocation.
    /// @param[in,out] func Source `gp::Function` to move from; left unbound after transfer.
    [[nodiscard]] explicit(false) UniqueFunction(Function<InRet(InArgs...), InInlineStorageSize>&& func) noexcept;

    /// @brief Constructs a `UniqueFunction` from any move-constructible callable object or lambda (including move-only
    ///        captures such as `gp::UniquePtr`).
    /// @tparam InCallableT Invocable callable type.
    /// @param[in] callable Callable instance to move or copy into storage.
    template <typename InCallableT>
    requires(!detail::IsUniqueFunction<std::remove_cvref_t<InCallableT>>::value) &&
            (!std::is_same_v<std::remove_cvref_t<InCallableT>, Function<InRet(InArgs...), InInlineStorageSize>>) &&
            (!std::is_same_v<std::remove_cvref_t<InCallableT>, Delegate<InRet(InArgs...)>>) &&
            (!detail::IsFunctionRef<std::remove_cvref_t<InCallableT>>::value) &&
            (!std::is_same_v<std::remove_cvref_t<InCallableT>, gp::NullPtrT>) &&
            (!std::is_pointer_v<std::decay_t<InCallableT>>) &&
            std::is_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT> &&
            std::is_move_constructible_v<std::remove_cvref_t<InCallableT>> &&
            std::is_invocable_r_v<InRet, std::remove_cvref_t<InCallableT>&, InArgs...>
    [[nodiscard]] explicit(false) UniqueFunction(
        InCallableT&& callable
    ) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT>)
    {
        this->bindCallable(std::forward<InCallableT>(callable));
    }

    UniqueFunction(const UniqueFunction&) = delete;
    UniqueFunction& operator=(const UniqueFunction&) = delete;

    /// @brief Move constructor. Transfers inline or heap callable and leaves @p other unbound.
    /// @param[in,out] other Source `UniqueFunction` to move from.
    [[nodiscard]] UniqueFunction(UniqueFunction&& other) noexcept = default;

    /// @brief Destructor.
    ~UniqueFunction() noexcept = default;

    /// @brief Move assignment operator.
    /// @param[in,out] other Source `UniqueFunction` to move from.
    /// @return Reference to `*this`.
    UniqueFunction& operator=(UniqueFunction&& other) noexcept = default;

    /// @brief Move-assigns from a copyable `gp::Function` of matching signature and inline capacity in $O(1)$.
    /// @param[in,out] func Source `gp::Function` to move from; left unbound after transfer.
    /// @return Reference to `*this`.
    UniqueFunction& operator=(Function<InRet(InArgs...), InInlineStorageSize>&& func) noexcept;

    /// @brief Unbinds this `UniqueFunction` when assigned `nullptr`.
    /// @param[in] nullValue Null pointer literal.
    /// @return Reference to `*this`.
    UniqueFunction& operator=([[maybe_unused]] gp::NullPtrT nullValue) noexcept
    {
        this->unbind();
        return *this;
    }

    /// @brief Rebinds this `UniqueFunction` to a new callable object or lambda.
    /// @tparam InCallableT Invocable callable type.
    /// @param[in] callable Callable instance to bind.
    /// @return Reference to `*this`.
    template <typename InCallableT>
    requires(!detail::IsUniqueFunction<std::remove_cvref_t<InCallableT>>::value) &&
            (!std::is_same_v<std::remove_cvref_t<InCallableT>, Function<InRet(InArgs...), InInlineStorageSize>>) &&
            (!detail::IsFunctionRef<std::remove_cvref_t<InCallableT>>::value) &&
            (!std::is_same_v<std::remove_cvref_t<InCallableT>, gp::NullPtrT>) &&
            std::is_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT> &&
            std::is_move_constructible_v<std::remove_cvref_t<InCallableT>> &&
            std::is_invocable_r_v<InRet, std::remove_cvref_t<InCallableT>&, InArgs...>
    UniqueFunction& operator=(
        InCallableT&& callable
    ) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT>)
    {
        if constexpr (std::is_same_v<std::remove_cvref_t<InCallableT>, Delegate<InRet(InArgs...)>>)
        {
            this->unbind();
            if (callable.isBound())
            {
                this->bindCallable(std::forward<InCallableT>(callable));
            }
        }
        else if constexpr (std::is_pointer_v<std::decay_t<InCallableT>>)
        {
            this->bindStatic(callable);
        }
        else
        {
            this->bindCallable(std::forward<InCallableT>(callable));
        }
        return *this;
    }

public:
    /// @brief Creates a `UniqueFunction` bound to a compile-time free or static function pointer.
    /// @tparam InFuncPtr Compile-time function pointer matching `InRet(InArgs...)`.
    /// @return Bound `UniqueFunction`.
    template <auto InFuncPtr>
    [[nodiscard]] static UniqueFunction createStatic() noexcept
    {
        UniqueFunction result;
        result.template bindStatic<InFuncPtr>();
        return result;
    }

    /// @brief Creates a `UniqueFunction` bound to a runtime free or static function pointer.
    /// @param[in] funcPtr Function pointer to invoke (`nullptr` produces an unbound `UniqueFunction`).
    /// @return `UniqueFunction`.
    [[nodiscard]] static UniqueFunction createStatic(InRet (*const funcPtr)(InArgs...)) noexcept
    {
        UniqueFunction result;
        result.bindStatic(funcPtr);
        return result;
    }

    /// @brief Creates a `UniqueFunction` bound to a member function on @p object.
    /// @tparam InMethodPtr Compile-time pointer-to-member-function (`&MyClass::onEvent`).
    /// @tparam InObjectT Class type (may be `const` for const member functions).
    /// @param[in] object Non-owning pointer to the target instance.
    /// @return Bound `UniqueFunction`, or unbound if @p object is `nullptr`.
    template <auto InMethodPtr, typename InObjectT>
    [[nodiscard]] static UniqueFunction createRaw(InObjectT* const object) noexcept
    {
        UniqueFunction result;
        result.template bindRaw<InMethodPtr>(object);
        return result;
    }

    /// @brief Creates a `UniqueFunction` storing a callable or lambda.
    /// @tparam InCallableT Move-constructible callable type.
    /// @param[in] callable Callable object to bind.
    /// @return Bound `UniqueFunction`.
    template <typename InCallableT>
    [[nodiscard]] static UniqueFunction createLambda(
        InCallableT&& callable
    ) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT>)
    {
        UniqueFunction result;
        result.bindLambda(std::forward<InCallableT>(callable));
        return result;
    }

public:
    /// @brief Binds a compile-time free or static function pointer with zero payload storage.
    /// @tparam InFuncPtr Compile-time function pointer matching `InRet(InArgs...)`.
    template <auto InFuncPtr>
    void bindStatic() noexcept
    {
        Base::template bindStatic<InFuncPtr>();
    }

    /// @brief Binds a runtime free or static function pointer (`nullptr` unbinds).
    /// @param[in] funcPtr Function pointer to invoke.
    void bindStatic(InRet (*const funcPtr)(InArgs...)) noexcept
    {
        Base::bindStatic(funcPtr);
    }

    /// @brief Binds a member function on a raw object pointer (`nullptr` unbinds).
    /// @tparam InMethodPtr Compile-time member function pointer (`&Class::method`).
    /// @tparam InObjectT Object type (const or non-const).
    /// @param[in] object Target object pointer.
    template <auto InMethodPtr, typename InObjectT>
    void bindRaw(InObjectT* const object) noexcept
    {
        Base::template bindRaw<InMethodPtr>(object);
    }

    /// @brief Binds a move-constructible callable object or lambda (inline SBO or heap spillover).
    /// @tparam InCallableT Invocable type matching `InRet(InArgs...)`.
    /// @param[in] callable Callable object to store.
    template <typename InCallableT>
    void bindLambda(
        InCallableT&& callable
    ) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT>)
    {
        Base::bindCallable(std::forward<InCallableT>(callable));
    }

    /// @brief Resets this `UniqueFunction` to the unbound state (alias for `unbind()` matching `UE::TUniqueFunction`).
    void reset() noexcept
    {
        this->unbind();
    }

    /// @brief Exchanges the bound state of this `UniqueFunction` with @p other.
    /// @param[in,out] other Other `UniqueFunction` to swap with.
    void swap(UniqueFunction& other) noexcept
    {
        this->swapStorage(other);
    }

    /// @brief Contextual boolean conversion returning `isBound()`.
    /// @return `true` if bound, `false` otherwise.
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return this->isBound();
    }

    /// @brief Invokes the bound callable unconditionally.
    /// @note Triggers `GP_ASSERT` if called on an unbound `UniqueFunction` (matching
    ///       `UE::TUniqueFunction::operator()`).
    /// @param[in] args Arguments forwarded to the target callable.
    /// @return Return value produced by the callable.
    InRet execute(InArgs... args) const
    {
        GP_ASSERT(this->isBound() && "Cannot execute an unbound gp::UniqueFunction.");
        return this->m_invoke(this->m_callablePtr, std::forward<InArgs>(args)...);
    }

    /// @brief Function-call operator forwarding directly to `execute()`.
    /// @param[in] args Arguments forwarded to the target callable.
    /// @return Return value produced by the callable.
    InRet operator()(InArgs... args) const
    {
        return execute(std::forward<InArgs>(args)...);
    }

    /// @brief Invokes the target only if bound (`void` return specialization).
    /// @param[in] args Arguments forwarded to the target callable.
    /// @return `true` if bound and executed, `false` if unbound.
    bool executeIfBound(InArgs... args) const requires std::is_void_v<InRet>
    {
        if (!this->isBound())
        {
            return false;
        }
        this->m_invoke(this->m_callablePtr, std::forward<InArgs>(args)...);
        return true;
    }

    /// @brief Invokes the target only if bound (value-returning specialization).
    /// @param[in] args Arguments forwarded to the target callable.
    /// @return Engaged `gp::Optional<InReturnT>` if bound, or empty `Optional` if unbound.
    template <typename InReturnT = InRet>
    requires(!std::is_void_v<InReturnT> && std::is_same_v<InReturnT, InRet>)
    [[nodiscard]] gp::Optional<InReturnT> executeIfBound(InArgs... args) const
    {
        if (!this->isBound())
        {
            return gp::Optional<InReturnT>{};
        }
        return gp::Optional<InReturnT>(this->m_invoke(this->m_callablePtr, std::forward<InArgs>(args)...));
    }
};

/// @brief Copyable and movable owning polymorphic callable wrapper modeled after `UE::TFunction<InRet(InArgs...)>`.
/// @details Unlike `gp::Delegate` (which rejects closures larger than 32 bytes at compile time), `gp::Function`
///          stores closures up to `InInlineStorageSize` bytes (default 32 bytes) inside its embedded SBO buffer with
///          zero heap allocations and seamlessly spills larger or over-aligned closures to `gp::memory::HeapAllocator`.
///          Requires the stored callable to be copy-constructible; for move-only captures (e.g. `gp::UniquePtr`), use
///          `gp::UniqueFunction`.
/// @tparam InRet Return type of the callable signature.
/// @tparam InArgs Parameter types forwarded when invoking the function.
/// @tparam InInlineStorageSize Inline SBO buffer size in bytes (defaults to `32`).
template <typename InRet, typename... InArgs, gp::USize InInlineStorageSize>
class Function<InRet(InArgs...), InInlineStorageSize> final
    : private detail::FunctionStorageCore<false, InInlineStorageSize, InRet, InArgs...>
{
private:
    using Base = detail::FunctionStorageCore<false, InInlineStorageSize, InRet, InArgs...>;

    template <typename OtherSignature, gp::USize OtherInlineSize>
    friend class UniqueFunction;

public:
    using Base::isBound;
    using Base::isHeapAllocated;
    using Base::kInlineStorageAlignment;
    using Base::kInlineStorageSize;
    using Base::unbind;

public:
    /// @brief Default-constructs an unbound `Function` (`isBound() == false`).
    [[nodiscard]] constexpr Function() noexcept = default;

    /// @brief Constructs an unbound `Function` from `nullptr`.
    /// @param[in] nullValue Null pointer literal.
    [[nodiscard]] constexpr explicit(false) Function([[maybe_unused]] gp::NullPtrT nullValue) noexcept
    {}

    /// @brief Constructs a `Function` from a free or static function pointer (`nullptr` yields unbound).
    /// @param[in] funcPtr Function pointer to bind.
    [[nodiscard]] explicit(false) Function(InRet (*const funcPtr)(InArgs...)) noexcept
    {
        this->bindStatic(funcPtr);
    }

    /// @brief Constructs a `Function` from a `gp::Delegate` with matching signature.
    /// @param[in] delegate Source delegate (`!delegate.isBound()` produces an unbound `Function`).
    [[nodiscard]] explicit(false) Function(const Delegate<InRet(InArgs...)>& delegate) noexcept
    {
        if (delegate.isBound())
        {
            this->bindCallable(delegate);
        }
    }

    /// @brief Constructs a `Function` by moving a `gp::Delegate` with matching signature.
    /// @param[in,out] delegate Source delegate (`!delegate.isBound()` produces an unbound `Function`).
    [[nodiscard]] explicit(false) Function(Delegate<InRet(InArgs...)>&& delegate) noexcept
    {
        if (delegate.isBound())
        {
            this->bindCallable(std::move(delegate));
        }
    }

    /// @brief Deleted conversions from `UniqueFunction` (a copyable `Function` cannot adopt a move-only
    ///        `UniqueFunction`, even via an intermediate `Delegate` conversion).
    template <gp::USize OtherInlineSize>
    Function(const UniqueFunction<InRet(InArgs...), OtherInlineSize>&) = delete;
    template <gp::USize OtherInlineSize>
    Function(UniqueFunction<InRet(InArgs...), OtherInlineSize>&&) = delete;
    template <gp::USize OtherInlineSize>
    Function& operator=(const UniqueFunction<InRet(InArgs...), OtherInlineSize>&) = delete;
    template <gp::USize OtherInlineSize>
    Function& operator=(UniqueFunction<InRet(InArgs...), OtherInlineSize>&&) = delete;

    /// @brief Constructs a `Function` from any copy-constructible callable object or lambda (inline SBO or heap
    ///        spillover).
    /// @tparam InCallableT Invocable copy-constructible callable type.
    /// @param[in] callable Callable instance to copy or move into storage.
    template <typename InCallableT>
    requires(!std::is_same_v<std::remove_cvref_t<InCallableT>, Function>) &&
            (!detail::IsUniqueFunction<std::remove_cvref_t<InCallableT>>::value) &&
            (!std::is_same_v<std::remove_cvref_t<InCallableT>, Delegate<InRet(InArgs...)>>) &&
            (!detail::IsFunctionRef<std::remove_cvref_t<InCallableT>>::value) &&
            (!std::is_same_v<std::remove_cvref_t<InCallableT>, gp::NullPtrT>) &&
            (!std::is_pointer_v<std::decay_t<InCallableT>>) &&
            std::is_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT> &&
            std::is_copy_constructible_v<std::remove_cvref_t<InCallableT>> &&
            std::is_invocable_r_v<InRet, std::remove_cvref_t<InCallableT>&, InArgs...>
    [[nodiscard]] explicit(false) Function(
        InCallableT&& callable
    ) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT>)
    {
        this->bindCallable(std::forward<InCallableT>(callable));
    }

    /// @brief Copy constructor. Copies the inline callable or clones the heap-allocated callable.
    /// @param[in] other Source `Function` to copy from.
    [[nodiscard]] Function(const Function& other) noexcept = default;

    /// @brief Move constructor. Transfers inline or heap callable and leaves @p other unbound.
    /// @param[in,out] other Source `Function` to move from.
    [[nodiscard]] Function(Function&& other) noexcept = default;

    /// @brief Destructor.
    ~Function() noexcept = default;

    /// @brief Copy assignment operator.
    /// @param[in] other Source `Function` to copy from.
    /// @return Reference to `*this`.
    Function& operator=(const Function& other) noexcept = default;

    /// @brief Move assignment operator.
    /// @param[in,out] other Source `Function` to move from.
    /// @return Reference to `*this`.
    Function& operator=(Function&& other) noexcept = default;

    /// @brief Unbinds this `Function` when assigned `nullptr`.
    /// @param[in] nullValue Null pointer literal.
    /// @return Reference to `*this`.
    Function& operator=([[maybe_unused]] gp::NullPtrT nullValue) noexcept
    {
        this->unbind();
        return *this;
    }

    /// @brief Rebinds this `Function` to a new copy-constructible callable object or lambda.
    /// @tparam InCallableT Invocable copy-constructible callable type.
    /// @param[in] callable Callable instance to bind.
    /// @return Reference to `*this`.
    template <typename InCallableT>
    requires(!std::is_same_v<std::remove_cvref_t<InCallableT>, Function>) &&
            (!detail::IsUniqueFunction<std::remove_cvref_t<InCallableT>>::value) &&
            (!detail::IsFunctionRef<std::remove_cvref_t<InCallableT>>::value) &&
            (!std::is_same_v<std::remove_cvref_t<InCallableT>, gp::NullPtrT>) &&
            std::is_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT> &&
            std::is_copy_constructible_v<std::remove_cvref_t<InCallableT>> &&
            std::is_invocable_r_v<InRet, std::remove_cvref_t<InCallableT>&, InArgs...>
    Function& operator=(
        InCallableT&& callable
    ) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT>)
    {
        if constexpr (std::is_same_v<std::remove_cvref_t<InCallableT>, Delegate<InRet(InArgs...)>>)
        {
            this->unbind();
            if (callable.isBound())
            {
                this->bindCallable(std::forward<InCallableT>(callable));
            }
        }
        else if constexpr (std::is_pointer_v<std::decay_t<InCallableT>>)
        {
            this->bindStatic(callable);
        }
        else
        {
            this->bindCallable(std::forward<InCallableT>(callable));
        }
        return *this;
    }

public:
    /// @brief Creates a `Function` bound to a compile-time free or static function pointer.
    /// @tparam InFuncPtr Compile-time function pointer matching `InRet(InArgs...)`.
    /// @return Bound `Function`.
    template <auto InFuncPtr>
    [[nodiscard]] static Function createStatic() noexcept
    {
        Function result;
        result.template bindStatic<InFuncPtr>();
        return result;
    }

    /// @brief Creates a `Function` bound to a runtime free or static function pointer.
    /// @param[in] funcPtr Function pointer to invoke (`nullptr` produces an unbound `Function`).
    /// @return `Function`.
    [[nodiscard]] static Function createStatic(InRet (*const funcPtr)(InArgs...)) noexcept
    {
        Function result;
        result.bindStatic(funcPtr);
        return result;
    }

    /// @brief Creates a `Function` bound to a member function on @p object.
    /// @tparam InMethodPtr Compile-time pointer-to-member-function (`&MyClass::onEvent`).
    /// @tparam InObjectT Class type (may be `const` for const member functions).
    /// @param[in] object Non-owning pointer to the target instance.
    /// @return Bound `Function`, or unbound if @p object is `nullptr`.
    template <auto InMethodPtr, typename InObjectT>
    [[nodiscard]] static Function createRaw(InObjectT* const object) noexcept
    {
        Function result;
        result.template bindRaw<InMethodPtr>(object);
        return result;
    }

    /// @brief Creates a `Function` storing a copyable callable or lambda (inline SBO or heap spillover).
    /// @tparam InCallableT Copy-constructible callable type.
    /// @param[in] callable Callable object to bind.
    /// @return Bound `Function`.
    template <typename InCallableT>
    [[nodiscard]] static Function createLambda(
        InCallableT&& callable
    ) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT>)
    {
        Function result;
        result.bindLambda(std::forward<InCallableT>(callable));
        return result;
    }

public:
    /// @brief Binds a compile-time free or static function pointer with zero payload storage.
    /// @tparam InFuncPtr Compile-time function pointer matching `InRet(InArgs...)`.
    template <auto InFuncPtr>
    void bindStatic() noexcept
    {
        Base::template bindStatic<InFuncPtr>();
    }

    /// @brief Binds a runtime free or static function pointer (`nullptr` unbinds).
    /// @param[in] funcPtr Function pointer to invoke.
    void bindStatic(InRet (*const funcPtr)(InArgs...)) noexcept
    {
        Base::bindStatic(funcPtr);
    }

    /// @brief Binds a member function on a raw object pointer (`nullptr` unbinds).
    /// @tparam InMethodPtr Compile-time member function pointer (`&Class::method`).
    /// @tparam InObjectT Object type (const or non-const).
    /// @param[in] object Target object pointer.
    template <auto InMethodPtr, typename InObjectT>
    void bindRaw(InObjectT* const object) noexcept
    {
        Base::template bindRaw<InMethodPtr>(object);
    }

    /// @brief Binds a copy-constructible callable object or lambda (inline SBO or heap spillover).
    /// @tparam InCallableT Invocable type matching `InRet(InArgs...)`.
    /// @param[in] callable Callable object to store.
    template <typename InCallableT>
    void bindLambda(
        InCallableT&& callable
    ) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT>)
    {
        Base::bindCallable(std::forward<InCallableT>(callable));
    }

    /// @brief Resets this `Function` to the unbound state (alias for `unbind()` matching `UE::TFunction::Reset()`).
    void reset() noexcept
    {
        this->unbind();
    }

    /// @brief Exchanges the bound state of this `Function` with @p other.
    /// @param[in,out] other Other `Function` to swap with.
    void swap(Function& other) noexcept
    {
        this->swapStorage(other);
    }

    /// @brief Contextual boolean conversion returning `isBound()`.
    /// @return `true` if bound, `false` otherwise.
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return this->isBound();
    }

    /// @brief Invokes the bound callable unconditionally.
    /// @note Triggers `GP_ASSERT` if called on an unbound `Function` (matching `UE::TFunction::operator()`).
    /// @param[in] args Arguments forwarded to the target callable.
    /// @return Return value produced by the callable.
    InRet execute(InArgs... args) const
    {
        GP_ASSERT(this->isBound() && "Cannot execute an unbound gp::Function.");
        return this->m_invoke(this->m_callablePtr, std::forward<InArgs>(args)...);
    }

    /// @brief Function-call operator forwarding directly to `execute()`.
    /// @param[in] args Arguments forwarded to the target callable.
    /// @return Return value produced by the callable.
    InRet operator()(InArgs... args) const
    {
        return execute(std::forward<InArgs>(args)...);
    }

    /// @brief Invokes the target only if bound (`void` return specialization).
    /// @param[in] args Arguments forwarded to the target callable.
    /// @return `true` if bound and executed, `false` if unbound.
    bool executeIfBound(InArgs... args) const requires std::is_void_v<InRet>
    {
        if (!this->isBound())
        {
            return false;
        }
        this->m_invoke(this->m_callablePtr, std::forward<InArgs>(args)...);
        return true;
    }

    /// @brief Invokes the target only if bound (value-returning specialization).
    /// @param[in] args Arguments forwarded to the target callable.
    /// @return Engaged `gp::Optional<InReturnT>` if bound, or empty `Optional` if unbound.
    template <typename InReturnT = InRet>
    requires(!std::is_void_v<InReturnT> && std::is_same_v<InReturnT, InRet>)
    [[nodiscard]] gp::Optional<InReturnT> executeIfBound(InArgs... args) const
    {
        if (!this->isBound())
        {
            return gp::Optional<InReturnT>{};
        }
        return gp::Optional<InReturnT>(this->m_invoke(this->m_callablePtr, std::forward<InArgs>(args)...));
    }
};

template <typename InRet, typename... InArgs, gp::USize InInlineStorageSize>
UniqueFunction<InRet(InArgs...), InInlineStorageSize>::UniqueFunction(
    Function<InRet(InArgs...), InInlineStorageSize>&& func
) noexcept
{
    this->moveFrom(std::move(func));
}

template <typename InRet, typename... InArgs, gp::USize InInlineStorageSize>
UniqueFunction<InRet(InArgs...), InInlineStorageSize>& UniqueFunction<InRet(InArgs...), InInlineStorageSize>::operator=(
    Function<InRet(InArgs...), InInlineStorageSize>&& func
) noexcept
{
    this->unbind();
    this->moveFrom(std::move(func));
    return *this;
}

/// @brief Compares a `UniqueFunction` against `nullptr` (`true` when unbound).
/// @tparam InSignature Function signature (`InRet(InArgs...)`).
/// @tparam InInlineSize Inline storage capacity in bytes.
/// @param[in] func `UniqueFunction` to test.
/// @param[in] nullValue Null pointer literal.
/// @return `true` if `!func.isBound()`, `false` otherwise.
template <typename InSignature, gp::USize InInlineSize>
[[nodiscard]] constexpr bool
    operator==(const UniqueFunction<InSignature, InInlineSize>& func, [[maybe_unused]] gp::NullPtrT nullValue) noexcept
{
    return !func.isBound();
}

/// @brief Compares a `Function` against `nullptr` (`true` when unbound).
/// @tparam InSignature Function signature (`InRet(InArgs...)`).
/// @tparam InInlineSize Inline storage capacity in bytes.
/// @param[in] func `Function` to test.
/// @param[in] nullValue Null pointer literal.
/// @return `true` if `!func.isBound()`, `false` otherwise.
template <typename InSignature, gp::USize InInlineSize>
[[nodiscard]] constexpr bool
    operator==(const Function<InSignature, InInlineSize>& func, [[maybe_unused]] gp::NullPtrT nullValue) noexcept
{
    return !func.isBound();
}

/// @brief ADL-friendly swap for two `UniqueFunction` instances.
/// @tparam InSignature Function signature (`InRet(InArgs...)`).
/// @tparam InInlineSize Inline storage capacity in bytes.
/// @param[in,out] lhs First `UniqueFunction`.
/// @param[in,out] rhs Second `UniqueFunction`.
template <typename InSignature, gp::USize InInlineSize>
void swap(UniqueFunction<InSignature, InInlineSize>& lhs, UniqueFunction<InSignature, InInlineSize>& rhs) noexcept
{
    lhs.swap(rhs);
}

/// @brief ADL-friendly swap for two `Function` instances.
/// @tparam InSignature Function signature (`InRet(InArgs...)`).
/// @tparam InInlineSize Inline storage capacity in bytes.
/// @param[in,out] lhs First `Function`.
/// @param[in,out] rhs Second `Function`.
template <typename InSignature, gp::USize InInlineSize>
void swap(Function<InSignature, InInlineSize>& lhs, Function<InSignature, InInlineSize>& rhs) noexcept
{
    lhs.swap(rhs);
}

}   // namespace gp
