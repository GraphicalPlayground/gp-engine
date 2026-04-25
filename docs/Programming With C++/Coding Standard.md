---
sidebar_position: 1
title: Coding Standard
description: Write maintainable code by adhering to established standards and best practices.
tags:
    - coding standard
    - c++
---
# Coding Standard

<p style={{ color: '#ffffffa6' }}> Write maintainable code by adhering to established standards and
best practices. </p>

At Graphical Playground, we have a few simple coding standards and conventions.
This document reflects the state of Graphical Playground' current coding standards.
Following the coding standards is mandatory.

Code conventions are important to programmers for several reasons:

- 80% of the lifetime cost of a piece of software goes to maintenance.
- Hardly any software is maintained for its whole life by the original author.
- Code conventions improve the readability of software, allowing engineers to understand new code
  quickly and thoroughly.
- If we decide to expose source code to mod community developers, we want it to be easily
  understood.
- Many of these conventions are required for cross-compiler compatibility.

The coding standards below are C++-centric; however, the standard is expected to be followed no
matter which language is used.
A section may provide equivalent rules or exceptions for specific languages where it's applicable.

## Class Organization

**Classes** should be organized with the reader in mind rather than the writer.
Since most readers will use the public interface of the class, the public implementation should be
declared first, followed by the class's private implementation.

```cpp showLineNumbers
class EXAMPLEPROJECT_API ExampleClass : public BaseClass
{
public:
    // Public interface goes here
    ExampleClass();

protected:
    // Private implementation goes here
    virtual void protectedMethod();
};
```

## Copyright Notice

Any source file (`.hpp`, `.cpp`, `.xaml`) provided by Graphical Playground for public distribution
must contain a copyright notice as the first line in the file.
The format of the notice must exactly match that shown below:

```
// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com
```

## Naming Conventions

When using Naming Conventions, all code and comments should use U.S. English spelling and grammar.

- We use a combination of PascalCase for types and enumerations, and camelCase for variables,
  constants and methods to ensure a clear distinction between definitions and instances.
  - All type names, and enumerations must start with a capital letter.
    There are no underscores between words.
    To further distinguish types from instances, we use specific prefixes.
  - Variables, constants and methods use lowerCamelCase, where the first letter is lowercase and
    subsequent words are capitalized.
    - Local Variables: Standard camelCase (e.g., `health`, `lastMouseCoordinates`).
    - Private/Protected Fields: Use the `m_` prefix followed by camelCase to denote member variables
      (e.g., `m_health`, `m_deltaCoordinates`).
    - Global Variables: Use the `g_` prefix followed by camelCase to denote global variables (e.g.,
      `g_gameState`, `g_windowWidth`).
    - Constants: Use the `k` prefix followed by PascalCase to denote constants (e.g., `kMaxHealth`,
      `kDefaultWindowSize`).
    - Methods: Use camelCase without any prefix (e.g., `calculateDamage()`,
      `updatePlayerPosition()`).
- Type template parameters and nested type aliases based on those template parameters are not
  subject to the above prefix rules, as the type category is unknown.
- Prefer a Type suffix after a descriptive term.
- Disambiguate template parameters from aliases by using an In prefix:
```cpp showLineNumbers
template <typename InElementType>
class Container
{
public:
    using ElementType = InElementType;
};
```
- Type and variable names are nouns.
- Method names are verbs that either describe the method's effect, or the return value of a method
  without an effect.
- Macro names should be fully capitalized with words separated by underscores, and prefixed with
  `GP_`.
```cpp
#define GP_AUDIT_SPRITER_IMPORT
```

Variable, method, and class names should be:

- Clear
- Unambiguous
- Descriptive

The greater the scope of the name, the greater the importance of a good, descriptive name.
Avoid over-abbreviation.

All variables should be declared on their own line so that you can provide comment on the meaning of
each variable.

You can use multi-line or single-line comments before a variable Blank lines are optional for
grouping variables.

All functions that return a bool should ask a true/false question, such as `isVisible()` or
`shouldClearBuffer()`.

A procedure (a function with no return value) should use a strong verb followed by an Object.
An exception is, if the Object of the method is the Object it is in.
In this case, the Object is understood from context.
Names to avoid include those beginning with "Handle" and "Process" because the verbs are ambiguous.

We encourage you to prefix function parameter names with "Out" if:
- The function parameters are passed by reference.
- The function is expected to write to that value.

This makes it obvious that the value passed in this argument is replaced by the function.

Functions that return a value should describe the return value.
The name should make clear what value the function returns.
This is particularly important for boolean functions.
Consider the following two example methods:

```cpp showLineNumbers
// what does true mean?
bool checkTea(Tea Tea);

// name makes it clear true means tea is fresh
bool isTeaFresh(Tea Tea);

float teaWeight;
int32 teaCount;
bool doesTeaStink;
FName teaName;
FString teaFriendlyName;
UClass* teaClass;
USoundCue* teaSound;
UTexture* teaTexture;
```

## Inclusive Word Choice

When you work in the Graphical Playground codebase, we encourage you to strive to use respectful,
inclusive, and professional language.

Word choice applies when you:
- name classes.
- functions.
- data structures.
- types.
- variables.
- files and folders.
- plugins.

It applies when you write snippets of user-facing text for the UI, error messages, and
notifications. It also applies when writing about code, such as in comments and changelist
descriptions.

The following sections provide guidance and suggestions to help you choose words and names that are
respectful and appropriate for all situations and audiences, and be a more effective communicator.

### Racial, ethnic, and religious inclusiveness

- Do not use metaphors or similes that reinforce stereotypes.
  Examples include contrast black and white or *blacklist* and *whitelist*.
- Do not use words that refer to historical trauma or lived experience of discrimination.
  Examples include *slave*, *master*, and *nuke*.

### Gender inclusiveness

- Refer to hypothetical people as *they*, *them*, and *their*, even in the singular.
- Refer to anything that is not a person as *it* and *its*. For example, a module, plugin, function,
  client, server, or any other software or hardware component.
- Do not assign a gender to anything that doesn't have one.
- Do not use collective nouns like guys that assume gender.
- Avoid colloquial phrases that contain arbitrary genders, like "a poor man's X".

### Slang

- Remember that your words are being read by a global audience that may not share the same idioms
  and attitudes, and who might not understand the same cultural references.
- Avoid slang and colloquialisms, even if you think they are funny or harmless.
  These may be hard to understand for people whose first language is not English, and might not
  translate well.
- Do not use profanity.

### Overloaded Words

- Many terms that we use for their technical meanings also have other meanings outside of
  technology. Examples include *abort*, *execute*, or *native*. When you use words like these, always
  be precise and examine the context in which they appear.

## Portable C++ code

The `int` and `unsigned int` types vary in size across platforms.
They are guaranteed to be at least 32 bits in width and are acceptable in code when the integer
width is unimportant.
Explicitly-sized types are used in serialized or replicated formats.

Below is a list of common types:

- `bool` for boolean values (NEVER assume the size of bool).
  `BOOL` will not compile.
- `UInt8` for unsigned bytes (1 byte).
- `Int8` for signed bytes (1 byte).
- `UInt16` for unsigned shorts (2 bytes).
- `Int16` for signed shorts (2 bytes).
- `UInt32` for unsigned ints (4 bytes).
- `Int32` for signed ints (4 bytes).
- `UInt64` for unsigned quad words (8 bytes).
- `Int64` for signed quad words (8 bytes).
- `float`/`Float32` for single precision floating point (4 bytes).
- `double`/`Float64` for double precision floating point (8 bytes).

## Comments

Comments are communication and communication is vital.
The following sections detail some important things to keep in mind about comments (from Kernighan &
Pike The Practice of Programming).

### Guidelines

- Write self-documenting code.
  For example:
```cpp showLineNumbers
// Bad:
t = s + l - b;

// Good:
totalLeaves = smallLeaves + largeLeaves - smallAndLargeLeaves;
```
- Write useful comments.
  For example:
```cpp showLineNumbers
// Bad:
// increment Leaves
++Leaves;

// Good:
// we know there is another tea leaf
++Leaves;
```
- Do not over comment bad code, rewrite it instead.
  For example:
```cpp showLineNumbers
// Bad:
// total number of leaves is sum of
// small and large leaves less the
// number of leaves that are both
t = s + l - b;

// Good:
TotalLeaves = SmallLeaves + LargeLeaves - SmallAndLargeLeaves;
```
- Do not contradict the code.
  For example:
```cpp showLineNumbers
// Bad:
// never increment Leaves!
++Leaves;

// Good:
// we know there is another tea leaf
++Leaves;
```

### Const Correctness

Const is documentation as much as it is a compiler directive.
All code should strive to be const-correct.
This includes the following guidelines:
- Pass function arguments by const pointer or reference if those arguments are not intended to be
  modified by the function.
- Flag methods as const if they do not modify the object.
- Use const iteration over containers if the loop isn't intended to modify the container.

Const Example:
```cpp showLineNumbers
void someMutatingOperation(Thing& outResult, const gp::Array<Int32>& inArray)
{
    // inArray will not be modified here, but outResult probably will be
}

void Thing::someNonMutatingOperation() const
{
    // This code will not modify the Thing it is invoked on
}

gp::Array<gp::String> stringArray;
for (const gp::String& : stringArray)
{
    // The body of this loop will not modify stringArray
}
```

Const is also preferred for by-value function parameters and locals.
This tells the reader that the variable will not be modified in the body of the function, which
makes it easier to understand.
If you do this, make sure that the declaration and the definition match, as this can affect the
JavaDoc process.

```cpp showLineNumbers
void addSomeThings(const Int32 count);

void addSomeThings(const Int32 count)
{
    const Int32 countPlusOne = count + 1;
    // Neither count nor countPlusOne can be changed during the body of the function
}
```

One exception to this is pass-by-value parameters, which are moved into a container.
For more information, see the "Move semantics" section on this page.

Example:
```cpp showLineNumbers
void Blah::setMemberArray(gp::Array<gp::String> inNewArray)
{
    MemberArray = moveTemp(inNewArray);
}
```

Never use const on a return type.
This inhibits move semantics for complex types, and will give compile warnings for built-in types.
This rule only applies to the return type itself, not the target type of a pointer or reference
being returned.

Example:

```cpp showLineNumbers
// Bad - returning a const array
const gp::Array<gp::String> getSomeArray();

// Fine - returning a reference to a const array
const gp::Array<gp::String>& getSomeArray();

// Fine - returning a pointer to a const array
const gp::Array<gp::String>* getSomeArray();

// Bad - returning a const pointer to a const array
const gp::Array<gp::String>* const getSomeArray();
```

## Modern C++ Language Syntax

Graphical Playground is built to be massively portable to many C++ compilers, so we are careful to
use features that are compatible with the compilers we might be supporting.
Sometimes, features are so useful that we will wrap them in macros and use them pervasively.
However, we usually wait until all the compilers we support are up to the latest standard.

Graphical Playground compiles with a language version of C++23 by default and requires a minimum
version of C++20 to build.
We use many modern language features that are well-supported across modern compilers.
In some cases, we wrap usage of these features in preprocessor conditionals.
However, sometimes we decide to avoid certain language features entirely, for portability or other
reasons.

Unless specified below, as a modern C++ compiler feature we are supporting, you should not use
compiler-specific language features unless they are wrapped in preprocessor macros or conditionals
and used sparingly.

### Static Assert

The `static_assert` keyword is valid for use where you need a compile-time assertion.

### Override and Final

The `override` and `final` keywords are valid for use, and their use is strongly encouraged.
There might be many places where these have been omitted, but they will be fixed over time.

### Nullptr

You should use `nullptr` instead of the C-style `NULL` macro in all cases.

One exception to this is the use of `nullptr` in C++/CX builds (such as for Xbox One).
In this case, the use of `nullptr` is actually the managed null reference type.
It is mostly compatible with `nullptr` from native C++ except in its type and some template
instantiation contexts, and so you should use the `TYPE_OF_NULLPTR` macro instead of the more usual
`decltype(nullptr)` for compatibility.

### Auto

You shouldn't use `auto` in C++ code, except for the few exceptions listed below.
Always be explicit about the type you're initializing.
This means that the type must be plainly visible to the reader.

C++20's structured binding feature should also not be used, as it is effectively a variadic `auto`.

Acceptable use of `auto`:
- When you need to bind a lambda to a variable, as lambda types are not expressible in code.
- For iterator variables, but only where the iterator's type is verbose and would impair
  readability.
- In template code, where the type of an expression cannot easily be discerned.
  This is an advanced case.

It's very important that types are clearly visible to someone who is reading the code.
Even though some IDEs are able to infer the type, doing so relies on the code being in a compilable
state. It also won't assist users of merge/diff tools, or when viewing individual source files in
isolation, such as on GitHub.

If you're sure you are using `auto` in an acceptable way, always remember to correctly use `const`,
`&`, or `*` just like you would with the type name.
With `auto`, this will coerce the inferred type to be what you want.

### Captures and Return Types

Explicit captures should be used rather than automatic capture (`[&]` and `[=]`). This is important
for readability, maintainability, safety, and performance reasons, particularly when used with large
lambdas and deferred execution.

Explicit captures declare the author's intent; therefore, mistakes are caught during code review.
Incorrect captures can cause serious bugs and crashes, which are more likely to become problematic
as the code is maintained over time.
Here are some additional things to keep in mind about lambda captures:
- By-reference capture and by-value capture of pointers (including the `this` pointer) can cause
  data corruption and crashes if the execution of the lambda is deferred.
  Local and member variables should never be captured by reference for deferred lambdas.
- By-value capture can be a performance concern if it makes unnecessary copies for a non-deferred
  lambda.
- Accidentally captured pointers are invisible to the garbage collector.
  Automatic capture catches `this` implicitly if any member variables are referenced, even though
  `[=]` gives the impression of the lambda having its own copies of everything.
- Any deferred lambda use that does not follow these guidelines must have a comment explaining why
  the lambda capture is safe.

Explicit return types should be used for large lambdas or when you are returning the result of
another function call.
These should be considered in the same way as the `auto` keyword.

### Strongly-Typed Enums

Enumerated (Enum) classes are a replacement for old-style namespaced enums.
For example:

```cpp showLineNumbers
// Old enum
namespace Thing
{
    enum Type
    {
        Thing1,
        Thing2
    };
}

// New enum
enum class Thing : UInt8
{
    Thing1,
    Thing2
}
```

Enum classes used as flags can take advantage of the `GP_ENABLE_ENUM_BITWISE_OPERATIONS(EnumType)`
macro to automatically define all of the bitwise operators:

```cpp showLineNumbers
enum class Flags
{
    None = 0x00,
    Flag1 = 0x01,
    Flag2 = 0x02,
    Flag3 = 0x04
};

GP_ENABLE_ENUM_BITWISE_OPERATIONS(Flags)
```

The one exception to this is the use of flags in a truth context - this is a limitation of the
language. Instead, all enum flags should have an enumerator called `None` which is set to 0 for
comparisons:

```cpp showLineNumbers
// Old
if (flags & Flags::Flag1)

// New
if ((flags & Flags::Flag1) != Flags::None)
```

### Move Semantics

All of the main container types, `gp::Array`, `gp::Map`, `gp::Set`, `gp::String` have move
constructors and move assignment operators.
These are often used automatically when passing or returning these types by value.

Returning containers or strings by value can be beneficial for expressivity, without the usual cost
of temporary copies.
Rules around pass-by-value and use of `std::move` are still being established, but can already be
found in some optimized areas of the codebase.

### Default Member Initializers

Default member initializers can be used to define the defaults of a class inside the class itself:

```cpp showLineNumbers
class Thing
{
public:
    Int32 maximumNumberOfWidgets = 10;
    float widgetSize = 11.5f;
    String widgetName = "DefaultWidget";
    WidgetType widgetType = WidgetType::Standard;
};
```

Code written like this has the following benefits:
- It doesn't need to duplicate initializers across multiple constructors.
- It isn't possible to mix the initialization order and declaration order.
- The member type, property flags, and default values are all in one place.
  This helps readability and maintainability.

However, there are also some downsides:
- Any change to the defaults requires a rebuild of all dependent files.
- Headers can't change in patch releases of the engine, so this style can limit the kinds of fixes
  that are possible.
- Some things can't be initialized in this way, such as base classes, pointers to forward-declared
  types, values deduced from constructor arguments, and members initialized over multiple steps.
- Putting some initializers in the header and the rest in constructors in the .cpp file, can reduce
  readability and maintainability.

:::tip

Use your best judgment when deciding whether to use default member initializers.
As a rule of thumb, default member initializers make more sense with in-game code than engine code.
Consider using config files for default values.

:::

## Code Formatting

### Braces

Brace wars are foul.
Graphical Playground has a long standing usage pattern of putting braces on a new line.
Please adhere to this usage, regardless of the size of the function or block.
For example:

```cpp showLineNumbers
// Bad
Int32 getSize() const { return Size; }

// Good
Int32 getSize() const
{
    return Size;
}
```

Always include braces in single-statement blocks.
For example:

```cpp showLineNumbers
if (thing)
{
    return;
}
```

### If - Else

Each block of execution in an if-else statement should be in braces.
This helps prevent editing mistakes.
When braces are not used, someone could unwittingly add another line to an if block.
The extra line wouldn't be controlled by the if expression, which would be bad.
It's also bad when conditionally compiled items cause if/else statements to break.
So always use braces.

```cpp showLineNumbers
if (thingToCheck)
{
    doSomethingHere();
}
else
{
    doOtherThingHere();
}
```

A multi-way if statement should be indented with each `else if` indented the same amount as the
first `if`; this makes the structure clear to a reader:

```cpp showLineNumbers
if (thingToCheck)
{
    doSomethingHere();
}
else if (otherThingToCheck)
{
    doOtherThingHere();
}
else
{
    doDefaultThingHere();
}
```

### Tabs and Indenting

Below are some standards for indenting your code.

- Indent code by execution block.
- Use spaces for whitespace at the beginning of a line, not tabs.

### Switch Statements

Except for empty cases (multiple cases having identical code), switch case statements should
explicitly label that a case falls through to the next case.
Either include a break, or include a "falls through" comment in each case.
Other code control-transfer commands (return, continue, and so on) are fine as well.

Always have a default case.
Include a break just in case someone adds a new case after the default.

```cpp showLineNumbers
switch (condition)
{
    case 1:
        ...
        // falls through

    case 2:
        ...
        break;

    case 3:
        ...
        return;

    case 4:
    case 5:
        ...
        break;

    default:
        break;
}
```

## Namespaces

You can use namespaces to organize your classes, functions and variables where appropriate.
If you do use them, follow the rules below.

- `Using` declarations:
  - Do not put `using` declarations in the global scope, even in a `.cpp` file (it will cause
    problems with our "unity" build system.)
- It's okay to put `using` declarations within another namespace, or within a function body.
- If you put `using` declarations within a namespace, this will carry over to other occurrences of
  that namespace in the same translation unit.
  As long as you are consistent, it will be fine.
- You can only use `using` declarations in header files safely if you follow the above rules.
- Forward-declared types need to be declared within their respective namespace.
  - If you don't do this, you will get link errors.
- If you declare a lot of classes or types within a namespace, it can be difficult to use those
  types in other global-scoped classes (for example, function signatures will need to use explicit
  namespace when appearing in class declarations).
- You can use using declarations to only alias specific variables within a namespace into your
  scope.
  - For example, using `Foo::Bar`. However, we don't usually do that in GP code.
- Macros cannot live in a namespace.
  - They should be prefixed with `GP_` instead of living in a namespace, for example `GP_LOG`.

## Physical Dependencies

- File names should not be prefixed where possible.
  - For example, `Mat2.cpp` instead of `TMat2.cpp`. This makes it easy to use tools like Workspace
    Whiz or Visual Assist's Open File in Solution, by reducing the number of letters needed to
    identify the file you want.
- All headers should protect against multiple includes with the `#pragma once` directive.
  - Note that all compilers we use support `#pragma once`.
```cpp showLineNumbers
#pragma once
//<file contents>
```
- Try to minimize physical coupling.
  - In particular, avoid including standard library headers from other headers.
- Forward declarations are preferred to including headers.
- When including a header, be as fine grained as possible.
  - For example, do not include `Core.hpp`. Instead, you should include the specific headers in Core
    that you need definitions from.
- Try to include every header you need directly to make fine-grained inclusion easier.
- Don't rely on a header that is included indirectly by another header you include.
- Don't rely on anything being included through another header.
  Include everything you need.
- Modules have Private, Internal and Public source directories.
  - Any definitions that are needed by other modules must be in headers in the Public directory.
    Everything else should be in the Private directory.
    The Internal directory is for definitions that are needed by other modules, but aren't intended
    to be used by external users of the engine.
    This is a soft distinction, but it can be useful for organization and maintainability.
- Split large functions into logical sub-functions.
  - One area of compilers' optimizations is the elimination of common subexpressions.
    The larger your functions are, the more work the compiler has to do to identify them.
    This leads to greatly inflated build times.
- Don't use a large number of inline functions.
  - Inline functions force rebuilds even in files which don't use them.
    Inline functions should only be used for trivial accessors and when profiling shows there is a
    benefit to doing so.
- Be conservative in your use of `GP_FORCEINLINE`.
  - All code and local variables will be expanded out into the calling function.
    This will cause the same build time problems as those caused by large functions.

## Encapsulation

Enforce encapsulation with the protection keywords.
Class members should almost always be declared private unless they are part of the public/protected
interface to the class.
Use your best judgment, but always be aware that a lack of accessors makes it hard to refactor later
without breaking plugins and existing projects.

If particular fields are only intended to be usable by derived classes, make them private and
provide protected accessors.

Use final if your class is not designed to be derived from.

## General Style Issues

- Minimize dependency distance.
  - When code depends on a variable having a certain value, try to set that variable's value right
    before using it. Initializing a variable at the top of an execution block, and not using it for a
    hundred lines of code, gives lots of space for someone to accidentally change the value without
    realizing the dependency.
    Having it on the next line makes it clear why the variable is initialized the way it is and
    where it is used.
- Split methods into sub-methods where possible.
  - It is easier for someone to look at a big picture, and then drill down to the interesting
    details, than it is to start with the details and reconstruct the big picture from them.
    In the same way, it is easier to understand a simple method, that calls a sequence of several
    well-named sub-methods, than it is to understand an equivalent method that simply contains all
    the code in those sub-methods.
- In function declarations or function call sites, do not add a space between the function's name
  and the parentheses that precede the argument list.
- Address compiler warnings.
  - Compiler warning messages mean something is wrong.
    Fix what the compiler is warning you about.
    If you absolutely can't address it, use `#pragma` to suppress the warning, but this should only
    be done as a last resort.
- Leave a blank line at the end of the file.
  - All `.cpp` and `.hpp` files should include a blank line, to coordinate with gcc.
- Debug code should either be useful and polished, or not checked in.
  - Debug code that is intermixed with other code makes the other code harder to read.
- Avoid repeating the same operation redundantly in loops.
  - Move common subexpressions out of loops to avoid redundant calculations.
    Make use of statics in some cases, to avoid globally-redundant operations across function calls,
    such as constructing an `gp::Name` from a string literal.
- Be mindful of hot reload.
  - Minimize dependencies to cut down on iteration time.
    Don't use inlining or templates for functions which are likely to change over a reload.
    Only use statics for things which are expected to remain constant over a reload.
- Use intermediate variables to simplify complicated expressions.
  - If you have a complicated expression, it can be easier to understand if you split it into
    sub-expressions, that are assigned to intermediate variables, with names describing the meaning
    of the sub-expression within the parent expression.
    For example:
```cpp showLineNumbers
if ((Blah->BlahP->WindowExists->etc && stuff) &&
    !(playerExists && gameStarted && playerStillHasPawn &&
    IsTuesday())))
{
    doSomething();
}
```
Should be replaced with:

```cpp showLineNumbers
const bool isLegalWindow = Blah->BlahP->WindowExists->etc && stuff;
const bool isPlayerDead = playerExists && gameStarted && playerStillHasPawn && isTuesday();
if (isLegalWindow && !isPlayerDead)
{
    doSomething();
}
```
- Pointers and references should only have one space to the right of the pointer or reference.
  - This makes it easy to quickly use Find in Files for all pointers or references to a certain
    type. For example:
```cpp showLineNumbers
// Use this
ShaderType* ptr

// Do not use these:
ShaderType *ptr
ShaderType * ptr
```
- Shadowed variables are not allowed.
  - C++ allows variables to be shadowed from an outer scope, but this makes usage ambiguous to a
    reader. For example, there are three usable `count` variables in this member function:
```cpp showLineNumbers
class SomeClass
{
public:
    void func(const Int32 count)
    {
        for (Int32 count = 0; count != 10; ++count)
        {
            // Use Count
        }
    }

private:
    Int32 count;
}
```
- Avoid using anonymous literals in function calls.
  - Prefer named constants which describe their meaning.
    This makes intent more obvious to a casual reader as it avoids the need to look up the function
    declaration to understand it.
```cpp showLineNumbers
// Old style
trigger("Soldier", 5, true);

// New style
const gp::Name objectName             = "Soldier";
const float cooldownInSeconds         = 5;
const bool vulnerableDuringCooldown   = true;
trigger(objectName, cooldownInSeconds, vulnerableDuringCooldown);
```
- Avoid defining non-trivial static variables in headers.
  - Non-trivial static variables cause an instance to be compiled into in every translation unit
    that includes that header:
```cpp showLineNumbers
// SomeModule.h
static const gp::String usefulNamedString = "String";

// *Replace the above with:*

// SomeModule.h
extern SOMEMODULE_API const gp::String usefulNamedString;

// SomeModule.cpp
const gp::String usefulNamedString = "String";
```
- Avoid making extensive changes which do not change the code's behavior (for example: changing
  whitespace or mass renaming of private variables) as these cause unnecessary noise in source
  history and are disruptive when merging.
  - If such a change is important, for example fixing broken indentation caused by an automated
    merge tool, it should be submitted on its own and not mixed with behavioral changes.
  - Prefer to fix whitespace or other minor coding standard violations only when other edits are
    being made to the same lines or nearby code.

## API Design Guidelines

- Boolean function parameters should be avoided.
  - In particular, boolean parameters should be avoided for flags passed to functions.
    These have the same anonymous literal problem as mentioned previously, but they also tend to
    multiply over time as APIs get extended with more behavior.
    Instead, prefer an enum (see the advice on use of enums as flags in the
    [Strongly-Typed Enums](#strongly-typed-enums) section):
```cpp showLineNumbers
// Old style
Cup* makeCupOfTea(Tea* tea, bool addSugar = false, bool addMilk = false, bool addHoney = false, bool addLemon = false);
Cup* cup = makeCupOfTea(tea, false, true, true);

// New style
enum class TeaFlags
{
    None,
    Milk  = 0x01,
    Sugar = 0x02,
    Honey = 0x04,
    Lemon = 0x08
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(TeaFlags)

Cup* makeCupOfTea(FTea* tea, TeaFlags flags = TeaFlags::None);
Cup* cup = makeCupOfTea(tea, TeaFlags::Milk | TeaFlags::Honey);
```
- This form prevents the accidental transposing of flags, avoids accidental conversion from pointer
  and integer arguments, removes the need to repeat redundant defaults, and is more efficient.
- It is acceptable to use `bools` as arguments when they are the complete state to be passed to a
  function like a setter, such as void `Widget::setEnabled(bool enabled)`. Though consider
  refactoring if this changes.
- Avoid overly-long function parameter lists.
  - If a function takes many parameters then consider passing a dedicated struct instead:
```cpp showLineNumbers
// Old style
gp::UniquePtr<Cup[]> makeTeaForParty(const TeaFlags* teaPreferences, UInt32 numCupsToMake, Kettle* kettle, TeaType teaType = TeaType::EnglishBreakfast, float brewingTimeInSeconds = 120.0f);

// New style
struct TeaPartyParams
{
    const TeaFlags*  teaPreferences       = nullptr;
    UInt32           numCupsToMake        = 0;
    Kettle*          kettle               = nullptr;
    TeaType          teaType              = TeaType::EnglishBreakfast;
    float            brewingTimeInSeconds = 120.0f;
};
gp::UniquePtr<Cup[]> makeTeaForParty(const TeaPartyParams& params);
```
- Interface classes should always be abstract.
  - Interface classes are prefixed with "I" and must not have member variables.
    Interfaces are allowed to contain methods that are not pure-virtual, and can contain methods
    that are non-virtual or static, as long as they are implemented inline.
- Use the `virtual` and `override` keywords when declaring an overriding method.

When declaring a virtual function in a derived class that overrides a virtual function in the parent
class, you must use both the `virtual` and the `override` keywords.
For example:

```cpp showLineNumbers
class A
{
public:
    virtual void func() {}
};

class B : public A
{
public:
    virtual void func() override;
}
```

## Platform-Specific Code

Platform-specific code should always be abstracted and implemented in platform-specific source files
in appropriately named subdirectories, for example:

```
/source/runtime/hal/private/[PLATFORM]/PlatformMemory.cpp
```

In general, you should avoid adding any uses of `PLATFORM_[PLATFORM]`. For example, avoid adding
`PLATFORM_XBOXONE` to code outside of a directory named `[PLATFORM]`. Instead, extend the hardware
abstraction layer to add a static function, for example in FPlatformMisc:

```cpp showLineNumbers
GP_FORCEINLINE static Int32 getMaxPathLength()
{
    return 128;
}
```

Platforms can then override this function, returning either a platform-specific constant value or
even using platform APIs to determine the result.
If you force-inline the function it has the same performance characteristics as using a define.

In cases where a define is absolutely necessary, create new `#define` directives that describe
particular properties that can apply to a platform, for example `PLATFORM_USE_PTHREADS`. Set the
default value in `Platform.hpp` and override for any platforms which require it in the
platform-specific `Platform.hpp` file.

For example, in `Platform.hpp` we have:
```cpp showLineNumbers
#ifndef PLATFORM_USE_PTHREADS 
    #define PLATFORM_USE_PTHREADS 1
#endif
```

`WindowsPlatform.hpp` has:
```cpp showLineNumbers
#define PLATFORM_USE_PTHREADS 0
```

Cross-platform code can then use the define directly without needing to know the platform.

```cpp showLineNumbers
#if PLATFORM_USE_PTHREADS 
    #include "hal/PThreadRunnableThread.h"
#endif
```

We centralize the platform-specific details of the engine which allows details to be contained
entirely within platform-specific source files.
Doing so makes it easier to maintain the engine across multiple platforms, additionally you are able
to port code to new platforms without the need to scour the codebase for platform-specific defines.

Keeping platform code in platform-specific folders is also a requirement for NDA platforms such as
PlayStation, Xbox and Nintendo Switch.

It is important to ensure the code compiles and runs regardless of whether the `[PLATFORM]`
subdirectory is present.
In other words, cross-platform code should never be dependent on platform-specific code.
