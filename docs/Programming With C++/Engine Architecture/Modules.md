---
sidebar_position: 1
title: Modules
description: Modules are the building blocks of GP Engine's software architecture. You can organize your code into modules to create more efficient and maintainable projects.
tags:
    - c++
    - architecture
---
import FileTree from '@site/src/components/FileTree';

# Graphical Playground Engine Modules

<p style={{ color: '#ffffffa6' }}> Modules are the building blocks of GP Engine's software
architecture. You can organize your code into modules to create more efficient and maintainable
projects. </p>

**Modules** are the basic building block of Graphical Playground Engine's (GPE) software
architecture. These encapsulate specific editor tools, runtime features, libraries, or other
functionality in standalone units of code.

All projects and plugins have their own **primary module** by default, however, you can define other
modules outside of these to organize your code.

This page provides an overview of how modules are structured and how they can benefit your GP Engine
projects.

:::note GP Engine modules are not related to C++ 20 modules.
:::

## Benefits of Using Modules

Organizing your project with modules provides the following benefits:
- Modules enforce good code separation, providing a means to encapsulate functionality and hide
  internal parts of the code.
- Modules are compiled as separate compilation units.
  This means only modules that have changed will need to compile, and build times for larger
  projects will be significantly faster.
- Modules are linked together in a dependency graph and limit header includes to code that is
  actually used, per the [Include What You Use (IWYU)](./Include%20What%20You%20Use.md) standard.
  This means modules that are not used in your project will be safely excluded from compilation.
- You can control when specific modules are loaded and unloaded at runtime.
  This provides a way to optimize the performance of your project by managing which systems are
  available and active.
- Modules can be included or excluded from your project based on certain conditions, such as which
  platform the project is being compiled for.

In summary, if you observe best practices with modules, your project's code will be better
organized, will compile more efficiently, and will be more reusable than if you put all of your
project's code into a single module.

## Setting Up a Module

The following is an overview of how to build and implement a module from scratch.
If you follow these steps, you will create a gameplay module separate from the primary module that
your project includes by default.

1. Create a directory for your module at the top level of your project's Source folder.
   This directory should have the same name as your module.

:::tip You can place modules in any subdirectory within your Source folder, at any number of levels
deep. This makes it possible to use subdirectories to group modules.
:::

2. Create a `CMakeLists.txt` file inside your module's root directory and use it to define
   dependencies with other modules.
   This makes it possible for the CMake build system to discover your module.
3. Create Private and Public subfolders inside your module's root directory.
4. Create a `[ModuleName]Module.cpp` file in the Private subfolder for your module.
   Use this to provide methods for starting up and shutting down your module, as well as other
   common functions that GP Engine uses to manage modules.
5. List your module as a dependency in the `CMakeList.txt` file for any module that will need to use
   it.

:::warning TODO: Describe how to set up a module to be loaded at runtime instead of startup, and how
to set up a module to be included or excluded based on certain conditions.
:::

## Understanding the Structure of a Module

All modules should be placed in the Source directory for either a plugin or project.
The module's root folder should have the same name as the corresponding module.

There should also be a `CMakeLists.txt` file for each module in its root folder, and its C++ code
should be contained in **Private** and **Public** folders.

The following is an example of the recommended folder structure for a module:

<FileTree
  data={[
    {
      name: '[modulename]',
      type: 'folder',
      children: [
        {
          name: 'private',
          type: 'folder',
          children: [
            { name: '[ModuleName]Module.cpp', type: 'file' }
          ]
        },
        {
          name: 'internal',
          type: 'folder',
          children: [
            { name: '[ModuleName]Internal.hpp', type: 'file' }
          ]
        },
        {
          name: 'public',
          type: 'folder',
          children: [
            { name: '[ModuleName].hpp', type: 'file' },
            { name: '[ModuleName]Build.hpp', type: 'file' }
          ]
        },
        {
          name: 'docs',
          type: 'folder',
          children: []
        },
        {
          name: 'tests',
          type: 'folder',
          children: []
        },
        {
          name: 'benchmarks',
          type: 'folder',
          children: []
        },
        { name: 'CMakeLists.txt', type: 'file', highlight: true }
      ]
    }
  ]}
/>

### Configuring Dependencies in the CMakeLists.txt File

:::warning TODO: Add example of how to set up a module to be loaded at runtime instead of startup,
and how to set up a module to be included or excluded based on certain conditions.
:::

### Private and Public Dependencies

You should use the `PublicDependencyModuleNames` list if you use the classes from a module publicly,
such as in a public `.hpp` file.
This will make it possible for other modules that depend on your module to include your header files
without issues.

You should put a module's name in the `PrivateDependencyModuleNames` list if they are only used
privately, such as in `.cpp` files.
Private dependencies are preferred wherever possible, as they can reduce your project's compile
times.

:::tip You can make many dependencies private instead of public by using forward declarations in
your header files. :::

### Using the Private and Public Folders

If your module is a regular C++ module, its C++ files should be placed in the Private and Public
subfolders inside your module's root directory.

These do not have any relation to the `Private`, `Public`, or `Protected` access specifiers in your
C++ code. Instead, they control the availability of the module's code to other modules.
If you use these folders, all `.cpp` files should be placed in the Private folder.
Header (`.hpp`) files should be placed in the Private and Public folders per the guidelines below.

If you place a header file in the Private folder, its contents will not be exposed to any modules
outside its owning module.
Classes, structs, and enums in this folder will be accessible to other classes inside the same
module, but they will not be available to classes in other modules.

If you place a header in the Public folder, the Graphical Playground build system will expose its
contents to any other module that has a dependency on the current module.
Classes in outside modules will be able to extend classes contained in the Public folder, and you
will be able to create variables and references using classes, structs, and enums in the Public
folder as well. The `Private`, `Public`, and `Protected` specifiers will take effect as normal for
functions and variables.

If you are working on a module that will not be made a dependency for others, you do not need to use
the `Private` and `Public` folders.
Any code outside of these folders will behave as if it were Private.
A typical example of this would be your game's primary module, which will likely be at the end of
the dependency chain.

You can further organize your code by creating subfolders within the `Public` and `Private` folders.
For any new folder you create within `Public`, create a corresponding folder with the same name in
`Private`. Similarly, for any header file you place in `Public`, make sure its corresponding `.cpp`
files are always in the corresponding folder in `Private`.
