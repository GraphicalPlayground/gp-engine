// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

/// @brief Defines a boolean false value for preprocessor conditions.
/// @see GP_TRUE
#define GP_FALSE (0)

/// @brief Defines a boolean true value for preprocessor conditions.
/// @see GP_FALSE
#define GP_TRUE (1)

/// @brief Defines an empty value for preprocessor conditions.
/// @see GP_EMPTY_FUNCTION
#define GP_EMPTY

/// @brief Defines an empty function for preprocessor conditions.
/// @see GP_EMPTY
#define GP_EMPTY_FUNCTION(...)

/// @brief Stringifies a token for preprocessor conditions.
/// @param[in] token The token to stringify.
/// @return The stringified token.
#define GP_STRINGIFY(token) GP_IMPL_STRINGIFY(token)
#define GP_IMPL_STRINGIFY(token) #token

/// @brief Concatenates two tokens for preprocessor conditions.
/// @param[in] tokenA The first token to concatenate.
/// @param[in] tokenB The second token to concatenate.
/// @return The concatenated tokens.
#define GP_JOIN(tokenA, tokenB) GP_IMPL_JOIN(tokenA, tokenB)
#define GP_IMPL_JOIN(tokenA, tokenB) tokenA##tokenB

/// @brief Concatenates the first token with the rest of the tokens for preprocessor conditions.
/// @param[in] token The first token to concatenate.
/// @param[in] __VA_ARGS__ The rest of the tokens to concatenate.
/// @return The concatenated tokens.
#define GP_JOIN_FIRST(token, ...) GP_IMPL_JOIN_FIRST(token, __VA_ARGS__)
#define GP_IMPL_JOIN_FIRST(token, ...) token##__VA_ARGS__

/// @brief Evaluates a condition and expands to the corresponding token for preprocessor conditions.
/// @param[in] condition The condition to evaluate. Accepts GP_TRUE, GP_FALSE, or any value that
///            expands to 1 or 0. The macro first expands condition, then dispatches on the
///            resulting integer token, so GP_TRUE (which expands to (1)) is NOT valid, use
///            values that resolve to the bare token 1 or 0.
/// @param[in] tokenTrue The token to expand if the condition is true (1).
/// @param[in] tokenFalse The token to expand if the condition is false (0).
/// @return The token corresponding to the evaluated condition.
///
/// @note The two-level indirection (GP_IF → GP_IMPL_IF_DISPATCH → GP_JOIN) ensures that
///       macro arguments are fully expanded before the token-paste occurs, which is
///       required for the dispatch to work with named boolean macros.
#define GP_IF(condition, tokenTrue, tokenFalse) GP_IMPL_IF_DISPATCH(condition, tokenTrue, tokenFalse)
#define GP_IMPL_IF_DISPATCH(n, tokenTrue, tokenFalse) GP_JOIN(GP_IMPL_IF_, n)(tokenTrue, tokenFalse)
#define GP_IMPL_IF_1(tokenTrue, tokenFalse) tokenTrue
#define GP_IMPL_IF_0(tokenTrue, tokenFalse) tokenFalse

/// @brief Generates a comma-separated list of tokens for preprocessor conditions.
/// @param[in] first The first token in the list.
/// @param[in] __VA_ARGS__ The rest of the tokens in the list.
/// @return A comma-separated list of the provided tokens.
#define GP_COMMA_SEPARATED(first, ...) first, __VA_ARGS__

/// @brief Generates a source location string for preprocessor conditions.
/// @return A string in the format "filename:line" representing the current source location.
#define GP_SOURCE_LOCATION __FILE__ ":" GP_STRINGIFY(__LINE__)

// clang-format off
#if !defined(_MSVC_TRADITIONAL) || !_MSVC_TRADITIONAL
    /// @brief Appends the count of variadic arguments to a prefix for preprocessor conditions.
    /// @details Supports up to 26 variadic arguments. If no variadic arguments are provided, the count will be 0.
    /// @param[in] prefix The prefix to which the count will be appended.
    /// @param[in] __VA_ARGS__ The variadic arguments whose count will be appended to the prefix.
    /// @return The prefix concatenated with the count of the variadic arguments.
    #define GP_APPEND_VA_ARG_COUNT(prefix, ...) GP_IMPL_APPEND_VA_ARG_COUNT(prefix, ##__VA_ARGS__, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#else
    /// @brief Appends the count of variadic arguments to a prefix for preprocessor conditions.
    /// @details Supports up to 26 variadic arguments. If no variadic arguments are provided, the count will be 0.
    /// @note MSVC's traditional preprocessor doesn't handle the zero-argument case correctly, so we use a workaround.
    /// @param[in] prefix The prefix to which the count will be appended.
    /// @param[in] __VA_ARGS__ The variadic arguments whose count will be appended to the prefix.
    /// @return The prefix concatenated with the count of the variadic arguments.
    #define GP_APPEND_VA_ARG_COUNT(prefix, ...) GP_IMPL_APPEND_VA_ARG_COUNT_INVOKE(GP_IMPL_APPEND_VA_ARG_COUNT, (prefix, ##__VA_ARGS__, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))
    #define GP_IMPL_APPEND_VA_ARG_COUNT_INVOKE(macro, args) GP_IMPL_APPEND_VA_ARG_COUNT_EXPAND(macro##args)
    #define GP_IMPL_APPEND_VA_ARG_COUNT_EXPAND(arg) arg
#endif
#define GP_IMPL_APPEND_VA_ARG_COUNT(prefix,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z,count,...) prefix##count
// clang-format on
