// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/CryptoForward.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

#if GP_SIMD_SSE && GP_ARCHITECTURE_X86_FAMILY && defined(__AES__)
    #include <smmintrin.h>
    #include <wmmintrin.h>
    #define GP_MEOWHASH_AVAILABLE 1
#else
    #define GP_MEOWHASH_AVAILABLE 0
#endif

namespace GP::Crypto
{

/// \brief MeowHash - an extremely fast AES-NI based non-cryptographic hash function.
/// Only available on x86/x64 platforms with AES-NI support. Not constexpr.
/// Based on MeowHash by Casey Muratori (Molly Rocket).
/// \warning This hash is NOT available on ARM, WASM, or x86 without AES-NI.
/// Check `GP_MEOWHASH_AVAILABLE` before use.
struct MeowHash final : public Template::NonInstantiable
{
public:
    using HashType = UInt64;   //<! Primary 64-bit output.

public:
    /// \brief Check at compile time whether MeowHash is available on this platform.
    /// \return True if AES-NI is available.
    GP_NODISCARD static constexpr bool IsAvailable() noexcept { return GP_MEOWHASH_AVAILABLE != 0; }

#if GP_MEOWHASH_AVAILABLE
    /// \brief Compute 128-bit MeowHash for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed 64-bit seed value.
    /// \return 128-bit hash result.
    /// \note Not constexpr - uses AES-NI intrinsics.
    GP_NODISCARD static Hash128Result Hash128(const void* data, SizeT length, UInt64 seed = 0) noexcept
    {
        __m128i xmm0, xmm1, xmm2, xmm3;
        __m128i xmm4, xmm5, xmm6, xmm7;

        const auto* source = static_cast<const UInt8*>(data);
        __m128i seedVec = _mm_set_epi64x(0, static_cast<long long>(seed));

        xmm0 = seedVec;
        xmm1 = _mm_setzero_si128();
        xmm2 = _mm_setzero_si128();
        xmm3 = _mm_setzero_si128();
        xmm4 = _mm_setzero_si128();
        xmm5 = _mm_setzero_si128();
        xmm6 = _mm_setzero_si128();
        xmm7 = _mm_setzero_si128();

        SizeT remaining = length;
        SizeT offset = 0;

        while (remaining >= 128)
        {
            xmm0 = _mm_aesdec_si128(xmm0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset)));
            xmm1 = _mm_aesdec_si128(xmm1, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset + 16)));
            xmm2 = _mm_aesdec_si128(xmm2, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset + 32)));
            xmm3 = _mm_aesdec_si128(xmm3, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset + 48)));
            xmm4 = _mm_aesdec_si128(xmm4, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset + 64)));
            xmm5 = _mm_aesdec_si128(xmm5, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset + 80)));
            xmm6 = _mm_aesdec_si128(xmm6, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset + 96)));
            xmm7 = _mm_aesdec_si128(xmm7, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset + 112)));
            offset += 128;
            remaining -= 128;
        }

        if (remaining >= 64)
        {
            xmm0 = _mm_aesdec_si128(xmm0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset)));
            xmm1 = _mm_aesdec_si128(xmm1, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset + 16)));
            xmm2 = _mm_aesdec_si128(xmm2, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset + 32)));
            xmm3 = _mm_aesdec_si128(xmm3, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset + 48)));
            offset += 64;
            remaining -= 64;
        }

        if (remaining >= 32)
        {
            xmm0 = _mm_aesdec_si128(xmm0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset)));
            xmm1 = _mm_aesdec_si128(xmm1, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset + 16)));
            offset += 32;
            remaining -= 32;
        }

        if (remaining >= 16)
        {
            xmm0 = _mm_aesdec_si128(xmm0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset)));
            offset += 16;
            remaining -= 16;
        }

        if (remaining > 0)
        {
            UInt8 tail[16]{};
            for (SizeT i = 0; i < remaining; ++i) { tail[i] = source[offset + i]; }
            xmm0 = _mm_aesdec_si128(xmm0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(tail)));
        }

        __m128i lenVec = _mm_set_epi64x(0, static_cast<long long>(length));
        xmm0 = _mm_aesdec_si128(xmm0, lenVec);

        xmm0 = _mm_aesdec_si128(xmm0, xmm4);
        xmm1 = _mm_aesdec_si128(xmm1, xmm5);
        xmm2 = _mm_aesdec_si128(xmm2, xmm6);
        xmm3 = _mm_aesdec_si128(xmm3, xmm7);

        xmm0 = _mm_aesdec_si128(xmm0, xmm2);
        xmm1 = _mm_aesdec_si128(xmm1, xmm3);

        xmm0 = _mm_aesdec_si128(xmm0, xmm1);

        xmm0 = _mm_aesdec_si128(xmm0, xmm0);
        xmm0 = _mm_aesdec_si128(xmm0, xmm0);

        UInt64 lo = static_cast<UInt64>(_mm_extract_epi64(xmm0, 0));
        UInt64 hi = static_cast<UInt64>(_mm_extract_epi64(xmm0, 1));

        return { lo, hi };
    }

    /// \brief Compute 64-bit MeowHash for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed 64-bit seed value.
    /// \return 64-bit hash value.
    GP_NODISCARD static UInt64 Hash64(const void* data, SizeT length, UInt64 seed = 0) noexcept
    {
        return Hash128(data, length, seed).low;
    }

    /// \brief Compute 64-bit MeowHash for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return 64-bit hash value.
    GP_NODISCARD static UInt64 Hash64(const char* str) noexcept
    {
        return Hash64(str, std::char_traits<char>::length(str), 0);
    }

    /// \brief Compute 64-bit MeowHash from a string view.
    /// \param view String view to hash.
    /// \param seed 64-bit seed value.
    /// \return 64-bit hash value.
    GP_NODISCARD static UInt64 Hash64(std::string_view view, UInt64 seed = 0) noexcept
    {
        return Hash64(view.data(), view.size(), seed);
    }

    /// \brief Convenience entry point.
    /// \param data Pointer to bytes.
    /// \param length Number of bytes.
    /// \return 64-bit hash value.
    GP_NODISCARD static HashType Hash(const void* data, SizeT length) noexcept { return Hash64(data, length, 0); }

    /// \brief Convenience overload for null-terminated strings.
    /// \param str Null-terminated input string.
    /// \return 64-bit hash value.
    GP_NODISCARD static HashType Hash(const char* str) noexcept
    {
        return Hash(str, std::char_traits<char>::length(str));
    }

    /// \brief Convenience overload for string views.
    /// \param view String view to hash.
    /// \return 64-bit hash value.
    GP_NODISCARD static HashType Hash(std::string_view view) noexcept { return Hash(view.data(), view.size()); }
#endif   // GP_MEOWHASH_AVAILABLE
};

}   // namespace GP::Crypto
