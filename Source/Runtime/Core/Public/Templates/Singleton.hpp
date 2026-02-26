// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Templates/Generic.hpp"
#include <atomic>
#include <mutex>
#include <type_traits>

namespace GP::Template
{

/// \brief Defines the policy for creating and managing the lifetime of a singleton instance.
enum class ESingletonCreationPolicy : UInt8
{
    /// \brief The instance is constructed on first access via GetInstance().
    ///        Thread safety of the initial construction is guaranteed by the C++11 magic-static
    ///        rule ([stmt.dcl]/4) regardless of the ThreadSafe template parameter.
    Lazy,

    /// \brief The instance is constructed explicitly via Init(), typically during static
    ///        initialization (before main). Supports thread-safe DCLP when ThreadSafe = true.
    Eager,

    /// \brief The lifetime is fully user-controlled via Create(...) and Destroy().
    ///        Suitable for engine subsystems that require strict initialization ordering.
    Manual
};

/// \brief Policy-based singleton template.
///
/// Specialize via the ESingletonCreationPolicy to select the desired lifetime strategy.
/// The ThreadSafe parameter adds mutex-based protection to creation and destruction paths
/// for scenarios where concurrent access cannot be avoided.
///
/// \tparam T The type managed as a singleton. Must be a class type.
/// \tparam CreationPolicy Lifetime management strategy. Defaults to Lazy.
/// \tparam ThreadSafe When true, concurrent access to lifecycle operations is safe.
template <typename T, ESingletonCreationPolicy CreationPolicy = ESingletonCreationPolicy::Lazy, bool ThreadSafe = false>
class TSingleton;

/// \brief Lazy singleton — instance is created on first call to GetInstance().
///
/// Relies on the C++11 guarantee that function-local static initialization is performed
/// exactly once, even under concurrent access (magic-static). The ThreadSafe parameter is
/// accepted for API symmetry but does not alter behaviour: the magic-static guarantee already
/// provides the required thread safety.
///
/// DLL note: Because the instance lives in a function-local static, each shared library that
/// independently instantiates this template will own a separate instance. To share a singleton
/// across module boundaries, expose a single accessor from one translation unit compiled into
/// the shared library.
///
/// \tparam T The singleton type. Must be default-constructible.
/// \tparam ThreadSafe Accepted for symmetry; magic-static is always thread-safe in C++11.
template <typename T, bool ThreadSafe>
class TSingleton<T, ESingletonCreationPolicy::Lazy, ThreadSafe> : private NonCopyableNonMovable
{
    GP_STATIC_ASSERT(std::is_default_constructible_v<T>, "TSingleton<Lazy>: T must be default-constructible.");

public:
    using InstanceType = T;

private:
    TSingleton() = delete;
    ~TSingleton() = delete;

public:
    /// \brief Returns a reference to the singleton instance, constructing it on first call.
    /// \return Reference to the unique instance of T.
    /// \note  Construction is guaranteed at most once across all threads (C++11 magic-static).
    GP_NODISCARD static T& GetInstance() noexcept(std::is_nothrow_default_constructible_v<T>)
    {
        static T s_instance;
        return s_instance;
    }

    /// \brief Returns a pointer to the singleton instance.
    /// \return Non-null pointer to the unique instance of T (triggers construction if needed).
    GP_NODISCARD static T* GetInstancePtr() noexcept(std::is_nothrow_default_constructible_v<T>)
    {
        return &GetInstance();
    }

    /// \brief Returns true once GetInstance() has been called at least once.
    /// \note  Implemented via a second magic-static; the result is reliable after construction.
    GP_NODISCARD static bool IsInitialized() noexcept
    {
        // Piggybacks on the magic-static to set the flag as a side-effect of construction.
        static bool s_initialized = (static_cast<void>(GetInstance()), true);
        return s_initialized;
    }
};

/// \brief Eager singleton — instance is explicitly initialized via Init().
///
/// Intended to be called during static initialization so the instance exists before any
/// dynamic code runs. Calling Init() multiple times is harmless; construction occurs only
/// once. When ThreadSafe = true, a mutex and double-checked locking (DCLP) protect
/// concurrent Init() calls.
///
/// Typical usage:
/// \code
///     // In a .cpp file, at namespace scope:
///     static auto& g_system = TEagerSingleton<MySystem>::Init();
/// \endcode
///
/// \tparam T The singleton type. Must be default-constructible.
/// \tparam ThreadSafe Enable mutex-guarded DCLP for concurrent Init() callers.
template <typename T, bool ThreadSafe>
class TSingleton<T, ESingletonCreationPolicy::Eager, ThreadSafe> : private NonCopyableNonMovable
{
    GP_STATIC_ASSERT(std::is_default_constructible_v<T>, "TSingleton<Eager>: T must be default-constructible.");

public:
    using InstanceType = T;

private:
    using AlignedStorage = std::aligned_storage_t<sizeof(T), alignof(T)>;

private:
    static inline AlignedStorage s_storage{};
    static inline std::conditional_t<ThreadSafe, std::atomic<T*>, T*> s_instance{ nullptr };
    static inline std::conditional_t<ThreadSafe, std::mutex, char> s_mutex{};

private:
    TSingleton() = delete;
    ~TSingleton() = delete;

public:
    /// \brief Eagerly constructs and returns the singleton instance.
    /// Safe to call multiple times; construction occurs only once. When ThreadSafe is true,
    /// concurrent callers are serialized via a mutex and DCLP.
    /// \return Reference to the singleton instance.
    GP_NODISCARD static T& Init()
    {
        if constexpr (ThreadSafe) { return InitThreadSafe(); }
        else { return InitUnsafe(); }
    }

    /// \brief Returns a reference to the singleton instance (calls Init() internally).
    /// \return Reference to the singleton instance.
    GP_NODISCARD static T& GetInstance() { return Init(); }

    /// \brief Returns a pointer to the singleton instance (calls Init() internally).
    /// \return Non-null pointer to the singleton instance.
    GP_NODISCARD static T* GetInstancePtr() { return &Init(); }

    /// \brief Returns true if the singleton has been initialized.
    /// \return True if the instance is live; false otherwise.
    GP_NODISCARD static bool IsInitialized() noexcept
    {
        if constexpr (ThreadSafe) { return s_instance.load(std::memory_order_acquire) != nullptr; }
        else { return s_instance != nullptr; }
    }

private:
    /// \brief Non-thread-safe initialization path.
    /// \return Reference to the singleton instance.
    GP_NODISCARD static T& InitUnsafe() noexcept(std::is_nothrow_default_constructible_v<T>)
    {
        if (!s_instance) { s_instance = ::new (&s_storage) T(); }
        return *s_instance;
    }

    /// \brief Thread-safe initialization using double-checked locking (DCLP).
    /// \return Reference to the singleton instance.
    GP_NODISCARD static T& InitThreadSafe()
    {
        // Acquire load: if a non-null value is visible here, the full construction is visible.
        T* ptr = s_instance.load(std::memory_order_acquire);
        if (!ptr)
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            ptr = s_instance.load(std::memory_order_relaxed);
            if (!ptr)
            {
                ptr = ::new (&s_storage) T();
                // Release store: makes the completed construction visible to acquiring loads.
                s_instance.store(ptr, std::memory_order_release);
            }
        }
        return *ptr;
    }
};

/// \brief Manual singleton — lifetime is fully controlled by the user.
///
/// No implicit construction occurs. The user must call Create(...) before accessing the
/// instance and Destroy() when the instance should be torn down. Violating this ordering
/// asserts in debug builds and produces undefined behaviour in release builds.
///
/// This policy is the correct choice for engine subsystems with strict initialization order
/// requirements (e.g., memory manager, RHI device, task scheduler).
///
/// Usage:
/// \code
///     // Startup (ordered in e.g. Engine::Init)
///     TManualSingleton<TaskSystem>::Create(threadCount);
///
///     // Access from anywhere
///     TaskSystem& tasks = TManualSingleton<TaskSystem>::GetInstance();
///
///     // Shutdown (ordered in e.g. Engine::Shutdown)
///     TManualSingleton<TaskSystem>::Destroy();
/// \endcode
///
/// \tparam T The singleton type. Must be destructible.
/// \tparam ThreadSafe Enable mutex-guarded Create, Destroy, and GetInstance.
template <typename T, bool ThreadSafe>
class TSingleton<T, ESingletonCreationPolicy::Manual, ThreadSafe> : private NonCopyableNonMovable
{
    GP_STATIC_ASSERT(std::is_destructible_v<T>, "TSingleton<Manual>: T must be destructible.");

public:
    using InstanceType = T;

private:
    using AlignedStorage = std::aligned_storage_t<sizeof(T), alignof(T)>;

private:
    static inline AlignedStorage s_storage{};
    static inline T* s_instance{ nullptr };
    static inline std::conditional_t<ThreadSafe, std::mutex, char> s_mutex{};

private:
    TSingleton() = delete;
    ~TSingleton() = delete;

public:
    /// \brief Constructs the singleton instance in-place with the provided arguments.
    /// \tparam Args  Constructor argument types for T.
    /// \param  args  Arguments forwarded to T's constructor.
    /// \return Reference to the newly created instance.
    /// \warning Calling Create() when an instance already exists is a programming error and asserts in debug builds.
    template <typename... Args>
    requires std::constructible_from<T, Args...> static T& Create(Args&&... args)
    {
        if constexpr (ThreadSafe)
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            GP_ASSERT(s_instance == nullptr, "TSingleton::Create() called while an instance already exists.");
            s_instance = ::new (&s_storage) T(std::forward<Args>(args)...);
        }
        else
        {
            GP_ASSERT(s_instance == nullptr, "TSingleton::Create() called while an instance already exists.");
            s_instance = ::new (&s_storage) T(std::forward<Args>(args)...);
        }
        return *s_instance;
    }

    /// \brief Destroys the singleton instance, invoking T's destructor.
    /// After this call IsInitialized() returns false. Create() may be called again to
    /// recreate the instance.
    /// \warning Calling Destroy() when no instance exists asserts in debug builds.
    static void Destroy()
    {
        if constexpr (ThreadSafe)
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            GP_ASSERT(s_instance != nullptr, "TSingleton::Destroy() called on a null instance.");
            s_instance->~T();
            s_instance = nullptr;
        }
        else
        {
            GP_ASSERT(s_instance != nullptr, "TSingleton::Destroy() called on a null instance.");
            s_instance->~T();
            s_instance = nullptr;
        }
    }

    /// \brief Returns a reference to the live singleton instance.
    /// \return Reference to the instance created by Create().
    /// \warning Must not be called before Create() or after Destroy(). Asserts in debug.
    GP_NODISCARD static T& GetInstance() noexcept
    {
        if constexpr (ThreadSafe)
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            GP_ASSERT(s_instance != nullptr, "TSingleton::GetInstance() called before Create().");
            return *s_instance;
        }
        else
        {
            GP_ASSERT(s_instance != nullptr, "TSingleton::GetInstance() called before Create().");
            return *s_instance;
        }
    }

    /// \brief Returns a pointer to the instance, or nullptr if not currently alive.
    /// \return Pointer to the live instance or nullptr.
    GP_NODISCARD static T* GetInstancePtr() noexcept { return s_instance; }

    /// \brief Returns true if an instance is currently alive (between Create and Destroy).
    /// \return True if the instance is live; false otherwise.
    GP_NODISCARD static bool IsInitialized() noexcept { return s_instance != nullptr; }
};

/// \brief Alias for a lazy-initialized singleton.
/// \tparam T The singleton type.
/// \tparam ThreadSafe Thread safety flag (magic-static is always safe regardless).
template <typename T, bool ThreadSafe = false>
using TLazySingleton = TSingleton<T, ESingletonCreationPolicy::Lazy, ThreadSafe>;

/// \brief Alias for an eagerly-initialized singleton.
/// \tparam T The singleton type.
/// \tparam ThreadSafe Enable DCLP-guarded Init() for concurrent callers.
template <typename T, bool ThreadSafe = false>
using TEagerSingleton = TSingleton<T, ESingletonCreationPolicy::Eager, ThreadSafe>;

/// \brief Alias for a manually-managed singleton.
/// \tparam T The singleton type.
/// \tparam ThreadSafe Enable mutex-guarded Create / Destroy / GetInstance.
template <typename T, bool ThreadSafe = false>
using TManualSingleton = TSingleton<T, ESingletonCreationPolicy::Manual, ThreadSafe>;

}   // namespace GP::Template
