# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

gpStartModule(rhi)
  gpAddPublicDependency(core)

  gpAddDynamicDependency(rhi/null)

  if (WIN32 AND GP_USE_D3D11)
    gpAddDynamicDependency(rhi/d3d11)
  endif()

  if (WIN32 AND GP_USE_D3D12)
    gpAddDynamicDependency(rhi/d3d12)
  endif()

  if ((UNIX OR WIN32) AND GP_USE_VULKAN)
    gpAddDynamicDependency(rhi/vulkan)
  endif()

  if ((WIN32 OR UNIX) AND GP_USE_OPENGL)
    gpAddDynamicDependency(rhi/opengl)
  endif()

  if (APPLE AND GP_USE_METAL)
    gpAddDynamicDependency(rhi/metal)
  endif()
gpEndModule()
