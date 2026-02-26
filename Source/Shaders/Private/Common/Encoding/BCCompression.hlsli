/// \file    BCCompression.hlsli
/// \brief   Block Compression (BC1–BC7) decode helpers for runtime use.
///          These functions decode BC-format data stored in raw buffers — used when the
///          engine needs to read compressed data programmatically rather than through
///          hardware texture sampling.
///
/// \details Use cases:
///          - TextureCompress_BC*.CS.hlsl: GPU-side real-time texture compression.
///          - VirtualGeometry streaming: decoding compressed virtual texture pages.
///          - Learning/reference: understanding BC block layouts for custom formats.
///
///          Hardware texture sampling decodes BC automatically — these functions are for
///          cases where you need to access the raw block data or simulate BC decode in a CS.
///
///          Format coverage:
///          BC1 — RGB + 1-bit alpha (opaque or punch-through). 4 bpp.
///          BC3 — RGBA (BC1 RGB + 4-bit alpha). 8 bpp.
///          BC4 — Single channel (R). 4 bpp.
///          BC5 — Two channels (RG, normal maps). 8 bpp.
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_BC_COMPRESSION_HLSLI
#define GP_BC_COMPRESSION_HLSLI

// ─── BC1 (DXT1) ───────────────────────────────────────────────────────────────────────────────
// Each 4x4 block is 8 bytes: 2x RGB565 endpoints + 16x 2-bit indices.
// Endpoint order determines opaque vs alpha-test mode.

/// Decode a 16-bit RGB565 packed color to float3.
Float3 GP_DecodeRGB565(UInt32 packed)
{
    Float32 r = (Float32)((packed >> 11u) & 0x1Fu) / 31.0f;
    Float32 g = (Float32)((packed >>  5u) & 0x3Fu) / 63.0f;
    Float32 b = (Float32)((packed       ) & 0x1Fu) / 31.0f;
    return Float3(r, g, b);
}

/// Decode a BC1 block (8 bytes) and return the color at a given texel (tx, ty) in [0,3].
/// \param block8  Raw 8 bytes of BC1 block data packed into two UInt32s.
/// \param tx      Texel X within the block [0, 3].
/// \param ty      Texel Y within the block [0, 3].
Float4 GP_DecodeBC1Texel(UInt2 block8, UInt32 tx, UInt32 ty)
{
    UInt32 c0_565 = block8.x & 0xFFFFu;
    UInt32 c1_565 = (block8.x >> 16u) & 0xFFFFu;

    Float3 c0 = GP_DecodeRGB565(c0_565);
    Float3 c1 = GP_DecodeRGB565(c1_565);

    // Interpolated palette colors
    Float3 c2, c3;
    Float32 alpha3 = 1.0f;

    if (c0_565 > c1_565)
    {
        // Opaque mode: 4-color palette
        c2 = lerp(c0, c1, 1.0f / 3.0f);
        c3 = lerp(c0, c1, 2.0f / 3.0f);
    }
    else
    {
        // Alpha-test mode: 3-color + transparent
        c2     = lerp(c0, c1, 0.5f);
        c3     = Float3(0, 0, 0);
        alpha3 = 0.0f;
    }

    // Extract 2-bit index for the requested texel
    UInt32 indexData = block8.y;
    UInt32 bitOffset = (ty * 4u + tx) * 2u;
    UInt32 index     = (indexData >> bitOffset) & 3u;

    Float3 color;
    Float32 alpha = 1.0f;

    if      (index == 0u) { color = c0; }
    else if (index == 1u) { color = c1; }
    else if (index == 2u) { color = c2; }
    else                   { color = c3; alpha = alpha3; }

    return Float4(color, alpha);
}

// ─── BC4 (ATI1 / RGTC1) ──────────────────────────────────────────────────────────────────────
// Single-channel (red). 8 bytes per 4x4 block: 2x uint8 endpoints + 48 bits of 3-bit indices.
// Used for: height maps, AO maps, roughness-only textures.

/// Decode a single-channel value from a BC4 block at texel (tx, ty).
/// \param block8   Raw BC4 block: first byte = endpoint0, second = endpoint1,
///                 remaining 6 bytes = 3-bit indices for 16 texels.
Float32 GP_DecodeBC4Texel(UInt2 block8, UInt32 tx, UInt32 ty)
{
    Float32 ep0 = (Float32)( block8.x        & 0xFFu) / 255.0f;
    Float32 ep1 = (Float32)((block8.x >>  8u) & 0xFFu) / 255.0f;

    // 6-entry interpolated palette
    Float32 palette[8];
    palette[0] = ep0;
    palette[1] = ep1;

    if (ep0 > ep1)
    {
        // 8-value mode
        palette[2] = (6.0f * ep0 + 1.0f * ep1) / 7.0f;
        palette[3] = (5.0f * ep0 + 2.0f * ep1) / 7.0f;
        palette[4] = (4.0f * ep0 + 3.0f * ep1) / 7.0f;
        palette[5] = (3.0f * ep0 + 4.0f * ep1) / 7.0f;
        palette[6] = (2.0f * ep0 + 5.0f * ep1) / 7.0f;
        palette[7] = (1.0f * ep0 + 6.0f * ep1) / 7.0f;
    }
    else
    {
        // 6-value mode + 0.0 and 1.0 sentinels
        palette[2] = (4.0f * ep0 + 1.0f * ep1) / 5.0f;
        palette[3] = (3.0f * ep0 + 2.0f * ep1) / 5.0f;
        palette[4] = (2.0f * ep0 + 3.0f * ep1) / 5.0f;
        palette[5] = (1.0f * ep0 + 4.0f * ep1) / 5.0f;
        palette[6] = 0.0f;
        palette[7] = 1.0f;
    }

    // Index data is packed into the upper 3 bytes of block8.x and all of block8.y
    // 48 bits total for 16 x 3-bit indices
    UInt32 texelIndex = ty * 4u + tx;
    UInt32 bitStart   = texelIndex * 3u + 16u; // First 16 bits = endpoints

    UInt32 indexBits;
    if (bitStart < 32u)
        indexBits = (block8.x >> bitStart) & 7u;
    else
        indexBits = (block8.y >> (bitStart - 32u)) & 7u;

    return palette[indexBits & 7u];
}

// ─── BC5 (ATI2 / RGTC2) ──────────────────────────────────────────────────────────────────────
// Two BC4 blocks concatenated — Red + Green channels. 16 bytes per 4x4 block.
// Primary use: normal maps (stores XY, reconstruct Z = sqrt(1 - x^2 - y^2)).

/// Decode BC5 texel as float2 (RG) from two consecutive BC4 blocks.
/// \param blockR  First 8 bytes (BC4 block for Red channel).
/// \param blockG  Next 8 bytes (BC4 block for Green channel).
Float2 GP_DecodeBC5Texel(UInt2 blockR, UInt2 blockG, UInt32 tx, UInt32 ty)
{
    return Float2(GP_DecodeBC4Texel(blockR, tx, ty),
                  GP_DecodeBC4Texel(blockG, tx, ty));
}

/// Decode BC5 block and reconstruct a tangent-space normal.
Float3 GP_DecodeBC5Normal(UInt2 blockR, UInt2 blockG, UInt32 tx, UInt32 ty)
{
    Float2 rg = GP_DecodeBC5Texel(blockR, blockG, tx, ty) * 2.0f - 1.0f;
    return GP_DecodeNormalMapBC5(rg);
}

// ─── Block Address Utilities ──────────────────────────────────────────────────────────────────
// Compute byte offsets into a compressed texture buffer for manual block access.

/// Compute the byte offset of a BC1/BC4 block (4 bpp, 8 bytes per 4x4 block) in a raw buffer.
/// \param blockX    Block column index.
/// \param blockY    Block row index.
/// \param widthInBlocks  Texture width in blocks (= texWidth / 4, rounded up).
UInt32 GP_BC1BlockOffset(UInt32 blockX, UInt32 blockY, UInt32 widthInBlocks)
{
    return (blockY * widthInBlocks + blockX) * 8u;
}

/// Compute the byte offset of a BC3/BC5 block (8 bpp, 16 bytes per 4x4 block).
UInt32 GP_BC3BlockOffset(UInt32 blockX, UInt32 blockY, UInt32 widthInBlocks)
{
    return (blockY * widthInBlocks + blockX) * 16u;
}

/// Convert a texel coordinate to its containing block coordinate and local offset.
/// \param texelX     Texel column (absolute).
/// \param texelY     Texel row (absolute).
/// \param blockX     Output: block column.
/// \param blockY     Output: block row.
/// \param localX     Output: texel X within the block [0, 3].
/// \param localY     Output: texel Y within the block [0, 3].
void GP_TexelToBlock(UInt32 texelX, UInt32 texelY,
                     out UInt32 blockX, out UInt32 blockY,
                     out UInt32 localX, out UInt32 localY)
{
    blockX = texelX / 4u;
    blockY = texelY / 4u;
    localX = texelX % 4u;
    localY = texelY % 4u;
}

// ─── BCn Compression Error Metrics ───────────────────────────────────────────────────────────
// Used by TextureCompress_BC*.CS.hlsl to score candidate endpoint pairs.

/// Mean squared error between original float3 colors and a BC1 block's decoded palette.
/// Used to find optimal endpoints during GPU BC1 compression.
Float32 GP_BC1BlockMSE(Float3 original[16], Float3 c0, Float3 c1)
{
    Float3 palette[4];
    palette[0] = c0;
    palette[1] = c1;
    palette[2] = lerp(c0, c1, 1.0f / 3.0f);
    palette[3] = lerp(c0, c1, 2.0f / 3.0f);

    Float32 mse = 0.0f;

    GP_UNROLL
    for (Int32 i = 0; i < 16; ++i)
    {
        Float32 best = GP_LARGE_FLOAT;

        GP_UNROLL
        for (Int32 j = 0; j < 4; ++j)
        {
            Float3 diff = original[i] - palette[j];
            best = min(best, dot(diff, diff));
        }

        mse += best;
    }

    return mse / 16.0f;
}

#endif // GP_BC_COMPRESSION_HLSLI
