// Copyright (c) - Graphical Playground. All rights reserved.

#include "Templates/Events/DelegateHandle.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

TEST_CASE("FDelegateHandle - default-constructed handle is invalid", "[Templates][DelegateHandle]")
{
    GP::FDelegateHandle handle;
    REQUIRE_FALSE(handle.IsValid());
}

TEST_CASE("FDelegateHandle - Generate() produces valid handles", "[Templates][DelegateHandle]")
{
    GP::FDelegateHandle h = GP::FDelegateHandle::Generate();
    REQUIRE(h.IsValid());
}

TEST_CASE("FDelegateHandle - Generate() produces unique handles", "[Templates][DelegateHandle]")
{
    static constexpr GP::Int32 k_count = 1000;
    std::vector<GP::FDelegateHandle> handles;
    handles.reserve(k_count);

    for (GP::Int32 i = 0; i < k_count; ++i) { handles.push_back(GP::FDelegateHandle::Generate()); }

    // All handles must be distinct.
    for (GP::Int32 i = 0; i < k_count; ++i)
    {
        for (GP::Int32 j = i + 1; j < k_count; ++j) { REQUIRE(handles[i] != handles[j]); }
    }
}

TEST_CASE("FDelegateHandle - Invalidate() marks handle invalid", "[Templates][DelegateHandle]")
{
    GP::FDelegateHandle h = GP::FDelegateHandle::Generate();
    REQUIRE(h.IsValid());
    h.Invalidate();
    REQUIRE_FALSE(h.IsValid());
}

TEST_CASE("FDelegateHandle - equality operators", "[Templates][DelegateHandle]")
{
    GP::FDelegateHandle h1 = GP::FDelegateHandle::Generate();
    GP::FDelegateHandle h2 = h1;   // copy
    GP::FDelegateHandle h3 = GP::FDelegateHandle::Generate();

    REQUIRE(h1 == h2);
    REQUIRE(h1 != h3);
}
