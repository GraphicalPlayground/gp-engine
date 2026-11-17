// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "console/ConsoleEnums.hpp"
#include "console/ConsoleForward.hpp"   // IWYU pragma: keep
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleObject.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "templates/Delegate.hpp"

namespace gp::console
{

/// @brief Abstract runtime console variable supporting Game Thread writes, lock-free Render Thread reads,
///        priority precedence (`ConsoleVarSetBy`), latched commits, and change delegates.
class GP_CORE_API IConsoleVariable : public IConsoleObject
{
public:
    /// @brief Returns the underlying concrete type stored by this variable.
    /// @return `ConsoleVarType` enumerator.
    [[nodiscard]] virtual ConsoleVarType getType() const noexcept = 0;

    /// @brief Returns the highest priority source that has written to this variable.
    /// @return Active `ConsoleVarSetBy` priority tier.
    [[nodiscard]] virtual ConsoleVarSetBy getSetBy() const noexcept = 0;

    /// @brief Reports whether the current active value differs from its constructor default.
    /// @return True if modified from default, false otherwise.
    [[nodiscard]] virtual bool isModified() const noexcept = 0;

    /// @brief Reports whether `get*OnRenderThread()` reads from a synchronized atomic or seqlocked render slot.
    /// @return True if render-thread-safe, false otherwise.
    [[nodiscard]] virtual bool isRenderThreadSafe() const noexcept = 0;

    /// @brief Reports whether writes to this variable are deferred until `flushLatchedVariables()`.
    /// @return True if `ConsoleVarFlags::Latched` is active, false otherwise.
    [[nodiscard]] virtual bool isLatched() const noexcept = 0;

    /// @brief Reports whether a latched write is waiting to be committed by `flushLatchedVariables()`.
    /// @return True if a pending latched value exists, false otherwise.
    [[nodiscard]] virtual bool hasPendingLatchedValue() const noexcept = 0;

    /// @brief Reports whether a Game Thread write has not yet been mirrored into the Render Thread slot.
    /// @return True if `syncRenderThreadValue()` needs to run, false otherwise.
    [[nodiscard]] virtual bool hasPendingRenderSync() const noexcept = 0;

    /// @brief Reports whether this variable mirrors an external caller-owned C++ reference (`AutoConsoleVariableRef`).
    /// @return True if backed by an external C++ reference, false if self-contained.
    [[nodiscard]] virtual bool isExternalReference() const noexcept = 0;

public:
    /// @section Game Thread Readers

    /// @brief Reads the current Game Thread value coerced to `bool`.
    /// @return Boolean value.
    [[nodiscard]] virtual bool getBool() const noexcept = 0;

    /// @brief Reads the current Game Thread value coerced to `gp::Int32`.
    /// @return 32-bit signed integer value.
    [[nodiscard]] virtual gp::Int32 getInt() const noexcept = 0;

    /// @brief Reads the current Game Thread value coerced to `gp::UInt32`.
    /// @return 32-bit unsigned integer value.
    [[nodiscard]] virtual gp::UInt32 getUInt() const noexcept = 0;

    /// @brief Reads the current Game Thread value coerced to `gp::Float32`.
    /// @return 32-bit floating-point value.
    [[nodiscard]] virtual gp::Float32 getFloat() const noexcept = 0;

    /// @brief Reads the current Game Thread value coerced to `gp::Float64`.
    /// @return 64-bit floating-point value.
    [[nodiscard]] virtual gp::Float64 getFloat64() const noexcept = 0;

    /// @brief Formats or copies the current Game Thread value into a stack `ConsoleString`.
    /// @return String representation of the Game Thread value.
    [[nodiscard]] virtual ConsoleString getString() const noexcept = 0;

    /// @brief Formats or copies the constructor default value into a stack `ConsoleString`.
    /// @return String representation of the default value.
    [[nodiscard]] virtual ConsoleString getDefaultString() const noexcept = 0;

public:
    /// @section Render Thread & Any-Thread Lock-Free Readers

    /// @brief Reads the frame-synchronized Render Thread value coerced to `bool` without locking.
    /// @return Synchronized boolean value.
    [[nodiscard]] virtual bool getBoolOnRenderThread() const noexcept = 0;

    /// @brief Reads the frame-synchronized Render Thread value coerced to `gp::Int32` without locking.
    /// @return Synchronized 32-bit signed integer value.
    [[nodiscard]] virtual gp::Int32 getIntOnRenderThread() const noexcept = 0;

    /// @brief Reads the frame-synchronized Render Thread value coerced to `gp::UInt32` without locking.
    /// @return Synchronized 32-bit unsigned integer value.
    [[nodiscard]] virtual gp::UInt32 getUIntOnRenderThread() const noexcept = 0;

    /// @brief Reads the frame-synchronized Render Thread value coerced to `gp::Float32` without locking.
    /// @return Synchronized 32-bit floating-point value.
    [[nodiscard]] virtual gp::Float32 getFloatOnRenderThread() const noexcept = 0;

    /// @brief Reads the frame-synchronized Render Thread value coerced to `gp::Float64` without locking.
    /// @return Synchronized 64-bit floating-point value.
    [[nodiscard]] virtual gp::Float64 getFloat64OnRenderThread() const noexcept = 0;

    /// @brief Reads a tear-free copy of the Render Thread string representation into a `ConsoleString`.
    /// @return Synchronized string representation.
    [[nodiscard]] virtual ConsoleString getStringOnRenderThread() const noexcept = 0;

    /// @brief Reads the latest committed value atomically from any thread coerced to `bool`.
    /// @return Latest committed boolean value.
    [[nodiscard]] virtual bool getBoolOnAnyThread() const noexcept = 0;

    /// @brief Reads the latest committed value atomically from any thread coerced to `gp::Int32`.
    /// @return Latest committed 32-bit signed integer value.
    [[nodiscard]] virtual gp::Int32 getIntOnAnyThread() const noexcept = 0;

    /// @brief Reads the latest committed value atomically from any thread coerced to `gp::UInt32`.
    /// @return Latest committed 32-bit unsigned integer value.
    [[nodiscard]] virtual gp::UInt32 getUIntOnAnyThread() const noexcept = 0;

    /// @brief Reads the latest committed value atomically from any thread coerced to `gp::Float32`.
    /// @return Latest committed 32-bit floating-point value.
    [[nodiscard]] virtual gp::Float32 getFloatOnAnyThread() const noexcept = 0;

    /// @brief Reads the latest committed value atomically from any thread coerced to `gp::Float64`.
    /// @return Latest committed 64-bit floating-point value.
    [[nodiscard]] virtual gp::Float64 getFloat64OnAnyThread() const noexcept = 0;

    /// @brief Reads a tear-free copy of the latest committed string representation from any thread.
    /// @return Latest committed string representation.
    [[nodiscard]] virtual ConsoleString getStringOnAnyThread() const noexcept = 0;

public:
    /// @section Mutators

    /// @brief Attempts to write a `bool` value honoring `ReadOnly`, `Cheat`, `Latched`, and `setBy` priority.
    /// @param[in] value New boolean value.
    /// @param[in] setBy Priority tier of the caller (defaults to `ConsoleVarSetBy::Code`).
    /// @return `ConsoleResult::Success` on commit or latch stage, or an error reason.
    virtual ConsoleResult set(bool value, ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept = 0;

    /// @brief Attempts to write a `gp::Int32` value honoring `ReadOnly`, `Cheat`, `Latched`, and `setBy` priority.
    /// @param[in] value New 32-bit signed integer value.
    /// @param[in] setBy Priority tier of the caller (defaults to `ConsoleVarSetBy::Code`).
    /// @return `ConsoleResult::Success` on commit or latch stage, or an error reason.
    virtual ConsoleResult set(gp::Int32 value, ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept = 0;

    /// @brief Attempts to write a `gp::UInt32` value honoring `ReadOnly`, `Cheat`, `Latched`, and `setBy` priority.
    /// @param[in] value New 32-bit unsigned integer value.
    /// @param[in] setBy Priority tier of the caller (defaults to `ConsoleVarSetBy::Code`).
    /// @return `ConsoleResult::Success` on commit or latch stage, or an error reason.
    virtual ConsoleResult set(gp::UInt32 value, ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept = 0;

    /// @brief Attempts to write a `gp::Float32` value honoring `ReadOnly`, `Cheat`, `Latched`, and `setBy` priority.
    /// @param[in] value New 32-bit floating-point value.
    /// @param[in] setBy Priority tier of the caller (defaults to `ConsoleVarSetBy::Code`).
    /// @return `ConsoleResult::Success` on commit or latch stage, or an error reason.
    virtual ConsoleResult set(gp::Float32 value, ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept = 0;

    /// @brief Attempts to write a `gp::Float64` value honoring `ReadOnly`, `Cheat`, `Latched`, and `setBy` priority.
    /// @param[in] value New 64-bit floating-point value.
    /// @param[in] setBy Priority tier of the caller (defaults to `ConsoleVarSetBy::Code`).
    /// @return `ConsoleResult::Success` on commit or latch stage, or an error reason.
    virtual ConsoleResult set(gp::Float64 value, ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept = 0;

    /// @brief Parses @p value according to `getType()` and writes it honoring flags and priority.
    /// @param[in] value UTF-8 string value representation.
    /// @param[in] setBy Priority tier of the caller (defaults to `ConsoleVarSetBy::Code`).
    /// @return `ConsoleResult::Success` on commit or latch stage, or `ParseFailed`/policy error.
    virtual ConsoleResult
        setFromString(gp::StringView value, ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept = 0;

    /// @brief Restores the constructor default value and resets priority to `ConsoleVarSetBy::Constructor`.
    /// @param[in] setBy Priority tier attempting the reset (must meet or exceed current `getSetBy()`).
    /// @return `ConsoleResult::Success` if reset succeeded, or a rejection code (`ReadOnly`, `PriorityTooLow`, etc.).
    virtual ConsoleResult reset(ConsoleVarSetBy setBy = ConsoleVarSetBy::Code) noexcept = 0;

public:
    /// @section Change Delegates & Synchronization

    /// @brief Assigns or replaces the single primary change delegate on this variable.
    /// @param[in] callback Delegate invoked with `this` whenever the variable commits a new value.
    virtual void setOnChangedCallback(const ConsoleVarChangedDelegate& callback) noexcept = 0;

    /// @brief Returns a copy or reference to the single primary change delegate.
    /// @return Const reference to the primary `ConsoleVarChangedDelegate`.
    [[nodiscard]] virtual const ConsoleVarChangedDelegate& getOnChangedCallback() const noexcept = 0;

    /// @brief Subscribes an additional change listener without displacing the primary callback.
    /// @param[in] callback Bound listener delegate.
    /// @return Valid `gp::DelegateHandle` on success, or invalid handle if unbound or subscriber capacity is full.
    [[nodiscard]] virtual gp::DelegateHandle addOnChangedCallback(ConsoleVarChangedDelegate callback) noexcept = 0;

    /// @brief Unsubscribes a multicast change listener previously added via `addOnChangedCallback()`.
    /// @param[in] handle Handle returned by `addOnChangedCallback()`.
    /// @return True if found and removed, false otherwise.
    virtual bool removeOnChangedCallback(gp::DelegateHandle handle) noexcept = 0;

    /// @brief Removes all multicast change listeners (does not clear the primary `setOnChangedCallback()`).
    virtual void clearOnChangedCallbacks() noexcept = 0;

    /// @brief Copies the committed Game Thread value into the Render Thread slot and clears `hasPendingRenderSync()`.
    virtual void syncRenderThreadValue() noexcept = 0;

    /// @brief Commits any pending latched value, triggers change delegates, and marks the Render Thread slot dirty.
    /// @return True if a latched value was applied, false if none was pending.
    virtual bool applyLatchedValue() noexcept = 0;
};

}   // namespace gp::console
