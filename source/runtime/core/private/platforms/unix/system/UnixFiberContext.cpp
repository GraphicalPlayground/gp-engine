// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/unix/system/UnixFiberContext.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include <sys/mman.h>
#include <unistd.h>

namespace gp::platform::unix
{

namespace detail
{

struct PendingFiberStart
{
    FiberContextEntryFunction entry{ nullptr };
    void* userData{ nullptr };
};

static thread_local PendingFiberStart tl_pendingStart{};
static thread_local gp::UInt64 tl_activeContextId{ 0ULL };
static thread_local gp::UInt64 tl_threadAnchorId{ 0ULL };

static GP_FORCEINLINE void compilerMemoryBarrier() noexcept
{
    __asm__ volatile("" ::: "memory");
}

static GP_FORCENOINLINE void
    setPendingFiberStartTLS(const FiberContextEntryFunction entry, void* const userData) noexcept
{
    compilerMemoryBarrier();
    tl_pendingStart.entry = entry;
    tl_pendingStart.userData = userData;
    compilerMemoryBarrier();
}

[[nodiscard]] static GP_FORCENOINLINE PendingFiberStart consumePendingFiberStartTLS() noexcept
{
    compilerMemoryBarrier();
    const PendingFiberStart startInfo = tl_pendingStart;
    tl_pendingStart.entry = nullptr;
    tl_pendingStart.userData = nullptr;
    compilerMemoryBarrier();
    return startInfo;
}

[[nodiscard]] static GP_FORCENOINLINE gp::UInt64 getActiveContextIdTLS() noexcept
{
    compilerMemoryBarrier();
    return tl_activeContextId;
}

static GP_FORCENOINLINE void setActiveContextIdTLS(const gp::UInt64 contextId) noexcept
{
    compilerMemoryBarrier();
    tl_activeContextId = contextId;
    compilerMemoryBarrier();
}

[[nodiscard]] static GP_FORCENOINLINE gp::UInt64 getThreadAnchorIdTLS() noexcept
{
    compilerMemoryBarrier();
    return tl_threadAnchorId;
}

static GP_FORCENOINLINE void setThreadAnchorIdTLS(const gp::UInt64 anchorId) noexcept
{
    compilerMemoryBarrier();
    tl_threadAnchorId = anchorId;
    compilerMemoryBarrier();
}

[[nodiscard]] static gp::UInt64 allocateNextContextId() noexcept
{
    static gp::thread::Atomic<gp::UInt64> s_nextId{ 1ULL };
    return s_nextId.fetchAdd(1ULL, gp::MemoryOrder::Relaxed);
}

[[nodiscard]] static gp::USize querySystemPageSize() noexcept
{
    const long pageSize = ::sysconf(_SC_PAGESIZE);
    return (pageSize > 0) ? static_cast<gp::USize>(pageSize) : 4096u;
}

static void unixFiberTrampoline() noexcept
{
    const PendingFiberStart startInfo = consumePendingFiberStartTLS();
    const FiberContextEntryFunction entry = startInfo.entry;
    void* const userData = startInfo.userData;

    GP_ASSERT(entry != nullptr, "POSIX FiberContext trampoline entered with a null entry function.");
    if (entry != nullptr) [[likely]]
    {
        entry(userData);
    }

    GP_ASSERT(
        false,
        "gp::platform::unix::FiberContext entry function returned to the bottom of the fiber stack without switching " "back to a caller or thread-anchor context."
    );
    for (;;)
    {
    }
}

#if defined(__x86_64__) || defined(_M_X64)

__attribute__((naked, noinline)) static void
    gpSwitchFiberContext([[maybe_unused]] void** fromSp, [[maybe_unused]] void* toSp) noexcept
{
    __asm__ volatile(
        "pushq %rbp\n\t"
        "pushq %rbx\n\t"
        "pushq %r12\n\t"
        "pushq %r13\n\t"
        "pushq %r14\n\t"
        "pushq %r15\n\t"
        "subq $8, %rsp\n\t"
        "stmxcsr (%rsp)\n\t"
        "fnstcw 4(%rsp)\n\t"
        "movq %rsp, (%rdi)\n\t"
        "movq %rsi, %rsp\n\t"
        "ldmxcsr (%rsp)\n\t"
        "fldcw 4(%rsp)\n\t"
        "addq $8, %rsp\n\t"
        "popq %r15\n\t"
        "popq %r14\n\t"
        "popq %r13\n\t"
        "popq %r12\n\t"
        "popq %rbx\n\t"
        "popq %rbp\n\t"
        "retq\n\t"
    );
}

[[nodiscard]] static void* initializeArchStackFrame(gp::UInt8* const stackTop) noexcept
{
    // Align top of stack down to 16 bytes
    const gp::UIntPtr alignedTopAddr =
        reinterpret_cast<gp::UIntPtr>(stackTop) & ~static_cast<gp::UIntPtr>(kFiberStackAlignment - 1u);
    gp::UInt8* const frameTop = reinterpret_cast<gp::UInt8*>(alignedTopAddr);

    // Layout from high address to low address (72 bytes total):
    // [frameTop - 8]  : 8-byte alignment slot so %rsp is (16n - 8) upon entry into unixFiberTrampoline
    // [frameTop - 16] : RIP (&unixFiberTrampoline)
    // [frameTop - 24] : RBP (0)
    // [frameTop - 32] : RBX (0)
    // [frameTop - 40] : R12 (0)
    // [frameTop - 48] : R13 (0)
    // [frameTop - 56] : R14 (0)
    // [frameTop - 64] : R15 (0)
    // [frameTop - 72] : MXCSR (4 bytes = 0x1F80) + FPUCW (2 bytes = 0x037F) + 2 bytes pad
    gp::UInt8* const initialSp = frameTop - 72u;
    gp::memory::zeroMemory(initialSp, 72u);

    const gp::UIntPtr trampolineAddr = reinterpret_cast<gp::UIntPtr>(&unixFiberTrampoline);
    gp::memory::copyMemory(frameTop - 16u, &trampolineAddr, sizeof(gp::UIntPtr));

    gp::UInt32 defaultMxcsr = 0x1F80u;
    gp::UInt16 defaultFpucw = 0x037Fu;
    __asm__ volatile("stmxcsr %0" : "=m"(defaultMxcsr));
    __asm__ volatile("fnstcw %0" : "=m"(defaultFpucw));
    gp::memory::copyMemory(initialSp, &defaultMxcsr, sizeof(gp::UInt32));
    gp::memory::copyMemory(initialSp + 4u, &defaultFpucw, sizeof(gp::UInt16));

    return initialSp;
}

#elif defined(__aarch64__) || defined(_M_ARM64)

__attribute__((naked, noinline)) static void
    gpSwitchFiberContext([[maybe_unused]] void** fromSp, [[maybe_unused]] void* toSp) noexcept
{
    __asm__ volatile(
        "sub sp, sp, #176\n\t"
        "stp x19, x20, [sp, #0]\n\t"
        "stp x21, x22, [sp, #16]\n\t"
        "stp x23, x24, [sp, #32]\n\t"
        "stp x25, x26, [sp, #48]\n\t"
        "stp x27, x28, [sp, #64]\n\t"
        "stp x29, x30, [sp, #80]\n\t"
        "stp d8,  d9,  [sp, #96]\n\t"
        "stp d10, d11, [sp, #112]\n\t"
        "stp d12, d13, [sp, #128]\n\t"
        "stp d14, d15, [sp, #144]\n\t"
        "mrs x10, fpcr\n\t"
        "mrs x11, fpsr\n\t"
        "stp x10, x11, [sp, #160]\n\t"
        "mov x9, sp\n\t"
        "str x9, [x0]\n\t"
        "mov sp, x1\n\t"
        "ldp x10, x11, [sp, #160]\n\t"
        "msr fpcr, x10\n\t"
        "msr fpsr, x11\n\t"
        "ldp x19, x20, [sp, #0]\n\t"
        "ldp x21, x22, [sp, #16]\n\t"
        "ldp x23, x24, [sp, #32]\n\t"
        "ldp x25, x26, [sp, #48]\n\t"
        "ldp x27, x28, [sp, #64]\n\t"
        "ldp x29, x30, [sp, #80]\n\t"
        "ldp d8,  d9,  [sp, #96]\n\t"
        "ldp d10, d11, [sp, #112]\n\t"
        "ldp d12, d13, [sp, #128]\n\t"
        "ldp d14, d15, [sp, #144]\n\t"
        "add sp, sp, #176\n\t"
        "ret\n\t"
    );
}

[[nodiscard]] static void* initializeArchStackFrame(gp::UInt8* const stackTop) noexcept
{
    const gp::UIntPtr alignedTopAddr =
        reinterpret_cast<gp::UIntPtr>(stackTop) & ~static_cast<gp::UIntPtr>(kFiberStackAlignment - 1u);
    gp::UInt8* const initialSp = reinterpret_cast<gp::UInt8*>(alignedTopAddr) - 176u;
    gp::memory::zeroMemory(initialSp, 176u);

    // Store &unixFiberTrampoline in x30 (link register slot at offset 88)
    const gp::UIntPtr trampolineAddr = reinterpret_cast<gp::UIntPtr>(&unixFiberTrampoline);
    gp::memory::copyMemory(initialSp + 88u, &trampolineAddr, sizeof(gp::UIntPtr));

    // Capture current FPCR and FPSR control/status registers into offsets 160 and 168
    gp::UInt64 defaultFpcr = 0ULL;
    gp::UInt64 defaultFpsr = 0ULL;
    __asm__ volatile("mrs %0, fpcr" : "=r"(defaultFpcr));
    __asm__ volatile("mrs %0, fpsr" : "=r"(defaultFpsr));
    gp::memory::copyMemory(initialSp + 160u, &defaultFpcr, sizeof(gp::UInt64));
    gp::memory::copyMemory(initialSp + 168u, &defaultFpsr, sizeof(gp::UInt64));

    return initialSp;
}

#else
    #define GP_UNIX_FIBER_UNSUPPORTED_ARCH 1
#endif

}   // namespace detail

FiberContext::~FiberContext() noexcept
{
    if (isValid())
    {
        (void)destroy();
    }
}

FiberContext::FiberContext(FiberContext&& other) noexcept
    : m_stackPointer(
          (other.m_isThreadAnchor && other.m_stackPointer == &other.m_stackPointer) ? &m_stackPointer
                                                                                    : other.m_stackPointer
      )
    , m_allocationBase(other.m_allocationBase)
    , m_allocationSize(other.m_allocationSize)
    , m_usableStackSize(other.m_usableStackSize)
    , m_entry(other.m_entry)
    , m_userData(other.m_userData)
    , m_contextId(other.m_contextId)
    , m_isThreadAnchor(other.m_isThreadAnchor)
{
    other.m_stackPointer = nullptr;
    other.m_allocationBase = nullptr;
    other.m_allocationSize = 0u;
    other.m_usableStackSize = 0u;
    other.m_entry = nullptr;
    other.m_userData = nullptr;
    other.m_contextId = 0ULL;
    other.m_isThreadAnchor = false;
}

FiberContext& FiberContext::operator=(FiberContext&& other) noexcept
{
    if (this != &other)
    {
        if (isValid())
        {
            (void)destroy();
        }

        m_stackPointer = (other.m_isThreadAnchor && other.m_stackPointer == &other.m_stackPointer)
                             ? &m_stackPointer
                             : other.m_stackPointer;
        m_allocationBase = other.m_allocationBase;
        m_allocationSize = other.m_allocationSize;
        m_usableStackSize = other.m_usableStackSize;
        m_entry = other.m_entry;
        m_userData = other.m_userData;
        m_contextId = other.m_contextId;
        m_isThreadAnchor = other.m_isThreadAnchor;

        other.m_stackPointer = nullptr;
        other.m_allocationBase = nullptr;
        other.m_allocationSize = 0u;
        other.m_usableStackSize = 0u;
        other.m_entry = nullptr;
        other.m_userData = nullptr;
        other.m_contextId = 0ULL;
        other.m_isThreadAnchor = false;
    }

    return *this;
}

FiberContextResult FiberContext::create(const FiberContextDesc& desc) noexcept
{
#if defined(GP_UNIX_FIBER_UNSUPPORTED_ARCH)
    (void)desc;
    return FiberContextResult::UnsupportedPlatform;
#else
    if (isValid())
    {
        return FiberContextResult::AlreadyInitialized;
    }

    if (desc.entry == nullptr || desc.stackReserveSize > kMaxFiberStackSize)
    {
        return FiberContextResult::InvalidArguments;
    }

    const gp::USize requestedReserve = (desc.stackReserveSize == 0u) ? kDefaultFiberStackSize : desc.stackReserveSize;
    if (desc.stackCommitSize > requestedReserve)
    {
        return FiberContextResult::InvalidArguments;
    }

    const gp::USize pageSize = detail::querySystemPageSize();
    const gp::USize clampedReserve = (requestedReserve < kMinFiberStackSize) ? kMinFiberStackSize : requestedReserve;
    const gp::USize alignedStackSize = ((clampedReserve + pageSize - 1u) / pageSize) * pageSize;

    const bool useGuardPage = (desc.flags & FiberStackFlags::EnableGuardPage) != FiberStackFlags::None;
    const gp::USize guardBytes = useGuardPage ? pageSize : 0u;
    const gp::USize totalMappedBytes = alignedStackSize + guardBytes;

    void* const mappedBase =
        ::mmap(nullptr, totalMappedBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mappedBase == MAP_FAILED || mappedBase == nullptr)
    {
        return FiberContextResult::AllocationFailed;
    }

    if (useGuardPage)
    {
        if (::mprotect(mappedBase, pageSize, PROT_NONE) != 0)
        {
            (void)::munmap(mappedBase, totalMappedBytes);
            return FiberContextResult::GuardPageFailed;
        }
    }

    gp::UInt8* const stackTop = static_cast<gp::UInt8*>(mappedBase) + totalMappedBytes;
    m_stackPointer = detail::initializeArchStackFrame(stackTop);
    m_allocationBase = mappedBase;
    m_allocationSize = totalMappedBytes;
    m_usableStackSize = alignedStackSize;
    m_entry = desc.entry;
    m_userData = desc.userData;
    m_contextId = detail::allocateNextContextId();
    m_isThreadAnchor = false;

    return FiberContextResult::Success;
#endif
}

FiberContextResult FiberContext::destroy() noexcept
{
    if (!isValid())
    {
        return FiberContextResult::NotInitialized;
    }

    if (m_isThreadAnchor)
    {
        return restoreCurrentThread();
    }

    if (isCurrentlyExecuting())
    {
        return FiberContextResult::CannotDestroyActiveContext;
    }

    if (m_allocationBase != nullptr && m_allocationSize > 0u)
    {
        (void)::munmap(m_allocationBase, m_allocationSize);
    }

    m_stackPointer = nullptr;
    m_allocationBase = nullptr;
    m_allocationSize = 0u;
    m_usableStackSize = 0u;
    m_entry = nullptr;
    m_userData = nullptr;
    m_contextId = 0ULL;
    m_isThreadAnchor = false;

    return FiberContextResult::Success;
}

FiberContextResult FiberContext::convertCurrentThread([[maybe_unused]] const FiberStackFlags flags) noexcept
{
#if defined(GP_UNIX_FIBER_UNSUPPORTED_ARCH)
    return FiberContextResult::UnsupportedPlatform;
#else
    if (isValid())
    {
        return FiberContextResult::AlreadyInitialized;
    }

    if (detail::getThreadAnchorIdTLS() != 0ULL)
    {
        return FiberContextResult::AlreadyThreadFiber;
    }

    const gp::UInt64 newId = detail::allocateNextContextId();
    m_stackPointer = &m_stackPointer;
    m_allocationBase = nullptr;
    m_allocationSize = 0u;
    m_usableStackSize = 0u;
    m_entry = nullptr;
    m_userData = nullptr;
    m_contextId = newId;
    m_isThreadAnchor = true;

    detail::setThreadAnchorIdTLS(newId);
    detail::setActiveContextIdTLS(newId);

    return FiberContextResult::Success;
#endif
}

FiberContextResult FiberContext::restoreCurrentThread() noexcept
{
    if (!isValid())
    {
        return FiberContextResult::NotInitialized;
    }

    if (!m_isThreadAnchor)
    {
        return FiberContextResult::NotAThreadFiber;
    }

    if (!isCurrentlyExecuting())
    {
        return FiberContextResult::CannotDestroyActiveContext;
    }

    if (detail::getThreadAnchorIdTLS() == m_contextId)
    {
        detail::setThreadAnchorIdTLS(0ULL);
    }

    if (detail::getActiveContextIdTLS() == m_contextId)
    {
        detail::setActiveContextIdTLS(0ULL);
    }

    m_stackPointer = nullptr;
    m_allocationBase = nullptr;
    m_allocationSize = 0u;
    m_usableStackSize = 0u;
    m_entry = nullptr;
    m_userData = nullptr;
    m_contextId = 0ULL;
    m_isThreadAnchor = false;

    return FiberContextResult::Success;
}

FiberContextResult FiberContext::switchTo(FiberContext& target) noexcept
{
#if defined(GP_UNIX_FIBER_UNSUPPORTED_ARCH)
    (void)target;
    return FiberContextResult::UnsupportedPlatform;
#else
    if (!isValid() || !target.isValid())
    {
        return FiberContextResult::NotInitialized;
    }

    if (this == &target || m_contextId == target.m_contextId)
    {
        return FiberContextResult::SelfSwitchError;
    }

    if (!isCurrentlyExecuting())
    {
        return FiberContextResult::SwitchFailed;
    }

    const gp::UInt64 callerContextId = m_contextId;
    if (!target.m_isThreadAnchor)
    {
        detail::setPendingFiberStartTLS(target.m_entry, target.m_userData);
    }
    detail::setActiveContextIdTLS(target.m_contextId);

    detail::gpSwitchFiberContext(&m_stackPointer, target.m_stackPointer);

    // Ensure the active context ID is restored on the resuming OS thread.
    detail::setActiveContextIdTLS(callerContextId);
    return FiberContextResult::Success;
#endif
}

bool FiberContext::isCurrentlyExecuting() const noexcept
{
    return isValid() && (detail::getActiveContextIdTLS() == m_contextId);
}

bool FiberContext::isCurrentThreadConverted() noexcept
{
    return detail::getThreadAnchorIdTLS() != 0ULL;
}

}   // namespace gp::platform::unix
