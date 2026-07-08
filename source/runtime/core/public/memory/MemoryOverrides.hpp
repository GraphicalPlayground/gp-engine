// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/GlobalMemory.hpp"             // IWYU pragma: keep
#include "memory/MemoryBase.hpp"               // IWYU pragma: keep
#include "platforms/base/PlatformMemory.hpp"   // IWYU pragma: keep

#if !GP_FORCE_ANSI_ALLOCATOR
static_assert(
    __STDCPP_DEFAULT_NEW_ALIGNMENT__ <= 16,
    "Expecting 16-byte default operator new alignment - alignments > 16 may have bloat"
);
// clang-format off
    #define GP_OVERLOAD_OPERATOR_NEW_AND_DELETE \
        GP_FORCENOINLINE void* operator new  (             std::size_t size                                                   ) GP_OPERATOR_NEW_THROW_SPEC      { return gp::memory::getGlobalMalloc()->allocate(size ? size : 1, size <= 8 ? 8 : __STDCPP_DEFAULT_NEW_ALIGNMENT__ ); } \
        GP_FORCENOINLINE void* operator new[](             std::size_t size                                                   ) GP_OPERATOR_NEW_THROW_SPEC      { return gp::memory::getGlobalMalloc()->allocate(size ? size : 1, size <= 8 ? 8 : __STDCPP_DEFAULT_NEW_ALIGNMENT__ ); } \
        GP_FORCENOINLINE void* operator new  (             std::size_t size,                             const std::nothrow_t&) GP_OPERATOR_NEW_NOTHROW_SPEC    { return gp::memory::getGlobalMalloc()->allocate(size ? size : 1, size <= 8 ? 8 : __STDCPP_DEFAULT_NEW_ALIGNMENT__ ); } \
        GP_FORCENOINLINE void* operator new[](             std::size_t size,                             const std::nothrow_t&) GP_OPERATOR_NEW_NOTHROW_SPEC    { return gp::memory::getGlobalMalloc()->allocate(size ? size : 1, size <= 8 ? 8 : __STDCPP_DEFAULT_NEW_ALIGNMENT__ ); } \
        GP_FORCENOINLINE void* operator new  (             std::size_t size, std::align_val_t alignment                       ) GP_OPERATOR_NEW_THROW_SPEC      { return gp::memory::getGlobalMalloc()->allocate(size ? size : 1, static_cast<gp::UInt32>(alignment)); } \
        GP_FORCENOINLINE void* operator new[](             std::size_t size, std::align_val_t alignment                       ) GP_OPERATOR_NEW_THROW_SPEC      { return gp::memory::getGlobalMalloc()->allocate(size ? size : 1, static_cast<gp::UInt32>(alignment)); } \
        GP_FORCENOINLINE void* operator new  (             std::size_t size, std::align_val_t alignment, const std::nothrow_t&) GP_OPERATOR_NEW_NOTHROW_SPEC    { return gp::memory::getGlobalMalloc()->allocate(size ? size : 1, static_cast<gp::UInt32>(alignment)); } \
        GP_FORCENOINLINE void* operator new[](             std::size_t size, std::align_val_t alignment, const std::nothrow_t&) GP_OPERATOR_NEW_NOTHROW_SPEC    { return gp::memory::getGlobalMalloc()->allocate(size ? size : 1, static_cast<gp::UInt32>(alignment)); } \
        GP_FORCENOINLINE void operator delete  (void* ptr                                                                     ) GP_OPERATOR_DELETE_THROW_SPEC   {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete[](void* ptr                                                                     ) GP_OPERATOR_DELETE_THROW_SPEC   {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete  (void* ptr,                                               const std::nothrow_t&) GP_OPERATOR_DELETE_NOTHROW_SPEC {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete[](void* ptr,                                               const std::nothrow_t&) GP_OPERATOR_DELETE_NOTHROW_SPEC {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete  (void* ptr, std::size_t                                                        ) GP_OPERATOR_DELETE_THROW_SPEC   {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete[](void* ptr, std::size_t                                                        ) GP_OPERATOR_DELETE_THROW_SPEC   {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete  (void* ptr, std::size_t,                                  const std::nothrow_t&) GP_OPERATOR_DELETE_NOTHROW_SPEC {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete[](void* ptr, std::size_t,                                  const std::nothrow_t&) GP_OPERATOR_DELETE_NOTHROW_SPEC {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete  (void* ptr,                   std::align_val_t                                 ) GP_OPERATOR_DELETE_THROW_SPEC   {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete[](void* ptr,                   std::align_val_t                                 ) GP_OPERATOR_DELETE_THROW_SPEC   {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete  (void* ptr,                   std::align_val_t          , const std::nothrow_t&) GP_OPERATOR_DELETE_NOTHROW_SPEC {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete[](void* ptr,                   std::align_val_t          , const std::nothrow_t&) GP_OPERATOR_DELETE_NOTHROW_SPEC {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete  (void* ptr, std::size_t,      std::align_val_t                                 ) GP_OPERATOR_DELETE_THROW_SPEC   {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete[](void* ptr, std::size_t,      std::align_val_t                                 ) GP_OPERATOR_DELETE_THROW_SPEC   {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete  (void* ptr, std::size_t,      std::align_val_t          , const std::nothrow_t&) GP_OPERATOR_DELETE_NOTHROW_SPEC {        gp::memory::getGlobalMalloc()->deallocate(ptr); } \
        GP_FORCENOINLINE void operator delete[](void* ptr, std::size_t,      std::align_val_t          , const std::nothrow_t&) GP_OPERATOR_DELETE_NOTHROW_SPEC {        gp::memory::getGlobalMalloc()->deallocate(ptr); }

// clang-format on
#else
    #define GP_OVERLOAD_OPERATOR_NEW_AND_DELETE
#endif
