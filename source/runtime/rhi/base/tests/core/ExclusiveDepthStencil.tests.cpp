// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/ExclusiveDepthStencil.hpp"
#include "containers/arrays/Vector.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashOf.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::resource::tests
{

static_assert(!ExclusiveDepthStencil::kDepthNopStencilNop.isUsingDepthStencil(), "Nop/Nop must not use depth-stencil.");
static_assert(ExclusiveDepthStencil::kDepthReadStencilNop.isUsingDepth(), "DepthRead must report isUsingDepth.");
static_assert(
    !ExclusiveDepthStencil::kDepthReadStencilNop.isUsingStencil(), "StencilNop must not report isUsingStencil."
);
static_assert(ExclusiveDepthStencil::kDepthWriteStencilRead.isDepthWrite(), "DepthWrite must report isDepthWrite.");
static_assert(ExclusiveDepthStencil::kDepthWriteStencilRead.isStencilRead(), "StencilRead must report isStencilRead.");
static_assert(ExclusiveDepthStencil::kDepthWriteStencilRead.isAnyWrite(), "DepthWrite must report isAnyWrite.");
static_assert(!ExclusiveDepthStencil::kDepthReadStencilRead.isAnyWrite(), "Read/Read must not report isAnyWrite.");

static_assert(ExclusiveDepthStencil::kDepthWriteStencilWrite.getIndex() == 0u, "Write/Write must map to index 0.");
static_assert(ExclusiveDepthStencil::kDepthNopStencilWrite.getIndex() == 1u, "Nop/Write must map to index 1.");
static_assert(ExclusiveDepthStencil::kDepthReadStencilWrite.getIndex() == 1u, "Read/Write must map to index 1.");
static_assert(ExclusiveDepthStencil::kDepthWriteStencilNop.getIndex() == 2u, "Write/Nop must map to index 2.");
static_assert(ExclusiveDepthStencil::kDepthWriteStencilRead.getIndex() == 2u, "Write/Read must map to index 2.");
static_assert(ExclusiveDepthStencil::kDepthNopStencilNop.getIndex() == 3u, "Nop/Nop must map to index 3.");
static_assert(ExclusiveDepthStencil::kDepthReadStencilNop.getIndex() == 3u, "Read/Nop must map to index 3.");
static_assert(ExclusiveDepthStencil::kDepthNopStencilRead.getIndex() == 3u, "Nop/Read must map to index 3.");
static_assert(ExclusiveDepthStencil::kDepthReadStencilRead.getIndex() == 3u, "Read/Read must map to index 3.");

static_assert(
    ExclusiveDepthStencil::kDepthWriteStencilRead.getReadableTransition() ==
        ExclusiveDepthStencil::kDepthReadStencilNop,
    "Writable depth with already-readable stencil must only transition depth to read."
);
static_assert(
    ExclusiveDepthStencil::kDepthReadStencilWrite.getWritableTransition() ==
        ExclusiveDepthStencil::kDepthWriteStencilNop,
    "Readable depth with already-writable stencil must only transition depth to write."
);

TEST(ExclusiveDepthStencilTest, MutatorsPreserveIndependentPlaneState)
{
    ExclusiveDepthStencil state = ExclusiveDepthStencil::kDepthReadStencilRead;
    state.setDepthWrite();
    EXPECT_EQ(state, ExclusiveDepthStencil::kDepthWriteStencilRead);

    state = ExclusiveDepthStencil::kDepthReadStencilRead;
    state.setStencilWrite();
    EXPECT_EQ(state, ExclusiveDepthStencil::kDepthReadStencilWrite);

    state.setDepthStencilWrite(true, true);
    EXPECT_EQ(state, ExclusiveDepthStencil::kDepthWriteStencilWrite);

    state.setDepthStencilWrite(true, false);
    EXPECT_EQ(state, ExclusiveDepthStencil::kDepthWriteStencilNop);

    state.setDepthStencilWrite(false, true);
    EXPECT_EQ(state, ExclusiveDepthStencil::kDepthNopStencilWrite);

    state.setDepthStencilWrite(false, false);
    EXPECT_EQ(state, ExclusiveDepthStencil::kDepthNopStencilNop);
}

TEST(ExclusiveDepthStencilTest, IsValidMatchesActivePlanesAgainstPassState)
{
    const ExclusiveDepthStencil passState = ExclusiveDepthStencil::kDepthWriteStencilRead;

    EXPECT_TRUE(ExclusiveDepthStencil::kDepthNopStencilNop.isValid(passState));
    EXPECT_TRUE(ExclusiveDepthStencil::kDepthWriteStencilNop.isValid(passState));
    EXPECT_TRUE(ExclusiveDepthStencil::kDepthNopStencilRead.isValid(passState));
    EXPECT_TRUE(ExclusiveDepthStencil::kDepthWriteStencilRead.isValid(passState));

    EXPECT_FALSE(ExclusiveDepthStencil::kDepthReadStencilNop.isValid(passState));
    EXPECT_FALSE(ExclusiveDepthStencil::kDepthNopStencilWrite.isValid(passState));
    EXPECT_FALSE(ExclusiveDepthStencil::kDepthWriteStencilWrite.isValid(passState));
}

TEST(ExclusiveDepthStencilTest, GetAccessMapsPlanesToDsvAccessFlags)
{
    sync::ResourceAccess depthAccess = sync::ResourceAccess::Unknown;
    sync::ResourceAccess stencilAccess = sync::ResourceAccess::Unknown;

    const sync::ResourceAccess kDsvReadWrite = sync::ResourceAccess::DSVRead | sync::ResourceAccess::DSVWrite;

    ExclusiveDepthStencil::kDepthNopStencilNop.getAccess(depthAccess, stencilAccess);
    EXPECT_EQ(depthAccess, sync::ResourceAccess::None);
    EXPECT_EQ(stencilAccess, sync::ResourceAccess::None);

    ExclusiveDepthStencil::kDepthWriteStencilRead.getAccess(depthAccess, stencilAccess);
    EXPECT_EQ(depthAccess, kDsvReadWrite);
    EXPECT_EQ(stencilAccess, sync::ResourceAccess::DSVRead);

    ExclusiveDepthStencil::kDepthReadStencilWrite.getAccess(depthAccess, stencilAccess);
    EXPECT_EQ(depthAccess, sync::ResourceAccess::DSVRead);
    EXPECT_EQ(stencilAccess, kDsvReadWrite);

    ExclusiveDepthStencil::kDepthWriteStencilNop.getAccess(depthAccess, stencilAccess);
    EXPECT_EQ(depthAccess, kDsvReadWrite);
    EXPECT_EQ(stencilAccess, sync::ResourceAccess::None);
}

TEST(ExclusiveDepthStencilTest, EnumerateSubresourcesEmitsMinimalPlaneCalls)
{
    struct SubresourceVisit
    {
        sync::ResourceAccess access{ sync::ResourceAccess::None };
        gp::UInt16 planeSlice{ 0u };
    };

    const auto collect = [](const ExclusiveDepthStencil state) -> gp::Vector<SubresourceVisit>
    {
        gp::Vector<SubresourceVisit> visits{};
        state.enumerateSubresources(
            [&visits](const sync::ResourceAccess access, const gp::UInt16 planeSlice) -> void
        {
            visits.emplaceBack(SubresourceVisit{ access, planeSlice });
        }
        );
        return visits;
    };

    // Unused depth-stencil emits zero subresource transitions.
    EXPECT_TRUE(collect(ExclusiveDepthStencil::kDepthNopStencilNop).isEmpty());

    // Matching depth and stencil access collapses into one whole-resource transition.
    const gp::Vector<SubresourceVisit> bothWritable = collect(ExclusiveDepthStencil::kDepthWriteStencilWrite);
    ASSERT_EQ(bothWritable.size(), 1u);
    EXPECT_EQ(bothWritable[0].access, sync::ResourceAccess::DSVRead | sync::ResourceAccess::DSVWrite);
    EXPECT_EQ(bothWritable[0].planeSlice, sync::SubresourceRange::kAllSubresources);

    const gp::Vector<SubresourceVisit> bothReadable = collect(ExclusiveDepthStencil::kDepthReadStencilRead);
    ASSERT_EQ(bothReadable.size(), 1u);
    EXPECT_EQ(bothReadable[0].access, sync::ResourceAccess::DSVRead);
    EXPECT_EQ(bothReadable[0].planeSlice, sync::SubresourceRange::kAllSubresources);

    // Divergent depth and stencil access emits separate depth and stencil plane transitions.
    const gp::Vector<SubresourceVisit> mixed = collect(ExclusiveDepthStencil::kDepthWriteStencilRead);
    ASSERT_EQ(mixed.size(), 2u);
    EXPECT_EQ(mixed[0].access, sync::ResourceAccess::DSVRead | sync::ResourceAccess::DSVWrite);
    EXPECT_EQ(mixed[0].planeSlice, sync::SubresourceRange::kDepthPlaneSlice);
    EXPECT_EQ(mixed[1].access, sync::ResourceAccess::DSVRead);
    EXPECT_EQ(mixed[1].planeSlice, sync::SubresourceRange::kStencilPlaneSlice);

    // Single-plane usage emits only that plane.
    const gp::Vector<SubresourceVisit> stencilOnly = collect(ExclusiveDepthStencil::kDepthNopStencilWrite);
    ASSERT_EQ(stencilOnly.size(), 1u);
    EXPECT_EQ(stencilOnly[0].access, sync::ResourceAccess::DSVRead | sync::ResourceAccess::DSVWrite);
    EXPECT_EQ(stencilOnly[0].planeSlice, sync::SubresourceRange::kStencilPlaneSlice);

    const gp::Vector<SubresourceVisit> depthOnlyWrite = collect(ExclusiveDepthStencil::kDepthWriteStencilNop);
    ASSERT_EQ(depthOnlyWrite.size(), 1u);
    EXPECT_EQ(depthOnlyWrite[0].access, sync::ResourceAccess::DSVRead | sync::ResourceAccess::DSVWrite);
    EXPECT_EQ(depthOnlyWrite[0].planeSlice, sync::SubresourceRange::kDepthPlaneSlice);

    const gp::Vector<SubresourceVisit> depthOnlyRead = collect(ExclusiveDepthStencil::kDepthReadStencilNop);
    ASSERT_EQ(depthOnlyRead.size(), 1u);
    EXPECT_EQ(depthOnlyRead[0].access, sync::ResourceAccess::DSVRead);
    EXPECT_EQ(depthOnlyRead[0].planeSlice, sync::SubresourceRange::kDepthPlaneSlice);
}

TEST(ExclusiveDepthStencilTest, DeltaTransitionsAndDistinctHashes)
{
    const ExclusiveDepthStencil defaultState{};
    EXPECT_EQ(defaultState, ExclusiveDepthStencil::kDepthNopStencilNop);
    EXPECT_EQ(defaultState.getValue(), ExclusiveDepthStencilMode::DepthNopStencilNop);

    const ExclusiveDepthStencil fromMode = ExclusiveDepthStencilMode::DepthNopStencilWrite;
    EXPECT_EQ(fromMode.getValue(), ExclusiveDepthStencilMode::DepthNopStencilWrite);
    EXPECT_TRUE(fromMode.isUsingDepthStencil());
    EXPECT_FALSE(fromMode.isUsingDepth());
    EXPECT_TRUE(fromMode.isUsingStencil());
    EXPECT_TRUE(fromMode.isStencilWrite());
    EXPECT_TRUE(fromMode.isAnyWrite());
    EXPECT_FALSE(fromMode.isDepthRead());
    EXPECT_TRUE(ExclusiveDepthStencil::kDepthReadStencilNop.isDepthRead());

    sync::ResourceAccess depthAccess = sync::ResourceAccess::Unknown;
    sync::ResourceAccess stencilAccess = sync::ResourceAccess::Unknown;
    fromMode.getAccess(depthAccess, stencilAccess);
    EXPECT_EQ(depthAccess, sync::ResourceAccess::None);
    EXPECT_EQ(stencilAccess, sync::ResourceAccess::DSVRead | sync::ResourceAccess::DSVWrite);

    EXPECT_EQ(
        ExclusiveDepthStencil::kDepthNopStencilNop.getReadableTransition(), ExclusiveDepthStencil::kDepthNopStencilNop
    );
    EXPECT_EQ(
        ExclusiveDepthStencil::kDepthNopStencilNop.getWritableTransition(), ExclusiveDepthStencil::kDepthNopStencilNop
    );

    gp::hashing::Fnv1a64 hashA{};
    gp::hashing::Fnv1a64 hashB{};
    fromMode.hashAppend(hashA);
    ExclusiveDepthStencil::kDepthWriteStencilRead.hashAppend(hashB);
    EXPECT_NE(hashA.finalize(), hashB.finalize());
    EXPECT_EQ(
        ExclusiveDepthStencil::kDepthWriteStencilWrite.getReadableTransition(),
        ExclusiveDepthStencil::kDepthReadStencilRead
    );
    EXPECT_EQ(
        ExclusiveDepthStencil::kDepthReadStencilRead.getReadableTransition(), ExclusiveDepthStencil::kDepthNopStencilNop
    );
    EXPECT_EQ(
        ExclusiveDepthStencil::kDepthReadStencilRead.getWritableTransition(),
        ExclusiveDepthStencil::kDepthWriteStencilWrite
    );
    EXPECT_EQ(
        ExclusiveDepthStencil::kDepthWriteStencilWrite.getWritableTransition(),
        ExclusiveDepthStencil::kDepthNopStencilNop
    );

    const ExclusiveDepthStencil allStates[9] = {
        ExclusiveDepthStencil::kDepthNopStencilNop,    ExclusiveDepthStencil::kDepthReadStencilNop,
        ExclusiveDepthStencil::kDepthWriteStencilNop,  ExclusiveDepthStencil::kDepthNopStencilRead,
        ExclusiveDepthStencil::kDepthReadStencilRead,  ExclusiveDepthStencil::kDepthWriteStencilRead,
        ExclusiveDepthStencil::kDepthNopStencilWrite,  ExclusiveDepthStencil::kDepthReadStencilWrite,
        ExclusiveDepthStencil::kDepthWriteStencilWrite
    };

    for (gp::USize i = 0u; i < 9u; ++i)
    {
        EXPECT_EQ(gp::hashing::hashOf(allStates[i]), allStates[i].getHash());
        for (gp::USize j = i + 1u; j < 9u; ++j)
        {
            EXPECT_NE(allStates[i], allStates[j]);
            EXPECT_NE(allStates[i].getHash(), allStates[j].getHash());
        }
    }
}

}   // namespace gp::rhi::resource::tests
