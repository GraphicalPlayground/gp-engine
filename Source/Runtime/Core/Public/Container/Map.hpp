// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/Allocator.hpp"
#include "Memory/MemoryForward.hpp"
#include "Memory/MemoryUtilities.hpp"
#include "Templates/Concepts.hpp"
#include <functional>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace GP::Container
{

/// \brief An ordered map from keys to values, stored in a red-black tree.
///
/// Keys are unique. Lookup, insertion, and removal are O(log N).
/// Iteration visits elements in ascending key order.
///
/// **Design note**: The TMap template parameter uses a single T that defaults to
/// TAllocator<T> for API uniformity with other containers. For the actual
/// key-value storage we define a separate template below. This matches the
/// forward declaration in ContainerForward.hpp where TMap<T, Alloc> is declared.
///
/// However, for a proper key-value map we need two type parameters (Key, Value).
/// The forward declaration's single T is treated as Key, and this implementation
/// provides full Key-Value functionality while maintaining the module's convention.
///
/// **Usage**: `TMap<Key, Value>` where Key must support operator<.
///
/// \tparam K Key type (must support operator<).
/// \tparam V Value type.
/// \tparam Alloc Allocator type.
template <typename Key, typename Value, GP::Concepts::IsAllocator Alloc = GP::Memory::TAllocator<Key>>
class TMap
{
public:
    using KeyType = Key;
    using MappedType = Value;
    using ValueType = std::pair<const Key, Value>;
    using Pointer = ValueType*;
    using ConstPointer = const ValueType*;
    using Reference = ValueType&;
    using ConstReference = const ValueType&;
    using SizeType = SizeT;
    using DifferenceType = OffsetT;

private:
    /// \brief Node structure for the red-black tree.
    enum class EColor : UInt8
    {
        Red,
        Black
    };

    /// \brief Node structure for the red-black tree.
    struct FNode
    {
    public:
        ValueType data;
        FNode* left = nullptr;
        FNode* right = nullptr;
        FNode* parent = nullptr;
        EColor color = EColor::Red;

    public:
        template <typename... Args>
        explicit FNode(Args&&... args)
            : data(std::forward<Args>(args)...)
        {}
    };

private:
    using NodeAllocator = Memory::TAllocator<FNode>;

public:
    /// \brief Bidirectional iterator for TMap, allowing traversal of the map in sorted order. This iterator is
    /// read-only and provides access to the key-value pairs stored in the map. It supports all standard bidirectional
    /// iterator operations.
    class ConstIterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = ValueType;
        using difference_type = OffsetT;
        using pointer = const ValueType*;
        using reference = const ValueType&;

    private:
        FNode* m_node = nullptr;       //<! Pointer to the current node in the tree.
        FNode* m_sentinel = nullptr;   //<! Pointer to the sentinel node used to represent null leaves.

    public:
        /// \brief Default constructor that creates an invalid iterator.
        ConstIterator() noexcept = default;

        /// \brief Constructor that initializes the iterator to point to a specific node in the tree.
        /// \param node Pointer to the node that the iterator should point to. This node must not be the sentinel.
        /// \param sentinel Pointer to the sentinel node used to represent null leaves in the tree.
        ConstIterator(const FNode* node, const FNode* sentinel) noexcept
            : m_node(const_cast<FNode*>(node))
            , m_sentinel(const_cast<FNode*>(sentinel))
        {}

    public:
        /// \brief Dereference operator that provides access to the key-value pair stored at the current node. The
        /// returned reference is const, as this iterator does not allow modification of the map's contents.
        /// \return A const reference to the key-value pair at the current node.
        reference operator*() const noexcept { return m_node->data; }

        /// \brief Arrow operator that provides access to the members of the key-value pair stored at the current node.
        /// The returned pointer is const, as this iterator does not allow modification of the map's contents.
        /// \return A const pointer to the key-value pair at the current node.
        pointer operator->() const noexcept { return &m_node->data; }

        /// \brief Pre-increment operator that advances the iterator to the next element in the map (in sorted order).
        /// \return A reference to the incremented iterator.
        ConstIterator& operator++() noexcept
        {
            if (m_node->right != m_sentinel)
            {
                m_node = m_node->right;
                while (m_node->left != m_sentinel) { m_node = m_node->left; }
            }
            else
            {
                FNode* p = m_node->parent;
                while (p != m_sentinel && m_node == p->right)
                {
                    m_node = p;
                    p = p->parent;
                }
                m_node = p;
            }
            return *this;
        }

        /// \brief Post-increment operator that advances the iterator to the next element in the map (in sorted order)
        /// and returns the previous position of the iterator.
        /// \return A copy of the iterator before it was incremented.
        ConstIterator operator++(int) noexcept
        {
            auto t = *this;
            ++(*this);
            return t;
        }

        /// \brief Pre-decrement operator that moves the iterator to the previous element in the map (in sorted order).
        /// \return A reference to the decremented iterator.
        ConstIterator& operator--() noexcept
        {
            if (m_node == m_sentinel)
            {
                m_node = m_sentinel->right;
                return *this;
            }
            if (m_node->left != m_sentinel)
            {
                m_node = m_node->left;
                while (m_node->right != m_sentinel) { m_node = m_node->right; }
            }
            else
            {
                FNode* p = m_node->parent;
                while (p != m_sentinel && m_node == p->left)
                {
                    m_node = p;
                    p = p->parent;
                }
                m_node = p;
            }
            return *this;
        }

        /// \brief Post-decrement operator that moves the iterator to the previous element in the map (in sorted order)
        /// and returns the previous position of the iterator.
        /// \return A copy of the iterator before it was decremented.
        ConstIterator operator--(int) noexcept
        {
            auto t = *this;
            --(*this);
            return t;
        }

        /// \brief Equality operator that checks if two iterators are equal, meaning they point to the same node in the
        /// tree.
        /// \param other The other iterator to compare with.
        /// \return true if the iterators point to the same node, false if they point to different nodes.
        bool operator==(const ConstIterator& other) const noexcept { return m_node == other.m_node; }

        /// \brief Inequality operator that checks if two iterators are not equal, meaning they point to different nodes
        /// in the tree.
        /// \param other The other iterator to compare with.
        /// \return true if the iterators point to different nodes, false if they point to the same node.
        bool operator!=(const ConstIterator& other) const noexcept { return m_node != other.m_node; }

    public:
        /// \brief Get the underlying node pointer of the iterator.
        /// \return The underlying node pointer.
        FNode* GetNode() const noexcept { return m_node; }
    };

    /// \brief Mutable iterator for TMap, allowing traversal and modification of the map in sorted order. This iterator
    /// provides access to the key-value pairs stored in the map and allows modification of the values (but not the
    /// keys, as they are const). It supports all standard bidirectional iterator operations.
    class MutableIterator : public ConstIterator
    {
    public:
        using ConstIterator::ConstIterator;
        using reference = ValueType&;
        using pointer = ValueType*;

    public:
        /// \brief Dereference operator that provides access to the key-value pair stored at the current node.
        /// \return A reference to the key-value pair at the current node, allowing modification of the value.
        reference operator*() const noexcept { return this->GetNode()->data; }

        /// \brief Arrow operator that provides access to the members of the key-value pair stored at the current node.
        /// \return A pointer to the key-value pair at the current node, allowing modification of the value.
        pointer operator->() const noexcept { return &this->GetNode()->data; }
    };

    using Iterator = MutableIterator;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

private:
    FNode* m_root = nullptr;         //<! Pointer to the root of the tree. nullptr if the tree is empty.
    FNode* m_sentinel = nullptr;     //<! Sentinel node used to represent null leaves.
    SizeType m_size = 0;             //<! Number of key-value pairs in the map.
    NodeAllocator m_nodeAllocator;   //<! Allocator for tree nodes.

public:
    /// \brief Default constructor that initializes an empty map with a sentinel node.
    TMap() { InitSentinel(); }

    /// \brief Constructs a TMap from an initializer list of key-value pairs.
    /// \param initList An initializer list of key-value pairs to initialize the map with.
    TMap(std::initializer_list<ValueType> initList)
    {
        InitSentinel();
        for (const auto& item: initList) { Insert(item); }
    }

    /// \brief Constructs a TMap from a range of key-value pairs denoted by a pair of input iterators.
    /// Duplicate keys are silently ignored (first occurrence wins), matching the unique-key contract.
    /// \tparam InputIt An input iterator whose value_type is convertible to ValueType.
    /// \param first Iterator to the first element of the range.
    /// \param last  Iterator one past the last element of the range.
    template <typename InputIt>
    TMap(InputIt first, InputIt last)
    {
        InitSentinel();
        for (; first != last; ++first) { Insert(*first); }
    }

    /// \brief Copy constructor that creates a new TMap as a copy of an existing one.
    /// \param other The TMap to copy from.
    TMap(const TMap& other)
    {
        InitSentinel();
        for (const auto& item: other) { Insert(item); }
    }

    /// \brief Move constructor that creates a new TMap by taking ownership of the resources of an existing one.
    /// \param other The TMap to move from. After this operation, other will be in a valid but unspecified state.
    TMap(TMap&& other) noexcept
    {
        InitSentinel();
        std::swap(m_root, other.m_root);
        std::swap(m_sentinel, other.m_sentinel);
        std::swap(m_size, other.m_size);
    }

    /// \brief Destructor that cleans up all resources used by the TMap.
    ~TMap()
    {
        Clear();
        DestroySentinel();
    }

public:
    /// \brief Copy assignment operator that assigns the contents of one TMap to another, replacing the existing
    /// contents.
    /// \param other The other map to copy from.
    /// \return Reference to the assigned TMap.
    TMap& operator=(const TMap& other)
    {
        if (this != &other)
        {
            Clear();
            for (const auto& item: other) { Insert(item); }
        }
        return *this;
    }

    /// \brief Move assignment operator that assigns the contents of one TMap to another by taking ownership of the
    /// resources of the source TMap.
    /// \param other The other map to move from.
    /// \return Reference to the assigned TMap.
    TMap& operator=(TMap&& other) noexcept
    {
        if (this != &other)
        {
            Clear();
            std::swap(m_root, other.m_root);
            std::swap(m_sentinel, other.m_sentinel);
            std::swap(m_size, other.m_size);
        }
        return *this;
    }

    /// \brief Subscript operator that provides access to the value associated with a given key.
    /// \param key The key to access or insert in the map.
    /// \return Reference to the value associated with the key.
    Value& operator[](const Key& key)
    {
        auto [it, inserted] = Insert({ key, Value{} });
        return const_cast<Value&>(it->second);
    }

    /// \brief Subscript operator that provides access to the value associated with a given key, allowing for move
    /// semantics.
    /// \param key The key to access or insert in the map. This key will be moved only when a new entry is inserted.
    /// If the key already exists the key is left unmodified.
    /// \return Reference to the value associated with the key.
    Value& operator[](Key&& key)
    {
        Iterator it = Find(key);
        if (it != end()) { return it->second; }
        auto [ins, ok] = Insert({ std::move(key), Value{} });
        return ins->second;
    }

public:
    /// \brief Gets the number of key-value pairs currently stored in the map.
    /// \return The number of key-value pairs in the map.
    GP_NODISCARD SizeType Size() const noexcept { return m_size; }

    /// \brief Checks whether the map is empty (contains no key-value pairs).
    /// \return true if the map is empty, false otherwise.
    GP_NODISCARD bool IsEmpty() const noexcept { return m_size == 0; }

    /// \brief Returns the theoretical maximum number of nodes this map can hold, limited by the allocator's single
    /// allocation granularity. This is not a hard capacity cap; it is an upper-bound hint.
    /// \return The maximum number of key-value pairs the map can theoretically hold.
    GP_NODISCARD SizeType MaxCount() const noexcept { return static_cast<SizeType>(-1) / sizeof(FNode); }

    /// \brief Inserts a key-value pair into the map.
    /// \param value The key-value pair to insert into the map.
    /// \return A pair consisting of an iterator to the inserted or existing element, and a bool indicating whether the
    /// insertion took place.
    std::pair<Iterator, bool> Insert(const ValueType& value) { return InsertImpl(value); }

    /// \brief Inserts a key-value pair into the map.
    /// \param value The key-value pair to insert into the map.
    /// \return A pair consisting of an iterator to the inserted or existing element, and a bool indicating whether the
    /// insertion took place.
    std::pair<Iterator, bool> Insert(ValueType&& value) { return InsertImpl(std::move(value)); }

    /// \brief Emplaces a key-value pair into the map by constructing it in-place with the provided arguments.
    /// \tparam Args The types of the arguments to forward to the constructor of the key-value pair. These should
    /// typically be compatible with the constructor of std::pair<const Key, Value>.
    /// \param args The arguments to forward to the constructor of the key-value pair. These will be perfectly forwarded
    /// to construct the key-value pair in-place.
    /// \return A pair consisting of an iterator to the inserted or existing element, and a bool indicating whether the
    /// insertion took place.
    template <typename... Args>
    std::pair<Iterator, bool> Emplace(Args&&... args)
    {
        ValueType val(std::forward<Args>(args)...);
        return InsertImpl(std::move(val));
    }

    /// \brief Erases the key-value pair at the specified position in the map, given by the iterator. The iterator must
    /// be valid and dereferenceable (i.e., it must not be the end iterator).
    /// \param pos An iterator pointing to the element to erase. This iterator must be valid and dereferenceable (not
    /// end()).
    /// \return An iterator pointing to the element following the erased element, or end() if the erased element was the
    /// last one.
    Iterator Erase(ConstIterator pos)
    {
        FNode* node = pos.GetNode();
        GP_ASSERT(node != m_sentinel, "Cannot erase end");
        ConstIterator next = pos;
        ++next;
        EraseNode(node);
        return Iterator(next.GetNode(), m_sentinel);
    }

    /// \brief Erases the key-value pair with the specified key from the map, if it exists.
    /// \param key The key of the element to erase.
    /// \return The number of elements erased (1 if the key was found and erased, 0 if the key was not found).
    SizeType Erase(const Key& key)
    {
        auto it = Find(key);
        if (it != end())
        {
            Erase(it);
            return 1;
        }
        return 0;
    }

    /// \brief Clears all key-value pairs from the map, leaving it empty. This function destroys all nodes in the tree
    /// and resets the size to zero, but does not deallocate the sentinel node or change the allocator state.
    void Clear() noexcept
    {
        DestroyTree(m_root);
        m_root = m_sentinel;
        m_sentinel->right = m_sentinel;
        m_size = 0;
    }

    /// \brief Finds the key-value pair with the specified key in the map and returns an iterator to it.
    /// \param key The key to search for in the map.
    /// \return An iterator to the key-value pair with the specified key if it exists, or end() if the key is not found
    /// in the map.
    GP_NODISCARD ConstIterator Find(const Key& key) const
    {
        FNode* node = m_root;
        while (node != m_sentinel)
        {
            if (key < node->data.first) { node = node->left; }
            else if (node->data.first < key) { node = node->right; }
            else { return ConstIterator(node, m_sentinel); }
        }
        return end();
    }

    /// \brief Finds the key-value pair with the specified key in the map and returns an iterator to it.
    /// \param key The key to search for in the map.
    /// \return An iterator to the key-value pair with the specified key if it exists, or end() if the key is not found
    /// in the map.
    GP_NODISCARD Iterator Find(const Key& key)
    {
        FNode* node = m_root;
        while (node != m_sentinel)
        {
            if (key < node->data.first) { node = node->left; }
            else if (node->data.first < key) { node = node->right; }
            else { return Iterator(node, m_sentinel); }
        }
        return end();
    }

    /// \brief Returns a reference to the value mapped to the given key. Asserts (debug) if the key does not exist.
    /// Unlike operator[], At() never inserts a new entry.
    /// \param key The key to look up.
    /// \return A reference to the value associated with the key.
    GP_NODISCARD Value& At(const Key& key)
    {
        Iterator it = Find(key);
        GP_ASSERT(it != end(), "TMap::At - key not found");
        return it->second;
    }

    /// \brief Read-only version of At(). Asserts (debug) if the key does not exist.
    /// \param key The key to look up.
    /// \return A const reference to the value associated with the key.
    GP_NODISCARD const Value& At(const Key& key) const
    {
        ConstIterator it = Find(key);
        GP_ASSERT(it != end(), "TMap::At - key not found");
        return it->second;
    }

    /// \brief Checks if the map contains a key-value pair with the specified key.
    /// \param key The key to check for existence in the map.
    /// \return true if the map contains a key-value pair with the specified key, false otherwise.
    GP_NODISCARD bool Contains(const Key& key) const { return Find(key) != end(); }

    /// \brief Returns an iterator to the first element in the map. If the map is empty, returns end().
    /// \return The iterator to the first element in the map, or end() if the map is empty.
    Iterator begin() noexcept
    {
        FNode* n = m_root;
        while (n != m_sentinel && n->left != m_sentinel) { n = n->left; }
        return Iterator(n, m_sentinel);
    }

    /// \brief Returns an iterator to the element following the last element in the map.
    /// \return An iterator to the element following the last element in the map (end iterator).
    Iterator end() noexcept { return Iterator(m_sentinel, m_sentinel); }

    /// \brief Returns a const iterator to the first element in the map. If the map is empty, returns end().
    /// \return A const iterator to the first element in the map, or end() if the map is empty.
    ConstIterator begin() const noexcept
    {
        FNode* n = m_root;
        while (n != m_sentinel && n->left != m_sentinel) { n = n->left; }
        return ConstIterator(n, m_sentinel);
    }

    /// \brief Returns a const iterator to the element following the last element in the map.
    /// \return A const iterator to the element following the last element in the map (end iterator).
    ConstIterator end() const noexcept { return ConstIterator(m_sentinel, m_sentinel); }

    /// \brief Returns a const iterator to the first element in the map. If the map is empty, returns end().
    /// \return A const iterator to the first element in the map, or end() if the map is empty.
    ConstIterator cbegin() const noexcept { return begin(); }

    /// \brief Returns a const iterator to the element following the last element in the map.
    /// \return A const iterator to the element following the last element in the map (end iterator).
    ConstIterator cend() const noexcept { return end(); }

    /// \brief Returns an iterator to the first element in the map. If the map is empty, returns End().
    /// \return The iterator to the first element in the map, or End() if the map is empty.
    Iterator Begin() noexcept
    {
        FNode* n = m_root;
        while (n != m_sentinel && n->left != m_sentinel) { n = n->left; }
        return Iterator(n, m_sentinel);
    }

    /// \brief Returns an iterator to the element following the last element in the map.
    /// \return An iterator to the element following the last element in the map (end iterator).
    Iterator End() noexcept { return Iterator(m_sentinel, m_sentinel); }

    /// \brief Returns a const iterator to the first element in the map. If the map is empty, returns End().
    /// \return A const iterator to the first element in the map, or End() if the map is empty.
    ConstIterator Begin() const noexcept
    {
        FNode* n = m_root;
        while (n != m_sentinel && n->left != m_sentinel) { n = n->left; }
        return ConstIterator(n, m_sentinel);
    }

    /// \brief Returns a const iterator to the element following the last element in the map.
    /// \return A const iterator to the element following the last element in the map (end iterator).
    ConstIterator End() const noexcept { return ConstIterator(m_sentinel, m_sentinel); }

    /// \brief Returns a const iterator to the first element in the map. If the map is empty, returns End().
    /// \return A const iterator to the first element in the map, or End() if the map is empty.
    ConstIterator CBegin() const noexcept { return Begin(); }

    /// \brief Returns a const iterator to the element following the last element in the map.
    /// \return A const iterator to the element following the last element in the map (end iterator).
    ConstIterator CEnd() const noexcept { return End(); }

    /// \brief Returns a reverse iterator to the last element in the map (highest key). If the map is empty,
    /// returns rend().
    /// \return A reverse iterator to the last element.
    ReverseIterator rbegin() noexcept { return ReverseIterator(end()); }

    /// \brief Returns a reverse iterator one before the first element in the map.
    /// \return A reverse iterator to the element preceding begin().
    ReverseIterator rend() noexcept { return ReverseIterator(begin()); }

    /// \brief Returns a const reverse iterator to the last element in the map.
    /// \return A const reverse iterator to the last element.
    ConstReverseIterator rbegin() const noexcept { return ConstReverseIterator(end()); }

    /// \brief Returns a const reverse iterator one before the first element.
    /// \return A const reverse iterator to the element preceding begin().
    ConstReverseIterator rend() const noexcept { return ConstReverseIterator(begin()); }

    /// \brief Returns a const reverse iterator to the last element in the map.
    /// \return A const reverse iterator to the last element.
    ConstReverseIterator crbegin() const noexcept { return ConstReverseIterator(end()); }

    /// \brief Returns a const reverse iterator one before the first element.
    /// \return A const reverse iterator to the element preceding begin().
    ConstReverseIterator crend() const noexcept { return ConstReverseIterator(begin()); }

    /// \brief Returns a reverse iterator to the last element in the map (GP-style naming).
    /// \return A reverse iterator to the last element.
    ReverseIterator RBegin() noexcept { return ReverseIterator(End()); }

    /// \brief Returns a reverse iterator one before the first element (GP-style naming).
    /// \return A reverse iterator to the element preceding Begin().
    ReverseIterator REnd() noexcept { return ReverseIterator(Begin()); }

    /// \brief Returns a const reverse iterator to the last element (GP-style naming).
    /// \return A const reverse iterator to the last element.
    ConstReverseIterator RBegin() const noexcept { return ConstReverseIterator(End()); }

    /// \brief Returns a const reverse iterator one before the first element (GP-style naming).
    /// \return A const reverse iterator to the element preceding Begin().
    ConstReverseIterator REnd() const noexcept { return ConstReverseIterator(Begin()); }

    /// \brief Returns a const reverse iterator to the last element (GP-style naming).
    /// \return A const reverse iterator to the last element.
    ConstReverseIterator CRBegin() const noexcept { return RBegin(); }

    /// \brief Returns a const reverse iterator one before the first element (GP-style naming).
    /// \return A const reverse iterator to the element preceding Begin().
    ConstReverseIterator CREnd() const noexcept { return REnd(); }

private:
    /// \brief Initializes the sentinel node used to represent null leaves in the red-black tree.
    /// \note m_sentinel->right is used as a cached pointer to the maximum (rightmost) node in the tree,
    /// enabling O(1) --end() decrement. It is maintained by InsertImpl and EraseNode.
    void InitSentinel()
    {
        void* memory = m_nodeAllocator.Allocate(sizeof(FNode), alignof(FNode));
        m_sentinel = static_cast<FNode*>(memory);
        m_sentinel->left = m_sentinel;
        m_sentinel->right = m_sentinel;
        m_sentinel->parent = m_sentinel;
        m_sentinel->color = EColor::Black;
        m_root = m_sentinel;
    }

    /// \brief Destroys the sentinel node and deallocates its memory.
    void DestroySentinel()
    {
        if (m_sentinel)
        {
            m_nodeAllocator.Deallocate(m_sentinel);
            m_sentinel = nullptr;
        }
    }

    /// \brief Inserts a new key-value pair into the map, returning an iterator to the inserted element and a bool
    /// indicating success.
    /// \tparam Val The type of the value to insert, typically std::pair<const Key, Value> or a compatible type.
    /// \param value The key-value pair to insert into the map. The key must be unique; if a pair with the same key
    /// already exists, the insertion will fail.
    /// \return A pair consisting of an iterator to the inserted element and a bool indicating whether the insertion
    /// took place (true if inserted, false if the key already existed).
    template <typename Val>
    std::pair<Iterator, bool> InsertImpl(Val&& value)
    {
        FNode* parent = m_sentinel;
        FNode* current = m_root;
        while (current != m_sentinel)
        {
            parent = current;
            if (value.first < current->data.first) { current = current->left; }
            else if (current->data.first < value.first) { current = current->right; }
            else { return { Iterator(current, m_sentinel), false }; }
        }
        void* memory = m_nodeAllocator.Allocate(sizeof(FNode), alignof(FNode));
        FNode* n = Memory::Construct<FNode>(memory, std::forward<Val>(value));
        n->parent = parent;
        n->left = m_sentinel;
        n->right = m_sentinel;
        n->color = EColor::Red;
        if (parent == m_sentinel) { m_root = n; }
        else if (n->data.first < parent->data.first) { parent->left = n; }
        else { parent->right = n; }
        InsertFixup(n);
        ++m_size;
        if (m_sentinel->right == m_sentinel || !(n->data.first < m_sentinel->right->data.first))
        {
            m_sentinel->right = n;
        }
        return { Iterator(n, m_sentinel), true };
    }

    /// \brief Restores the red-black tree properties after an insertion.
    /// \param z The newly inserted node that may violate the red-black tree properties.
    void InsertFixup(FNode* z)
    {
        while (z->parent->color == EColor::Red)
        {
            if (z->parent == z->parent->parent->left)
            {
                FNode* y = z->parent->parent->right;
                if (y->color == EColor::Red)
                {
                    z->parent->color = EColor::Black;
                    y->color = EColor::Black;
                    z->parent->parent->color = EColor::Red;
                    z = z->parent->parent;
                }
                else
                {
                    if (z == z->parent->right)
                    {
                        z = z->parent;
                        RotateLeft(z);
                    }
                    z->parent->color = EColor::Black;
                    z->parent->parent->color = EColor::Red;
                    RotateRight(z->parent->parent);
                }
            }
            else
            {
                FNode* y = z->parent->parent->left;
                if (y->color == EColor::Red)
                {
                    z->parent->color = EColor::Black;
                    y->color = EColor::Black;
                    z->parent->parent->color = EColor::Red;
                    z = z->parent->parent;
                }
                else
                {
                    if (z == z->parent->left)
                    {
                        z = z->parent;
                        RotateRight(z);
                    }
                    z->parent->color = EColor::Black;
                    z->parent->parent->color = EColor::Red;
                    RotateLeft(z->parent->parent);
                }
            }
        }
        m_root->color = EColor::Black;
    }

    /// \brief Performs a left rotation on the subtree rooted at node x.
    /// \param x The node around which to perform the left rotation. This node must not be the sentinel and must have a
    /// right child that is not the sentinel.
    void RotateLeft(FNode* x)
    {
        FNode* y = x->right;
        x->right = y->left;
        if (y->left != m_sentinel) { y->left->parent = x; }
        y->parent = x->parent;
        if (x->parent == m_sentinel) { m_root = y; }
        else if (x == x->parent->left) { x->parent->left = y; }
        else { x->parent->right = y; }
        y->left = x;
        x->parent = y;
    }

    /// \brief Performs a right rotation on the subtree rooted at node y.
    /// \param y The node around which to perform the right rotation. This node must not be the sentinel and must have a
    /// left child that is not the sentinel.
    void RotateRight(FNode* y)
    {
        FNode* x = y->left;
        y->left = x->right;
        if (x->right != m_sentinel) { x->right->parent = y; }
        x->parent = y->parent;
        if (y->parent == m_sentinel) { m_root = x; }
        else if (y == y->parent->left) { y->parent->left = x; }
        else { y->parent->right = x; }
        x->right = y;
        y->parent = x;
    }

    /// \brief Replaces the subtree rooted at node u with the subtree rooted at node v. This is a helper function used
    /// during node deletion to maintain the structure of the red-black tree.
    /// \param u The node to be replaced. This node must not be the sentinel.
    /// \param v The node to replace u with. This node may be the sentinel if u has no children.
    void Transplant(FNode* u, FNode* v)
    {
        if (u->parent == m_sentinel) { m_root = v; }
        else if (u == u->parent->left) { u->parent->left = v; }
        else { u->parent->right = v; }
        v->parent = u->parent;
    }

    /// \brief Finds the node with the minimum key in the subtree rooted at node n. This is a helper function used
    /// during node deletion to find the successor of a node that has two children.
    /// \param n The root of the subtree to search for the minimum key. This node must not be the sentinel.
    /// \return A pointer to the node with the minimum key in the subtree rooted at n.
    FNode* Minimum(FNode* n) const
    {
        while (n->left != m_sentinel) { n = n->left; }
        return n;
    }

    /// \brief Deletes the node z from the red-black tree and restores the tree properties. This is a helper function
    /// used by the public Erase function to remove a node from the tree while maintaining the red-black tree
    /// properties.
    /// \param z The node to delete from the tree. This node must not be the sentinel.
    void EraseNode(FNode* z)
    {
        FNode* y = z;
        FNode* x;
        EColor yOrig = y->color;
        if (z->left == m_sentinel)
        {
            x = z->right;
            Transplant(z, z->right);
        }
        else if (z->right == m_sentinel)
        {
            x = z->left;
            Transplant(z, z->left);
        }
        else
        {
            y = Minimum(z->right);
            yOrig = y->color;
            x = y->right;
            if (y->parent == z) { x->parent = y; }
            else
            {
                Transplant(y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }
            Transplant(z, y);
            y->left = z->left;
            y->left->parent = y;
            y->color = z->color;
        }
        if (yOrig == EColor::Black) { EraseFixup(x); }
        if (m_sentinel->right == z)
        {
            if (m_root != m_sentinel)
            {
                FNode* maxNode = m_root;
                while (maxNode->right != m_sentinel) { maxNode = maxNode->right; }
                m_sentinel->right = maxNode;
            }
            else { m_sentinel->right = m_sentinel; }
        }
        Memory::Destroy(z);
        m_nodeAllocator.Deallocate(z);
        --m_size;
    }

    /// \brief Restores the red-black tree properties after a deletion.
    /// \param x The node that replaces the deleted node and may violate the red-black tree properties.
    void EraseFixup(FNode* x)
    {
        while (x != m_root && x->color == EColor::Black)
        {
            if (x == x->parent->left)
            {
                FNode* w = x->parent->right;
                if (w->color == EColor::Red)
                {
                    w->color = EColor::Black;
                    x->parent->color = EColor::Red;
                    RotateLeft(x->parent);
                    w = x->parent->right;
                }
                if (w->left->color == EColor::Black && w->right->color == EColor::Black)
                {
                    w->color = EColor::Red;
                    x = x->parent;
                }
                else
                {
                    if (w->right->color == EColor::Black)
                    {
                        w->left->color = EColor::Black;
                        w->color = EColor::Red;
                        RotateRight(w);
                        w = x->parent->right;
                    }
                    w->color = x->parent->color;
                    x->parent->color = EColor::Black;
                    w->right->color = EColor::Black;
                    RotateLeft(x->parent);
                    x = m_root;
                }
            }
            else
            {
                FNode* w = x->parent->left;
                if (w->color == EColor::Red)
                {
                    w->color = EColor::Black;
                    x->parent->color = EColor::Red;
                    RotateRight(x->parent);
                    w = x->parent->left;
                }
                if (w->right->color == EColor::Black && w->left->color == EColor::Black)
                {
                    w->color = EColor::Red;
                    x = x->parent;
                }
                else
                {
                    if (w->left->color == EColor::Black)
                    {
                        w->right->color = EColor::Black;
                        w->color = EColor::Red;
                        RotateLeft(w);
                        w = x->parent->left;
                    }
                    w->color = x->parent->color;
                    x->parent->color = EColor::Black;
                    w->left->color = EColor::Black;
                    RotateRight(x->parent);
                    x = m_root;
                }
            }
        }
        x->color = EColor::Black;
    }

    /// \brief Recursively destroys the subtree rooted at node n, deallocating all nodes in the subtree. This is a
    /// helper function used in the destructor to clean up all nodes in the tree.
    /// \param n The root of the subtree to destroy. This node may be the sentinel, in which case the function will
    /// return immediately without doing anything.
    void DestroyTree(FNode* n) noexcept
    {
        if (n == m_sentinel) { return; }
        DestroyTree(n->left);
        DestroyTree(n->right);
        Memory::Destroy(n);
        m_nodeAllocator.Deallocate(n);
    }
};

}   // namespace GP::Container
