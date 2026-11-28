// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "diagnostics/Breadcrumbs.hpp"
#include "diagnostics/IntelBreadcrumbs.hpp"
#include "diagnostics/NvidiaAftermath.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::diagnostics::tests
{

TEST(RHIGPUCrashHooksTests, IntelBreadcrumbsLifecycleAndMarkerResolution)
{
    intel::reset();
    EXPECT_FALSE(intel::isEnabled());

    gp::SharedRef<BreadcrumbAllocator> alloc = BreadcrumbAllocator::create();
    BreadcrumbNode* const node = alloc->allocFormattedBreadcrumb(BreadcrumbData("Pass", "Pass %d"), 5);

    const intel::Marker nullMarker(nullptr);
    EXPECT_STREQ(nullMarker.getText(), "<root>");
    EXPECT_FALSE(nullMarker.isNodeMarker());

    const intel::Marker sentinelMarker(BreadcrumbNode::getSentinel());
    EXPECT_STREQ(sentinelMarker.getText(), "<root>");
    EXPECT_FALSE(sentinelMarker.isNodeMarker());

    const intel::Marker validMarker(node);
    EXPECT_STREQ(validMarker.getText(), "Pass 5");
    EXPECT_TRUE(validMarker.isNodeMarker());
    EXPECT_EQ(validMarker.getNodeId(), node->id);

    struct IntelHookContext
    {
        gp::UInt32 initCount{ 0u };
        gp::UInt32 dumpCount{ 0u };
    } ctx{};

    intel::BackendCallbacks callbacks{};
    callbacks.userData = &ctx;
    callbacks.initDriver = [](const intel::CrashDumpFlags, void* const user) noexcept -> bool
    {
        ++static_cast<IntelHookContext*>(user)->initCount;
        return true;
    };
    callbacks.writeDump = [](const gp::StringView, const intel::CrashDumpFlags, void* const user) noexcept -> bool
    {
        ++static_cast<IntelHookContext*>(user)->dumpCount;
        return true;
    };

    intel::registerBackendCallbacks(callbacks);
    intel::setEnabled(true);
    intel::setCrashDumpFlags(intel::CrashDumpFlags::All);
    EXPECT_TRUE(intel::initializeBeforeDeviceCreation());
    EXPECT_EQ(ctx.initCount, 1u);
    EXPECT_TRUE(intel::onGPUCrash(".cache/crashes"));
    EXPECT_EQ(ctx.dumpCount, 1u);

    intel::reset();
}

TEST(RHIGPUCrashHooksTests, NvidiaAftermathShaderRegistrationAndCrashFlow)
{
    nvidia::reset();
    EXPECT_FALSE(nvidia::isEnabled());
    EXPECT_FALSE(nvidia::areMarkersEnabled());

    gp::SharedRef<BreadcrumbAllocator> alloc = BreadcrumbAllocator::create();
    BreadcrumbNode* const node = alloc->allocBreadcrumb(BreadcrumbData("RayGenDispatch"));

    const nvidia::Marker rootMarker(nullptr);
    EXPECT_STREQ(rootMarker.getText(), "<root>");
    EXPECT_EQ(rootMarker.getNodeId(), 0u);

    const nvidia::Marker nodeMarker(node);
    EXPECT_STREQ(nodeMarker.getText(), "RayGenDispatch");
    EXPECT_EQ(nodeMarker.getNodeId(), node->id);

    struct AftermathContext
    {
        gp::UInt32 registeredCount{ 0u };
        gp::UInt32 deregisteredCount{ 0u };
        gp::UInt32 crashCalls{ 0u };
    } ctx{};

    static bool s_lateAssociated = false;
    s_lateAssociated = false;

    nvidia::BackendCallbacks callbacks{};
    callbacks.userData = &ctx;
    callbacks.registerShader = [](const nvidia::ShaderHash, const void*, const gp::UInt32, void* const u) noexcept
    {
        ++static_cast<AftermathContext*>(u)->registeredCount;
    };
    callbacks.deregisterShader = [](const nvidia::ShaderHash, void* const u) noexcept
    {
        ++static_cast<AftermathContext*>(u)->deregisteredCount;
    };
    callbacks.waitCrashDump = [](const gp::Float32, const gp::StringView, void* const u) noexcept -> nvidia::CrashResult
    {
        ++static_cast<AftermathContext*>(u)->crashCalls;
        return nvidia::CrashResult::Dumped;
    };

    nvidia::registerBackendCallbacks(callbacks);
    nvidia::setEnabled(true);
    EXPECT_TRUE(nvidia::areMarkersEnabled());
    EXPECT_TRUE(nvidia::initializeBeforeDeviceCreation());
    int dummyDevice = 1;
    EXPECT_TRUE(nvidia::initializeDevice(&dummyDevice));

    const gp::UInt8 spirvBytes[8] = { 0x03u, 0x02u, 0x23u, 0x07u, 1u, 0u, 0u, 0u };
    const nvidia::ShaderHash hash = nvidia::registerShaderBinary(spirvBytes, sizeof(spirvBytes), "LightingCS.spv");
    EXPECT_NE(hash, 0u);
    EXPECT_EQ(nvidia::getRegisteredShaderCount(), 1u);
    EXPECT_EQ(ctx.registeredCount, 1u);

    nvidia::RegisteredShaderEntry entry{};
    ASSERT_TRUE(nvidia::findRegisteredShader(hash, entry));
    EXPECT_EQ(entry.byteSize, sizeof(spirvBytes));
    EXPECT_EQ(entry.debugName, "LightingCS.spv");

    nvidia::setLateShaderAssociateCallback(
        []() noexcept -> void
    {
        s_lateAssociated = true;
    }
    );

    EXPECT_EQ(nvidia::onGPUCrash(2.0f, ".cache/crashes"), nvidia::CrashResult::Dumped);
    EXPECT_TRUE(s_lateAssociated);
    EXPECT_EQ(ctx.crashCalls, 1u);

    EXPECT_TRUE(nvidia::deregisterShaderBinary(hash));
    EXPECT_EQ(nvidia::getRegisteredShaderCount(), 0u);
    EXPECT_EQ(ctx.deregisteredCount, 1u);

    nvidia::reset();
}

}   // namespace gp::rhi::diagnostics::tests
