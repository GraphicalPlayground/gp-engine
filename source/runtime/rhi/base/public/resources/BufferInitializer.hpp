// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"          // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "memory/Memory.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "resources/Buffer.hpp"                // IWYU pragma: keep
#include "RHIBase.hpp"
#include "templates/Function.hpp"
#include <utility>

namespace gp::rhi::resource
{

/// @brief Move-only RAII handle providing direct CPU write access during GPU buffer creation.
/// @details Returned by `DynamicRHI::createBufferInitializer()` and `CommandListBase::createBufferInitializer()`.
///          Callers write initial bytes into `writableData` (via `writeBytes()` or `writeArray()`) and call
///          `finalize()` to commit the upload and obtain the owning `BufferRef`. If destroyed without calling
///          `finalize()`, the destructor invokes the backend finalizer automatically so staging allocations never leak.
struct GP_RHI_API BufferInitializer
{
public:
    /// @brief Backend callback signature invoked when finalizing the buffer creation.
    using FinalizeCallback = gp::UniqueFunction<BufferRef(command::CommandListBase&)>;

public:
    /// @brief Command list associated with this buffer creation and upload.
    command::CommandListBase* cmdList{ nullptr };

    /// @brief Newly created buffer resource (valid until `finalize()` transfers ownership).
    BufferRef buffer{};

    /// @brief CPU-writable staging or mapped pointer for initial data, or `nullptr` if not in `Initializer` mode.
    void* writableData{ nullptr };

    /// @brief Byte size of the writable region pointed to by `writableData`.
    gp::UInt64 writableDataSize{ 0u };

    /// @brief Backend finalization callback executed once by `finalize()` or destruction.
    FinalizeCallback finalizeCallback{};

public:
    /// @brief Constructs an empty, already-finalized initializer.
    BufferInitializer() noexcept = default;

    /// @brief Constructs an active buffer initializer owned by the caller until `finalize()`.
    /// @param[in] inCmdList Command list driving initialization.
    /// @param[in] inBuffer Newly created `BufferRef`.
    /// @param[in] inWritableData Writable CPU pointer, or `nullptr`.
    /// @param[in] inWritableDataSize Capacity in bytes of `inWritableData`.
    /// @param[in] inFinalizeCallback Callback invoked to complete initialization and return the finalized buffer.
    BufferInitializer(
        command::CommandListBase& inCmdList,
        BufferRef inBuffer,
        void* const inWritableData,
        const gp::UInt64 inWritableDataSize,
        FinalizeCallback inFinalizeCallback
    ) noexcept
        : cmdList(&inCmdList)
        , buffer(std::move(inBuffer))
        , writableData(inWritableData)
        , writableDataSize(inWritableDataSize)
        , finalizeCallback(std::move(inFinalizeCallback))
    {}

    BufferInitializer(const BufferInitializer&) = delete;
    BufferInitializer& operator=(const BufferInitializer&) = delete;

    /// @brief Move constructor transferring ownership of the pending initialization.
    /// @param[in,out] other Source initializer left empty.
    BufferInitializer(BufferInitializer&& other) noexcept
        : cmdList(other.cmdList)
        , buffer(std::move(other.buffer))
        , writableData(other.writableData)
        , writableDataSize(other.writableDataSize)
        , finalizeCallback(std::move(other.finalizeCallback))
    {
        other.cmdList = nullptr;
        other.writableData = nullptr;
        other.writableDataSize = 0u;
    }

    /// @brief Move assignment operator finalizing any active buffer before adopting `other`.
    /// @param[in,out] other Source initializer left empty.
    /// @return Reference to `*this`.
    BufferInitializer& operator=(BufferInitializer&& other) noexcept
    {
        if (this != &other)
        {
            if (isValid())
            {
                static_cast<void>(finalize());
            }
            cmdList = other.cmdList;
            buffer = std::move(other.buffer);
            writableData = other.writableData;
            writableDataSize = other.writableDataSize;
            finalizeCallback = std::move(other.finalizeCallback);
            other.cmdList = nullptr;
            other.writableData = nullptr;
            other.writableDataSize = 0u;
        }
        return *this;
    }

    /// @brief Finalizes any unfinalized buffer so backend staging state is always released cleanly.
    ~BufferInitializer() noexcept
    {
        if (isValid())
        {
            static_cast<void>(finalize());
        }
    }

    /// @brief Reports whether this initializer still holds an unfinalized buffer.
    /// @return True when `buffer.isValid()` and `cmdList != nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isValid() const noexcept
    {
        return (cmdList != nullptr) && buffer.isValid();
    }

    /// @brief Returns the raw writable pointer for initial data uploads.
    /// @return Writable pointer, or `nullptr` if unavailable.
    [[nodiscard]] GP_FORCEINLINE_HINT void* getWritableData() const noexcept
    {
        return writableData;
    }

    /// @brief Returns the writable byte capacity.
    /// @return Writable size in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt64 getWritableDataSize() const noexcept
    {
        return writableDataSize;
    }

    /// @brief Copies `numBytes` from `srcData` into `writableData` starting at `destOffset`.
    /// @param[in] destOffset Byte offset into `writableData`.
    /// @param[in] srcData Source byte pointer.
    /// @param[in] numBytes Number of bytes to copy.
    void writeBytes(const gp::UInt64 destOffset, const void* const srcData, const gp::UInt64 numBytes) noexcept
    {
        if (numBytes == 0u)
        {
            return;
        }
        GP_CHECKF(writableData != nullptr, "BufferInitializer has no writable data pointer.");
        GP_CHECKF(srcData != nullptr, "BufferInitializer::writeBytes requires a non-null source pointer.");
        GP_CHECKF(
            destOffset <= writableDataSize && numBytes <= (writableDataSize - destOffset),
            "BufferInitializer::writeBytes range exceeds writable buffer size."
        );

        gp::UInt8* const dst = static_cast<gp::UInt8*>(writableData) + destOffset;
        gp::memory::copyMemory(dst, srcData, static_cast<gp::USize>(numBytes));
    }

    /// @brief Copies `numBytes` from `srcData` into `writableData` starting at `destOffset` (data-first convenience).
    /// @param[in] srcData Source byte pointer.
    /// @param[in] numBytes Number of bytes to copy.
    /// @param[in] destOffset Optional byte offset into `writableData`.
    void writeData(const void* const srcData, const gp::UInt64 numBytes, const gp::UInt64 destOffset = 0u) noexcept
    {
        writeBytes(destOffset, srcData, numBytes);
    }

    /// @brief Fills the entire writable staging region with the constant byte `value`.
    /// @param[in] value Byte fill value.
    void fillWithValue(const gp::UInt8 value) noexcept
    {
        if (writableDataSize == 0u)
        {
            return;
        }
        GP_CHECKF(writableData != nullptr, "BufferInitializer::fillWithValue requires a writable data pointer.");
        gp::memory::setMemory(writableData, static_cast<gp::Int32>(value), static_cast<gp::USize>(writableDataSize));
    }

    /// @brief Returns a typed mutable view over the writable buffer staging memory.
    /// @tparam InElementType Element type stored in the buffer.
    /// @return `gp::VectorView<InElementType>` covering `writableDataSize / sizeof(InElementType)` elements.
    template <typename InElementType>
    [[nodiscard]] gp::VectorView<InElementType> getWriteView() noexcept
    {
        if (writableData == nullptr || writableDataSize < sizeof(InElementType))
        {
            return gp::VectorView<InElementType>{};
        }
        return gp::VectorView<InElementType>(
            static_cast<InElementType*>(writableData), static_cast<gp::USize>(writableDataSize / sizeof(InElementType))
        );
    }

    /// @brief Copies a typed span of elements into `writableData` starting at `destElementOffset`.
    /// @tparam InElementType Element type stored in `elements`.
    /// @param[in] elements Contiguous source elements.
    /// @param[in] destElementOffset Element offset into `writableData`.
    template <typename InElementType>
    void writeArray(const gp::ConstVectorView<InElementType> elements, const gp::UInt64 destElementOffset = 0u) noexcept
    {
        const gp::UInt64 byteOffset = destElementOffset * static_cast<gp::UInt64>(sizeof(InElementType));
        const gp::UInt64 byteSize =
            static_cast<gp::UInt64>(elements.size()) * static_cast<gp::UInt64>(sizeof(InElementType));
        writeBytes(byteOffset, elements.data(), byteSize);
    }

    /// @brief Commits the initial buffer upload and transfers ownership of the finalized `BufferRef`.
    /// @return Finalized `BufferRef` (or null if already finalized).
    [[nodiscard]] BufferRef finalize() noexcept
    {
        if (!isValid())
        {
            return BufferRef{};
        }

        command::CommandListBase* const activeCmdList = cmdList;
        FinalizeCallback callback = std::move(finalizeCallback);
        BufferRef localBuffer = std::move(buffer);

        cmdList = nullptr;
        writableData = nullptr;
        writableDataSize = 0u;

        if (callback.isBound())
        {
            return callback(*activeCmdList);
        }
        return localBuffer;
    }
};

}   // namespace gp::rhi::resource
