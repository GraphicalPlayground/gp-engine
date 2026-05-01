// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "container/BasicString.hpp"
#include "container/Forward.hpp"
#include "errors/Error.hpp"
#include "errors/ErrorConfig.hpp"
#include "errors/ErrorRegistry.hpp"
#include "errors/ErrorSystem.hpp"
#include <iostream>

int main()
{
    std::cout << "Hello, Graphical Playground Editor!" << std::endl;
    // TODO: Implement editor main function.

    gp::String str = "Hello, World!";
    std::cout << "gp::String says: " << str << std::endl;

    auto& registry = gp::error::ErrorRegistry::instance();
    std::cout << registry.dumpAll() << std::endl;

    gp::error::ErrorSystemConfig config = gp::error::ErrorSystemConfig::getDevelopmentConfig();
    config.abort.abortFrom = gp::error::Severity::Critical;
    gp::error::ErrorSystem::initialize(config);

    GP_TRACE("This is a trace message!");
    GP_DEBUG("This is a debug message!");
    GP_INFO("This is an info message!");
    GP_WARN("This is a warning message!");
    GP_ERROR("This is an error message!");
    GP_FATAL("This is a fatal message!");
    GP_PANIC("This is a critical message!");

    gp::error::ErrorSystem::flushAll();
    gp::error::ErrorSystem::shutdown();

    return 0;
}
