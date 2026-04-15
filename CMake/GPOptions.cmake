# Copyright (c) - Graphical Playground. All rights reserved.

# Feature toggles
option(GP_ENABLE_RTTI             "Enable C++ Run-Time Type Information (RTTI)"            OFF)
option(GP_ENABLE_EXCEPTIONS       "Enable C++ Exception Handling"                          ON)
option(GP_ENABLE_LTO              "Enable Link-Time / Whole-Program Optimization"          ON)
option(GP_ENABLE_HARDENING        "Enable security-hardening flags"                        ON)
option(GP_WARNINGS_AS_ERRORS      "Treat all compiler warnings as errors"                  OFF)
option(GP_ENABLE_DOUBLE_PRECISION "Use double-precision floating point (where applicable)" OFF)

# Architecture
# OFF = safe distribution baseline (x86-64-v2 / ARMv8-A).
# ON  = optimize for THIS machine only. Never ship a GP_NATIVE_ARCH binary.
option(GP_NATIVE_ARCH "Optimize for the host CPU (-march=native)" OFF)

# Sanitizers (Debug builds only)
# ASAN + UBSAN may be combined. TSAN is mutually exclusive with both.
option(GP_ENABLE_ASAN  "Enable AddressSanitizer (Debug only)"           OFF)
option(GP_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (Debug only)" OFF)
option(GP_ENABLE_TSAN  "Enable ThreadSanitizer (Debug only)"            OFF)

# Benchmarks and Tests
option(GP_ENABLE_BENCHMARKS "Build benchmark targets" ON)
option(GP_ENABLE_TESTS      "Build test targets"      ON)

function(gp_apply_options)
    # Uses CMake's native IPO support so it works across MSVC (/GL+/LTCG),
    # GCC (-flto), and Clang (-flto=thin).
    if(GP_ENABLE_LTO)
        include(CheckIPOSupported)
        check_ipo_supported(RESULT _GP_IPO_OK OUTPUT _GP_IPO_MSG)
        if(_GP_IPO_OK)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE         ON PARENT_SCOPE)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO  ON PARENT_SCOPE)
            message(STATUS "[GP] LTO/IPO: enabled for Release / RelWithDebInfo.")
        else()
            message(WARNING "[GP] LTO/IPO: not supported on this toolchain: ${_GP_IPO_MSG}")
        endif()
    endif()

    if(GP_ENABLE_TSAN AND (GP_ENABLE_ASAN OR GP_ENABLE_UBSAN))
        message(FATAL_ERROR "[GP] ThreadSanitizer cannot be combined with AddressSanitizer or UndefinedBehaviorSanitizer.")
    endif()
endfunction()

function(gp_apply_compile_options)
#    # Compiler identification
#    set(_GP_MSVC  FALSE)
#    set(_GP_GCC   FALSE)
#    set(_GP_CLANG FALSE)
#
#    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
#        set(_GP_MSVC TRUE)
#    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#        set(_GP_GCC TRUE)
#    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
#        set(_GP_CLANG TRUE)
#    else()
#        message(WARNING "[GP] Unknown compiler '${CMAKE_CXX_COMPILER_ID}'. Compile flags may not be fully applied.")
#    endif()
#
#    message(STATUS "[GP] Applying compile options for ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
#
#    if(_GP_MSVC)
#        # CMake injects /W3 into CMAKE_CXX_FLAGS by default; strip it so our
#        # /W4 does not produce a "overriding /W3 with /W4" diagnostic.
#        foreach(_flag_var CMAKE_CXX_FLAGS CMAKE_C_FLAGS)
#            string(REGEX REPLACE "/W[0-4]" "" ${_flag_var} "${${_flag_var}}")
#            set(${_flag_var} "${${_flag_var}}" CACHE STRING "" FORCE)
#        endforeach()
#
#        # Standards Conformance
#        # These collectively bring MSVC to the same behaviour as GCC/Clang by
#        # default, which is essential for portable, standard-only C++23 code.
#        add_compile_options(
#            /permissive-         # Strict ISO C++: disables MSVC language extensions
#            /Zc:__cplusplus      # Report the real __cplusplus value, not 199711L
#            /Zc:preprocessor     # Conforming preprocessor (required for __VA_OPT__, etc.)
#            /Zc:inline           # Strip unreferenced COMDAT, smaller .obj, faster link
#            /Zc:throwingNew      # Assume operator new throws; removes post-alloc null checks
#            /Zc:referenceBinding # Disallow binding a temporary to a non-const lvalue ref
#            /Zc:rvalueCast       # Standard rules for explicit type-conversion expressions
#            /Zc:strictStrings    # Disallow deprecated char* = "literal" conversion
#            /Zc:ternary          # Standard conditional-operator type rules
#        )
#
#        # VS 2022 17.4+ (MSVC 19.34+), skip gracefully on older toolchains
#        if(MSVC_VERSION GREATER_EQUAL 1934)
#            add_compile_options(
#                /Zc:enumTypes       # Correct underlying type for unscoped enums (DR)
#                /Zc:templateScope   # Fix template-parameter shadowing (CWG DR 2667)
#            )
#        endif()
#
#        # Diagnostics & Code Generation
#        add_compile_options(
#            /diagnostics:caret  # Show ^ at the exact error token (not just column number)
#            /utf-8              # Source + execution charset = UTF-8 (no BOM surprises)
#            /bigobj             # Allow > 65 535 COMDAT sections (needed in template-heavy code)
#            /nologo             # Suppress the compiler banner in build logs
#            /MP                 # Parallel .cpp compilation (one cl.exe per core)
#            /GF                 # String pooling: merge identical literals into one read-only page
#            /Gy                 # COMDAT functions: required for /OPT:REF dead-function stripping
#        )
#
#        # Warnings
#        add_compile_options(
#            /W4                 # High-signal warning level (below the noise floor of /Wall)
#
#            # Opt-in to useful warnings that /W4 doesn't enable by default
#            /w44062             # switch on enum without default: missing case
#            /w44191             # unsafe function-pointer conversion
#            /w44242             # narrowing: int -> char (potential data loss)
#            /w44263             # member function doesn't override a base virtual
#            /w44265             # class has virtual functions but non-virtual destructor
#            /w44296             # expression is always true/false
#            /w44355             # 'this' used in base-member initializer list
#            /w44365             # signed/unsigned mismatch on assignment
#            /w44388             # signed/unsigned mismatch on comparison
#            /w44574             # identifier defined as '0': was a preprocessor directive intended?
#            /w44668             # symbol not defined as a preprocessor macro, replaced with '0'
#            /w44800             # implicit conversion to bool (performance warning)
#
#            # Suppress low-signal / unavoidable warnings
#            /wd4514             # unreferenced inline function removed (expected with templates)
#            /wd4820             # padding bytes added after struct member (unavoidable)
#            /wd5045             # Spectre mitigation inserted (informational, not actionable)
#            /wd4201             # Anonymous struct/union, intentional engine pattern (Vector swizzle unions)
#            /wd4324             # Structure padded due to alignas, expected for cache-line aligned types
#        )
#
#        if(GP_WARNINGS_AS_ERRORS)
#            add_compile_options(/WX)
#        endif()
#
#        # Exception Handling
#        if(GP_ENABLE_EXCEPTIONS)
#            # /EHsc: stack-unwinding for C++ exceptions + extern "C" won't throw.
#            # Omitting 'c' (/EHs only) would cause UB when a C callback throws.
#            add_compile_options(/EHsc)
#        else()
#            add_compile_options(/EHs-c-)
#            add_compile_definitions(_HAS_EXCEPTIONS=0)
#        endif()
#
#        # RTTI
#        if(NOT GP_ENABLE_RTTI)
#            # Disabling RTTI eliminates typeinfo/vtable metadata from every
#            # translation unit. Saves ~5-15% binary size in template-heavy code.
#            # dynamic_cast and typeid become ill-formed, replace with GP_CAST.
#            add_compile_options(/GR-)
#        endif()
#
#        # Security Hardening
#        if(GP_ENABLE_HARDENING)
#            add_compile_options(
#                /sdl            # SDL checks: superset of /GS + extra warnings (e.g. C26xxx)
#                /guard:cf       # Control Flow Guard: validates indirect call targets at runtime
#            )
#            add_link_options(/guard:cf) # CFG must be enabled at the linker as well
#        else()
#            # /GS- is only safe when not using /sdl; disabling saves a stack-probe
#            # per function but removes buffer-overrun canaries.
#            add_compile_options(/GS-)
#        endif()
#
#        # Debug
#        add_compile_options(
#            $<$<CONFIG:Debug>:/Od>      # Disable all optimization
#            $<$<CONFIG:Debug>:/Ob0>     # Disable inlining (inspect real call stacks)
#            # /RTC1 = /RTCs + /RTCu: stack-frame corruption + uninitialized local vars.
#            # Incompatible with any /O flag, keep Debug-only.
#            $<$<CONFIG:Debug>:/RTC1>
#            $<$<CONFIG:Debug>:/JMC>     # Just My Code: F10/F11 skip CRT/STL internals
#            $<$<CONFIG:Debug>:/Zi>      # Full PDB with edit-and-continue support
#            $<$<CONFIG:Debug>:/DDEBUG>
#            $<$<CONFIG:Debug>:/D_DEBUG>
#        )
#        add_link_options(
#            $<$<CONFIG:Debug>:/DEBUG:FULL>  # Embed full symbol info in PDB
#            $<$<CONFIG:Debug>:/INCREMENTAL> # Incremental link for fast iteration
#        )
#
#        # Release
#        # /Ob3 (VS 2019+): more aggressive inlining than /Ob2's heuristics.
#        # /Oi: replace library calls with intrinsic equivalents (e.g. memset → rep stosd).
#        # /Ot: when size vs speed is ambiguous, choose speed.
#        # /fp:fast: allows reassociation, FMA fusion, ignores signed-zero / NaN semantics.
#        #           Disable per-file for any code requiring strict IEEE 754.
#        add_compile_options(
#            $<$<CONFIG:Release>:/O2>
#            $<$<CONFIG:Release>:/Ob3>
#            $<$<CONFIG:Release>:/Oi>
#            $<$<CONFIG:Release>:/Ot>
#            $<$<CONFIG:Release>:/fp:fast>
#            $<$<CONFIG:Release>:/GS->   # Redundant with /guard:cf; remove the probe overhead
#            $<$<CONFIG:Release>:/DNDEBUG>
#        )
#        add_link_options(
#            # /OPT:REF removes unreferenced COMDATs (requires /Gy at compile time).
#            # /OPT:ICF folds identical COMDATs (safe for non-address-sensitive code).
#            $<$<CONFIG:Release>:/OPT:REF>
#            $<$<CONFIG:Release>:/OPT:ICF>
#            $<$<CONFIG:Release>:/INCREMENTAL:NO> # Full link: more optimal binary
#        )
#
#        # RelWithDebInfo
#        add_compile_options(
#            $<$<CONFIG:RelWithDebInfo>:/O2>
#            $<$<CONFIG:RelWithDebInfo>:/Ob2>
#            $<$<CONFIG:RelWithDebInfo>:/Zi>
#            $<$<CONFIG:RelWithDebInfo>:/fp:fast>
#            $<$<CONFIG:RelWithDebInfo>:/DNDEBUG>
#        )
#        add_link_options(
#            $<$<CONFIG:RelWithDebInfo>:/DEBUG:FULL>
#            $<$<CONFIG:RelWithDebInfo>:/OPT:REF>
#            $<$<CONFIG:RelWithDebInfo>:/OPT:ICF>
#            $<$<CONFIG:RelWithDebInfo>:/INCREMENTAL:NO>
#        )
#
#        # MinSizeRel
#        add_compile_options(
#            $<$<CONFIG:MinSizeRel>:/O1>     # Optimize for size (trumps speed heuristics)
#            $<$<CONFIG:MinSizeRel>:/Os>     # Favour small code when equally fast
#            $<$<CONFIG:MinSizeRel>:/DNDEBUG>
#        )
#        add_link_options(
#            $<$<CONFIG:MinSizeRel>:/OPT:REF>
#            $<$<CONFIG:MinSizeRel>:/OPT:ICF>
#            $<$<CONFIG:MinSizeRel>:/INCREMENTAL:NO>
#        )
#
#        message(STATUS "[GP] MSVC flags applied.")
#
#    elseif(_GP_GCC OR _GP_CLANG)
#
#        # Warnings (shared GCC + Clang)
#        add_compile_options(
#            -Wall                           # ~40 high-signal warnings enabled by default
#            -Wextra                         # ~20 additional warnings (-W is an alias)
#            -Wpedantic                      # Strict ISO C++; reject non-standard extensions
#            -Wshadow                        # Local variable shadows outer scope / parameter
#            -Wnon-virtual-dtor              # Base class with virtuals but no virtual destructor
#            -Wcast-align                    # Cast increases required alignment (UB on strict archs)
#            -Woverloaded-virtual            # Derived member hides, not overrides, a base virtual
#            -Wconversion                    # Implicit narrowing (int→short, double→float, etc.)
#            -Wsign-conversion               # Implicit signed↔unsigned conversion
#            -Wnull-dereference              # Flow-sensitive null-pointer dereference analysis
#            -Wdouble-promotion              # float silently promoted to double in expressions
#            -Wformat=2                      # printf/scanf format-string correctness + security
#            -Wimplicit-fallthrough          # switch case falls through without [[fallthrough]]
#            -Wunused                        # Umbrella: unused variable/parameter/function/label
#            -Wundef                         # Undefined identifier evaluated in #if
#            -Wmissing-field-initializers    # Partial struct/aggregate initialisation
#            -Wuninitialized                 # Variable used before assignment
#            -Wvla                           # Variable-length arrays (UB-prone, heap-alloc instead)
#            -Wswitch-enum                   # switch on enum type without covering all enumerators
#        )
#
#        if(GP_WARNINGS_AS_ERRORS)
#            add_compile_options(-Werror)
#        endif()
#
#        # GCC-specific warnings
#        if(_GP_GCC)
#            add_compile_options(
#                # Code-quality
#                -Wmisleading-indentation    # Misleading if/for indentation (Heartbleed pattern)
#                -Wduplicated-cond           # Duplicate condition in if / else-if chain
#                -Wduplicated-branches       # Both branches of if/else are identical
#                -Wlogical-op                # Suspicious logical operator (e.g. & vs &&)
#                -Wuseless-cast              # Cast to the same type as the expression
#
#                # Memory / overflow
#                -Wstringop-overflow        # Destination buffer overflowed by string op
#                -Wformat-overflow          # Overflow in sprintf/snprintf destination
#                -Wformat-truncation        # Output truncated by snprintf/strncat
#                -Walloca                   # Calls to alloca (stack-overflow risk)
#                -Warray-bounds             # Array index out of bounds
#            )
#        endif()
#
#        # Clang-specific warnings
#        if(_GP_CLANG)
#            add_compile_options(
#                -Wthread-safety             # Thread-safety annotation violations (TSan lite)
#                -Wloop-analysis             # Suspicious loop-variable mutations
#                -Wcomma                     # Comma operator in an unusual context
#                -Wconsumed                  # Incorrect use of [[clang::consumable]] types
#                -Wunreachable-code          # Statically unreachable code paths
#                -Wgnu-anonymous-struct      # Warn on anonymous structs (non-standard)
#            )
#        endif()
#
#        # Exception handling
#        if(NOT GP_ENABLE_EXCEPTIONS)
#            add_compile_options(-fno-exceptions)
#        endif()
#
#        # RTTI
#        if(NOT GP_ENABLE_RTTI)
#            add_compile_options(-fno-rtti)
#        endif()
#
#        # Symbol visibility
#        # All symbols hidden by default; only GP_API-decorated ones are exported.
#        # This halves dynamic-linker work on large shared libraries and matches
#        # MSVC's __declspec(dllexport) model.
#        add_compile_options(
#            -fvisibility=hidden
#            -fvisibility-inlines-hidden
#        )
#
#        # Dead-code stripping (compile side)
#        # Each function/datum lives in its own ELF section so the linker can
#        # remove unreferenced ones with --gc-sections / -dead_strip.
#        add_compile_options(
#            -ffunction-sections
#            -fdata-sections
#        )
#
#        # Architecture / CPU baseline
#        if(GP_NATIVE_ARCH)
#            add_compile_options(-march=native -mtune=native)
#            message(STATUS "[GP] Native CPU arch enabled. Do NOT distribute this binary.")
#        else()
#            if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x86-64")
#                # x86-64-v2: SSE4.2 + POPCNT, supported by every x86-64 CPU since ~2009.
#                # Safe default for distribution. Bump to x86-64-v3 (AVX2) for internal tools.
#                # Requires GCC 11+ / Clang 12+. Falls back silently on older toolchains.
#                add_compile_options(-march=x86-64-v2 -mtune=generic)
#            elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
#                # ARMv8-A+CRC+SIMD: baseline for Apple Silicon, modern Android, AWS Graviton.
#                add_compile_options(-march=armv8-a+crc+simd -mtune=generic)
#            endif()
#        endif()
#
#        # Security hardening
#        if(GP_ENABLE_HARDENING)
#            add_compile_options(
#                # Stack canary around functions with local arrays or pointer-taking locals.
#                # "strong" is the right balance: wider coverage than plain, cheaper than "all".
#                -fstack-protector-strong
#            )
#
#            if(_GP_GCC OR (_GP_CLANG AND NOT APPLE))
#                # Stack clash: probe each page when growing the stack past 4 KB.
#                # Prevents exploits that jump over the guard page.
#                add_compile_options(-fstack-clash-protection)
#
#                # Control-Flow Enforcement Technology (Intel CET / Shadow Stack).
#                # Hardware-enforced CFI on x86-64 Tiger Lake+ and later AMD.
#                if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
#                    add_compile_options(-fcf-protection=full)
#                endif()
#            endif()
#
#            # Linker hardening
#            if(WIN32)
#                add_link_options(
#                    -Wl,--dynamicbase   # ASLR: randomise base address at load time
#                    -Wl,--nxcompat      # DEP: mark stack/heap non-executable
#                    -Wl,--high-entropy-va # 64-bit ASLR (requires --dynamicbase)
#                )
#            elseif(UNIX AND NOT APPLE)
#                add_link_options(
#                    -Wl,-z,relro        # Relocations read-only after startup (partial RELRO)
#                    -Wl,-z,now          # Resolve all symbols at load (full RELRO, safe for engines)
#                    -Wl,-z,noexecstack  # Mark stack as non-executable
#                )
#            endif()
#        endif()
#
#        # Debug
#        add_compile_options(
#            $<$<CONFIG:Debug>:-O0>                          # No optimisation
#            # -g3 includes macro definitions in the debug info (useful with gdb/lldb).
#            $<$<CONFIG:Debug>:-g3>
#            $<$<CONFIG:Debug>:-fno-omit-frame-pointer>      # Keep RBP; required for perf/sanitizers
#            # Accurate call stacks: prevents the compiler from reusing a callee's
#            # stack frame for a tail-call (sibling call).
#            $<$<CONFIG:Debug>:-fno-optimize-sibling-calls>
#            $<$<CONFIG:Debug>:-DDEBUG>
#            $<$<CONFIG:Debug>:-D_DEBUG>
#        )
#
#        # libstdc++ (GCC) debug hardening, incompatible with Clang's libc++
#        if(_GP_GCC)
#            add_compile_options(
#                # Enables bounds checks on operator[], iterators and more.
#                # Significant overhead, keep Debug-only.
#                $<$<CONFIG:Debug>:-D_GLIBCXX_DEBUG>
#                $<$<CONFIG:Debug>:-D_GLIBCXX_DEBUG_PEDANTIC>
#                $<$<CONFIG:Debug>:-D_GLIBCXX_SANITIZE_VECTOR>
#            )
#        endif()
#
#        # libc++ (Clang) equivalent hardening
#        if(_GP_CLANG)
#            add_compile_options(
#                # _LIBCPP_HARDENING_MODE_DEBUG is the libc++ equivalent of _GLIBCXX_DEBUG.
#                $<$<CONFIG:Debug>:-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG>
#            )
#        endif()
#
#        # Sanitizers (Debug only)
#        if(GP_ENABLE_ASAN)
#            add_compile_options(
#                $<$<CONFIG:Debug>:-fsanitize=address>
#                # Detect use-after-return through stack variable escaping its scope.
#                $<$<CONFIG:Debug>:-fsanitize-address-use-after-scope>
#            )
#            add_link_options($<$<CONFIG:Debug>:-fsanitize=address>)
#            message(STATUS "[GP] ASan: enabled for Debug builds.")
#        endif()
#
#        if(GP_ENABLE_UBSAN)
#            add_compile_options(
#                $<$<CONFIG:Debug>:-fsanitize=undefined>
#                # Catch float ÷ 0 even when -ffast-math would silently hide it.
#                $<$<CONFIG:Debug>:-fsanitize=float-divide-by-zero>
#                # Abort on first violation rather than continuing with corrupt state.
#                $<$<CONFIG:Debug>:-fno-sanitize-recover=all>
#            )
#            # Clang-only: catch implicit integer conversions that change value.
#            if(_GP_CLANG)
#                add_compile_options($<$<CONFIG:Debug>:-fsanitize=implicit-conversion>)
#            endif()
#            add_link_options($<$<CONFIG:Debug>:-fsanitize=undefined>)
#            message(STATUS "[GP] UBSan: enabled for Debug builds.")
#        endif()
#
#        if(GP_ENABLE_TSAN)
#            add_compile_options($<$<CONFIG:Debug>:-fsanitize=thread>)
#            add_link_options($<$<CONFIG:Debug>:-fsanitize=thread>)
#            message(STATUS "[GP] TSan: enabled for Debug builds.")
#        endif()
#
#        # Release
#        add_compile_options(
#            $<$<CONFIG:Release>:-O3>                    # Maximum speed (enables auto-vectorization)
#            # -ffast-math implies: -fno-math-errno, -funsafe-math-optimizations,
#            # -ffinite-math-only, -fno-rounding-math, -fno-trapping-math.
#            # This enables SIMD vectorization, FMA fusion and loop re-association.
#            # WARNING: breaks NaN/Inf tests, signed-zero and strict IEEE 754 ordering.
#            # Use -fno-finite-math-only per-file if any code checks for NaN/Inf.
#            $<$<CONFIG:Release>:-ffast-math>
#            $<$<CONFIG:Release>:-funroll-loops>         # Unroll inner loops with known trip count
#            $<$<CONFIG:Release>:-fomit-frame-pointer>   # Reclaim RBP as a general register
#            $<$<CONFIG:Release>:-DNDEBUG>
#        )
#
#        # _FORTIFY_SOURCE=2: runtime checks for glibc string/memory functions.
#        # Requires at least -O1 to be effective; adding to Release and RWI only.
#        if(GP_ENABLE_HARDENING)
#            # Undefine first to silence redefinition warnings if system headers define it.
#            add_compile_options(
#                $<$<CONFIG:Release>:-U_FORTIFY_SOURCE>
#                $<$<CONFIG:Release>:-D_FORTIFY_SOURCE=3>    # Level 3 available on glibc 2.35+
#            )
#        endif()
#
#        # Dead-code stripping (linker side)
#        if(APPLE)
#            add_link_options(
#                $<$<CONFIG:Release>:-Wl,-dead_strip>
#            )
#        elseif(UNIX)
#            add_link_options(
#                $<$<CONFIG:Release>:-Wl,--gc-sections>  # Drop unreferenced ELF sections
#            )
#        endif()
#
#        # RelWithDebInfo
#        add_compile_options(
#            $<$<CONFIG:RelWithDebInfo>:-O2>
#            $<$<CONFIG:RelWithDebInfo>:-g>                      # DWARF debug info (no macros)
#            $<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer> # Preserve stacks for profilers
#            $<$<CONFIG:RelWithDebInfo>:-ffast-math>
#            $<$<CONFIG:RelWithDebInfo>:-DNDEBUG>
#        )
#        if(GP_ENABLE_HARDENING)
#            add_compile_options(
#                $<$<CONFIG:RelWithDebInfo>:-U_FORTIFY_SOURCE>
#                $<$<CONFIG:RelWithDebInfo>:-D_FORTIFY_SOURCE=3>
#            )
#        endif()
#        if(APPLE)
#            add_link_options($<$<CONFIG:RelWithDebInfo>:-Wl,-dead_strip>)
#        elseif(UNIX)
#            add_link_options($<$<CONFIG:RelWithDebInfo>:-Wl,--gc-sections>)
#        endif()
#
#        # MinSizeRel
#        add_compile_options(
#            $<$<CONFIG:MinSizeRel>:-Os>     # Optimize for size; implied by -O2 where cheaper
#            $<$<CONFIG:MinSizeRel>:-DNDEBUG>
#        )
#        if(APPLE)
#            add_link_options($<$<CONFIG:MinSizeRel>:-Wl,-dead_strip>)
#        elseif(UNIX)
#            add_link_options($<$<CONFIG:MinSizeRel>:-Wl,--gc-sections>)
#        endif()
#
#        message(STATUS "[GP] GCC/Clang flags applied.")
#    endif()
#
#    # Add compile definition GP_DEBUG, GP_RELEASE, etc. for use in the codebase.
#    foreach(_config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
#        string(TOUPPER ${_config} _config_upper)
#        add_compile_definitions($<$<CONFIG:${_config}>:GP_${_config_upper}>)
#    endforeach()
endfunction()
