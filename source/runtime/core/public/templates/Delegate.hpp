// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "templates/Optional.hpp"
#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

namespace gp
{

/// @brief Lightweight token identifying a registered subscription in a multicast delegate or sink list.
class DelegateHandle final
{
public:
    /// @brief Default-constructs an invalid (null) handle.
    [[nodiscard]] constexpr DelegateHandle() noexcept = default;

    /// @brief Constructs a handle from a raw non-zero identifier.
    /// @param[in] rawId Encoded identifier value (`0` represents invalid).
    [[nodiscard]] constexpr explicit DelegateHandle(const gp::UInt64 rawId) noexcept
        : m_id(rawId)
    {}

    /// @brief Reports whether this handle references an active binding.
    /// @return True if non-zero, false if invalid or reset.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_id != 0ULL;
    }

    /// @brief Contextual boolean conversion returning `isValid()`.
    /// @return True if valid, false otherwise.
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return isValid();
    }

    /// @brief Clears this handle back to the invalid state (`isValid() == false`).
    constexpr void reset() noexcept
    {
        m_id = 0ULL;
    }

    /// @brief Returns the underlying 64-bit token value.
    /// @return Raw handle identifier.
    [[nodiscard]] constexpr gp::UInt64 rawValue() const noexcept
    {
        return m_id;
    }

    /// @brief Equality comparison between two `DelegateHandle` tokens.
    /// @param[in] other Handle to compare against.
    /// @return True if both handles hold identical token values, false otherwise.
    [[nodiscard]] constexpr bool operator==(const DelegateHandle& other) const noexcept = default;

private:
    gp::UInt64 m_id{ 0ULL };
};

template <typename InSignature>
class Delegate;

/// @brief Zero-allocation single-cast callback wrapper using fixed 32-byte inline small-buffer storage.
/// @details Stores free function pointers, non-const/const member function bindings, and small callable objects
///          inline without touching heap allocators or `std::function`. Trivially copyable callables omit lifecycle
///          management overhead completely.
/// @tparam InRet Return type of the bound callable.
/// @tparam InArgs Parameter types forwarded when invoking the delegate.
template <typename InRet, typename... InArgs>
class Delegate<InRet(InArgs...)> final
{
public:
    /// @brief Inline storage capacity in bytes reserved inside each `Delegate` instance.
    static constexpr gp::USize kInlineStorageSize = 32u;

    /// @brief Alignment of the inline callable buffer.
    static constexpr gp::USize kInlineStorageAlignment = alignof(std::max_align_t);

private:
    enum class StorageOp : gp::UInt8
    {
        Copy = 0u,
        Move = 1u,
        Destroy = 2u
    };

    using InvokeFn = InRet (*)(gp::Byte*, InArgs...);
    using OpsFn = void (*)(StorageOp, gp::Byte*, gp::Byte*);

    template <typename InObjectT>
    struct MemberBinding final
    {
        InObjectT* object{ nullptr };
    };

    template <typename InFuncPtrT>
    struct FunctionBinding final
    {
        InFuncPtrT func{ nullptr };
    };

public:
    /// @brief Default-constructs an unbound delegate (`isBound() == false`).
    [[nodiscard]] constexpr Delegate() noexcept = default;

    /// @brief Constructs an unbound delegate from `nullptr`.
    [[nodiscard]] constexpr explicit(false) Delegate(gp::NullPtrT) noexcept
    {}

    /// @brief Constructs a delegate from a free function pointer (`nullptr` yields unbound).
    /// @param[in] funcPtr Function pointer to bind.
    [[nodiscard]] explicit(false) Delegate(InRet (*const funcPtr)(InArgs...)) noexcept
    {
        bindStatic(funcPtr);
    }

    /// @brief Constructs a delegate from a copyable callable object or lambda that fits inline storage.
    /// @tparam InCallableT Invocable callable type.
    /// @param[in] callable Callable instance to copy or move into inline storage.
    template <typename InCallableT>
    requires(!std::is_same_v<std::remove_cvref_t<InCallableT>, Delegate>) &&
            (!std::is_same_v<std::remove_cvref_t<InCallableT>, gp::NullPtrT>) &&
            (!std::is_pointer_v<std::decay_t<InCallableT>>) &&
            std::is_invocable_r_v<InRet, std::remove_cvref_t<InCallableT>&, InArgs...>
    [[nodiscard]] explicit(false) Delegate(
        InCallableT&& callable
    ) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT>)
    {
        bindLambda(std::forward<InCallableT>(callable));
    }

    /// @brief Copy constructor. Copies the inline callable if bound.
    /// @param[in] other Source delegate to copy from.
    [[nodiscard]] Delegate(const Delegate& other) noexcept
    {
        copyFrom(other);
    }

    /// @brief Move constructor. Transfers the inline callable and leaves @p other unbound.
    /// @param[in,out] other Source delegate to move from.
    [[nodiscard]] Delegate(Delegate&& other) noexcept
    {
        moveFrom(std::move(other));
    }

    /// @brief Destructor. Destroys any non-trivial bound callable in inline storage.
    ~Delegate() noexcept
    {
        unbind();
    }

    /// @brief Copy assignment operator.
    /// @param[in] other Source delegate to copy from.
    /// @return Reference to this delegate.
    Delegate& operator=(const Delegate& other) noexcept
    {
        if (this != &other)
        {
            unbind();
            copyFrom(other);
        }
        return *this;
    }

    /// @brief Move assignment operator. Transfers ownership and leaves @p other unbound.
    /// @param[in,out] other Source delegate to move from.
    /// @return Reference to this delegate.
    Delegate& operator=(Delegate&& other) noexcept
    {
        if (this != &other)
        {
            unbind();
            moveFrom(std::move(other));
        }
        return *this;
    }

    /// @brief Unbinds this delegate when assigned `nullptr`.
    /// @return Reference to this delegate.
    Delegate& operator=(gp::NullPtrT) noexcept
    {
        unbind();
        return *this;
    }

public:
    /// @brief Creates a delegate bound to a compile-time free or static function pointer.
    /// @tparam InFuncPtr Compile-time function pointer matching `InRet(InArgs...)`.
    /// @return Bound `Delegate`.
    template <auto InFuncPtr>
    [[nodiscard]] static Delegate createStatic() noexcept
    {
        Delegate result;
        result.bindStatic<InFuncPtr>();
        return result;
    }

    /// @brief Creates a delegate bound to a runtime free or static function pointer.
    /// @param[in] funcPtr Function pointer to invoke (`nullptr` produces an unbound delegate).
    /// @return `Delegate`.
    [[nodiscard]] static Delegate createStatic(InRet (*const funcPtr)(InArgs...)) noexcept
    {
        Delegate result;
        result.bindStatic(funcPtr);
        return result;
    }

    /// @brief Creates a delegate bound to a non-const or const member function on @p object.
    /// @tparam InMethodPtr Compile-time pointer-to-member-function (`&MyClass::onEvent`).
    /// @tparam InObjectT Class type (may be `const` for const member functions).
    /// @param[in] object Non-owning pointer to the target instance (caller guarantees lifetime).
    /// @return Bound `Delegate`, or unbound if @p object is `nullptr`.
    template <auto InMethodPtr, typename InObjectT>
    [[nodiscard]] static Delegate createRaw(InObjectT* const object) noexcept
    {
        Delegate result;
        result.bindRaw<InMethodPtr>(object);
        return result;
    }

    /// @brief Creates a delegate storing a callable or lambda in inline SBO storage.
    /// @tparam InCallableT Callable type fitting within `kInlineStorageSize`.
    /// @param[in] callable Callable object to bind.
    /// @return Bound `Delegate`.
    template <typename InCallableT>
    [[nodiscard]] static Delegate createLambda(
        InCallableT&& callable
    ) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT>)
    {
        Delegate result;
        result.bindLambda(std::forward<InCallableT>(callable));
        return result;
    }

public:
    /// @brief Binds a compile-time free or static function pointer with zero payload storage.
    /// @tparam InFuncPtr Compile-time function pointer matching `InRet(InArgs...)`.
    template <auto InFuncPtr>
    void bindStatic() noexcept
    {
        static_assert(std::is_invocable_r_v<InRet, decltype(InFuncPtr), InArgs...>, "Incompatible function signature.");
        unbind();
        m_invoke = []([[maybe_unused]] gp::Byte* const storage, InArgs... args) -> InRet
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

    /// @brief Binds a runtime free or static function pointer (`nullptr` unbinds).
    /// @param[in] funcPtr Function pointer to invoke.
    void bindStatic(InRet (*const funcPtr)(InArgs...)) noexcept
    {
        unbind();
        if (funcPtr == nullptr)
        {
            return;
        }

        using BindingT = FunctionBinding<InRet (*)(InArgs...)>;
        std::construct_at(reinterpret_cast<BindingT*>(m_storage), BindingT{ funcPtr });
        m_invoke = [](gp::Byte* const storage, InArgs... args) -> InRet
        {
            const BindingT* const binding = reinterpret_cast<const BindingT*>(storage);
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

    /// @brief Binds a member function on a raw object pointer (`nullptr` unbinds).
    /// @note The caller owns @p object and must unbind or destroy this delegate before @p object dies.
    /// @tparam InMethodPtr Compile-time member function pointer (`&Class::method`).
    /// @tparam InObjectT Object type (const or non-const).
    /// @param[in] object Target object pointer.
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

        using BindingT = MemberBinding<InObjectT>;
        static_assert(sizeof(BindingT) <= kInlineStorageSize, "MemberBinding exceeds inline storage.");
        std::construct_at(reinterpret_cast<BindingT*>(m_storage), BindingT{ object });
        m_invoke = [](gp::Byte* const storage, InArgs... args) -> InRet
        {
            const BindingT* const binding = reinterpret_cast<const BindingT*>(storage);
            if constexpr (std::is_void_v<InRet>)
            {
                (binding->object->*InMethodPtr)(std::forward<InArgs>(args)...);
            }
            else
            {
                return (binding->object->*InMethodPtr)(std::forward<InArgs>(args)...);
            }
        };
        m_ops = nullptr;
    }

    /// @brief Binds a copyable callable object or lambda inside the 32-byte inline SBO buffer.
    /// @tparam InCallableT Invocable type matching `InRet(InArgs...)`.
    /// @param[in] callable Callable object to store in place.
    template <typename InCallableT>
    void bindLambda(
        InCallableT&& callable
    ) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<InCallableT>, InCallableT>)
    {
        using DecayedT = std::remove_cvref_t<InCallableT>;
        static_assert(
            std::is_invocable_r_v<InRet, DecayedT&, InArgs...>, "Callable signature does not match gp::Delegate."
        );
        static_assert(
            sizeof(DecayedT) <= kInlineStorageSize,
            "Callable exceeds gp::Delegate 32-byte inline storage; capture pointers instead of large objects."
        );
        static_assert(
            alignof(DecayedT) <= kInlineStorageAlignment,
            "Callable alignment exceeds gp::Delegate inline storage alignment."
        );
        static_assert(
            std::is_copy_constructible_v<DecayedT>,
            "Callable stored in gp::Delegate must be copy-constructible so callbacks can be snapshotted safely."
        );

        unbind();
        std::construct_at(reinterpret_cast<DecayedT*>(m_storage), std::forward<InCallableT>(callable));

        m_invoke = [](gp::Byte* const storage, InArgs... args) -> InRet
        {
            DecayedT* const target = reinterpret_cast<DecayedT*>(storage);
            if constexpr (std::is_void_v<InRet>)
            {
                (*target)(std::forward<InArgs>(args)...);
            }
            else
            {
                return (*target)(std::forward<InArgs>(args)...);
            }
        };

        if constexpr (std::is_trivially_copyable_v<DecayedT> && std::is_trivially_destructible_v<DecayedT>)
        {
            m_ops = nullptr;
        }
        else
        {
            m_ops = [](const StorageOp op, gp::Byte* const src, gp::Byte* const dst) -> void
            {
                DecayedT* const srcObj = reinterpret_cast<DecayedT*>(src);
                DecayedT* const dstObj = reinterpret_cast<DecayedT*>(dst);
                switch (op)
                {
                case StorageOp::Copy:
                    std::construct_at(dstObj, *srcObj);
                    break;
                case StorageOp::Move:
                    std::construct_at(dstObj, std::move(*srcObj));
                    std::destroy_at(srcObj);
                    break;
                case StorageOp::Destroy:
                    std::destroy_at(srcObj);
                    break;
                default:
                    break;
                }
            };
        }
    }

    /// @brief Releases any bound callable and resets this delegate to unbound.
    void unbind() noexcept
    {
        if (m_ops != nullptr)
        {
            m_ops(StorageOp::Destroy, m_storage, nullptr);
        }
        m_invoke = nullptr;
        m_ops = nullptr;
    }

    /// @brief Reports whether this delegate holds a valid target callable.
    /// @return True if bound, false otherwise.
    [[nodiscard]] constexpr bool isBound() const noexcept
    {
        return m_invoke != nullptr;
    }

    /// @brief Contextual boolean conversion returning `isBound()`.
    /// @return True if bound, false otherwise.
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return isBound();
    }

    /// @brief Invokes the bound callable unconditionally.
    /// @note Triggers an assertion failure if called on an unbound delegate.
    /// @param[in] args Arguments forwarded to the target callable.
    /// @return Return value produced by the callable (void if `InRet` is void).
    InRet execute(InArgs... args) const
    {
        GP_ASSERT(isBound() && "Cannot execute an unbound gp::Delegate.");
        return m_invoke(m_storage, std::forward<InArgs>(args)...);
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
    /// @return True if the delegate was bound and executed, false if unbound.
    bool executeIfBound(InArgs... args) const requires std::is_void_v<InRet>
    {
        if (!isBound())
        {
            return false;
        }
        m_invoke(m_storage, std::forward<InArgs>(args)...);
        return true;
    }

    /// @brief Invokes the target only if bound (value-returning specialization).
    /// @param[in] args Arguments forwarded to the target callable.
    /// @return Engaged `gp::Optional<InReturnT>` if bound, or empty `Optional` if unbound.
    template <typename InReturnT = InRet>
    requires(!std::is_void_v<InReturnT> && std::is_same_v<InReturnT, InRet>)
    [[nodiscard]] gp::Optional<InReturnT> executeIfBound(InArgs... args) const
    {
        if (!isBound())
        {
            return gp::Optional<InReturnT>{};
        }
        return gp::Optional<InReturnT>(m_invoke(m_storage, std::forward<InArgs>(args)...));
    }

private:
    void copyFrom(const Delegate& other) noexcept
    {
        m_invoke = other.m_invoke;
        m_ops = other.m_ops;
        if (other.m_invoke == nullptr)
        {
            return;
        }

        if (other.m_ops != nullptr)
        {
            other.m_ops(StorageOp::Copy, other.m_storage, m_storage);
        }
        else
        {
            std::memcpy(m_storage, other.m_storage, kInlineStorageSize);
        }
    }

    void moveFrom(Delegate&& other) noexcept
    {
        m_invoke = other.m_invoke;
        m_ops = other.m_ops;
        if (other.m_invoke == nullptr)
        {
            return;
        }

        if (other.m_ops != nullptr)
        {
            other.m_ops(StorageOp::Move, other.m_storage, m_storage);
        }
        else
        {
            std::memcpy(m_storage, other.m_storage, kInlineStorageSize);
        }
        other.m_invoke = nullptr;
        other.m_ops = nullptr;
    }

private:
    alignas(kInlineStorageAlignment) mutable gp::Byte m_storage[kInlineStorageSize]{};
    InvokeFn m_invoke{ nullptr };
    OpsFn m_ops{ nullptr };
};

/// @brief Fixed-capacity, zero-allocation multicast delegate supporting safe handle-based removal during broadcast.
/// @tparam InCapacity Maximum simultaneous subscriber slots.
/// @tparam InArgs Argument types passed to every bound listener on `broadcast()`.
template <gp::USize InCapacity, typename... InArgs>
class FixedMulticastDelegate final
{
    static_assert(InCapacity > 0u, "FixedMulticastDelegate capacity must be greater than zero.");

public:
    /// @brief Delegate type stored in each subscriber slot.
    using DelegateType = Delegate<void(InArgs...)>;

    /// @brief Maximum subscriber slots supported by this instance.
    static constexpr gp::USize kCapacity = InCapacity;

private:
    struct Slot final
    {
        DelegateType callback{};
        DelegateHandle handle{};
        gp::UInt32 generation{ 1u };
        bool isActive{ false };
    };

public:
    /// @brief Default-constructs an empty multicast delegate.
    [[nodiscard]] FixedMulticastDelegate() noexcept = default;

public:
    /// @brief Subscribes a delegate and returns a unique `DelegateHandle` for later removal.
    /// @param[in] callback Bound listener delegate (`!isBound()` is rejected).
    /// @return Valid `DelegateHandle` on success, or invalid `DelegateHandle{}` if unbound or full.
    [[nodiscard]] DelegateHandle add(DelegateType callback) noexcept
    {
        if (!callback.isBound())
        {
            return DelegateHandle{};
        }

        for (gp::USize i = 0u; i < InCapacity; ++i)
        {
            Slot& slot = m_slots[i];
            if (!slot.isActive)
            {
                const gp::UInt64 rawId =
                    (static_cast<gp::UInt64>(slot.generation) << 32u) | static_cast<gp::UInt64>(i + 1u);
                slot.callback = std::move(callback);
                slot.handle = DelegateHandle(rawId);
                slot.isActive = true;
                ++m_boundCount;
                return slot.handle;
            }
        }

        return DelegateHandle{};
    }

    /// @brief Unsubscribes the listener identified by @p handle.
    /// @note Safe to invoke re-entrantly from inside an active `broadcast()`.
    /// @param[in] handle Handle returned by `add()`.
    /// @return True if a matching active listener was removed, false otherwise.
    bool remove(const DelegateHandle handle) noexcept
    {
        if (!handle.isValid())
        {
            return false;
        }

        const gp::UInt64 rawId = handle.rawValue();
        const gp::USize indexPlusOne = static_cast<gp::USize>(rawId & 0xFFFFFFFFULL);
        if (indexPlusOne == 0u || indexPlusOne > InCapacity)
        {
            return false;
        }

        Slot& slot = m_slots[indexPlusOne - 1u];
        if (!slot.isActive || slot.handle != handle)
        {
            return false;
        }

        slot.callback.unbind();
        slot.handle.reset();
        slot.isActive = false;
        ++slot.generation;
        if (slot.generation == 0u)
        {
            slot.generation = 1u;
        }
        --m_boundCount;
        return true;
    }

    /// @brief Unbinds all currently registered listeners and advances slot generations.
    void clear() noexcept
    {
        for (gp::USize i = 0u; i < InCapacity; ++i)
        {
            Slot& slot = m_slots[i];
            if (slot.isActive)
            {
                slot.callback.unbind();
                slot.handle.reset();
                slot.isActive = false;
                ++slot.generation;
                if (slot.generation == 0u)
                {
                    slot.generation = 1u;
                }
            }
        }
        m_boundCount = 0u;
    }

    /// @brief Invokes all active subscribers with @p args, snapshotting handles so removal during broadcast is safe.
    /// @param[in] args Arguments forwarded to each bound listener.
    void broadcast(InArgs... args) const
    {
        if (m_boundCount == 0u)
        {
            return;
        }

        DelegateHandle snapshotHandles[InCapacity]{};
        DelegateType snapshotCallbacks[InCapacity]{};
        gp::USize snapshotCount = 0u;

        for (gp::USize i = 0u; i < InCapacity; ++i)
        {
            if (m_slots[i].isActive && m_slots[i].callback.isBound())
            {
                snapshotHandles[snapshotCount] = m_slots[i].handle;
                snapshotCallbacks[snapshotCount] = m_slots[i].callback;
                ++snapshotCount;
            }
        }

        for (gp::USize i = 0u; i < snapshotCount; ++i)
        {
            // Re-check liveness in case an earlier listener unsubscribed this handle mid-broadcast.
            const gp::USize slotIdx = static_cast<gp::USize>(snapshotHandles[i].rawValue() & 0xFFFFFFFFULL) - 1u;
            if (m_slots[slotIdx].isActive && m_slots[slotIdx].handle == snapshotHandles[i])
            {
                snapshotCallbacks[i].execute(args...);
            }
        }
    }

    /// @brief Returns the number of currently bound subscribers.
    /// @return Active subscriber count in `[0, InCapacity]`.
    [[nodiscard]] constexpr gp::USize getBoundCount() const noexcept
    {
        return m_boundCount;
    }

    /// @brief Reports whether at least one subscriber is currently bound.
    /// @return True if `getBoundCount() > 0`, false otherwise.
    [[nodiscard]] constexpr bool isBound() const noexcept
    {
        return m_boundCount > 0u;
    }

    /// @brief Returns the maximum number of subscribers (`InCapacity`).
    /// @return Fixed slot capacity.
    [[nodiscard]] static constexpr gp::USize getCapacity() noexcept
    {
        return InCapacity;
    }

private:
    Slot m_slots[InCapacity]{};
    gp::USize m_boundCount{ 0u };
};

}   // namespace gp
