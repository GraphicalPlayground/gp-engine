// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Relationship.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleForward.hpp"   // IWYU pragma: keep
#include "console/ConsoleManager.hpp"
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "templates/Delegate.hpp"
#include "threading/atomics/Atomic.hpp"
#include <type_traits>
#include <utility>

namespace gp::console
{

namespace detail
{

template <typename InValueT>
consteval ConsoleVarType resolveConsoleVarType() noexcept
{
    using DecayedT = std::remove_cvref_t<InValueT>;
    if constexpr (gp::concepts::IsSameAs<DecayedT, bool>)
    {
        return ConsoleVarType::Bool;
    }
    else if constexpr (gp::concepts::IsSameAs<DecayedT, gp::Int32>)
    {
        return ConsoleVarType::Int32;
    }
    else if constexpr (gp::concepts::IsSameAs<DecayedT, gp::UInt32>)
    {
        return ConsoleVarType::UInt32;
    }
    else if constexpr (gp::concepts::IsSameAs<DecayedT, gp::Float32>)
    {
        return ConsoleVarType::Float32;
    }
    else if constexpr (gp::concepts::IsSameAs<DecayedT, gp::Float64>)
    {
        return ConsoleVarType::Float64;
    }
    else
    {
        return ConsoleVarType::String;
    }
}

template <typename InValueT>
struct ConsoleReturnTraits final
{
    using ReturnType = std::remove_cvref_t<InValueT>;
    using ArgType = std::remove_cvref_t<InValueT>;
};

template <>
struct ConsoleReturnTraits<gp::StringView> final
{
    using ReturnType = gp::StringView;
    using ArgType = gp::StringView;
};

template <>
struct ConsoleReturnTraits<const char*> final
{
    using ReturnType = const char*;
    using ArgType = gp::StringView;
};

template <>
struct ConsoleReturnTraits<ConsoleString> final
{
    using ReturnType = ConsoleString;
    using ArgType = gp::StringView;
};

template <>
struct ConsoleReturnTraits<gp::String> final
{
    using ReturnType = gp::String;
    using ArgType = gp::StringView;
};

}   // namespace detail

/// @brief Self-registering typed console variable with inline dual Game/Render slots and zero heap allocations.
/// @details Static instances register onto the global `.bss` deferred list with no Static Initialization Order Fiasco
///          and unlink cleanly on destruction or dynamic module unload. Reads via `getValueOnRenderThread()` use
///          lock-free atomic loads (or seqlocks for strings) that reflect the last `syncRenderThreadValues()` barrier.
/// @tparam InValueT Value type satisfying `IsConsoleValueType` (`bool`, `Int32`, `UInt32`, `Float32`, `Float64`,
///                  `StringView`, `ConsoleString`, `String`, or `const char*`).
template <typename InValueT>
requires IsConsoleValueType<InValueT> class AutoConsoleVariable final : public internal::ConsoleVariableBase
{
public:
    /// @brief Stored underlying value category type.
    using ValueType = std::remove_cvref_t<InValueT>;

    /// @brief Type returned by `getValueOnGameThread()` and `getValueOnRenderThread()`.
    using ReturnType = typename detail::ConsoleReturnTraits<ValueType>::ReturnType;

    /// @brief Input parameter type accepted by `set()`.
    using ArgType = typename detail::ConsoleReturnTraits<ValueType>::ArgType;

private:
    static constexpr bool kIsString = IsConsoleStringType<ValueType>;
    using ScalarStorageT = std::conditional_t<kIsString, gp::UInt8, ValueType>;

public:
    /// @brief Constructs and registers a console variable with the global `IConsoleManager::get()` registry.
    /// @param[in] name Unique case-insensitive lookup name (e.g., `"r.ShadowResolution"`).
    /// @param[in] defaultValue Initial default value.
    /// @param[in] help Human-readable help string.
    /// @param[in] flags Optional behavioral flags (`RenderThreadSafe` is enabled automatically).
    [[nodiscard]] AutoConsoleVariable(
        const gp::StringView name,
        const ArgType defaultValue,
        const gp::StringView help,
        const ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept
        : ConsoleVariableBase(
              detail::resolveConsoleVarType<ValueType>(), name, help, flags | ConsoleVarFlags::RenderThreadSafe
          )
    {
        initializeDefaultValue(defaultValue);
        internal::enqueueDeferredConsoleObject(this);
    }

    /// @brief Constructs and registers a console variable directly with a specific `IConsoleManager`.
    /// @param[in,out] manager Target console manager instance (caller ensures it outlives this variable).
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in] defaultValue Initial default value.
    /// @param[in] help Human-readable help string.
    /// @param[in] flags Optional behavioral flags (`RenderThreadSafe` is enabled automatically).
    [[nodiscard]] AutoConsoleVariable(
        IConsoleManager& manager,
        const gp::StringView name,
        const ArgType defaultValue,
        const gp::StringView help,
        const ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept
        : ConsoleVariableBase(
              detail::resolveConsoleVarType<ValueType>(), name, help, flags | ConsoleVarFlags::RenderThreadSafe
          )
    {
        initializeDefaultValue(defaultValue);
        (void)manager.registerExternalObject(this);
    }

    /// @brief Destructor. Safely unregisters this variable from its owning manager or deferred static queue.
    virtual ~AutoConsoleVariable() noexcept override
    {
        internal::unregisterAutoConsoleObject(this);
    }

    AutoConsoleVariable(const AutoConsoleVariable&) = delete;
    AutoConsoleVariable& operator=(const AutoConsoleVariable&) = delete;
    AutoConsoleVariable(AutoConsoleVariable&&) = delete;
    AutoConsoleVariable& operator=(AutoConsoleVariable&&) = delete;

public:
    /// @copydoc IConsoleVariable::isExternalReference()
    [[nodiscard]] virtual bool isExternalReference() const noexcept override
    {
        return false;
    }

    /// @brief Reports whether this variable is currently registered inside an `IConsoleManager`.
    /// @return True if actively registered, false if deferred or failed registration.
    [[nodiscard]] bool isRegistered() const noexcept
    {
        return getOwnerManager() != nullptr;
    }

    /// @brief Returns the `IConsoleVariable*` interface pointer for delegating metadata, callbacks, or string sets.
    /// @return Mutable `IConsoleVariable*`.
    [[nodiscard]] virtual IConsoleVariable* asVariable() noexcept override
    {
        return this;
    }

    /// @brief Returns a const `IConsoleVariable*` interface pointer.
    /// @return Const `IConsoleVariable*`.
    [[nodiscard]] virtual const IConsoleVariable* asVariable() const noexcept override
    {
        return this;
    }

    /// @brief Pointer-like convenience access to the underlying `IConsoleVariable` methods.
    /// @return Mutable `IConsoleVariable*`.
    [[nodiscard]] IConsoleVariable* operator->() noexcept
    {
        return this;
    }

    /// @brief Const pointer-like convenience access to the underlying `IConsoleVariable` methods.
    /// @return Const `IConsoleVariable*`.
    [[nodiscard]] const IConsoleVariable* operator->() const noexcept
    {
        return this;
    }

    /// @brief Reads the active Game Thread value directly without locking.
    /// @return Current Game Thread value of type `ReturnType`.
    [[nodiscard]] ReturnType getValueOnGameThread() const noexcept
    {
        if constexpr (!kIsString)
        {
            return m_gameScalar.load(gp::MemoryOrder::Relaxed);
        }
        else
        {
            return formatStringReturn(m_gameString);
        }
    }

    /// @brief Reads the frame-synchronized Render Thread value atomically without locking.
    /// @note Reflects the latest snapshot published by `IConsoleManager::syncRenderThreadValues()`.
    /// @return Synchronized Render Thread value of type `ReturnType`.
    [[nodiscard]] ReturnType getValueOnRenderThread() const noexcept
    {
        if constexpr (!kIsString)
        {
            return m_renderScalar.load(gp::MemoryOrder::Acquire);
        }
        else if constexpr (gp::concepts::IsSameAs<ValueType, ConsoleString>)
        {
            return getStringOnRenderThread();
        }
        else if constexpr (gp::concepts::IsSameAs<ValueType, gp::String>)
        {
            return gp::String(getStringOnRenderThread().view());
        }
        else
        {
            return formatStringReturn(m_renderString);
        }
    }

    /// @brief Reads the latest committed value from any thread, or the Render Thread slot when @p forceGameThread is
    ///        false and immediate Game Thread state is not requested.
    /// @param[in] forceGameThread True to read the live Game Thread value atomically; false to read the Render Thread
    ///                            snapshot (for scalars) or tear-free seqlock copy (for strings).
    /// @return Value of type `ReturnType` (or `ConsoleString` when `ReturnType` is `StringView`/`const char*` on
    ///         concurrent string reads; use `getStringOnAnyThread()` for guaranteed string lifetime).
    [[nodiscard]] ReturnType getValueOnAnyThread(const bool forceGameThread = false) const noexcept
    {
        if constexpr (!kIsString)
        {
            return forceGameThread ? m_gameScalar.load(gp::MemoryOrder::Acquire)
                                   : m_renderScalar.load(gp::MemoryOrder::Acquire);
        }
        else if constexpr (gp::concepts::IsSameAs<ValueType, ConsoleString>)
        {
            return forceGameThread ? getStringOnAnyThread() : getStringOnRenderThread();
        }
        else if constexpr (gp::concepts::IsSameAs<ValueType, gp::String>)
        {
            const ConsoleString copy = forceGameThread ? getStringOnAnyThread() : getStringOnRenderThread();
            return gp::String(copy.view());
        }
        else
        {
            return forceGameThread ? formatStringReturn(m_gameString) : formatStringReturn(m_renderString);
        }
    }

    /// @brief Returns the constructor default value.
    /// @return Initial default value.
    [[nodiscard]] ReturnType getDefaultValue() const noexcept
    {
        if constexpr (!kIsString)
        {
            return m_defaultScalar;
        }
        else
        {
            return formatStringReturn(m_defaultString.cString());
        }
    }

    using internal::ConsoleVariableBase::set;

    /// @brief Writes a null-terminated C-string literal value (disambiguates `const char*` from `bool`).
    /// @param[in] value Null-terminated UTF-8 C-string (treated as empty if `nullptr`).
    /// @param[in] setBy Caller priority tier (defaults to `ConsoleVarSetBy::Code`).
    /// @return `ConsoleResult::Success` on commit or latch stage, or a rejection code.
    ConsoleResult set(const char* const value, const ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept
        requires kIsString
    {
        return ConsoleVariableBase::setFromString(value != nullptr ? gp::StringView(value) : gp::StringView{}, setBy);
    }

    /// @brief Writes a new string value honoring `ReadOnly`, `Cheat`, `Latched`, and `setBy` priority precedence.
    /// @param[in] value New UTF-8 string value to apply.
    /// @param[in] setBy Caller priority tier (defaults to `ConsoleVarSetBy::Code`).
    /// @return `ConsoleResult::Success` on commit or latch stage, or a rejection code.
    ConsoleResult set(const gp::StringView value, const ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept
        requires kIsString
    {
        return ConsoleVariableBase::setFromString(value, setBy);
    }

protected:
    /// @copydoc ConsoleVariableBase::readSlot()
    virtual void
        readSlot(const internal::ConsoleSlotKind slot, internal::ConsoleVarPayload& outValue) const noexcept override
    {
        outValue.type = detail::resolveConsoleVarType<ValueType>();
        if constexpr (!kIsString)
        {
            ScalarStorageT val{};
            switch (slot)
            {
            case internal::ConsoleSlotKind::Default:
                val = m_defaultScalar;
                break;
            case internal::ConsoleSlotKind::Game:
                val = m_gameScalar.load(gp::MemoryOrder::Relaxed);
                break;
            case internal::ConsoleSlotKind::Render:
                val = m_renderScalar.load(gp::MemoryOrder::Acquire);
                break;
            case internal::ConsoleSlotKind::AnyThread:
                val = m_gameScalar.load(gp::MemoryOrder::Acquire);
                break;
            case internal::ConsoleSlotKind::Latched:
                val = m_latchedScalar;
                break;
            default:
                break;
            }
            assignScalarToPayload(val, outValue);
        }
        else
        {
            switch (slot)
            {
            case internal::ConsoleSlotKind::Default:
                outValue.stringValue = m_defaultString;
                break;
            case internal::ConsoleSlotKind::Game:
                outValue.stringValue.assign(gp::StringView(m_gameString));
                break;
            case internal::ConsoleSlotKind::Render:
                internal::readConsoleStringSeqlock(m_renderString, m_renderStringSeq, outValue.stringValue);
                break;
            case internal::ConsoleSlotKind::AnyThread:
                internal::readConsoleStringSeqlock(m_gameString, m_gameStringSeq, outValue.stringValue);
                break;
            case internal::ConsoleSlotKind::Latched:
                outValue.stringValue = m_latchedString;
                break;
            default:
                break;
            }
        }
    }

    /// @copydoc ConsoleVariableBase::writeSlot()
    virtual void
        writeSlot(const internal::ConsoleSlotKind slot, const internal::ConsoleVarPayload& value) noexcept override
    {
        if constexpr (!kIsString)
        {
            const ScalarStorageT val = extractScalarFromPayload(value);
            switch (slot)
            {
            case internal::ConsoleSlotKind::Game:
                m_gameScalar.store(val, gp::MemoryOrder::Release);
                break;
            case internal::ConsoleSlotKind::Render:
                m_renderScalar.store(val, gp::MemoryOrder::Release);
                break;
            case internal::ConsoleSlotKind::Latched:
                m_latchedScalar = val;
                break;
            case internal::ConsoleSlotKind::Default:
            case internal::ConsoleSlotKind::AnyThread:
            default:
                break;
            }
        }
        else
        {
            switch (slot)
            {
            case internal::ConsoleSlotKind::Game:
                internal::writeConsoleStringSeqlock(m_gameString, m_gameStringSeq, value.stringValue.view());
                break;
            case internal::ConsoleSlotKind::Render:
                internal::writeConsoleStringSeqlock(m_renderString, m_renderStringSeq, value.stringValue.view());
                break;
            case internal::ConsoleSlotKind::Latched:
                m_latchedString = value.stringValue;
                break;
            case internal::ConsoleSlotKind::Default:
            case internal::ConsoleSlotKind::AnyThread:
            default:
                break;
            }
        }
    }

    /// @copydoc ConsoleVariableBase::equalsDefault()
    [[nodiscard]] virtual bool equalsDefault(const internal::ConsoleVarPayload& value) const noexcept override
    {
        if constexpr (!kIsString)
        {
            return extractScalarFromPayload(value) == m_defaultScalar;
        }
        else
        {
            return value.stringValue.view() == m_defaultString.view();
        }
    }

private:
    void initializeDefaultValue(const ArgType defaultValue) noexcept
    {
        if constexpr (!kIsString)
        {
            m_defaultScalar = defaultValue;
            m_gameScalar.store(defaultValue, gp::MemoryOrder::Relaxed);
            m_renderScalar.store(defaultValue, gp::MemoryOrder::Relaxed);
            m_latchedScalar = defaultValue;
        }
        else
        {
            const gp::StringView clamped = defaultValue.size() > kMaxConsoleStringLength
                                               ? defaultValue.substr(0u, kMaxConsoleStringLength)
                                               : defaultValue;
            m_defaultString.assign(clamped);
            m_latchedString.assign(clamped);
            internal::writeConsoleStringSeqlock(m_gameString, m_gameStringSeq, clamped);
            internal::writeConsoleStringSeqlock(m_renderString, m_renderStringSeq, clamped);
        }
    }

    [[nodiscard]] static ReturnType formatStringReturn(const char* const buffer) noexcept requires kIsString
    {
        if constexpr (gp::concepts::IsSameAs<ValueType, gp::StringView>)
        {
            return gp::StringView(buffer);
        }
        else if constexpr (gp::concepts::IsSameAs<ValueType, const char*>)
        {
            return buffer;
        }
        else if constexpr (gp::concepts::IsSameAs<ValueType, ConsoleString>)
        {
            return ConsoleString(gp::StringView(buffer));
        }
        else
        {
            return gp::String(gp::StringView(buffer));
        }
    }

    static void assignScalarToPayload(const ScalarStorageT val, internal::ConsoleVarPayload& outValue) noexcept
        requires(!kIsString)
    {
        if constexpr (gp::concepts::IsSameAs<ValueType, bool>)
        {
            outValue.boolValue = val;
        }
        else if constexpr (gp::concepts::IsSameAs<ValueType, gp::Int32>)
        {
            outValue.intValue = val;
        }
        else if constexpr (gp::concepts::IsSameAs<ValueType, gp::UInt32>)
        {
            outValue.uintValue = val;
        }
        else if constexpr (gp::concepts::IsSameAs<ValueType, gp::Float32>)
        {
            outValue.floatValue = val;
        }
        else if constexpr (gp::concepts::IsSameAs<ValueType, gp::Float64>)
        {
            outValue.doubleValue = val;
        }
    }

    [[nodiscard]] static ScalarStorageT extractScalarFromPayload(const internal::ConsoleVarPayload& value) noexcept
        requires(!kIsString)
    {
        if constexpr (gp::concepts::IsSameAs<ValueType, bool>)
        {
            return value.boolValue;
        }
        else if constexpr (gp::concepts::IsSameAs<ValueType, gp::Int32>)
        {
            return value.intValue;
        }
        else if constexpr (gp::concepts::IsSameAs<ValueType, gp::UInt32>)
        {
            return value.uintValue;
        }
        else if constexpr (gp::concepts::IsSameAs<ValueType, gp::Float32>)
        {
            return value.floatValue;
        }
        else
        {
            return value.doubleValue;
        }
    }

private:
    ScalarStorageT m_defaultScalar{};
    gp::thread::Atomic<ScalarStorageT> m_gameScalar{};
    ScalarStorageT m_latchedScalar{};

    // Isolate the Render Thread read-mostly snapshot onto its own cache line to avoid false sharing with Game writes.
    alignas(gp::memory::HardwareConstants::cacheLineSize) gp::thread::Atomic<ScalarStorageT> m_renderScalar{};

    ConsoleString m_defaultString{};
    ConsoleString m_latchedString{};
    char m_gameString[kMaxConsoleStringLength + 1u]{};
    gp::thread::Atomic<gp::UInt32> m_gameStringSeq{ 0u };

    alignas(gp::memory::HardwareConstants::cacheLineSize) char m_renderString[kMaxConsoleStringLength + 1u]{};
    gp::thread::Atomic<gp::UInt32> m_renderStringSeq{ 0u };
};

/// @brief Self-registering console variable wrapper binding an existing caller-owned C++ variable by reference.
/// @details Supports `bool&`, `gp::Int32&`, `gp::UInt32&`, `gp::Float32&`, `gp::Float64&`, `ConsoleString&`, and
///          `gp::String&`. Captures the bound variable's value at construction as its reset default, routes console
///          writes back to the bound variable, mirrors snapshots into the Render Thread slot during
///          `syncRenderThreadValues()`, and unregisters automatically on destruction.
class GP_CORE_API AutoConsoleVariableRef final : public internal::ConsoleVariableBase
{
public:
    /// @brief Binds a `bool&` variable globally or onto the deferred static list.
    [[nodiscard]] AutoConsoleVariableRef(
        gp::StringView name, bool& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Binds a `gp::Int32&` variable globally or onto the deferred static list.
    [[nodiscard]] AutoConsoleVariableRef(
        gp::StringView name, gp::Int32& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Binds a `gp::UInt32&` variable globally or onto the deferred static list.
    [[nodiscard]] AutoConsoleVariableRef(
        gp::StringView name, gp::UInt32& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Binds a `gp::Float32&` variable globally or onto the deferred static list.
    [[nodiscard]] AutoConsoleVariableRef(
        gp::StringView name, gp::Float32& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Binds a `gp::Float64&` variable globally or onto the deferred static list.
    [[nodiscard]] AutoConsoleVariableRef(
        gp::StringView name, gp::Float64& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Binds a `ConsoleString&` buffer globally or onto the deferred static list.
    [[nodiscard]] AutoConsoleVariableRef(
        gp::StringView name, ConsoleString& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Binds a `gp::String&` variable globally or onto the deferred static list.
    [[nodiscard]] AutoConsoleVariableRef(
        gp::StringView name, gp::String& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Binds a `bool&` variable directly onto @p manager.
    [[nodiscard]] AutoConsoleVariableRef(
        IConsoleManager& manager,
        gp::StringView name,
        bool& refValue,
        gp::StringView help,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Binds a `gp::Int32&` variable directly onto @p manager.
    [[nodiscard]] AutoConsoleVariableRef(
        IConsoleManager& manager,
        gp::StringView name,
        gp::Int32& refValue,
        gp::StringView help,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Binds a `gp::UInt32&` variable directly onto @p manager.
    [[nodiscard]] AutoConsoleVariableRef(
        IConsoleManager& manager,
        gp::StringView name,
        gp::UInt32& refValue,
        gp::StringView help,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Binds a `gp::Float32&` variable directly onto @p manager.
    [[nodiscard]] AutoConsoleVariableRef(
        IConsoleManager& manager,
        gp::StringView name,
        gp::Float32& refValue,
        gp::StringView help,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Binds a `gp::Float64&` variable directly onto @p manager.
    [[nodiscard]] AutoConsoleVariableRef(
        IConsoleManager& manager,
        gp::StringView name,
        gp::Float64& refValue,
        gp::StringView help,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Binds a `ConsoleString&` variable directly onto @p manager.
    [[nodiscard]] AutoConsoleVariableRef(
        IConsoleManager& manager,
        gp::StringView name,
        ConsoleString& refValue,
        gp::StringView help,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Binds a `gp::String&` variable directly onto @p manager.
    [[nodiscard]] AutoConsoleVariableRef(
        IConsoleManager& manager,
        gp::StringView name,
        gp::String& refValue,
        gp::StringView help,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Destructor. Safely unregisters this reference variable.
    virtual ~AutoConsoleVariableRef() noexcept override;

    AutoConsoleVariableRef(const AutoConsoleVariableRef&) = delete;
    AutoConsoleVariableRef& operator=(const AutoConsoleVariableRef&) = delete;
    AutoConsoleVariableRef(AutoConsoleVariableRef&&) = delete;
    AutoConsoleVariableRef& operator=(AutoConsoleVariableRef&&) = delete;

public:
    /// @copydoc IConsoleVariable::isExternalReference()
    [[nodiscard]] virtual bool isExternalReference() const noexcept override
    {
        return true;
    }

    /// @brief Reports whether this reference variable is actively registered inside an `IConsoleManager`.
    /// @return True if registered, false otherwise.
    [[nodiscard]] bool isRegistered() const noexcept
    {
        return getOwnerManager() != nullptr;
    }

    /// @brief Returns the `IConsoleVariable*` interface pointer.
    /// @return Mutable `IConsoleVariable*`.
    [[nodiscard]] virtual IConsoleVariable* asVariable() noexcept override
    {
        return this;
    }

    /// @brief Returns a const `IConsoleVariable*` interface pointer.
    /// @return Const `IConsoleVariable*`.
    [[nodiscard]] virtual const IConsoleVariable* asVariable() const noexcept override
    {
        return this;
    }

    /// @brief Pointer-like convenience access to `IConsoleVariable` methods.
    /// @return Mutable `IConsoleVariable*`.
    [[nodiscard]] IConsoleVariable* operator->() noexcept
    {
        return this;
    }

    /// @brief Const pointer-like convenience access to `IConsoleVariable` methods.
    /// @return Const `IConsoleVariable*`.
    [[nodiscard]] const IConsoleVariable* operator->() const noexcept
    {
        return this;
    }

protected:
    /// @copydoc ConsoleVariableBase::readSlot()
    virtual void
        readSlot(internal::ConsoleSlotKind slot, internal::ConsoleVarPayload& outValue) const noexcept override;

    /// @copydoc ConsoleVariableBase::writeSlot()
    virtual void writeSlot(internal::ConsoleSlotKind slot, const internal::ConsoleVarPayload& value) noexcept override;

    /// @copydoc ConsoleVariableBase::equalsDefault()
    [[nodiscard]] virtual bool equalsDefault(const internal::ConsoleVarPayload& value) const noexcept override;

private:
    void captureInitialSnapshot() noexcept;

private:
    void* m_refPtr{ nullptr };
    bool m_isGpString{ false };
    gp::UInt64 m_defaultBits{ 0ULL };
    gp::UInt64 m_latchedBits{ 0ULL };
    ConsoleString m_defaultString{};
    ConsoleString m_latchedString{};

    alignas(gp::memory::HardwareConstants::cacheLineSize) gp::thread::Atomic<gp::UInt64> m_renderBits{ 0ULL };
    alignas(gp::memory::HardwareConstants::cacheLineSize) char m_renderString[kMaxConsoleStringLength + 1u]{};
    gp::thread::Atomic<gp::UInt32> m_renderStringSeq{ 0u };
};

/// @brief Self-registering batch callback invoked once per frame via `IConsoleManager::callAllConsoleVariableSinks()`.
/// @details Allows subsystems that depend on multiple interrelated CVars (for example renderer quality or shadow
///          cascades) to recompute derived state once after any CVar change instead of reacting per variable write.
class GP_CORE_API AutoConsoleVariableSink final
{
public:
    /// @brief Registers a sink globally or onto the deferred static queue until `IConsoleManager::get()` starts.
    /// @param[in] callback Sink callback delegate.
    [[nodiscard]] explicit AutoConsoleVariableSink(ConsoleSinkDelegate callback) noexcept;

    /// @brief Registers a sink directly onto a specific `IConsoleManager`.
    /// @param[in,out] manager Target console manager instance.
    /// @param[in] callback Sink callback delegate.
    [[nodiscard]] AutoConsoleVariableSink(IConsoleManager& manager, ConsoleSinkDelegate callback) noexcept;

    /// @brief Destructor. Automatically unregisters the sink from its manager or deferred queue.
    ~AutoConsoleVariableSink() noexcept;

    AutoConsoleVariableSink(const AutoConsoleVariableSink&) = delete;
    AutoConsoleVariableSink& operator=(const AutoConsoleVariableSink&) = delete;
    AutoConsoleVariableSink(AutoConsoleVariableSink&&) = delete;
    AutoConsoleVariableSink& operator=(AutoConsoleVariableSink&&) = delete;

public:
    /// @brief Reports whether this sink is actively registered with an `IConsoleManager`.
    /// @return True if registered with a live manager, false if deferred or invalid.
    [[nodiscard]] bool isRegistered() const noexcept
    {
        return m_ownerManager != nullptr && m_handle.isValid();
    }

    /// @brief Returns the registration handle assigned by the manager (valid when registered).
    /// @return `gp::DelegateHandle`.
    [[nodiscard]] gp::DelegateHandle getHandle() const noexcept
    {
        return m_handle;
    }

private:
    IConsoleManager* m_ownerManager{ nullptr };
    gp::DelegateHandle m_handle{};
    bool m_registeredGlobally{ false };
};

/// @brief Self-registering runtime console command supporting zero-argument and argument-accepting callbacks.
class GP_CORE_API AutoConsoleCommand final : public internal::ConsoleCommandBase
{
public:
    /// @brief Constructs and registers a zero-argument command with `IConsoleManager::get()`.
    /// @param[in] name Unique case-insensitive command name.
    /// @param[in] help Human-readable help string.
    /// @param[in] callback Bound callback taking no parameters.
    /// @param[in] flags Optional behavioral flags (`Cheat`, `Invisible`).
    [[nodiscard]] AutoConsoleCommand(
        gp::StringView name,
        gp::StringView help,
        ConsoleCommandDelegate callback,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Constructs and registers an argument-accepting command with `IConsoleManager::get()`.
    /// @param[in] name Unique case-insensitive command name.
    /// @param[in] help Human-readable help string.
    /// @param[in] callback Bound callback taking `const ConsoleCommandArgs&`.
    /// @param[in] flags Optional behavioral flags (`Cheat`, `Invisible`).
    [[nodiscard]] AutoConsoleCommand(
        gp::StringView name,
        gp::StringView help,
        ConsoleCommandWithArgsDelegate callback,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Constructs and registers a command accepting any callable matching `void(const ConsoleCommandArgs&)`
    ///        with `IConsoleManager::get()`.
    /// @tparam InCallableT Invocable type matching `void(const ConsoleCommandArgs&)`.
    /// @param[in] name Unique case-insensitive command name.
    /// @param[in] help Human-readable help string.
    /// @param[in] callback Callable object or lambda.
    /// @param[in] flags Optional behavioral flags.
    template <typename InCallableT>
    requires(!std::is_same_v<std::remove_cvref_t<InCallableT>, ConsoleCommandDelegate>) &&
            (!std::is_same_v<std::remove_cvref_t<InCallableT>, ConsoleCommandWithArgsDelegate>) &&
            std::is_invocable_r_v<void, std::remove_cvref_t<InCallableT>&, const ConsoleCommandArgs&>
    [[nodiscard]] AutoConsoleCommand(
        const gp::StringView name,
        const gp::StringView help,
        InCallableT&& callback,
        const ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept
        : ConsoleCommandBase(
              name, help, ConsoleCommandWithArgsDelegate::createLambda(std::forward<InCallableT>(callback)), flags
          )
    {
        internal::enqueueDeferredConsoleObject(this);
    }

    /// @brief Constructs and registers a command accepting any zero-argument callable matching `void()` with
    ///        `IConsoleManager::get()`.
    /// @tparam InCallableT Invocable type matching `void()`.
    /// @param[in] name Unique case-insensitive command name.
    /// @param[in] help Human-readable help string.
    /// @param[in] callback Callable object or lambda.
    /// @param[in] flags Optional behavioral flags.
    template <typename InCallableT>
    requires(!std::is_same_v<std::remove_cvref_t<InCallableT>, ConsoleCommandDelegate>) &&
            (!std::is_same_v<std::remove_cvref_t<InCallableT>, ConsoleCommandWithArgsDelegate>) &&
            (!std::is_invocable_r_v<void, std::remove_cvref_t<InCallableT>&, const ConsoleCommandArgs&>) &&
            std::is_invocable_r_v<void, std::remove_cvref_t<InCallableT>&> [[nodiscard]] AutoConsoleCommand(
        const gp::StringView name,
        const gp::StringView help,
        InCallableT&& callback,
        const ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept
        : ConsoleCommandBase(
              name, help, ConsoleCommandDelegate::createLambda(std::forward<InCallableT>(callback)), flags
          )
    {
        internal::enqueueDeferredConsoleObject(this);
    }

    /// @brief Constructs and registers a zero-argument command directly onto @p manager.
    [[nodiscard]] AutoConsoleCommand(
        IConsoleManager& manager,
        gp::StringView name,
        gp::StringView help,
        ConsoleCommandDelegate callback,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Constructs and registers an argument-accepting command directly onto @p manager.
    [[nodiscard]] AutoConsoleCommand(
        IConsoleManager& manager,
        gp::StringView name,
        gp::StringView help,
        ConsoleCommandWithArgsDelegate callback,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept;

    /// @brief Constructs and registers an argument-taking callable directly onto @p manager.
    template <typename InCallableT>
    requires(!std::is_same_v<std::remove_cvref_t<InCallableT>, ConsoleCommandDelegate>) &&
            (!std::is_same_v<std::remove_cvref_t<InCallableT>, ConsoleCommandWithArgsDelegate>) &&
            std::is_invocable_r_v<void, std::remove_cvref_t<InCallableT>&, const ConsoleCommandArgs&>
    [[nodiscard]] AutoConsoleCommand(
        IConsoleManager& manager,
        const gp::StringView name,
        const gp::StringView help,
        InCallableT&& callback,
        const ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept
        : ConsoleCommandBase(
              name, help, ConsoleCommandWithArgsDelegate::createLambda(std::forward<InCallableT>(callback)), flags
          )
    {
        (void)manager.registerExternalObject(this);
    }

    /// @brief Constructs and registers a zero-argument callable directly onto @p manager.
    template <typename InCallableT>
    requires(!std::is_same_v<std::remove_cvref_t<InCallableT>, ConsoleCommandDelegate>) &&
            (!std::is_same_v<std::remove_cvref_t<InCallableT>, ConsoleCommandWithArgsDelegate>) &&
            (!std::is_invocable_r_v<void, std::remove_cvref_t<InCallableT>&, const ConsoleCommandArgs&>) &&
            std::is_invocable_r_v<void, std::remove_cvref_t<InCallableT>&> [[nodiscard]] AutoConsoleCommand(
        IConsoleManager& manager,
        const gp::StringView name,
        const gp::StringView help,
        InCallableT&& callback,
        const ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept
        : ConsoleCommandBase(
              name, help, ConsoleCommandDelegate::createLambda(std::forward<InCallableT>(callback)), flags
          )
    {
        (void)manager.registerExternalObject(this);
    }

    /// @brief Destructor. Safely unregisters this command from its manager or deferred static queue.
    virtual ~AutoConsoleCommand() noexcept override;

    AutoConsoleCommand(const AutoConsoleCommand&) = delete;
    AutoConsoleCommand& operator=(const AutoConsoleCommand&) = delete;
    AutoConsoleCommand(AutoConsoleCommand&&) = delete;
    AutoConsoleCommand& operator=(AutoConsoleCommand&&) = delete;

public:
    /// @brief Reports whether this command is actively registered inside an `IConsoleManager`.
    /// @return True if registered, false otherwise.
    [[nodiscard]] bool isRegistered() const noexcept
    {
        return getOwnerManager() != nullptr;
    }

    /// @brief Returns the `IConsoleCommand*` interface pointer.
    /// @return Mutable `IConsoleCommand*`.
    [[nodiscard]] IConsoleCommand* asCommandInterface() noexcept
    {
        return this;
    }

    /// @brief Returns a const `IConsoleCommand*` interface pointer.
    /// @return Const `IConsoleCommand*`.
    [[nodiscard]] const IConsoleCommand* asCommandInterface() const noexcept
    {
        return this;
    }
};

}   // namespace gp::console
