// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "container/BasicString.hpp"
#include "container/BasicStringView.hpp"
#include "container/Forward.hpp"
#include "CoreMinimal.hpp"
#include <iostream>

int main()
{
    std::cout << "Hello, Graphical Playground Editor!" << std::endl;
    // TODO: Implement editor main function.

    gp::String str = "Hello, World!";
    std::cout << "gp::String says: " << str << std::endl;

    return 0;
}
