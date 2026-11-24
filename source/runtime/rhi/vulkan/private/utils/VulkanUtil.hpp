// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "hashing/Crc32.hpp"
#include "memory/Memory.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "utils/VulkanResult.hpp"   // IWYU pragma: export
#include <utility>

namespace gp::rhi::vulkan
{

/// @brief Assigns `value` to `target` and returns whether `target` changed.
/// @tparam InType Equality-comparable and copy-assignable value type.
/// @param[in,out] target Destination variable updated in place.
/// @param[in] value Candidate replacement value.
/// @return True if `target != value` before the assignment; false if unchanged.
template <typename InType>
[[nodiscard]] inline bool copyAndReturnNotEqual(InType& target, const InType value) noexcept
{
    const bool changed = (target != value);
    target = value;
    return changed;
}

/// @brief Storage policy for `DataKey` parameterized by collision-checking mode.
/// @tparam InVersion `0` = hash only (no byte storage), `1` = hash + assertion-checked byte storage,
///                   `2` = hash + runtime byte comparison.
template <gp::Int32 InVersion>
class DataKeyBase;

/// @brief Hash-only `DataKeyBase` specialization storing a 32-bit CRC without retaining backing bytes.
template <>
class DataKeyBase<0>
{
protected:
    /// @brief Provides a temporary scratch byte buffer to `receiveData` without retaining it.
    /// @tparam InDataReceiver Callable accepting `gp::Vector<gp::UInt8>&`.
    /// @param[in,out] receiveData Callback populating the transient buffer.
    template <typename InDataReceiver>
    void getData(InDataReceiver&& receiveData) noexcept
    {
        gp::Vector<gp::UInt8> tempData;
        receiveData(tempData);
    }

    /// @brief No-op byte storage hook for hash-only keys.
    /// @param[in] inData Unused data pointer.
    /// @param[in] inSize Unused byte count.
    void setData(const void* const inData, const gp::UInt32 inSize) noexcept
    {
        static_cast<void>(inData);
        static_cast<void>(inSize);
    }

    /// @brief No-op deep copy hook for hash-only keys.
    /// @param[out] outResult Destination key.
    void copyDataDeep(DataKeyBase& outResult) const noexcept
    {
        static_cast<void>(outResult);
    }

    /// @brief No-op shallow copy hook for hash-only keys.
    /// @param[out] outResult Destination key.
    void copyDataShallow(DataKeyBase& outResult) const noexcept
    {
        static_cast<void>(outResult);
    }

    /// @brief Always returns `true` once hashes match in hash-only mode.
    /// @param[in] other Key to compare against.
    /// @return Always `true`.
    [[nodiscard]] bool isDataEquals(const DataKeyBase& other) const noexcept
    {
        static_cast<void>(other);
        return true;
    }

    /// @brief Cached 32-bit CRC hash over the serialized key payload.
    gp::UInt32 m_hash{ 0u };
};

/// @brief Backing-byte `DataKeyBase` specialization storing payload bytes and asserting hash collisions in checked
///        builds.
template <>
class DataKeyBase<1>
{
public:
    DataKeyBase() noexcept = default;
    ~DataKeyBase() noexcept = default;

    DataKeyBase(const DataKeyBase&) = delete;
    DataKeyBase& operator=(const DataKeyBase&) = delete;

    /// @brief Move-constructs a key, transferring storage ownership and clearing the source's raw data pointer.
    /// @param[in,out] other Source key to move from.
    DataKeyBase(DataKeyBase&& other) noexcept
        : m_hash(other.m_hash)
        , m_data(other.m_data)
        , m_dataStorage(std::move(other.m_dataStorage))
    {
        other.m_hash = 0u;
        other.m_data = nullptr;
    }

    /// @brief Move-assigns a key, transferring storage ownership and clearing the source's raw data pointer.
    /// @param[in,out] other Source key to move from.
    /// @return Reference to `*this`.
    DataKeyBase& operator=(DataKeyBase&& other) noexcept
    {
        if (this != &other)
        {
            m_hash = other.m_hash;
            m_data = other.m_data;
            m_dataStorage = std::move(other.m_dataStorage);
            other.m_hash = 0u;
            other.m_data = nullptr;
        }
        return *this;
    }

    /// @brief Returns a mutable reference to the stored payload byte vector.
    /// @return Reference to backing payload bytes.
    [[nodiscard]] gp::Vector<gp::UInt8>& getDataRef() noexcept
    {
        GP_CHECK(m_data != nullptr);
        return *m_data;
    }

    /// @brief Returns a const reference to the stored payload byte vector.
    /// @return Const reference to backing payload bytes.
    [[nodiscard]] const gp::Vector<gp::UInt8>& getDataRef() const noexcept
    {
        GP_CHECK(m_data != nullptr);
        return *m_data;
    }

protected:
    /// @brief Ensures storage exists and passes the backing byte vector to `receiveData`.
    /// @tparam InDataReceiver Callable accepting `gp::Vector<gp::UInt8>&`.
    /// @param[in,out] receiveData Callback populating the payload bytes.
    template <typename InDataReceiver>
    void getData(InDataReceiver&& receiveData) noexcept
    {
        ensureDataStorage();
        receiveData(*m_data);
    }

    /// @brief Replaces stored payload bytes with a copy of `[inData, inData + inSize)`.
    /// @param[in] inData Source byte pointer (must be non-null when `inSize > 0`).
    /// @param[in] inSize Number of bytes to copy.
    void setData(const void* const inData, const gp::UInt32 inSize) noexcept
    {
        GP_CHECK(inData != nullptr || inSize == 0u);
        ensureDataStorage();
        const gp::USize safeSize = (inData != nullptr) ? static_cast<gp::USize>(inSize) : 0u;
        m_data->resize(safeSize);
        if (safeSize > 0u)
        {
            gp::memory::copyMemory(m_data->data(), inData, safeSize);
        }
    }

    /// @brief Deep-copies the backing byte vector into `outResult`.
    /// @param[out] outResult Destination key receiving owned byte storage.
    void copyDataDeep(DataKeyBase& outResult) const noexcept
    {
        GP_CHECK(m_data != nullptr);
        outResult.m_dataStorage = gp::makeUnique<gp::Vector<gp::UInt8>>(*m_data);
        outResult.m_data = outResult.m_dataStorage.get();
    }

    /// @brief Shallow-copies the non-owning byte vector pointer into `outResult`.
    /// @param[out] outResult Destination key aliasing `m_data`.
    void copyDataShallow(DataKeyBase& outResult) const noexcept
    {
        GP_CHECK(m_data != nullptr);
        outResult.m_data = m_data;
    }

    /// @brief Asserts that two hash-matching keys have byte-identical payloads and returns `true`.
    /// @param[in] other Key to verify against.
    /// @return Always `true` when assertions pass.
    [[nodiscard]] bool isDataEquals(const DataKeyBase& other) const noexcept
    {
        GP_CHECK(m_data != nullptr && other.m_data != nullptr);
        GP_CHECK(m_data->size() == other.m_data->size());
        if (!m_data->isEmpty())
        {
            GP_CHECK(gp::memory::compareMemory(m_data->data(), other.m_data->data(), m_data->size()) == 0);
        }
        return true;
    }

    /// @brief Cached 32-bit CRC hash over the serialized key payload.
    gp::UInt32 m_hash{ 0u };

    /// @brief Pointer to active payload bytes (either owned by `m_dataStorage` or aliased by `copyDataShallow`).
    gp::Vector<gp::UInt8>* m_data{ nullptr };

private:
    void ensureDataStorage() noexcept
    {
        if (!m_dataStorage)
        {
            m_dataStorage = gp::makeUnique<gp::Vector<gp::UInt8>>();
            m_data = m_dataStorage.get();
        }
    }

    gp::UniquePtr<gp::Vector<gp::UInt8>> m_dataStorage{};
};

/// @brief Full-comparison `DataKeyBase` specialization performing exact byte comparison after hash equality.
template <>
class DataKeyBase<2> : public DataKeyBase<1>
{
protected:
    /// @brief Compares payload length and bytes between `this` and `other`.
    /// @param[in] other Key to compare against.
    /// @return True when payload sizes and contents match exactly.
    [[nodiscard]] bool isDataEquals(const DataKeyBase& other) const noexcept
    {
        GP_CHECK(m_data != nullptr && other.m_data != nullptr);
        if (m_data->size() != other.m_data->size())
        {
            return false;
        }
        return m_data->isEmpty() ||
               (gp::memory::compareMemory(m_data->data(), other.m_data->data(), m_data->size()) == 0);
    }
};

/// @brief CRTP hash-key helper for Vulkan descriptor/pipeline cache keys backed by IEEE CRC-32.
/// @tparam InDerivedType Concrete CRTP derived key type.
/// @tparam InAlwaysCompareData True to always store and compare payload bytes; false to compare only in checked builds.
template <typename InDerivedType, bool InAlwaysCompareData = false>
class DataKey : public DataKeyBase<InAlwaysCompareData ? 2 : (GP_DO_CHECK != 0 ? 1 : 0)>
{
public:
    /// @brief Generates the key by letting `writeBytes` populate a raw byte buffer, then hashing it with CRC-32.
    /// @tparam InWriterType Callable invocable with `(gp::Vector<gp::UInt8>&)`.
    /// @param[in,out] writeBytes Callback that appends serialized key bytes into the provided buffer.
    /// @param[in] dataReserve Optional initial byte capacity hint.
    template <typename InWriterType>
    void generateFromWriter(InWriterType&& writeBytes, const gp::USize dataReserve = 0u) noexcept
    {
        this->getData(
            [&writeBytes, dataReserve, this](gp::Vector<gp::UInt8>& inData) noexcept -> void
        {
            inData.clear();
            if (dataReserve > 0u)
            {
                inData.reserve(dataReserve);
            }
            writeBytes(inData);

            gp::hashing::Crc32 hasher;
            if (!inData.isEmpty())
            {
                hasher.update(inData.data(), inData.size());
            }
            this->m_hash = hasher.finalize();
        }
        );
    }

    /// @brief Generates the key directly from the raw byte representation of `object`.
    /// @tparam InObjectType Trivially copyable key struct.
    /// @param[in] object Source object whose bytes are hashed.
    template <typename InObjectType>
    void generateFromObject(const InObjectType& object) noexcept
    {
        generateFromData(&object, static_cast<gp::UInt32>(sizeof(object)));
    }

    /// @brief Generates the key from an arbitrary byte span `[inData, inData + inSize)`.
    /// @param[in] inData Source byte pointer (must be non-null when `inSize > 0`).
    /// @param[in] inSize Byte count to hash and store.
    void generateFromData(const void* const inData, const gp::UInt32 inSize) noexcept
    {
        GP_CHECK(inData != nullptr || inSize == 0u);
        this->setData(inData, inSize);
        gp::hashing::Crc32 hasher;
        if (inData != nullptr && inSize > 0u)
        {
            hasher.update(static_cast<const gp::UInt8*>(inData), static_cast<gp::USize>(inSize));
        }
        this->m_hash = hasher.finalize();
    }

    /// @brief Returns the cached 32-bit CRC hash of this key.
    /// @return 32-bit CRC hash.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getHash() const noexcept
    {
        return this->m_hash;
    }

    /// @brief Produces an independent deep copy of this key (duplicating backing bytes when stored).
    /// @return Deep-copied `InDerivedType`.
    [[nodiscard]] InDerivedType copyDeep() const noexcept
    {
        InDerivedType result;
        result.m_hash = this->m_hash;
        this->copyDataDeep(result);
        return result;
    }

    /// @brief Produces a lightweight shallow copy aliasing this key's backing bytes.
    /// @note The source key must outlive any shallow copies that read or compare its backing payload.
    /// @return Shallow-copied `InDerivedType`.
    [[nodiscard]] InDerivedType copyShallow() const noexcept
    {
        InDerivedType result;
        result.m_hash = this->m_hash;
        this->copyDataShallow(result);
        return result;
    }

    /// @brief Compares two derived keys by hash and active storage policy.
    /// @param[in] lhs Left-hand key.
    /// @param[in] rhs Right-hand key.
    /// @return True when hashes (and payloads, when configured) match.
    [[nodiscard]] friend bool operator==(const InDerivedType& lhs, const InDerivedType& rhs) noexcept
    {
        return (lhs.m_hash == rhs.m_hash) && lhs.isDataEquals(rhs);
    }
};

}   // namespace gp::rhi::vulkan
