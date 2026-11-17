// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "console/ConsoleDesc.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleForward.hpp"   // IWYU pragma: keep
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleCommand.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleObject.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "templates/Delegate.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/atomics/AtomicFlag.hpp"

namespace gp::console
{

namespace internal
{

/// @brief Slot selector passed to `ConsoleVariableBase::readSlot()` and `writeSlot()`.
enum class ConsoleSlotKind : gp::UInt8
{
    Default = 0u,
    Game = 1u,
    Render = 2u,
    AnyThread = 3u,
    Latched = 4u
};

/// @brief Tagged stack payload bridging typed CVar storage with shared coercion and policy logic.
struct ConsoleVarPayload final
{
    ConsoleVarType type{ ConsoleVarType::Int32 };

    union
    {
        bool boolValue;
        gp::Int32 intValue;
        gp::UInt32 uintValue;
        gp::Float32 floatValue;
        gp::Float64 doubleValue;
    };

    ConsoleString stringValue{};

    [[nodiscard]] constexpr ConsoleVarPayload() noexcept
        : doubleValue(0.0)
    {}
};

/// @brief Shared concrete base for console variables and commands owning name, help, flags, and registration links.
class GP_CORE_API ConsoleObjectBase
{
public:
    /// @brief Constructs a console object metadata block and precomputes its case-insensitive FNV-1a name hash.
    /// @param[in] name Unique console identifier.
    /// @param[in] help Human-readable help description.
    /// @param[in] flags Initial behavioral flags.
    ConsoleObjectBase(gp::StringView name, gp::StringView help, ConsoleVarFlags flags) noexcept;

    ConsoleObjectBase(const ConsoleObjectBase&) = delete;
    ConsoleObjectBase& operator=(const ConsoleObjectBase&) = delete;
    ConsoleObjectBase(ConsoleObjectBase&&) = delete;
    ConsoleObjectBase& operator=(ConsoleObjectBase&&) = delete;

    /// @brief Destructor.
    virtual ~ConsoleObjectBase() noexcept = default;

public:
    /// @brief Returns the registered lookup name.
    /// @return Name view backed by inline fixed storage.
    [[nodiscard]] gp::StringView getObjectName() const noexcept
    {
        return m_name.view();
    }

    /// @brief Returns the precomputed 64-bit case-insensitive FNV-1a name hash.
    /// @return 64-bit name hash.
    [[nodiscard]] gp::UInt64 getObjectNameHash() const noexcept
    {
        return m_nameHash;
    }

    /// @brief Returns the help text stored for this object.
    /// @return Help text view.
    [[nodiscard]] gp::StringView getObjectHelp() const noexcept
    {
        return m_help.view();
    }

    /// @brief Reads the active behavioral flags atomically.
    /// @return Current `ConsoleVarFlags` mask.
    [[nodiscard]] ConsoleVarFlags getObjectFlags() const noexcept
    {
        return m_flags.load(gp::MemoryOrder::Acquire);
    }

    /// @brief Replaces the behavioral flags atomically.
    /// @param[in] flags New flags value.
    void setObjectFlags(const ConsoleVarFlags flags) noexcept
    {
        m_flags.store(flags, gp::MemoryOrder::Release);
    }

    /// @brief Sets the bits in @p flags atomically while preserving existing bits.
    /// @param[in] flags Bits to enable.
    void addObjectFlags(const ConsoleVarFlags flags) noexcept;

    /// @brief Clears the bits in @p flags atomically while preserving remaining bits.
    /// @param[in] flags Bits to clear.
    void clearObjectFlags(const ConsoleVarFlags flags) noexcept;

    /// @brief Checks whether all bits in @p flags are active.
    /// @param[in] flags Mask to test.
    /// @return True if all requested bits are set.
    [[nodiscard]] bool hasObjectFlags(const ConsoleVarFlags flags) const noexcept
    {
        const ConsoleVarFlags current = getObjectFlags();
        return (current & flags) == flags;
    }

    /// @brief Returns the owning manager assigned at registration, or `nullptr` if unregistered.
    /// @return Owning `IConsoleManager*`.
    [[nodiscard]] IConsoleManager* getOwnerManager() const noexcept
    {
        return m_ownerManager.load(gp::MemoryOrder::Acquire);
    }

    /// @brief Assigns the owning manager pointer on registration or clears it on unregistration.
    /// @param[in] manager Owning manager pointer or `nullptr`.
    void setOwnerManager(IConsoleManager* const manager) noexcept
    {
        m_ownerManager.store(manager, gp::MemoryOrder::Release);
    }

    /// @brief Returns the `IConsoleObject*` interface pointer implemented by the derived variable or command.
    /// @return Pointer to `IConsoleObject`.
    [[nodiscard]] virtual IConsoleObject* asConsoleObject() noexcept = 0;

    /// @brief Intrusive link pointer used exclusively by the global deferred static registration list.
    ConsoleObjectBase* m_deferredNext{ nullptr };

    /// @brief Tracks whether this node currently sits on the global deferred static list.
    bool m_isInDeferredList{ false };

protected:
    /// @brief Re-initializes name, help, hash, flags, and owner pointer when recycling a pooled slot.
    void reinitializeObject(gp::StringView name, gp::StringView help, ConsoleVarFlags flags) noexcept;

private:
    gp::container::BasicFixedString<char, kMaxConsoleNameLength> m_name{};
    gp::container::BasicFixedString<char, kMaxConsoleHelpLength> m_help{};
    gp::UInt64 m_nameHash{ 0ULL };
    gp::thread::Atomic<ConsoleVarFlags> m_flags{ ConsoleVarFlags::None };
    gp::thread::Atomic<IConsoleManager*> m_ownerManager{ nullptr };
};

/// @brief Shared concrete base implementing coercion, policy enforcement, priority checks, and change delegates for
///        all `IConsoleVariable` implementations.
class GP_CORE_API ConsoleVariableBase
    : public IConsoleVariable
    , public ConsoleObjectBase
{
public:
    /// @brief Constructs a variable base with type metadata and initial flags.
    /// @param[in] type Concrete variable type.
    /// @param[in] name Unique lookup name.
    /// @param[in] help Help string.
    /// @param[in] flags Initial behavioral flags.
    ConsoleVariableBase(ConsoleVarType type, gp::StringView name, gp::StringView help, ConsoleVarFlags flags) noexcept;

    /// @brief Virtual destructor.
    virtual ~ConsoleVariableBase() noexcept override = default;

public:
    /// @copydoc IConsoleObject::getName()
    [[nodiscard]] virtual gp::StringView getName() const noexcept override
    {
        return getObjectName();
    }

    /// @copydoc IConsoleObject::getNameHash()
    [[nodiscard]] virtual gp::UInt64 getNameHash() const noexcept override
    {
        return getObjectNameHash();
    }

    /// @copydoc IConsoleObject::getHelp()
    [[nodiscard]] virtual gp::StringView getHelp() const noexcept override
    {
        return getObjectHelp();
    }

    /// @copydoc IConsoleObject::getFlags()
    [[nodiscard]] virtual ConsoleVarFlags getFlags() const noexcept override
    {
        return getObjectFlags();
    }

    /// @copydoc IConsoleObject::setFlags()
    virtual void setFlags(ConsoleVarFlags flags) noexcept override
    {
        setObjectFlags(flags);
    }

    /// @copydoc IConsoleObject::addFlags()
    virtual void addFlags(ConsoleVarFlags flags) noexcept override
    {
        addObjectFlags(flags);
    }

    /// @copydoc IConsoleObject::clearFlags()
    virtual void clearFlags(ConsoleVarFlags flags) noexcept override
    {
        clearObjectFlags(flags);
    }

    /// @copydoc IConsoleObject::hasFlags()
    [[nodiscard]] virtual bool hasFlags(ConsoleVarFlags flags) const noexcept override
    {
        return hasObjectFlags(flags);
    }

    /// @copydoc IConsoleObject::asVariable()
    [[nodiscard]] virtual IConsoleVariable* asVariable() noexcept override
    {
        return this;
    }

    /// @copydoc IConsoleObject::asVariable()
    [[nodiscard]] virtual const IConsoleVariable* asVariable() const noexcept override
    {
        return this;
    }

    /// @copydoc IConsoleObject::asCommand()
    [[nodiscard]] virtual IConsoleCommand* asCommand() noexcept override
    {
        return nullptr;
    }

    /// @copydoc IConsoleObject::asCommand()
    [[nodiscard]] virtual const IConsoleCommand* asCommand() const noexcept override
    {
        return nullptr;
    }

    /// @copydoc ConsoleObjectBase::asConsoleObject()
    [[nodiscard]] virtual IConsoleObject* asConsoleObject() noexcept override
    {
        return this;
    }

    /// @copydoc IConsoleObject::onRegistered()
    virtual void onRegistered(IConsoleManager* const manager) noexcept override
    {
        setOwnerManager(manager);
    }

    /// @copydoc IConsoleObject::onUnregistered()
    virtual void onUnregistered() noexcept override
    {
        setOwnerManager(nullptr);
    }

    /// @copydoc IConsoleVariable::getType()
    [[nodiscard]] virtual ConsoleVarType getType() const noexcept override
    {
        return m_type;
    }

    /// @copydoc IConsoleVariable::getSetBy()
    [[nodiscard]] virtual ConsoleVarSetBy getSetBy() const noexcept override
    {
        return m_setBy.load(gp::MemoryOrder::Acquire);
    }

    /// @copydoc IConsoleVariable::isModified()
    [[nodiscard]] virtual bool isModified() const noexcept override
    {
        return hasObjectFlags(ConsoleVarFlags::Modified);
    }

    /// @copydoc IConsoleVariable::isRenderThreadSafe()
    [[nodiscard]] virtual bool isRenderThreadSafe() const noexcept override
    {
        return hasObjectFlags(ConsoleVarFlags::RenderThreadSafe);
    }

    /// @copydoc IConsoleVariable::isLatched()
    [[nodiscard]] virtual bool isLatched() const noexcept override
    {
        return hasObjectFlags(ConsoleVarFlags::Latched);
    }

    /// @copydoc IConsoleVariable::hasPendingLatchedValue()
    [[nodiscard]] virtual bool hasPendingLatchedValue() const noexcept override
    {
        return m_hasPendingLatched.load(gp::MemoryOrder::Acquire);
    }

    /// @copydoc IConsoleVariable::hasPendingRenderSync()
    [[nodiscard]] virtual bool hasPendingRenderSync() const noexcept override
    {
        return m_hasPendingRenderSync.load(gp::MemoryOrder::Acquire);
    }

    /// @copydoc IConsoleVariable::getBool()
    [[nodiscard]] virtual bool getBool() const noexcept override;

    /// @copydoc IConsoleVariable::getInt()
    [[nodiscard]] virtual gp::Int32 getInt() const noexcept override;

    /// @copydoc IConsoleVariable::getUInt()
    [[nodiscard]] virtual gp::UInt32 getUInt() const noexcept override;

    /// @copydoc IConsoleVariable::getFloat()
    [[nodiscard]] virtual gp::Float32 getFloat() const noexcept override;

    /// @copydoc IConsoleVariable::getFloat64()
    [[nodiscard]] virtual gp::Float64 getFloat64() const noexcept override;

    /// @copydoc IConsoleVariable::getString()
    [[nodiscard]] virtual ConsoleString getString() const noexcept override;

    /// @copydoc IConsoleVariable::getDefaultString()
    [[nodiscard]] virtual ConsoleString getDefaultString() const noexcept override;

    /// @copydoc IConsoleVariable::getBoolOnRenderThread()
    [[nodiscard]] virtual bool getBoolOnRenderThread() const noexcept override;

    /// @copydoc IConsoleVariable::getIntOnRenderThread()
    [[nodiscard]] virtual gp::Int32 getIntOnRenderThread() const noexcept override;

    /// @copydoc IConsoleVariable::getUIntOnRenderThread()
    [[nodiscard]] virtual gp::UInt32 getUIntOnRenderThread() const noexcept override;

    /// @copydoc IConsoleVariable::getFloatOnRenderThread()
    [[nodiscard]] virtual gp::Float32 getFloatOnRenderThread() const noexcept override;

    /// @copydoc IConsoleVariable::getFloat64OnRenderThread()
    [[nodiscard]] virtual gp::Float64 getFloat64OnRenderThread() const noexcept override;

    /// @copydoc IConsoleVariable::getStringOnRenderThread()
    [[nodiscard]] virtual ConsoleString getStringOnRenderThread() const noexcept override;

    /// @copydoc IConsoleVariable::getBoolOnAnyThread()
    [[nodiscard]] virtual bool getBoolOnAnyThread() const noexcept override;

    /// @copydoc IConsoleVariable::getIntOnAnyThread()
    [[nodiscard]] virtual gp::Int32 getIntOnAnyThread() const noexcept override;

    /// @copydoc IConsoleVariable::getUIntOnAnyThread()
    [[nodiscard]] virtual gp::UInt32 getUIntOnAnyThread() const noexcept override;

    /// @copydoc IConsoleVariable::getFloatOnAnyThread()
    [[nodiscard]] virtual gp::Float32 getFloatOnAnyThread() const noexcept override;

    /// @copydoc IConsoleVariable::getFloat64OnAnyThread()
    [[nodiscard]] virtual gp::Float64 getFloat64OnAnyThread() const noexcept override;

    /// @copydoc IConsoleVariable::getStringOnAnyThread()
    [[nodiscard]] virtual ConsoleString getStringOnAnyThread() const noexcept override;

    /// @copydoc IConsoleVariable::set(bool, ConsoleVarSetBy)
    virtual ConsoleResult set(bool value, ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept override;

    /// @copydoc IConsoleVariable::set(gp::Int32, ConsoleVarSetBy)
    virtual ConsoleResult set(gp::Int32 value, ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept override;

    /// @copydoc IConsoleVariable::set(gp::UInt32, ConsoleVarSetBy)
    virtual ConsoleResult set(gp::UInt32 value, ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept override;

    /// @copydoc IConsoleVariable::set(gp::Float32, ConsoleVarSetBy)
    virtual ConsoleResult set(gp::Float32 value, ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept override;

    /// @copydoc IConsoleVariable::set(gp::Float64, ConsoleVarSetBy)
    virtual ConsoleResult set(gp::Float64 value, ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept override;

    /// @copydoc IConsoleVariable::setFromString()
    virtual ConsoleResult
        setFromString(gp::StringView value, ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept override;

    /// @copydoc IConsoleVariable::reset()
    virtual ConsoleResult reset(ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept override;

    /// @copydoc IConsoleVariable::setOnChangedCallback()
    virtual void setOnChangedCallback(const ConsoleVarChangedDelegate& callback) noexcept override;

    /// @copydoc IConsoleVariable::getOnChangedCallback()
    [[nodiscard]] virtual const ConsoleVarChangedDelegate& getOnChangedCallback() const noexcept override
    {
        return m_primaryCallback;
    }

    /// @copydoc IConsoleVariable::addOnChangedCallback()
    [[nodiscard]] virtual gp::DelegateHandle addOnChangedCallback(ConsoleVarChangedDelegate callback) noexcept override;

    /// @copydoc IConsoleVariable::removeOnChangedCallback()
    virtual bool removeOnChangedCallback(gp::DelegateHandle handle) noexcept override;

    /// @copydoc IConsoleVariable::clearOnChangedCallbacks()
    virtual void clearOnChangedCallbacks() noexcept override;

    /// @copydoc IConsoleVariable::syncRenderThreadValue()
    virtual void syncRenderThreadValue() noexcept override;

    /// @copydoc IConsoleVariable::applyLatchedValue()
    virtual bool applyLatchedValue() noexcept override;

protected:
    /// @brief Reads the typed value from the selected storage slot (`Default`, `Game`, `Render`, `AnyThread`, or
    ///        `Latched`).
    /// @param[in] slot Target slot identifier.
    /// @param[out] outValue Receives the slot's type and value.
    virtual void readSlot(ConsoleSlotKind slot, ConsoleVarPayload& outValue) const noexcept = 0;

    /// @brief Writes @p value into the selected storage slot.
    /// @param[in] slot Target slot identifier.
    /// @param[in] value Payload matching `getType()`.
    virtual void writeSlot(ConsoleSlotKind slot, const ConsoleVarPayload& value) noexcept = 0;

    /// @brief Checks whether @p value equals the constructor default value.
    /// @param[in] value Candidate payload matching `getType()`.
    /// @return True if equal to constructor default, false otherwise.
    [[nodiscard]] virtual bool equalsDefault(const ConsoleVarPayload& value) const noexcept = 0;

    /// @brief Checks write permissions (`ReadOnly`, `Cheat`, `setBy` priority) before mutating this variable.
    /// @param[in] setBy Caller priority tier.
    /// @return `ConsoleResult::Success` if write is allowed, or the blocking failure code.
    [[nodiscard]] ConsoleResult validateWritePolicy(ConsoleVarSetBy setBy) const noexcept;

    /// @brief Commits a typed payload onto the Game Thread (or stages into `Latched`), fires change delegates, and
    ///        notifies the owning manager.
    /// @param[in] payload Typed value payload matching `getType()`.
    /// @param[in] setBy Priority tier of the write.
    /// @return `ConsoleResult::Success` on commit or stage, or a rejection code.
    ConsoleResult applyTypedWrite(const ConsoleVarPayload& payload, ConsoleVarSetBy setBy) noexcept;

    /// @brief Fires the primary and multicast change callbacks outside any internal lock and notifies the manager.
    void dispatchChangedNotifications() noexcept;

    /// @brief Updates `m_setBy` and clears `Modified` / `Latched` flags.
    void resetPolicyMetadata() noexcept;

    /// @brief Re-initializes a recycled variable slot in place without placement-new on `this`.
    void reinitializeVariable(
        ConsoleVarType type, gp::StringView name, gp::StringView help, ConsoleVarFlags flags
    ) noexcept;

    /// @brief Marks whether the Render Thread slot is out of sync with the Game Thread slot.
    void setPendingRenderSync(const bool pending) noexcept
    {
        m_hasPendingRenderSync.store(pending, gp::MemoryOrder::Release);
    }

private:
    ConsoleVarType m_type{ ConsoleVarType::Int32 };
    gp::thread::Atomic<ConsoleVarSetBy> m_setBy{ ConsoleVarSetBy::Constructor };
    gp::thread::Atomic<ConsoleVarSetBy> m_pendingLatchedSetBy{ ConsoleVarSetBy::Constructor };
    gp::thread::Atomic<bool> m_hasPendingLatched{ false };
    gp::thread::Atomic<bool> m_hasPendingRenderSync{ false };
    mutable gp::thread::AtomicFlag m_callbackLock{};
    ConsoleVarChangedDelegate m_primaryCallback{};
    gp::FixedMulticastDelegate<kMaxConsoleCallbacksPerVariable, IConsoleVariable*> m_multicastCallbacks{};
};

/// @brief Shared concrete base implementing `IConsoleCommand` policy and callback dispatch.
class GP_CORE_API ConsoleCommandBase
    : public IConsoleCommand
    , public ConsoleObjectBase
{
public:
    /// @brief Constructs a zero-argument command.
    ConsoleCommandBase(
        gp::StringView name, gp::StringView help, ConsoleCommandDelegate callback, ConsoleVarFlags flags
    ) noexcept;

    /// @brief Constructs an argument-accepting command.
    ConsoleCommandBase(
        gp::StringView name, gp::StringView help, ConsoleCommandWithArgsDelegate callback, ConsoleVarFlags flags
    ) noexcept;

    /// @brief Virtual destructor.
    virtual ~ConsoleCommandBase() noexcept override = default;

public:
    /// @copydoc IConsoleObject::getName()
    [[nodiscard]] virtual gp::StringView getName() const noexcept override
    {
        return getObjectName();
    }

    /// @copydoc IConsoleObject::getNameHash()
    [[nodiscard]] virtual gp::UInt64 getNameHash() const noexcept override
    {
        return getObjectNameHash();
    }

    /// @copydoc IConsoleObject::getHelp()
    [[nodiscard]] virtual gp::StringView getHelp() const noexcept override
    {
        return getObjectHelp();
    }

    /// @copydoc IConsoleObject::getFlags()
    [[nodiscard]] virtual ConsoleVarFlags getFlags() const noexcept override
    {
        return getObjectFlags();
    }

    /// @copydoc IConsoleObject::setFlags()
    virtual void setFlags(ConsoleVarFlags flags) noexcept override
    {
        setObjectFlags(flags);
    }

    /// @copydoc IConsoleObject::addFlags()
    virtual void addFlags(ConsoleVarFlags flags) noexcept override
    {
        addObjectFlags(flags);
    }

    /// @copydoc IConsoleObject::clearFlags()
    virtual void clearFlags(ConsoleVarFlags flags) noexcept override
    {
        clearObjectFlags(flags);
    }

    /// @copydoc IConsoleObject::hasFlags()
    [[nodiscard]] virtual bool hasFlags(ConsoleVarFlags flags) const noexcept override
    {
        return hasObjectFlags(flags);
    }

    /// @copydoc IConsoleObject::asVariable()
    [[nodiscard]] virtual IConsoleVariable* asVariable() noexcept override
    {
        return nullptr;
    }

    /// @copydoc IConsoleObject::asVariable()
    [[nodiscard]] virtual const IConsoleVariable* asVariable() const noexcept override
    {
        return nullptr;
    }

    /// @copydoc IConsoleObject::asCommand()
    [[nodiscard]] virtual IConsoleCommand* asCommand() noexcept override
    {
        return this;
    }

    /// @copydoc IConsoleObject::asCommand()
    [[nodiscard]] virtual const IConsoleCommand* asCommand() const noexcept override
    {
        return this;
    }

    /// @copydoc ConsoleObjectBase::asConsoleObject()
    [[nodiscard]] virtual IConsoleObject* asConsoleObject() noexcept override
    {
        return this;
    }

    /// @copydoc IConsoleObject::onRegistered()
    virtual void onRegistered(IConsoleManager* const manager) noexcept override
    {
        setOwnerManager(manager);
    }

    /// @copydoc IConsoleObject::onUnregistered()
    virtual void onUnregistered() noexcept override
    {
        setOwnerManager(nullptr);
    }

    /// @copydoc IConsoleCommand::execute()
    virtual ConsoleResult execute(const ConsoleCommandArgs& args) noexcept override;

    /// @copydoc IConsoleCommand::isEnabled()
    [[nodiscard]] virtual bool isEnabled() const noexcept override;

protected:
    /// @brief Rebinds this command slot in place for pooled dynamic commands.
    void rebind(
        gp::StringView name, gp::StringView help, ConsoleCommandDelegate callback, ConsoleVarFlags flags
    ) noexcept;

    /// @brief Rebinds this command slot with an argument-taking callback.
    void rebind(
        gp::StringView name, gp::StringView help, ConsoleCommandWithArgsDelegate callback, ConsoleVarFlags flags
    ) noexcept;

    /// @brief Releases stored callbacks when a dynamic command slot is recycled.
    void resetCommandSlot() noexcept;

private:
    ConsoleCommandDelegate m_voidCallback{};
    ConsoleCommandWithArgsDelegate m_argsCallback{};
    bool m_takesArgs{ false };
};

/// @brief Fixed-pool concrete console variable supporting all 6 types and external reference bindings.
class GP_CORE_API ConsoleDynamicVariable final : public ConsoleVariableBase
{
public:
    /// @brief Constructs an inactive pool slot.
    [[nodiscard]] ConsoleDynamicVariable() noexcept;

    /// @brief Destructor.
    virtual ~ConsoleDynamicVariable() noexcept override = default;

public:
    /// @copydoc IConsoleVariable::isExternalReference()
    [[nodiscard]] virtual bool isExternalReference() const noexcept override
    {
        return m_refPtr != nullptr;
    }

    /// @brief Reports whether this pool slot is currently leased to a live dynamic CVar.
    [[nodiscard]] bool isInUse() const noexcept
    {
        return m_inUse;
    }

    /// @brief Initializes this slot as a self-contained scalar or string variable.
    void initializeOwned(
        ConsoleVarType type,
        gp::StringView name,
        const ConsoleVarPayload& defaultValue,
        gp::StringView help,
        ConsoleVarFlags flags
    ) noexcept;

    /// @brief Initializes this slot as a variable backed by an external C++ reference pointer.
    void initializeRef(
        ConsoleVarType type,
        gp::StringView name,
        void* refPtr,
        bool isGpStringRef,
        gp::StringView help,
        ConsoleVarFlags flags
    ) noexcept;

    /// @brief Clears this slot and marks it available for future dynamic allocations.
    void releaseSlot() noexcept;

protected:
    /// @copydoc ConsoleVariableBase::readSlot()
    virtual void readSlot(ConsoleSlotKind slot, ConsoleVarPayload& outValue) const noexcept override;

    /// @copydoc ConsoleVariableBase::writeSlot()
    virtual void writeSlot(ConsoleSlotKind slot, const ConsoleVarPayload& value) noexcept override;

    /// @copydoc ConsoleVariableBase::equalsDefault()
    [[nodiscard]] virtual bool equalsDefault(const ConsoleVarPayload& value) const noexcept override;

private:
    gp::UInt64 m_defaultBits{ 0ULL };
    gp::thread::Atomic<gp::UInt64> m_gameBits{ 0ULL };
    gp::UInt64 m_latchedBits{ 0ULL };

    // Keep Render-Thread snapshots on an isolated cache line so Game writes never invalidate concurrent RDG loads.
    alignas(gp::memory::HardwareConstants::cacheLineSize) gp::thread::Atomic<gp::UInt64> m_renderBits{ 0ULL };

    ConsoleString m_defaultString{};
    ConsoleString m_latchedString{};
    char m_gameString[kMaxConsoleStringLength + 1u]{};
    gp::thread::Atomic<gp::UInt32> m_gameStringSeq{ 0u };

    alignas(gp::memory::HardwareConstants::cacheLineSize) char m_renderString[kMaxConsoleStringLength + 1u]{};
    gp::thread::Atomic<gp::UInt32> m_renderStringSeq{ 0u };

    void* m_refPtr{ nullptr };
    bool m_isGpStringRef{ false };
    bool m_inUse{ false };
};

/// @brief Fixed-pool concrete console command node used by `IConsoleManager::registerConsoleCommand()`.
class GP_CORE_API ConsoleDynamicCommand final : public ConsoleCommandBase
{
public:
    /// @brief Constructs an inactive command pool slot.
    [[nodiscard]] ConsoleDynamicCommand() noexcept;

    /// @brief Destructor.
    virtual ~ConsoleDynamicCommand() noexcept override = default;

public:
    /// @brief Reports whether this command slot is currently leased.
    [[nodiscard]] bool isInUse() const noexcept
    {
        return m_inUse;
    }

    /// @brief Leases this slot with a zero-argument command callback.
    void initialize(
        gp::StringView name, gp::StringView help, ConsoleCommandDelegate callback, ConsoleVarFlags flags
    ) noexcept;

    /// @brief Leases this slot with an argument-taking command callback.
    void initialize(
        gp::StringView name, gp::StringView help, ConsoleCommandWithArgsDelegate callback, ConsoleVarFlags flags
    ) noexcept;

    /// @brief Releases this pool slot.
    void releaseSlot() noexcept;

private:
    bool m_inUse{ false };
};

/// @brief Open-addressed hash table slot storing a precomputed 64-bit name hash and `IConsoleObject*` pointer.
struct ConsoleHashSlot final
{
    gp::UInt64 nameHash{ 0ULL };
    IConsoleObject* object{ nullptr };
};

/// @brief Registered sink entry pairing a callback with a generation-checked `DelegateHandle`.
struct ConsoleSinkSlot final
{
    ConsoleSinkDelegate callback{};
    gp::DelegateHandle handle{};
    gp::UInt32 generation{ 1u };
    bool isActive{ false };
};

/// @brief Non-owning view of caller-provided backing arrays supplied to `ConsoleManagerBase`.
struct ConsoleManagerStorageView final
{
    ConsoleHashSlot* hashSlots{ nullptr };
    gp::USize hashSlotCapacity{ 0u };

    ConsoleDynamicVariable* dynamicVariables{ nullptr };
    gp::USize dynamicVariableCapacity{ 0u };

    ConsoleDynamicCommand* dynamicCommands{ nullptr };
    gp::USize dynamicCommandCapacity{ 0u };

    ConsoleSinkSlot* sinkSlots{ nullptr };
    gp::USize sinkCapacity{ 0u };

    IConsoleVariable** dirtyRenderQueue{ nullptr };
    gp::USize dirtyRenderCapacity{ 0u };

    IConsoleVariable** dirtyLatchedQueue{ nullptr };
    gp::USize dirtyLatchedCapacity{ 0u };

    IConsoleVariable** refVariables{ nullptr };
    gp::USize refVariableCapacity{ 0u };
};

/// @brief Packs a scalar `ConsoleVarPayload` bit-pattern into a 64-bit atomic storage word.
[[nodiscard]] GP_CORE_API gp::UInt64
    scalarPayloadToBits(ConsoleVarType type, const ConsoleVarPayload& payload) noexcept;

/// @brief Unpacks a 64-bit storage word into the typed scalar member of @p outPayload.
GP_CORE_API void bitsToScalarPayload(ConsoleVarType type, gp::UInt64 bits, ConsoleVarPayload& outPayload) noexcept;

/// @brief Reads a tear-free string snapshot from a seqlock-guarded character buffer.
GP_CORE_API void readConsoleStringSeqlock(
    const char* buffer, const gp::thread::Atomic<gp::UInt32>& seq, ConsoleString& outStr
) noexcept;

/// @brief Writes @p str into a fixed buffer guarded by a seqlock sequence counter.
GP_CORE_API void
    writeConsoleStringSeqlock(char* buffer, gp::thread::Atomic<gp::UInt32>& seq, gp::StringView str) noexcept;

/// @brief Reads a live external C++ variable referenced by `refPtr` into @p outValue.
GP_CORE_API void readExternalRefValue(
    ConsoleVarType type, const void* refPtr, bool isGpString, ConsoleVarPayload& outValue
) noexcept;

/// @brief Writes @p value into an external C++ variable referenced by `refPtr`.
GP_CORE_API void
    writeExternalRefValue(ConsoleVarType type, void* refPtr, bool isGpString, const ConsoleVarPayload& value) noexcept;

/// @brief Enqueues a static `AutoConsole*` object onto the `.bss` deferred list or registers immediately if ready.
GP_CORE_API void enqueueDeferredConsoleObject(ConsoleObjectBase* object) noexcept;

/// @brief Removes a static or scoped `AutoConsole*` object from either the deferred list or its owning manager.
GP_CORE_API void unregisterAutoConsoleObject(ConsoleObjectBase* object) noexcept;

/// @brief Enqueues an `AutoConsoleVariableSink` globally or registers immediately if the global manager is ready.
GP_CORE_API gp::DelegateHandle
    registerGlobalOrDeferredSink(ConsoleSinkDelegate callback, bool& outInDeferredList) noexcept;

/// @brief Unregisters a global/deferred sink safely at destruction.
GP_CORE_API void unregisterGlobalOrDeferredSink(gp::DelegateHandle handle) noexcept;

}   // namespace internal

/// @brief Non-templated console registry engine shared by both the global singleton and stack-allocated test managers.
class GP_CORE_API ConsoleManagerBase : public IConsoleManager
{
    friend class IConsoleManager;

public:
    /// @brief Constructs a console manager operating over the storage arrays described by @p storage.
    /// @param[in] storage Non-owning spans of backing arrays (must outlive this `ConsoleManagerBase`).
    [[nodiscard]] explicit ConsoleManagerBase(const internal::ConsoleManagerStorageView& storage) noexcept;

    /// @brief Destructor. Shuts down if still initialized.
    virtual ~ConsoleManagerBase() noexcept override;

    ConsoleManagerBase(const ConsoleManagerBase&) = delete;
    ConsoleManagerBase& operator=(const ConsoleManagerBase&) = delete;
    ConsoleManagerBase(ConsoleManagerBase&&) = delete;
    ConsoleManagerBase& operator=(ConsoleManagerBase&&) = delete;

public:
    /// @copydoc IConsoleManager::initialize()
    virtual ConsoleResult initialize(const ConsoleManagerDesc& desc = {}) noexcept override;

    /// @copydoc IConsoleManager::shutdown()
    virtual void shutdown() noexcept override;

    /// @copydoc IConsoleManager::isInitialized()
    [[nodiscard]] virtual bool isInitialized() const noexcept override
    {
        return m_isInitialized.load(gp::MemoryOrder::Acquire);
    }

    /// @copydoc IConsoleManager::setConsoleEnabled()
    virtual void setConsoleEnabled(bool enabled) noexcept override
    {
        m_consoleEnabled.store(enabled, gp::MemoryOrder::Release);
    }

    /// @copydoc IConsoleManager::isConsoleEnabled()
    [[nodiscard]] virtual bool isConsoleEnabled() const noexcept override
    {
        return m_consoleEnabled.load(gp::MemoryOrder::Acquire);
    }

    /// @copydoc IConsoleManager::setCheatsEnabled()
    virtual void setCheatsEnabled(bool enabled) noexcept override
    {
        m_cheatsEnabled.store(enabled, gp::MemoryOrder::Release);
    }

    /// @copydoc IConsoleManager::areCheatsEnabled()
    [[nodiscard]] virtual bool areCheatsEnabled() const noexcept override
    {
        return m_cheatsEnabled.load(gp::MemoryOrder::Acquire);
    }

    /// @copydoc IConsoleManager::registerConsoleVariable(gp::StringView, bool, gp::StringView, ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariable(
        gp::StringView name, bool defaultValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleVariable(gp::StringView, gp::Int32, gp::StringView, ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariable(
        gp::StringView name, gp::Int32 defaultValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleVariable(gp::StringView, gp::UInt32, gp::StringView, ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariable(
        gp::StringView name, gp::UInt32 defaultValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleVariable(gp::StringView, gp::Float32, gp::StringView, ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariable(
        gp::StringView name,
        gp::Float32 defaultValue,
        gp::StringView help,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleVariable(gp::StringView, gp::Float64, gp::StringView, ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariable(
        gp::StringView name,
        gp::Float64 defaultValue,
        gp::StringView help,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleVariable(gp::StringView, gp::StringView, gp::StringView,
    /// ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariable(
        gp::StringView name,
        gp::StringView defaultValue,
        gp::StringView help,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleVariableRef(gp::StringView, bool&, gp::StringView, ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, bool& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleVariableRef(gp::StringView, gp::Int32&, gp::StringView,
    /// ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, gp::Int32& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleVariableRef(gp::StringView, gp::UInt32&, gp::StringView,
    /// ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, gp::UInt32& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleVariableRef(gp::StringView, gp::Float32&, gp::StringView,
    /// ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, gp::Float32& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleVariableRef(gp::StringView, gp::Float64&, gp::StringView,
    /// ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, gp::Float64& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleVariableRef(gp::StringView, ConsoleString&, gp::StringView,
    /// ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, ConsoleString& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleVariableRef(gp::StringView, gp::String&, gp::StringView,
    /// ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, gp::String& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleCommand(gp::StringView, gp::StringView, ConsoleCommandDelegate,
    /// ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleCommand* registerConsoleCommand(
        gp::StringView name,
        gp::StringView help,
        ConsoleCommandDelegate command,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleCommand(gp::StringView, gp::StringView, ConsoleCommandWithArgsDelegate,
    /// ConsoleVarFlags)
    [[nodiscard]] virtual IConsoleCommand* registerConsoleCommand(
        gp::StringView name,
        gp::StringView help,
        ConsoleCommandWithArgsDelegate command,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept override;

    /// @copydoc IConsoleManager::registerExternalObject()
    virtual ConsoleResult registerExternalObject(IConsoleObject* object) noexcept override;

    /// @copydoc IConsoleManager::unregisterConsoleObject(gp::StringView)
    virtual bool unregisterConsoleObject(gp::StringView name) noexcept override;

    /// @copydoc IConsoleManager::unregisterConsoleObject(IConsoleObject*)
    virtual bool unregisterConsoleObject(IConsoleObject* object) noexcept override;

    /// @copydoc IConsoleManager::registerConsoleVariableSink()
    [[nodiscard]] virtual gp::DelegateHandle registerConsoleVariableSink(ConsoleSinkDelegate sink) noexcept override;

    /// @copydoc IConsoleManager::unregisterConsoleVariableSink()
    virtual bool unregisterConsoleVariableSink(gp::DelegateHandle handle) noexcept override;

    /// @copydoc IConsoleManager::notifyVariableChanged()
    virtual void notifyVariableChanged(IConsoleVariable* variable) noexcept override;

    /// @copydoc IConsoleManager::notifyVariableLatched()
    virtual void notifyVariableLatched(IConsoleVariable* variable) noexcept override;

    /// @copydoc IConsoleManager::findConsoleObject()
    [[nodiscard]] virtual IConsoleObject* findConsoleObject(gp::StringView name) const noexcept override;

    /// @copydoc IConsoleManager::findConsoleVariable()
    [[nodiscard]] virtual IConsoleVariable* findConsoleVariable(gp::StringView name) const noexcept override;

    /// @copydoc IConsoleManager::findConsoleCommand()
    [[nodiscard]] virtual IConsoleCommand* findConsoleCommand(gp::StringView name) const noexcept override;

    /// @copydoc IConsoleManager::forEachConsoleObjectStartingWith()
    virtual void forEachConsoleObjectStartingWith(
        const ConsoleObjectVisitor& visitor, gp::StringView prefix = {}, bool includeInvisible = false
    ) const noexcept override;

    /// @copydoc IConsoleManager::autoComplete()
    [[nodiscard]] virtual gp::USize autoComplete(
        gp::StringView prefix, gp::StringView* outNames, gp::USize maxResults, bool includeInvisible = false
    ) const noexcept override;

    /// @copydoc IConsoleManager::getVariableCount()
    [[nodiscard]] virtual gp::USize getVariableCount() const noexcept override
    {
        return m_variableCount.load(gp::MemoryOrder::Acquire);
    }

    /// @copydoc IConsoleManager::getCommandCount()
    [[nodiscard]] virtual gp::USize getCommandCount() const noexcept override
    {
        return m_commandCount.load(gp::MemoryOrder::Acquire);
    }

    /// @copydoc IConsoleManager::getSinkCount()
    [[nodiscard]] virtual gp::USize getSinkCount() const noexcept override
    {
        return m_sinkCount.load(gp::MemoryOrder::Acquire);
    }

    /// @copydoc IConsoleManager::processUserConsoleInput()
    virtual ConsoleResult
        processUserConsoleInput(gp::StringView input, ConsoleOutputDelegate* output = nullptr) noexcept override;

    /// @copydoc IConsoleManager::applyCommandLineOverrides()
    virtual gp::USize applyCommandLineOverrides(ConsoleVarSetBy setBy = ConsoleVarSetBy::CommandLine) noexcept override;

    /// @copydoc IConsoleManager::syncRenderThreadValues()
    virtual gp::USize syncRenderThreadValues() noexcept override;

    /// @copydoc IConsoleManager::flushLatchedVariables()
    virtual gp::USize flushLatchedVariables() noexcept override;

    /// @copydoc IConsoleManager::callAllConsoleVariableSinks()
    virtual gp::USize callAllConsoleVariableSinks(bool force = false) noexcept override;

    /// @copydoc IConsoleManager::resetAllVariables()
    virtual gp::USize resetAllVariables(ConsoleVarSetBy maxPriority = ConsoleVarSetBy::Console) noexcept override;

protected:
    /// @brief Registers built-in diagnostic commands (`help`, `cvar.list`, `cvar.dump`, `cvar.reset`).
    void registerBuiltInDiagnostics() noexcept;

    /// @brief Marks this instance as the process-wide global singleton so it drains deferred static registrations.
    void markAsGlobalSingleton() noexcept
    {
        m_isGlobalSingleton = true;
    }

    /// @brief Drains any static `AutoConsole*` objects and sinks queued prior to global initialization.
    void drainGlobalDeferredQueue() noexcept;

private:
    [[nodiscard]] IConsoleVariable* registerDynamicVariableImpl(
        ConsoleVarType type,
        gp::StringView name,
        const internal::ConsoleVarPayload& defaultValue,
        gp::StringView help,
        ConsoleVarFlags flags
    ) noexcept;

    [[nodiscard]] IConsoleVariable* registerDynamicVariableRefImpl(
        ConsoleVarType type,
        gp::StringView name,
        void* refPtr,
        bool isGpStringRef,
        gp::StringView help,
        ConsoleVarFlags flags
    ) noexcept;

    [[nodiscard]] internal::ConsoleDynamicVariable* acquireDynamicVariableSlotLocked(gp::StringView name) noexcept;
    [[nodiscard]] internal::ConsoleDynamicCommand* acquireDynamicCommandSlotLocked(gp::StringView name) noexcept;
    [[nodiscard]] IConsoleObject* findObjectLocked(gp::StringView name, gp::UInt64 nameHash) const noexcept;
    [[nodiscard]] ConsoleResult insertObjectLocked(IConsoleObject* object) noexcept;
    [[nodiscard]] bool removeObjectLocked(IConsoleObject* object) noexcept;
    void removeFromQueueLocked(IConsoleVariable** queue, gp::USize& count, IConsoleVariable* target) noexcept;
    void applySingleCommandLineOverride(IConsoleVariable* variable, ConsoleVarSetBy setBy) noexcept;
    void acquireLock() const noexcept;
    void releaseLock() const noexcept;

private:
    internal::ConsoleManagerStorageView m_storage{};
    gp::USize m_hashMask{ 0u };
    gp::USize m_objectCount{ 0u };
    gp::USize m_dirtyRenderCount{ 0u };
    gp::USize m_dirtyLatchedCount{ 0u };
    gp::USize m_refVariableCount{ 0u };
    bool m_dirtyRenderOverflow{ false };
    bool m_dirtyLatchedOverflow{ false };
    gp::thread::Atomic<bool> m_sinksDirty{ false };
    bool m_applyCliOnRegister{ false };
    bool m_ownsBuiltInCommands{ false };
    bool m_isGlobalSingleton{ false };
    ConsoleVarSetBy m_cliOverrideSetBy{ ConsoleVarSetBy::CommandLine };

    mutable gp::thread::AtomicFlag m_lock{};
    gp::thread::Atomic<gp::USize> m_variableCount{ 0u };
    gp::thread::Atomic<gp::USize> m_commandCount{ 0u };
    gp::thread::Atomic<gp::USize> m_sinkCount{ 0u };
    gp::thread::Atomic<bool> m_isInitialized{ false };
    gp::thread::Atomic<bool> m_consoleEnabled{ true };
    gp::thread::Atomic<bool> m_cheatsEnabled{ true };
};

/// @brief Templated storage wrapper pairing `ConsoleManagerBase` with fixed inline arrays (zero heap allocations).
/// @tparam InHashSlotCapacity Power-of-two hash table slot capacity (`>= 16`).
/// @tparam InMaxDynamicVariables Pool capacity for imperative `registerConsoleVariable()` calls.
/// @tparam InMaxDynamicCommands Pool capacity for imperative `registerConsoleCommand()` calls.
/// @tparam InMaxSinks Maximum registered `AutoConsoleVariableSink` / `registerConsoleVariableSink` callbacks.
template <
    gp::USize InHashSlotCapacity,
    gp::USize InMaxDynamicVariables,
    gp::USize InMaxDynamicCommands,
    gp::USize InMaxSinks>
class BasicConsoleManager final : public ConsoleManagerBase
{
    static_assert(
        InHashSlotCapacity >= 16u && (InHashSlotCapacity & (InHashSlotCapacity - 1u)) == 0u,
        "BasicConsoleManager hash slot capacity must be a power of two >= 16."
    );
    static_assert(InMaxDynamicVariables > 0u, "BasicConsoleManager dynamic variable capacity must be > 0.");
    static_assert(InMaxDynamicCommands > 0u, "BasicConsoleManager dynamic command capacity must be > 0.");
    static_assert(InMaxSinks > 0u, "BasicConsoleManager sink capacity must be > 0.");

public:
    /// @brief Constructs an uninitialized `BasicConsoleManager` bound to its inline storage arrays.
    [[nodiscard]] BasicConsoleManager() noexcept
        : ConsoleManagerBase(makeStorageView())
    {}

    /// @brief Destructor.
    virtual ~BasicConsoleManager() noexcept override
    {
        shutdown();
    }

    BasicConsoleManager(const BasicConsoleManager&) = delete;
    BasicConsoleManager& operator=(const BasicConsoleManager&) = delete;
    BasicConsoleManager(BasicConsoleManager&&) = delete;
    BasicConsoleManager& operator=(BasicConsoleManager&&) = delete;

private:
    [[nodiscard]] internal::ConsoleManagerStorageView makeStorageView() noexcept
    {
        internal::ConsoleManagerStorageView view{};
        view.hashSlots = m_hashSlots;
        view.hashSlotCapacity = InHashSlotCapacity;
        view.dynamicVariables = m_dynamicVariables;
        view.dynamicVariableCapacity = InMaxDynamicVariables;
        view.dynamicCommands = m_dynamicCommands;
        view.dynamicCommandCapacity = InMaxDynamicCommands;
        view.sinkSlots = m_sinkSlots;
        view.sinkCapacity = InMaxSinks;
        view.dirtyRenderQueue = m_dirtyRenderQueue;
        view.dirtyRenderCapacity = InHashSlotCapacity;
        view.dirtyLatchedQueue = m_dirtyLatchedQueue;
        view.dirtyLatchedCapacity = InHashSlotCapacity;
        view.refVariables = m_refVariables;
        view.refVariableCapacity = InHashSlotCapacity;
        return view;
    }

private:
    internal::ConsoleHashSlot m_hashSlots[InHashSlotCapacity]{};
    internal::ConsoleDynamicVariable m_dynamicVariables[InMaxDynamicVariables]{};
    internal::ConsoleDynamicCommand m_dynamicCommands[InMaxDynamicCommands]{};
    internal::ConsoleSinkSlot m_sinkSlots[InMaxSinks]{};
    IConsoleVariable* m_dirtyRenderQueue[InHashSlotCapacity]{};
    IConsoleVariable* m_dirtyLatchedQueue[InHashSlotCapacity]{};
    IConsoleVariable* m_refVariables[InHashSlotCapacity]{};
};

}   // namespace gp::console
