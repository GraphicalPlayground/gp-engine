// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/SystemMallocForNew.hpp"
#include "memory/Memory.hpp"

namespace gp::memory
{

void* UseSystemMallocForNew::operator new(std::size_t size) GP_OPERATOR_NEW_THROW_SPEC
{
    return systemAllocate(size);
}

void* UseSystemMallocForNew::operator new[](std::size_t size) GP_OPERATOR_NEW_THROW_SPEC
{
    return systemAllocate(size);
}

void UseSystemMallocForNew::operator delete(void* ptr) GP_OPERATOR_DELETE_THROW_SPEC
{
    return systemDeallocate(ptr);
}

void UseSystemMallocForNew::operator delete[](void* ptr) GP_OPERATOR_DELETE_THROW_SPEC
{
    return systemDeallocate(ptr);
}

}   // namespace gp::memory
