// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "container/BasicString.hpp"
#include "container/Forward.hpp"
#include "errors/Error.hpp"
#include "errors/ErrorConfig.hpp"
#include "errors/ErrorRegistry.hpp"
#include "errors/ErrorSystem.hpp"
#include "platform/PlatformMemory.hpp"
#include <iostream>

void tryAllocation()
{
    unsigned char buffer[16];

    gp::memory::PlatformMemory::setMemory(buffer, 0xAB, sizeof(buffer));

    for (size_t i = 0; i < sizeof(buffer); ++i)
    {
        if (buffer[i] != 0xAB)
        {
            std::cout << "memset failed at byte " << i << std::endl;
            return;
        }
    }

    std::cout << "memset works!" << std::endl;
}

int main(int argc, char* argv[])
{
    std::cout << "Hello, Graphical Playground Editor!" << std::endl;
    // TODO: Implement editor main function.

    gp::String str = "Hello, World!";
    std::cout << "gp::String says: " << str << std::endl;

    tryAllocation();

    auto& registry = gp::error::ErrorRegistry::instance();
    std::cout << registry.dumpAll() << std::endl;

    gp::error::ErrorSystemConfig config = gp::error::ErrorSystemConfig::getDevelopmentConfig();
    config.abort.abortFrom = gp::error::Severity::Critical;
    gp::error::ErrorSystem::initialize(config);

    gp::Vector<gp::StringView> args;
    for (int i = 0; i < argc; ++i)
    {
        args.pushBack(argv[i]);
    }

    for (const auto& arg: args)
    {
        // clang-format off
        if (arg == "--trace") GP_TRACE("This is a trace message!");
        else if (arg == "--debug") GP_DEBUG("This is a debug message!");
        else if (arg == "--info") GP_INFO("This is an info message!");
        else if (arg == "--warn") GP_WARN("This is a warning message!");
        else if (arg == "--error") GP_ERROR("This is an error message!");
        else if (arg == "--fatal") GP_FATAL("This is a fatal message!");
        else if (arg == "--panic") GP_PANIC("This is a critical message!");
        // clang-format on
    }

    gp::error::ErrorSystem::flushAll();
    gp::error::ErrorSystem::shutdown();

    return 0;
}
