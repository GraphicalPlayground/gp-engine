// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platforms/base/Platform.hpp"   // IWYU pragma: keep
#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/system/SharedLibrary.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/system/SharedLibrary.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/macos/system/SharedLibrary.hpp"
#else
    #include "platforms/generic/system/SharedLibrary.hpp"
#endif

namespace gp::platform
{

#if GP_PLATFORM_WINDOWS
using SharedLibrary = gp::platform::windows::SharedLibrary;
#elif GP_PLATFORM_LINUX
using SharedLibrary = gp::platform::linux::SharedLibrary;
#elif GP_PLATFORM_MACOS
using SharedLibrary = gp::platform::macos::SharedLibrary;
#else
using SharedLibrary = gp::platform::generic::SharedLibrary;
#endif

}   // namespace gp::platform

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// @class gp::platform::SharedLibrary
/// @ingroup platform
///
/// `gp::platform::SharedLibrary` provides a unified, cross-platform interface for dynamically loading and managing
/// shared libraries.
///
/// By abstracting the underlying OS-specific APIs (such as `LoadLibrary` on Windows or `dlopen` on POSIX systems),
/// this class allows you to load `.dll`, `.so`, or `.dylib` files without writing platform-specific boilerplate.
/// The API seamlessly delegates to the correct backend based on your target platform at compile time.
///
/// It provides static methods to acquire library handles, retrieve exported function pointers, free handles, and
/// manipulate the library directory search paths.
///
/// Usage example:
/// @code {cpp}
/// // Temporarily add a custom directory to the search path
/// gp::platform::SharedLibrary::pushDirectory("plugins/rendering");
///
/// // Load a shared library
/// void* handle = gp::platform::SharedLibrary::getHandle("vulkan_renderer");
///
/// if (handle)
/// {
///     // Retrieve an exported function pointer
///     void* symbol = gp::platform::SharedLibrary::getExport(handle, "CreateRenderer");
///
///     if (symbol)
///     {
///         // Cast and call the exported function
///         using CreateRendererFunc = void*(*)();
///         auto createRenderer = reinterpret_cast<CreateRendererFunc>(symbol);
///         void* renderer = createRenderer();
///     }
///
///     // Unload the library when finished
///     gp::platform::SharedLibrary::freeHandle(handle);
/// }
///
/// // Clean up the search path
/// gp::platform::SharedLibrary::popDirectory("plugins/rendering");
/// @endcode
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
