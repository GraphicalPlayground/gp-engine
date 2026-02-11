// Copyright (c) - Mallory SCOTTON. All rights reserved.

#pragma once

namespace GP::Template
{

/// \brief A class that cannot be copied.
class NonCopyable
{
public:
    NonCopyable() = default;
    ~NonCopyable() = default;

    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

/// \brief A class that cannot be moved.
class NonMovable
{
public:
    NonMovable() = default;
    ~NonMovable() = default;

    NonMovable(NonMovable&&) = delete;
    NonMovable& operator=(NonMovable&&) = delete;
};

/// \brief A class that cannot be copied or moved.
class NonCopyableNonMovable
{
public:
    NonCopyableNonMovable() = default;
    ~NonCopyableNonMovable() = default;

    NonCopyableNonMovable(const NonCopyableNonMovable&) = delete;
    NonCopyableNonMovable& operator=(const NonCopyableNonMovable&) = delete;

    NonCopyableNonMovable(NonCopyableNonMovable&&) = delete;
    NonCopyableNonMovable& operator=(NonCopyableNonMovable&&) = delete;
};

/// \brief A class that cannot be instantiated, copied, or moved.
class NonInstantiable
{
public:
    NonInstantiable() = delete;

    NonInstantiable(const NonInstantiable&) = delete;
    NonInstantiable& operator=(const NonInstantiable&) = delete;

    NonInstantiable(NonInstantiable&&) = delete;
    NonInstantiable& operator=(NonInstantiable&&) = delete;
};

}   // namespace GP::Template
