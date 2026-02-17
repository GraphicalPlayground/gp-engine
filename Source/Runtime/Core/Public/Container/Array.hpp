// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/Allocator.hpp"
#include "Memory/MemoryForward.hpp"
#include "Memory/MemoryUtilities.hpp"
#include "Templates/Concepts.hpp"
#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace GP::Container
{

/// \brief
/// \tparam T
/// \tparam Alloc
template <typename T, Concepts::IsAllocator Alloc = Memory::TAllocator<T>>
class TArray
{
public:
    using ValueType = T;
    using AllocatorType = Alloc;
    using Pointer = T*;
    using ConstPointer = const T*;
    using Reference = T&;
    using ConstReference = const T&;
    using SizeType = SizeT;
    using DifferenceType = OffsetT;
    using Iterator = T*;
    using ConstIterator = const T*;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

public:
    static constexpr SizeType NPOS = static_cast<SizeType>(-1);

private:
    Pointer m_data = nullptr;            //<! Pointer to the contiguous block of memory storing the elements.
    SizeType m_size = 0;                 //<! The number of elements currently stored in the array.
    SizeType m_capacity = 0;             //<! The total number of elements that can be stored in the allocated block.
    GP_MAYBE_UNUSED Alloc m_allocator;   //<! The allocator instance used for memory management.

public:
    /// \brief Default constructor. Initializes an empty array with no allocated memory.
    constexpr TArray() noexcept = default;

    /// \brief Construct with a specific allocator instance.
    /// \param allocator The allocator instance to use for memory management.
    explicit constexpr TArray(const Alloc& allocator) noexcept
        : m_allocator(allocator)
    {}

    /// \brief Construct an array with a specified number of default-initialized elements.
    /// \param count The number of elements to create. Each element is default-constructed.
    explicit TArray(SizeType count) { Resize(count); }

    /// \brief Construct an array with a specified number of elements, each initialized to a given value.
    /// \param count The number of elements to create. Each element is copy-constructed from the provided value.
    /// \param value The value to initialize each element with.
    TArray(SizeType count, const T& value) { Resize(count, value); }

    /// \brief Construct an array from an initializer list. The array will contain a copy of each element in the list.
    /// \param initList The initializer list containing the elements to copy into the array.
    TArray(std::initializer_list<T> initList)
    {
        Reserve(initList.size());
        for (const auto& item: initList) { EmplaceBack(item); }
    }

    /// \brief Construct an array by copying elements from a range defined by two input iterators.
    /// \tparam InputIt The type of the input iterators. Must satisfy the InputIterator concept.
    /// \param first The beginning of the range to copy from.
    /// \param last The end of the range to copy from. The range includes all elements in the half-open interval.
    template <typename InputIt>
    requires(!Concepts::IsIntegral<InputIt>) TArray(InputIt first, InputIt last)
    {
        for (; first != last; ++first) { EmplaceBack(*first); }
    }

    /// \brief Copy constructor.
    /// \param other The array to copy from. The new array will contain a copy of each element in the original array.
    TArray(const TArray& other)
    {
        if (other.m_size > 0)
        {
            GrowTo(other.m_size);
            Memory::CopyConstructRange(m_data, other.m_data, other.m_size);
            m_size = other.m_size;
        }
    }

    /// \brief Move constructor.
    /// \param other The array to move from. The new array will take ownership of the memory and elements from the
    /// original array, leaving it in a valid but unspecified state.
    TArray(TArray&& other) noexcept
        : m_data(other.m_data)
        , m_size(other.m_size)
        , m_capacity(other.m_capacity)
        , m_allocator(std::move(other.m_allocator))
    {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    /// \brief Destructor. Destroys all elements in the array and deallocates any allocated memory.
    ~TArray()
    {
        Clear();
        FreeBuffer();
    }

public:
    /// \brief Copy assignment operator.
    /// \param other The array to copy from. The current array will be replaced with a copy of the elements in the
    /// original array.
    /// \return A reference to the current array after the assignment.
    TArray& operator=(const TArray& other)
    {
        if (this != &other)
        {
            Clear();
            if (other.m_size > m_capacity)
            {
                FreeBuffer();
                GrowTo(other.m_size);
            }
            Memory::CopyConstructRange(m_data, other.m_data, other.m_size);
            m_size = other.m_size;
        }
        return *this;
    }

    /// \brief Move assignment operator.
    /// \param other The array to move from. The current array will take ownership of the memory and elements from the
    /// original array, leaving it in a valid but unspecified state.
    /// \return A reference to the current array after the assignment.
    TArray& operator=(TArray&& other) noexcept
    {
        if (this != &other)
        {
            Clear();
            FreeBuffer();
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            m_allocator = std::move(other.m_allocator);
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    /// \brief Assignment operator for initializer lists.
    /// \param initList The initializer list containing the elements to copy into the array. The current array will be
    /// replaced with a copy of each element in the list.
    /// \return A reference to the current array after the assignment.
    TArray& operator=(std::initializer_list<T> initList)
    {
        Clear();
        Reserve(initList.size());
        for (const auto& item: initList) { EmplaceBack(item); }
        return *this;
    }

    /// \brief Subscript operator for non-const access.
    /// \param index The index of the element to access. Must be less than the current size of the array.
    /// \return A reference to the element at the specified index.
    GP_NODISCARD Reference operator[](SizeType index) noexcept
    {
        GP_ASSERT(index < m_size, "Array index out of bounds");
        return m_data[index];
    }

    /// \brief Subscript operator for const access.
    /// \param index The index of the element to access. Must be less than the current size of the array.
    /// \return A const reference to the element at the specified index.
    GP_NODISCARD ConstReference operator[](SizeType index) const noexcept
    {
        GP_ASSERT(index < m_size, "Array index out of bounds");
        return m_data[index];
    }

public:
    /// \brief Provides access to the element at the specified index with bounds checking.
    /// \param index The index of the element to access. Must be less than the current size of the array.
    /// \return A reference to the element at the specified index.
    GP_NODISCARD Reference At(SizeType index)
    {
        GP_ASSERT(index < m_size, "Array::At index out of bounds");
        return m_data[index];
    }

    /// \brief Provides read-only access to the element at the specified index with bounds checking.
    /// \param index The index of the element to access. Must be less than the current size of the array.
    /// \return A const reference to the element at the specified index.
    GP_NODISCARD ConstReference At(SizeType index) const
    {
        GP_ASSERT(index < m_size, "Array::At index out of bounds");
        return m_data[index];
    }

    /// \brief Provides access to the first element of the array.
    /// \return A reference to the first element of the array.
    GP_NODISCARD Reference Front() noexcept
    {
        GP_ASSERT(m_size > 0, "Front() on empty array");
        return m_data[0];
    }

    /// \brief Provides read-only access to the first element of the array.
    /// \return A const reference to the first element of the array.
    GP_NODISCARD ConstReference Front() const noexcept
    {
        GP_ASSERT(m_size > 0, "Front() on empty array");
        return m_data[0];
    }

    /// \brief Provides access to the last element of the array.
    /// \return A reference to the last element of the array.
    GP_NODISCARD Reference Back() noexcept
    {
        GP_ASSERT(m_size > 0, "Back() on empty array");
        return m_data[m_size - 1];
    }

    /// \brief Provides read-only access to the last element of the array.
    /// \return A const reference to the last element of the array.
    GP_NODISCARD ConstReference Back() const noexcept
    {
        GP_ASSERT(m_size > 0, "Back() on empty array");
        return m_data[m_size - 1];
    }

    /// \brief Provides direct access to the underlying array data.
    /// \return A pointer to the first element of the array's internal storage
    GP_NODISCARD Pointer Data() noexcept { return m_data; }

    /// \brief Provides read-only access to the underlying array data.
    /// \return A const pointer to the first element of the array's internal storage
    GP_NODISCARD ConstPointer Data() const noexcept { return m_data; }

    /// \brief Returns the number of elements currently stored in the array.
    /// \return The number of elements in the array.
    GP_NODISCARD SizeType Size() const noexcept { return m_size; }

    /// \brief Returns the total number of elements that can be stored in the currently allocated memory block.
    /// \return The capacity of the array.
    GP_NODISCARD SizeType Capacity() const noexcept { return m_capacity; }

    /// \brief Checks whether the array is empty (i.e., contains no elements).
    /// \return true if the array is empty; otherwise, false.
    GP_NODISCARD bool IsEmpty() const noexcept { return m_size == 0; }

    /// \brief Returns the maximum number of elements that the array can theoretically hold based on the allocator's
    /// limits and the size of the element type.
    /// \return The maximum number of elements that can be stored in the array.
    GP_NODISCARD SizeType MaxSize() const noexcept { return static_cast<SizeType>(-1) / sizeof(T); }

    /// \brief Reserves memory to hold at least the specified number of elements. If the current capacity is less than
    /// the requested capacity, the array will reallocate its internal storage to accommodate the new capacity.
    /// \param newCapacity The minimum capacity to reserve. Must be greater than or equal to the current size of the
    /// array.
    void Reserve(SizeType newCapacity)
    {
        if (newCapacity > m_capacity) { Reallocate(newCapacity); }
    }

    /// \brief Reduces the capacity of the array to fit its current size. If the capacity is greater than the size,
    /// the array will reallocate its internal storage to match the current size, freeing any excess memory.
    void ShrinkToFit()
    {
        if (m_capacity > m_size)
        {
            if (m_size == 0) { FreeBuffer(); }
            else { Reallocate(m_size); }
        }
    }

    /// \brief Clears the contents of the array. Destroys all elements in the array and resets the size to zero, but
    /// does not change the capacity or deallocate memory.
    void Clear() noexcept
    {
        Memory::DestroyRange(m_data, m_size);
        m_size = 0;
    }

    /// \brief Adds a copy of the specified value to the end of the array.
    /// \param value The value to add to the array. The value will be copy-constructed.
    void PushBack(const T& value) { EmplaceBack(value); }

    /// \brief Adds the specified value to the end of the array, transferring ownership.
    /// \param value The value to add to the array. The value will be move-constructed.
    void PushBack(T&& value) { EmplaceBack(std::move(value)); }

    /// \brief Constructs an element in-place at the end of the array.
    /// \tparam Args The types of the arguments to forward to the element's constructor.
    /// \param args The arguments to forward to the element's constructor.
    /// \return A reference to the newly constructed element.
    template <typename... Args>
    Reference EmplaceBack(Args&&... args)
    {
        if (m_size == m_capacity) { Grow(); }
        Memory::Construct<T>(m_data + m_size, std::forward<Args>(args)...);
        return m_data[m_size++];
    }

    /// \brief Removes the last element from the array.
    /// The array must not be empty when this method is called.
    void PopBack()
    {
        GP_ASSERT(m_size > 0, "PopBack() on empty array");
        --m_size;
        Memory::Destroy(m_data + m_size);
    }

    /// \brief Resizes the array to the specified size. If the new size is greater than the current size, new elements
    /// are default-constructed. If the new size is smaller, excess elements are destroyed.
    /// \param newSize The desired size of the array.
    void Resize(SizeType newSize)
    {
        if (newSize > m_size)
        {
            Reserve(newSize);
            Memory::ConstructRange(m_data + m_size, newSize - m_size);
        }
        else if (newSize < m_size) { Memory::DestroyRange(m_data + newSize, m_size - newSize); }
        m_size = newSize;
    }

    /// \brief Resizes the array to the specified size and initializes new elements with the given value. If the new
    /// size is greater than the current size, new elements are copy-constructed from the provided value. If the new
    /// size is smaller, excess elements are destroyed.
    /// \param newSize The desired size of the array.
    /// \param value The value to initialize new elements with.
    void Resize(SizeType newSize, const T& value)
    {
        if (newSize > m_size)
        {
            Reserve(newSize);
            Memory::UninitializedFill(m_data + m_size, newSize - m_size, value);
        }
        else if (newSize < m_size) { Memory::DestroyRange(m_data + newSize, m_size - newSize); }
        m_size = newSize;
    }

    /// \brief Inserts a copy of the specified value at the given position in the array.
    /// \param pos The position at which to insert the element. Must be a valid iterator within the range [begin(),
    /// end()].
    /// \param value The value to insert. The value will be copy-constructed.
    /// \return An iterator pointing to the newly inserted element.
    Iterator Insert(ConstIterator pos, const T& value) { return Emplace(pos, value); }

    /// \brief Inserts the specified value at the given position in the array, transferring ownership.
    /// \param pos The position at which to insert the element. Must be a valid iterator within the range [begin(),
    /// end()].
    /// \param value The value to insert. The value will be move-constructed.
    /// \return An iterator pointing to the newly inserted element.
    Iterator Insert(ConstIterator pos, T&& value) { return Emplace(pos, std::move(value)); }

    /// \brief Constructs an element in-place at the specified position in the array.
    /// \tparam Args The types of the arguments to forward to the element's constructor.
    /// \param pos The position at which to insert the element. Must be a valid iterator within the range [begin(),
    /// end()].
    /// \param args The arguments to forward to the element's constructor.
    /// \return An iterator pointing to the newly constructed element.
    template <typename... Args>
    Iterator Emplace(ConstIterator pos, Args&&... args)
    {
        SizeType index = static_cast<SizeType>(pos - m_data);
        GP_ASSERT(index <= m_size, "Insert position out of bounds");

        if (m_size == m_capacity)
        {
            SizeType newCapacity = CalculateGrowth(m_size + 1);
            Pointer newData = AllocateBuffer(newCapacity);

            Memory::RelocateRange(newData, m_data, index);
            Memory::Construct<T>(newData + index, std::forward<Args>(args)...);
            Memory::RelocateRange(newData + index + 1, m_data + index, m_size - index);

            FreeBuffer();
            m_data = newData;
            m_capacity = newCapacity;
        }
        else
        {
            if (index < m_size)
            {
                Memory::Construct<T>(m_data + m_size, std::move(m_data[m_size - 1]));
                for (SizeType i = m_size - 1; i > index; --i) { m_data[i] = std::move(m_data[i - 1]); }
                m_data[index] = T(std::forward<Args>(args)...);
            }
            else { Memory::Construct<T>(m_data + index, std::forward<Args>(args)...); }
        }
        ++m_size;
        return m_data + index;
    }

    /// \brief Removes the element at the specified position from the array.
    /// \param pos The position of the element to remove. Must be a valid iterator within the range [begin(), end()).
    /// \return An iterator pointing to the element following the erased element, or end() if the erased element was the
    /// last element.
    Iterator Erase(ConstIterator pos)
    {
        SizeType index = static_cast<SizeType>(pos - m_data);
        GP_ASSERT(index < m_size, "Erase position out of bounds");

        // Shift elements left
        for (SizeType i = index; i < m_size - 1; ++i) { m_data[i] = std::move(m_data[i + 1]); }
        --m_size;
        Memory::Destroy(m_data + m_size);

        return m_data + index;
    }

    /// \brief Removes all elements in the specified range from the array.
    /// \param first The beginning of the range to erase. Must be a valid iterator within the array.
    /// \param last The end of the range to erase (exclusive). The range includes all elements in the half-open interval
    /// [first, last).
    /// \return An iterator pointing to the element following the erased elements, or end() if all elements after the
    /// erase position were removed.
    Iterator Erase(ConstIterator first, ConstIterator last)
    {
        SizeType startIndex = static_cast<SizeType>(first - m_data);
        SizeType endIndex = static_cast<SizeType>(last - m_data);
        GP_ASSERT(startIndex <= endIndex && endIndex <= m_size, "Erase range out of bounds");
        SizeType eraseCount = endIndex - startIndex;

        if (eraseCount == 0) { return m_data + startIndex; }

        // Shift elements left
        Memory::MoveAssignRange(m_data + startIndex, m_data + endIndex, m_size - endIndex);
        Memory::DestroyRange(m_data + m_size - eraseCount, eraseCount);
        m_size -= eraseCount;

        return m_data + startIndex;
    }

    /// \brief Removes the element at the specified index by swapping it with the last element and then removing the
    /// last element. This is more efficient than Erase for unordered arrays.
    /// \param index The index of the element to remove. Must be less than the current size of the array.
    void EraseSwapBack(SizeType index)
    {
        GP_ASSERT(index < m_size, "EraseSwapBack index out of bounds");
        if (index != m_size - 1) { m_data[index] = std::move(m_data[m_size - 1]); }
        PopBack();
    }

    /// \brief Exchanges the contents of this array with another array.
    /// \param other The array to swap with. After the swap, this array will contain the elements and state of the other
    /// array, and vice versa.
    void Swap(TArray& other) noexcept
    {
        std::swap(m_data, other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
        std::swap(m_allocator, other.m_allocator);
    }

    /// \brief Searches for the first occurrence of the specified value in the array.
    /// \param value The value to search for.
    /// \return An iterator pointing to the first element equal to the specified value, or End() if the element is not
    /// found.
    GP_NODISCARD Iterator Find(const T& value) noexcept
    {
        for (SizeType i = 0; i < m_size; ++i)
        {
            if (m_data[i] == value) { return m_data + i; }
        }
        return End();
    }

    /// \brief Searches for the first occurrence of the specified value in the array (const version).
    /// \param value The value to search for.
    /// \return A const iterator pointing to the first element equal to the specified value, or CEnd() if the element is
    /// not found.
    GP_NODISCARD ConstIterator Find(const T& value) const noexcept
    {
        for (SizeType i = 0; i < m_size; ++i)
        {
            if (m_data[i] == value) { return m_data + i; }
        }
        return CEnd();
    }

    /// \brief Searches for the first occurrence of the specified value in the array (alias for Find).
    /// \param value The value to search for.
    /// \return An iterator pointing to the first element equal to the specified value, or End() if the element is not
    /// found.
    GP_NODISCARD Iterator FindFirstOf(const T& value) noexcept { return Find(value); }

    /// \brief Searches for the first occurrence of the specified value in the array (const alias for Find).
    /// \param value The value to search for.
    /// \return A const iterator pointing to the first element equal to the specified value, or CEnd() if the element is
    /// not found.
    GP_NODISCARD ConstIterator FindFirstOf(const T& value) const noexcept { return Find(value); }

    /// \brief Searches for the last occurrence of the specified value in the array.
    /// \param value The value to search for.
    /// \return An iterator pointing to the last element equal to the specified value, or End() if the element is not
    /// found.
    GP_NODISCARD Iterator FindLastOf(const T& value) noexcept
    {
        for (SizeType i = m_size; i > 0; --i)
        {
            if (m_data[i - 1] == value) { return m_data + i - 1; }
        }
        return End();
    }

    /// \brief Searches for the last occurrence of the specified value in the array (const version).
    /// \param value The value to search for.
    /// \return A const iterator pointing to the last element equal to the specified value, or CEnd() if the element is
    /// not found.
    GP_NODISCARD ConstIterator FindLastOf(const T& value) const noexcept
    {
        for (SizeType i = m_size; i > 0; --i)
        {
            if (m_data[i - 1] == value) { return m_data + i - 1; }
        }
        return CEnd();
    }

    /// \brief Searches for the first element that is not equal to the specified value.
    /// \param value The value to search for elements not equal to.
    /// \return An iterator pointing to the first element not equal to the specified value, or End() if no such element
    /// is found.
    GP_NODISCARD Iterator FindFirstNotOf(const T& value) noexcept
    {
        for (SizeType i = 0; i < m_size; ++i)
        {
            if (m_data[i] != value) { return m_data + i; }
        }
        return End();
    }

    /// \brief Searches for the first element that is not equal to the specified value (const version).
    /// \param value The value to search for elements not equal to.
    /// \return A const iterator pointing to the first element not equal to the specified value, or CEnd() if no such
    /// element is found.
    GP_NODISCARD ConstIterator FindFirstNotOf(const T& value) const noexcept
    {
        for (SizeType i = 0; i < m_size; ++i)
        {
            if (m_data[i] != value) { return m_data + i; }
        }
        return CEnd();
    }

    /// \brief Searches for the last element that is not equal to the specified value.
    /// \param value The value to search for elements not equal to.
    /// \return An iterator pointing to the last element not equal to the specified value, or End() if no such element
    /// is found.
    GP_NODISCARD Iterator FindLastNotOf(const T& value) noexcept
    {
        for (SizeType i = m_size; i > 0; --i)
        {
            if (m_data[i - 1] != value) { return m_data + i - 1; }
        }
        return End();
    }

    /// \brief Searches for the last element that is not equal to the specified value (const version).
    /// \param value The value to search for elements not equal to.
    /// \return A const iterator pointing to the last element not equal to the specified value, or CEnd() if no such
    /// element is found.
    GP_NODISCARD ConstIterator FindLastNotOf(const T& value) const noexcept
    {
        for (SizeType i = m_size; i > 0; --i)
        {
            if (m_data[i - 1] != value) { return m_data + i - 1; }
        }
        return CEnd();
    }

    /// \brief Checks whether the specified value exists in the array.
    /// \param value The value to search for.
    /// \return true if the value is found in the array; otherwise, false.
    GP_NODISCARD bool Contains(const T& value) const noexcept { return Find(value) != End(); }

    /// \brief Returns the index of the first occurrence of the specified value in the array.
    /// \param value The value to search for.
    /// \return The index of the first element equal to the specified value, or NPOS if the element is not found.
    GP_NODISCARD SizeType IndexOf(const T& value) const noexcept
    {
        for (SizeType i = 0; i < m_size; ++i)
        {
            if (m_data[i] == value) { return i; }
        }
        return NPOS;
    }

    /// \brief Returns the index of the first occurrence of the specified value in the array (alias for IndexOf).
    /// \param value The value to search for.
    /// \return The index of the first element equal to the specified value, or NPOS if the element is not found.
    GP_NODISCARD SizeType IndexFirstOf(const T& value) const noexcept { return IndexOf(value); }

    /// \brief Returns the index of the last occurrence of the specified value in the array.
    /// \param value The value to search for.
    /// \return The index of the last element equal to the specified value, or NPOS if the element is not found.
    GP_NODISCARD SizeType IndexLastOf(const T& value) const noexcept
    {
        for (SizeType i = m_size; i > 0; --i)
        {
            if (m_data[i - 1] == value) { return i - 1; }
        }
        return NPOS;
    }

    /// \brief Returns the index of the first element that is not equal to the specified value.
    /// \param value The value to search for elements not equal to.
    /// \return The index of the first element not equal to the specified value, or NPOS if no such element is found.
    GP_NODISCARD SizeType IndexFirstNotOf(const T& value) const noexcept
    {
        for (SizeType i = 0; i < m_size; ++i)
        {
            if (m_data[i] != value) { return i; }
        }
        return NPOS;
    }

    /// \brief Returns the index of the last element that is not equal to the specified value.
    /// \param value The value to search for elements not equal to.
    /// \return The index of the last element not equal to the specified value, or NPOS if no such element is found.
    GP_NODISCARD SizeType IndexLastNotOf(const T& value) const noexcept
    {
        for (SizeType i = m_size; i > 0; --i)
        {
            if (m_data[i - 1] != value) { return i - 1; }
        }
        return NPOS;
    }

    /// \brief Returns a reference to the allocator used by this array for memory management.
    /// \return A reference to the allocator instance.
    GP_NODISCARD Alloc& GetAllocator() noexcept { return m_allocator; }

    /// \brief Returns a const reference to the allocator used by this array for memory management.
    /// \return A const reference to the allocator instance.
    GP_NODISCARD const Alloc& GetAllocator() const noexcept { return m_allocator; }

    /// \brief Returns an iterator to the first element of the array.
    /// \return An iterator pointing to the first element of the array. If the array is empty, the returned iterator
    /// will be equal to the end iterator.
    Iterator Begin() noexcept { return m_data; }

    /// \brief Returns an iterator to the element following the last element of the array.
    /// \return An iterator pointing to the element following the last element of the array. This element acts as a
    /// placeholder and is not dereferenceable. If the array is empty, this will be equal to the begin iterator.
    Iterator End() noexcept { return m_data + m_size; }

    /// \brief Returns a const iterator to the first element of the array.
    /// \return A const iterator pointing to the first element of the array. If the array is empty, the returned
    /// iterator will be equal to the end iterator.
    ConstIterator Begin() const noexcept { return m_data; }

    /// \brief Returns a const iterator to the element following the last element of the array.
    /// \return A const iterator pointing to the element following the last element of the array. This element acts as a
    /// placeholder and is not dereferenceable. If the array is empty, this will be equal to the begin iterator.
    ConstIterator End() const noexcept { return m_data + m_size; }

    /// \brief Returns a const iterator to the first element of the array.
    /// \return A const iterator pointing to the first element of the array. If the array is empty, the returned
    /// iterator will be equal to the end iterator.
    ConstIterator CBegin() const noexcept { return m_data; }

    /// \brief Returns a const iterator to the element following the last element of the array.
    /// \return A const iterator pointing to the element following the last element of the array. This element acts as a
    /// placeholder and is not dereferenceable. If the array is empty, this will be equal to the begin iterator.
    ConstIterator CEnd() const noexcept { return m_data + m_size; }

    /// \brief Returns a reverse iterator to the last element of the array.
    /// \return A reverse iterator pointing to the last element of the array.
    ReverseIterator RBegin() noexcept { return ReverseIterator(End()); }

    /// \brief Returns a reverse iterator to the element preceding the first element of the array.
    /// \return A reverse iterator pointing to the element preceding the first element of the array.
    ReverseIterator REnd() noexcept { return ReverseIterator(Begin()); }

    /// \brief Returns a const reverse iterator to the last element of the array.
    /// \return A const reverse iterator pointing to the last element of the array.
    ConstReverseIterator CRBegin() const noexcept { return ConstReverseIterator(CEnd()); }

    /// \brief Returns a const reverse iterator to the element preceding the first element of the array.
    /// \return A const reverse iterator pointing to the element preceding the first element of the array.
    ConstReverseIterator CREnd() const noexcept { return ConstReverseIterator(CBegin()); }

    /// \brief Returns an iterator to the first element of the array (lowercase version for STL compatibility).
    /// \return An iterator pointing to the first element of the array. If the array is empty, the returned iterator
    /// will be equal to the end iterator.
    Iterator begin() noexcept { return m_data; }

    /// \brief Returns an iterator to the element following the last element of the array (lowercase version for STL
    /// compatibility).
    /// \return An iterator pointing to the element following the last element of the array. This element acts as a
    /// placeholder and is not dereferenceable.
    Iterator end() noexcept { return m_data + m_size; }

    /// \brief Returns a const iterator to the first element of the array (lowercase version for STL compatibility).
    /// \return A const iterator pointing to the first element of the array. If the array is empty, the returned
    /// iterator will be equal to the end iterator.
    ConstIterator begin() const noexcept { return m_data; }

    /// \brief Returns a const iterator to the element following the last element of the array (lowercase version for
    /// STL compatibility).
    /// \return A const iterator pointing to the element following the last element of the array. This element acts as a
    /// placeholder and is not dereferenceable.
    ConstIterator end() const noexcept { return m_data + m_size; }

    /// \brief Returns a const iterator to the first element of the array.
    /// \return A const iterator pointing to the first element of the array. If the array is empty, the returned
    /// iterator will be equal to the end iterator.
    ConstIterator cbegin() const noexcept { return m_data; }

    /// \brief Returns a const iterator to the element following the last element of the array.
    /// \return A const iterator pointing to the element following the last element of the array. This element acts as a
    /// placeholder and is not dereferenceable.
    ConstIterator cend() const noexcept { return m_data + m_size; }

    /// \brief Returns a reverse iterator to the last element of the array.
    /// \return A reverse iterator pointing to the last element of the array.
    ReverseIterator rbegin() noexcept { return ReverseIterator(end()); }

    /// \brief Returns a reverse iterator to the element preceding the first element of the array.
    /// \return A reverse iterator pointing to the element preceding the first element of the array.
    ReverseIterator rend() noexcept { return ReverseIterator(begin()); }

    /// \brief Returns a const reverse iterator to the last element of the array.
    /// \return A const reverse iterator pointing to the last element of the array.
    ConstReverseIterator crbegin() const noexcept { return ConstReverseIterator(cend()); }

    /// \brief Returns a const reverse iterator to the element preceding the first element of the array.
    /// \return A const reverse iterator pointing to the element preceding the first element of the array.
    ConstReverseIterator crend() const noexcept { return ConstReverseIterator(cbegin()); }

private:
    /// \brief Calculates the new capacity for the array when growing. The growth strategy typically increases the
    /// capacity by a factor (e.g., 1.5x) to balance between minimizing reallocations and controlling memory usage. The
    /// new capacity is also adjusted to ensure it can accommodate the required number of elements and meets a minimum
    /// threshold.
    /// \param required The minimum number of elements that the new capacity must be able to accommodate. This is
    /// typically the current size plus one for a single element growth.
    /// \return The calculated new capacity for the array.
    GP_NODISCARD SizeType CalculateGrowth(SizeType required) const noexcept
    {
        SizeType newCapacity = m_capacity + m_capacity / 2;   // 1.5x
        if (newCapacity < required) { newCapacity = required; }
        if (newCapacity < 4) { newCapacity = 4; }
        return newCapacity;
    }

    /// \brief Grows the array's capacity to accommodate at least one more element. The new capacity is determined by
    /// the growth strategy implemented in the CalculateGrowth function, which typically increases the capacity by a
    /// factor (e.g., 1.5x) to balance between minimizing reallocations and controlling memory usage.
    void Grow() { Reallocate(CalculateGrowth(m_size + 1)); }

    /// \brief Grows the array's capacity to at least the specified new capacity. If the new capacity is greater than
    /// the current capacity, the array will reallocate its internal storage to accommodate the new capacity.
    /// \param newCapacity The minimum capacity to grow to. Must be greater than the current capacity of the array.
    void GrowTo(SizeType newCapacity)
    {
        if (newCapacity > m_capacity) { Reallocate(newCapacity); }
    }

    /// \brief Allocates a new memory buffer with the specified capacity. The capacity is the number of elements, not
    /// bytes.
    /// \param capacity The number of elements the new buffer should be able to hold. Must be greater than zero.
    /// \return A pointer to the newly allocated buffer, or nullptr if allocation fails.
    GP_NODISCARD Pointer AllocateBuffer(SizeType capacity)
    {
        return static_cast<Pointer>(m_allocator.Allocate(capacity * sizeof(T), alignof(T)));
    }

    /// \brief Deallocates the internal memory buffer if it exists, and resets the data pointer and capacity to indicate
    /// that the array is now empty and has no allocated storage.
    void FreeBuffer()
    {
        if (m_data)
        {
            m_allocator.Deallocate(m_data);
            m_data = nullptr;
            m_capacity = 0;
        }
    }

    /// \brief Reallocates the internal storage to a new capacity. The new capacity must be greater than or equal to the
    /// current size of the array.
    /// \param newCapacity The new capacity for the array. Must be greater than or equal to the current size of the
    /// array.
    void Reallocate(SizeType newCapacity)
    {
        GP_ASSERT(newCapacity >= m_size, "Cannot reallocate below current size");

        Pointer newData = AllocateBuffer(newCapacity);
        if (m_data && m_size > 0) { Memory::RelocateRange(newData, m_data, m_size); }
        if (m_data) { m_allocator.Deallocate(m_data); }
        m_data = newData;
        m_capacity = newCapacity;
    }
};

}   // namespace GP::Container
