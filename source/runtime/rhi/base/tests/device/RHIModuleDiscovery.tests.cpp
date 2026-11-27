// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/RHIModuleDiscovery.hpp"
#include "commands/CommandListExecutor.hpp"
#include "commands/ICommandContext.hpp"
#include "device/DynamicRHI.hpp"
#include "device/DynamicRHIPSOFallback.hpp"
#include "device/IDynamicRHIModule.hpp"
#include "device/RHIBootstrapDesc.hpp"
#include "device/RHIGlobals.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "modules/ModuleManager.hpp"
#include "pipeline/BlendState.hpp"          // IWYU pragma: keep
#include "pipeline/BoundShaderState.hpp"    // IWYU pragma: keep
#include "pipeline/DepthStencilState.hpp"   // IWYU pragma: keep
#include "pipeline/RasterizerState.hpp"     // IWYU pragma: keep
#include "pipeline/SamplerState.hpp"        // IWYU pragma: keep
#include "pipeline/VertexDeclaration.hpp"
#include "presentation/Viewport.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferInitializer.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureInitializer.hpp"
#include "resources/UniformBuffer.hpp"
#include "shaders/ComputeShader.hpp"   // IWYU pragma: keep
#include "shaders/GeometryShader.hpp"
#include "shaders/PixelShader.hpp"
#include "shaders/VertexShader.hpp"
#include "sync/GPUFence.hpp"               // IWYU pragma: keep
#include "sync/RenderQuery.hpp"
#include "views/ShaderResourceView.hpp"    // IWYU pragma: keep
#include "views/UnorderedAccessView.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::tests
{

namespace
{

/// @brief Minimal `DynamicRHI` stub instantiated by mock `IDynamicRHIModule` implementations during discovery tests.
class DiscoveryMockRHI final : public DynamicRHIPSOFallback
{
public:
    InterfaceType interfaceType{ InterfaceType::Vulkan };
    FeatureLevel negotiatedLevel{ FeatureLevel::SM5 };
    const char* backendName{ "MockRHI" };
    gp::UInt32 initCallCount{ 0u };
    gp::UInt32 postInitCallCount{ 0u };
    gp::UInt32 shutdownCallCount{ 0u };

public:
    explicit DiscoveryMockRHI(
        const InterfaceType inInterfaceType, const FeatureLevel inLevel, const char* const inName
    ) noexcept
        : interfaceType(inInterfaceType)
        , negotiatedLevel(inLevel)
        , backendName(inName)
    {}

    virtual ~DiscoveryMockRHI() noexcept override = default;

    virtual void init() noexcept override
    {
        ++initCallCount;
        RHIGlobals& globals = getRHIGlobals();
        globals.maxSupportedFeatureLevel = negotiatedLevel;
        globals.usingNullRHI = (interfaceType == InterfaceType::Null);
        globals.gpuInfo.adapterName = gp::String(backendName);
    }

    virtual void postInit() noexcept override
    {
        ++postInitCallCount;
    }

    virtual void shutdown() noexcept override
    {
        ++shutdownCallCount;
    }

    [[nodiscard]] virtual const char* getName() const noexcept override
    {
        return backendName;
    }

    [[nodiscard]] virtual InterfaceType getInterfaceType() const noexcept override
    {
        return interfaceType;
    }

    [[nodiscard]] virtual pipeline::SamplerStateRef
        createSamplerState(const pipeline::SamplerStateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::RasterizerStateRef
        createRasterizerState(const pipeline::RasterizerStateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::DepthStencilStateRef
        createDepthStencilState(const pipeline::DepthStencilStateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::BlendStateRef createBlendState(const pipeline::BlendStateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::VertexDeclarationRef
        createVertexDeclaration(const pipeline::VertexDeclarationElementList&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual shader::VertexShaderRef
        createVertexShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual shader::PixelShaderRef
        createPixelShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual shader::GeometryShaderRef
        createGeometryShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual shader::ComputeShaderRef
        createComputeShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::BoundShaderStateRef createBoundShaderState(
        pipeline::VertexDeclaration*, shader::VertexShader*, shader::PixelShader*, shader::GeometryShader*
    ) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual resource::UniformBufferRef createUniformBuffer(
        const void*,
        const resource::UniformBufferLayout*,
        resource::UniformBufferUsage,
        resource::UniformBufferValidation
    ) noexcept override
    {
        return {};
    }

    virtual void updateUniformBuffer(command::CommandListBase&, resource::UniformBuffer*, const void*) noexcept override
    {}

    [[nodiscard]] virtual resource::BufferInitializer
        createBufferInitializer(command::CommandListBase&, const resource::BufferCreateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual void* lockBufferBottomOfPipe(
        command::CommandListBase&, resource::Buffer*, gp::UInt32, gp::UInt32, resource::ResourceLockMode
    ) noexcept override
    {
        return nullptr;
    }

    virtual void unlockBufferBottomOfPipe(command::CommandListBase&, resource::Buffer*) noexcept override
    {}

    [[nodiscard]] virtual resource::TextureInitializer
        createTextureInitializer(command::CommandListBase&, const resource::TextureCreateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual resource::CalcTextureSizeResult
        calcTexturePlatformSize(const resource::TextureDesc&, gp::UInt32) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual gp::UInt64 computeMemorySize(resource::Texture*) noexcept override
    {
        return 0u;
    }

    [[nodiscard]] virtual resource::TextureLockResult
        lockTexture(command::CommandListImmediate&, const resource::TextureLockArgs&) noexcept override
    {
        return {};
    }

    virtual void unlockTexture(command::CommandListImmediate&, const resource::TextureLockArgs&) noexcept override
    {}

    virtual void updateTexture2D(
        command::CommandListBase&,
        resource::Texture*,
        gp::UInt32,
        const resource::UpdateTextureRegion2D&,
        gp::UInt32,
        const gp::UInt8*
    ) noexcept override
    {}

    virtual void updateTexture3D(
        command::CommandListBase&,
        resource::Texture*,
        gp::UInt32,
        const resource::UpdateTextureRegion3D&,
        gp::UInt32,
        gp::UInt32,
        const gp::UInt8*
    ) noexcept override
    {}

    virtual void readSurfaceData(
        resource::Texture*,
        const resource::IntRect&,
        gp::Vector<gp::math::Color>&,
        const resource::ReadSurfaceDataFlags&
    ) noexcept override
    {}

    [[nodiscard]] virtual view::ShaderResourceViewRef
        createShaderResourceView(command::CommandListBase&, resource::Buffer*, const view::ViewDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual view::ShaderResourceViewRef
        createShaderResourceView(command::CommandListBase&, resource::Texture*, const view::ViewDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual view::UnorderedAccessViewRef
        createUnorderedAccessView(command::CommandListBase&, resource::Buffer*, const view::ViewDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual view::UnorderedAccessViewRef createUnorderedAccessView(
        command::CommandListBase&, resource::Texture*, const view::ViewDesc&
    ) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual sync::GPUFenceRef createGPUFence(gp::Name) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual sync::RenderQueryRef createRenderQuery(sync::RenderQueryType) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual bool
        getRenderQueryResult(sync::RenderQuery*, gp::UInt64& outResult, bool, gp::UInt32) noexcept override
    {
        outResult = 0u;
        return false;
    }

    [[nodiscard]] virtual presentation::ViewportRef
        createViewport(void*, gp::UInt32, gp::UInt32, bool, PixelFormat) noexcept override
    {
        return {};
    }

    virtual void resizeViewport(presentation::Viewport*, gp::UInt32, gp::UInt32, bool, PixelFormat) noexcept override
    {}

    virtual void beginDrawingViewport(presentation::Viewport*, resource::Texture*) noexcept override
    {}

    using DynamicRHI::endDrawingViewport;

    virtual void
        endDrawingViewport(presentation::Viewport*, const presentation::PresentArgsWithStats&) noexcept override
    {}

    virtual void blockUntilGPUIdle() noexcept override
    {}

    [[nodiscard]] virtual resource::TextureRef getViewportBackBuffer(presentation::Viewport*) noexcept override
    {
        return {};
    }

    virtual void advanceFrameForGetViewportBackBuffer(presentation::Viewport*) noexcept override
    {}

    [[nodiscard]] virtual command::ICommandContext* getDefaultContext() noexcept override
    {
        return nullptr;
    }

    [[nodiscard]] virtual command::IComputeContext* getDefaultAsyncComputeContext() noexcept override
    {
        return nullptr;
    }
};

/// @brief Configurable mock `IDynamicRHIModule` supporting custom interface types and feature level ceilings.
template <InterfaceType InType, FeatureLevel InMaxLevel, bool InSupported>
class MockDynamicRHIModule final : public IDynamicRHIModule
{
public:
    static inline FeatureLevel s_lastRequestedLevel{ FeatureLevel::Count };
    static inline gp::UInt32 s_createCount{ 0u };

    static void resetTracking() noexcept
    {
        s_lastRequestedLevel = FeatureLevel::Count;
        s_createCount = 0u;
    }

    [[nodiscard]] virtual bool supportsDynamicReloading() const noexcept override
    {
        return true;
    }

    [[nodiscard]] virtual bool isSupported() const noexcept override
    {
        return InSupported;
    }

    [[nodiscard]] virtual bool isSupported(const FeatureLevel requestedFeatureLevel) const noexcept override
    {
        if (!InSupported)
        {
            return false;
        }
        if (requestedFeatureLevel == FeatureLevel::Count)
        {
            return true;
        }
        return static_cast<gp::Int32>(requestedFeatureLevel) <= static_cast<gp::Int32>(InMaxLevel);
    }

    [[nodiscard]] virtual DynamicRHI* createRHI(const FeatureLevel requestedFeatureLevel) noexcept override
    {
        s_lastRequestedLevel = requestedFeatureLevel;
        ++s_createCount;
        const FeatureLevel activeLevel =
            (requestedFeatureLevel == FeatureLevel::Count) ? InMaxLevel : requestedFeatureLevel;
        return new DiscoveryMockRHI(InType, activeLevel, "MockRHI");
    }
};

template <typename InModuleType>
[[nodiscard]] gp::modules::IModule*
    createMockModule(gp::memory::Malloc* const hostMalloc, gp::memory::Malloc** const outBoundShadowMalloc) noexcept
{
    if (outBoundShadowMalloc != nullptr)
    {
        *outBoundShadowMalloc = hostMalloc;
    }
    return new InModuleType();
}

void destroyMockModule(gp::modules::IModule* const instance) noexcept
{
    delete instance;
}

class RHIModuleDiscoveryTest : public ::testing::Test
{
protected:
    virtual void SetUp() noexcept override
    {
        rhiExit();
        gp::CommandLine::shutdown();
    }

    virtual void TearDown() noexcept override
    {
        rhiExit();
        gp::CommandLine::shutdown();
    }
};

}   // namespace

TEST_F(RHIModuleDiscoveryTest, InterfaceAndFeatureLevelNameConversions)
{
    EXPECT_EQ(getModuleNameForInterfaceType(InterfaceType::Null), kNullRHIModuleName);
    EXPECT_EQ(getModuleNameForInterfaceType(InterfaceType::D3D12), kD3D12RHIModuleName);
    EXPECT_EQ(getModuleNameForInterfaceType(InterfaceType::D3D11), kD3D11RHIModuleName);
    EXPECT_EQ(getModuleNameForInterfaceType(InterfaceType::Vulkan), kVulkanRHIModuleName);
    EXPECT_EQ(getModuleNameForInterfaceType(InterfaceType::Metal), kMetalRHIModuleName);
    EXPECT_EQ(getModuleNameForInterfaceType(InterfaceType::OpenGL), kOpenGLRHIModuleName);
    EXPECT_TRUE(getModuleNameForInterfaceType(InterfaceType::NotSet).isEmpty());

    EXPECT_EQ(getInterfaceTypeFromName("null"), InterfaceType::Null);
    EXPECT_EQ(getInterfaceTypeFromName("rhi/null"), InterfaceType::Null);
    EXPECT_EQ(getInterfaceTypeFromName("DX12"), InterfaceType::D3D12);
    EXPECT_EQ(getInterfaceTypeFromName("rhi-d3d12"), InterfaceType::D3D12);
    EXPECT_EQ(getInterfaceTypeFromName("d3d11"), InterfaceType::D3D11);
    EXPECT_EQ(getInterfaceTypeFromName("vk"), InterfaceType::Vulkan);
    EXPECT_EQ(getInterfaceTypeFromName("rhi/vulkan"), InterfaceType::Vulkan);
    EXPECT_EQ(getInterfaceTypeFromName("mtl"), InterfaceType::Metal);
    EXPECT_EQ(getInterfaceTypeFromName("gl"), InterfaceType::OpenGL);
    EXPECT_EQ(getInterfaceTypeFromName("unknown-backend"), InterfaceType::NotSet);
    EXPECT_EQ(getInterfaceTypeFromName(""), InterfaceType::NotSet);

    EXPECT_EQ(getFeatureLevelName(FeatureLevel::ES3_1), "ES3_1");
    EXPECT_EQ(getFeatureLevelName(FeatureLevel::SM5), "SM5");
    EXPECT_EQ(getFeatureLevelName(FeatureLevel::SM6), "SM6");
    EXPECT_EQ(getFeatureLevelName(FeatureLevel::Count), "Highest");

    FeatureLevel parsed = FeatureLevel::Count;
    EXPECT_TRUE(tryParseFeatureLevel("es31", parsed));
    EXPECT_EQ(parsed, FeatureLevel::ES3_1);
    EXPECT_TRUE(tryParseFeatureLevel("FeatureLevelES3_1", parsed));
    EXPECT_EQ(parsed, FeatureLevel::ES3_1);
    EXPECT_TRUE(tryParseFeatureLevel("SM5", parsed));
    EXPECT_EQ(parsed, FeatureLevel::SM5);
    EXPECT_TRUE(tryParseFeatureLevel("sm6", parsed));
    EXPECT_EQ(parsed, FeatureLevel::SM6);
    EXPECT_FALSE(tryParseFeatureLevel("sm9", parsed));
}

TEST_F(RHIModuleDiscoveryTest, PlatformSearchOrderReturnsBoundedCandidates)
{
    gp::StringView order[kMaxPlatformRHICandidates]{};
    const gp::UInt32 count = getPlatformRHISearchOrder(order, kMaxPlatformRHICandidates);
    EXPECT_GT(count, 0u);
    EXPECT_LE(count, kMaxPlatformRHICandidates);
    EXPECT_FALSE(order[0].isEmpty());

    EXPECT_EQ(getPlatformRHISearchOrder(nullptr, kMaxPlatformRHICandidates), 0u);
    EXPECT_EQ(getPlatformRHISearchOrder(order, 0u), 0u);
    EXPECT_EQ(getPlatformRHISearchOrder(order, 1u), 1u);
}

TEST_F(RHIModuleDiscoveryTest, CommandLineOverridesSelectBackendAndFeatureLevel)
{
    gp::CommandLine::initialize("app.exe -vulkan -sm6");
    RHIBootstrapDesc desc{};
    applyRHICommandLineOverrides(desc);
    EXPECT_FALSE(desc.forceNullRHI);
    EXPECT_EQ(desc.preferredModuleName, kVulkanRHIModuleName);
    EXPECT_EQ(desc.preferredFeatureLevel, FeatureLevel::SM6);
    EXPECT_TRUE(desc.forcedModuleByCommandLine);
    EXPECT_TRUE(desc.forcedFeatureLevelByCommandLine);

    gp::CommandLine::initialize("app.exe -nullrhi -es31");
    RHIBootstrapDesc nullDesc{};
    applyRHICommandLineOverrides(nullDesc);
    EXPECT_TRUE(nullDesc.forceNullRHI);
    EXPECT_EQ(nullDesc.preferredModuleName, kNullRHIModuleName);
    EXPECT_EQ(nullDesc.preferredFeatureLevel, FeatureLevel::ES3_1);

    gp::CommandLine::initialize("app.exe -rhi=custom/rhi -sm5");
    RHIBootstrapDesc customDesc{};
    applyRHICommandLineOverrides(customDesc);
    EXPECT_FALSE(customDesc.forceNullRHI);
    EXPECT_EQ(customDesc.preferredModuleName, "custom/rhi");
    EXPECT_EQ(customDesc.preferredFeatureLevel, FeatureLevel::SM5);
}

TEST_F(RHIModuleDiscoveryTest, SelectionNegotiatesFeatureLevelAndUnloadsUnsupportedCandidates)
{
    using UnsupportedD3D12 = MockDynamicRHIModule<InterfaceType::D3D12, FeatureLevel::SM6, false>;
#if GP_PLATFORM_MACOS
    using SupportedFallbackSM5 = MockDynamicRHIModule<InterfaceType::Metal, FeatureLevel::SM5, true>;
    constexpr gp::StringView kExpectedFallbackModule = kMetalRHIModuleName;
#else
    using SupportedFallbackSM5 = MockDynamicRHIModule<InterfaceType::Vulkan, FeatureLevel::SM5, true>;
    constexpr gp::StringView kExpectedFallbackModule = kVulkanRHIModuleName;
#endif
    using SupportedNull = MockDynamicRHIModule<InterfaceType::Null, FeatureLevel::SM5, true>;

    UnsupportedD3D12::resetTracking();
    SupportedFallbackSM5::resetTracking();
    SupportedNull::resetTracking();

    gp::modules::ModuleManager manager{};
    ASSERT_TRUE(
        manager.registerStaticModule(kD3D12RHIModuleName, &createMockModule<UnsupportedD3D12>, &destroyMockModule)
    );
    ASSERT_TRUE(manager.registerStaticModule(
        kExpectedFallbackModule, &createMockModule<SupportedFallbackSM5>, &destroyMockModule
    ));
    ASSERT_TRUE(manager.registerStaticModule(kNullRHIModuleName, &createMockModule<SupportedNull>, &destroyMockModule));

    RHIBootstrapDesc desc{};
    desc.preferredModuleName = kD3D12RHIModuleName;
    desc.preferredFeatureLevel = FeatureLevel::SM6;
    desc.applyCommandLineOverrides = false;

    const RHISelectionResult result = selectDynamicRHIModule(desc, manager);
    ASSERT_TRUE(result.isValid());
    EXPECT_EQ(result.moduleName, kExpectedFallbackModule);
    EXPECT_EQ(result.featureLevel, FeatureLevel::SM5);
    EXPECT_FALSE(result.usedNullFallback);
    EXPECT_FALSE(manager.isModuleLoaded(kD3D12RHIModuleName));
    EXPECT_TRUE(manager.isModuleLoaded(kExpectedFallbackModule));
}

TEST_F(RHIModuleDiscoveryTest, SelectionFallsBackToNullWhenHardwareUnavailable)
{
    using SupportedNull = MockDynamicRHIModule<InterfaceType::Null, FeatureLevel::SM5, true>;
    SupportedNull::resetTracking();

    gp::modules::ModuleManager manager{};
    ASSERT_TRUE(manager.registerStaticModule(kNullRHIModuleName, &createMockModule<SupportedNull>, &destroyMockModule));

    RHIBootstrapDesc fallbackDesc{};
    fallbackDesc.preferredModuleName = "rhi/nonexistent";
    fallbackDesc.allowNullFallback = true;
    fallbackDesc.applyCommandLineOverrides = false;

    const RHISelectionResult fallbackRes = selectDynamicRHIModule(fallbackDesc, manager);
    ASSERT_TRUE(fallbackRes.isValid());
    EXPECT_EQ(fallbackRes.moduleName, kNullRHIModuleName);
    EXPECT_TRUE(fallbackRes.usedNullFallback);

    RHIBootstrapDesc noFallbackDesc = fallbackDesc;
    noFallbackDesc.allowNullFallback = false;
    const RHISelectionResult failRes = selectDynamicRHIModule(noFallbackDesc, manager);
    EXPECT_FALSE(failRes.isValid());
}

TEST_F(RHIModuleDiscoveryTest, RHIInitFullLifecycleAndBypassAndEditorValidation)
{
    using VulkanES31Only = MockDynamicRHIModule<InterfaceType::Vulkan, FeatureLevel::ES3_1, true>;
    using VulkanSM6 = MockDynamicRHIModule<InterfaceType::Vulkan, FeatureLevel::SM6, true>;
    using SupportedNull = MockDynamicRHIModule<InterfaceType::Null, FeatureLevel::SM5, true>;

    gp::modules::ModuleManager manager{};
    ASSERT_TRUE(manager.registerStaticModule("rhi/es31mock", &createMockModule<VulkanES31Only>, &destroyMockModule));
    ASSERT_TRUE(manager.registerStaticModule(kVulkanRHIModuleName, &createMockModule<VulkanSM6>, &destroyMockModule));
    ASSERT_TRUE(manager.registerStaticModule(kNullRHIModuleName, &createMockModule<SupportedNull>, &destroyMockModule));

    // Editor token rejects hardware backend below SM5.
    RHIBootstrapDesc editorFailDesc{};
    editorFailDesc.preferredModuleName = "rhi/es31mock";
    editorFailDesc.preferredFeatureLevel = FeatureLevel::ES3_1;
    editorFailDesc.hasEditorToken = true;
    editorFailDesc.allowNullFallback = false;
    editorFailDesc.applyCommandLineOverrides = false;
    EXPECT_FALSE(rhiInit(editorFailDesc, manager));
    EXPECT_FALSE(hasValidDynamicRHI());
    EXPECT_TRUE(getLoadedRHIModuleName().isEmpty());

    // Valid SM6 backend initializes cleanly with -rhibypass latched from CLI.
    gp::CommandLine::initialize("app.exe -vulkan -sm6 -rhibypass");
    RHIBootstrapDesc okDesc{};
    okDesc.hasEditorToken = true;
    ASSERT_TRUE(rhiInit(okDesc, manager));
    EXPECT_TRUE(hasValidDynamicRHI());
    EXPECT_TRUE(getRHIGlobals().isRHIInitialized);
    EXPECT_FALSE(getRHIGlobals().usingNullRHI);
    EXPECT_EQ(getLoadedRHIModuleName(), kVulkanRHIModuleName);
    EXPECT_EQ(getSelectedDynamicRHIModuleName(true, manager), kVulkanRHIModuleName);
    EXPECT_TRUE(command::getCommandListExecutor().bypass());

    // Idempotent second call succeeds without replacing the active device.
    DynamicRHI* const firstDevice = getDynamicRHI();
    EXPECT_TRUE(rhiInit(okDesc, manager));
    EXPECT_EQ(getDynamicRHI(), firstDevice);

    rhiPostInit();
    rhiExit();
    EXPECT_FALSE(hasValidDynamicRHI());
    EXPECT_TRUE(getLoadedRHIModuleName().isEmpty());
}

TEST_F(RHIModuleDiscoveryTest, InitNullRHIAndHeadlessBootstrap)
{
    using SupportedNull = MockDynamicRHIModule<InterfaceType::Null, FeatureLevel::SM5, true>;
    gp::modules::ModuleManager manager{};
    ASSERT_TRUE(manager.registerStaticModule(kNullRHIModuleName, &createMockModule<SupportedNull>, &destroyMockModule));

    // Probe before init with cleanupIfProbed=true unloads module after inspection.
    EXPECT_EQ(getSelectedDynamicRHIModuleName(true, manager), kNullRHIModuleName);
    EXPECT_FALSE(manager.isModuleLoaded(kNullRHIModuleName));

    RHIBootstrapDesc headlessDesc{};
    headlessDesc.canRender = false;
    headlessDesc.applyCommandLineOverrides = false;
    ASSERT_TRUE(rhiInit(headlessDesc, manager));
    EXPECT_TRUE(hasValidDynamicRHI());
    EXPECT_TRUE(getRHIGlobals().usingNullRHI);
    EXPECT_FALSE(getRHIGlobals().supportsTextureStreaming);
    EXPECT_EQ(getLoadedRHIModuleName(), kNullRHIModuleName);

    rhiExit();
    EXPECT_FALSE(hasValidDynamicRHI());
}

TEST_F(RHIModuleDiscoveryTest, RejectsPlainIModuleWithoutDynamicRHIInterfaceTag)
{
    using SupportedNull = MockDynamicRHIModule<InterfaceType::Null, FeatureLevel::SM5, true>;
    gp::modules::ModuleManager manager{};
    ASSERT_TRUE(manager.registerStaticModule(
        "rhi/plain-stub", &createMockModule<gp::modules::DefaultModuleImpl>, &destroyMockModule
    ));
    ASSERT_TRUE(manager.registerStaticModule(kNullRHIModuleName, &createMockModule<SupportedNull>, &destroyMockModule));

    RHIBootstrapDesc desc{};
    desc.preferredModuleName = "rhi/plain-stub";
    desc.allowNullFallback = true;
    desc.applyCommandLineOverrides = false;

    const RHISelectionResult result = selectDynamicRHIModule(desc, manager);
    ASSERT_TRUE(result.isValid());
    EXPECT_EQ(result.moduleName, kNullRHIModuleName);
    EXPECT_TRUE(result.usedNullFallback);
    EXPECT_FALSE(manager.isModuleLoaded("rhi/plain-stub"));
}

TEST_F(RHIModuleDiscoveryTest, RejectsModuleThatDeclinesRequestedFeatureLevel)
{
    class SM5OnlyModule final : public IDynamicRHIModule
    {
    public:
        [[nodiscard]] virtual bool isSupported() const noexcept override
        {
            return true;
        }

        [[nodiscard]] virtual bool isSupported(const FeatureLevel level) const noexcept override
        {
            return level == FeatureLevel::SM5;
        }

        [[nodiscard]] virtual DynamicRHI* createRHI(const FeatureLevel) noexcept override
        {
            return nullptr;
        }
    };

    gp::modules::ModuleManager manager{};
    ASSERT_TRUE(manager.registerStaticModule("rhi/sm5only", &createMockModule<SM5OnlyModule>, &destroyMockModule));

    RHIBootstrapDesc desc{};
    desc.preferredModuleName = "rhi/sm5only";
    desc.preferredFeatureLevel = FeatureLevel::ES3_1;
    desc.allowNullFallback = false;
    desc.forcedModuleByCommandLine = true;
    desc.applyCommandLineOverrides = false;

    const RHISelectionResult result = selectDynamicRHIModule(desc, manager);
    EXPECT_FALSE(result.isValid());
    EXPECT_FALSE(manager.isModuleLoaded("rhi/sm5only"));
}

}   // namespace gp::rhi::tests
