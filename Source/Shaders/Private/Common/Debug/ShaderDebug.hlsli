/// \file    ShaderDebug.hlsli
/// \brief   GPU-driven debug output system. Allows shaders to write structured debug data
///          to a readback buffer, print text overlays, and draw debug primitives —
///          all from within shader code, without stopping the GPU pipeline.
///
/// \details This file is only compiled in Debug and Development builds (not GP_BUILD_SHIPPING).
///          In Release, all GP_DEBUG_* macros expand to nothing.
///
///          Three output channels:
///          1. GP_DEBUG_PRINT  — Printf-style text written to a GPU ring buffer,
///                               consumed by the CPU and forwarded to the log system.
///          2. GP_DEBUG_VALUE  — Write an arbitrary float4 to a structured UAV at a
///                               pixel coordinate for overlay visualization.
///          3. GP_DEBUG_ASSERT — Writes a structured error record when a condition fails.
///                               The CPU checks this buffer after every frame in debug mode.
///
///          The debug UAV (g_DebugBuffer) is bound at a well-known u0, space10 in all
///          pipelines during debug builds. The CPU-side DebugRenderer reads it out.
///
/// \warning Never include this file directly — Common.hlsli wraps it in #ifndef GP_BUILD_SHIPPING.
/// \warning Never write to the debug buffer from a hot path in performance captures.
///          Disable with GP_DEBUG_ENABLED = 0 in shader compile flags if needed.

#ifndef GP_SHADER_DEBUG_HLSLI
#define GP_SHADER_DEBUG_HLSLI

// ─── Debug Buffer Layout ──────────────────────────────────────────────────────────────────────
// A single RWByteAddressBuffer is used for all debug output. The first 4 bytes are an atomic
// counter (current write position). Each record is 32 bytes.
//
// Record layout (32 bytes):
//   [0..3]  : UInt32 — Record type (GP_DEBUG_RECORD_*)
//   [4..7]  : UInt32 — Source info (encoded shader stage + pixel/thread coord)
//   [8..23] : Float4 — Payload (value, color, or encoded string ID)
//   [24..27]: UInt32 — Condition / line number
//   [28..31]: UInt32 — Frame index (for temporal filtering)

static const UInt32 GP_DEBUG_RECORD_VALUE   = 1u;
static const UInt32 GP_DEBUG_RECORD_ASSERT  = 2u;
static const UInt32 GP_DEBUG_RECORD_COUNTER = 3u;

RWByteAddressBuffer g_DebugBuffer : register(u0, space10);

/// Global debug enable flag. Set to 0 per-frame by the CPU to silence the buffer if needed.
/// Stored in the first word after the counter.
static const UInt32 GP_DEBUG_MAX_RECORDS = 4096u;

// ─── Internal Helpers ─────────────────────────────────────────────────────────────────────────

/// Atomically allocate a debug record slot. Returns the byte offset for the record, or
/// GP_UINT_MAX if the buffer is full.
UInt32 GP_Debug_AllocRecord()
{
    UInt32 prevCount;
    g_DebugBuffer.InterlockedAdd(0u, 1u, prevCount);
    if (prevCount >= GP_DEBUG_MAX_RECORDS)
        return GP_UINT_MAX;
    // Records start at byte 16 (skip counter + 3 reserved words)
    return 16u + prevCount * 32u;
}

/// Write a 32-byte debug record to the buffer.
void GP_Debug_WriteRecord(UInt32 byteOffset, UInt32 type, UInt2 coord, Float4 payload, UInt32 extra)
{
    if (byteOffset == GP_UINT_MAX) return;
    g_DebugBuffer.Store (byteOffset +  0u, type);
    g_DebugBuffer.Store (byteOffset +  4u, coord.x | (coord.y << 16u));
    g_DebugBuffer.Store4(byteOffset +  8u, asuint(payload));
    g_DebugBuffer.Store (byteOffset + 24u, extra);
    g_DebugBuffer.Store (byteOffset + 28u, GP_FRAME_INDEX);
}

// ─── Public Debug API ─────────────────────────────────────────────────────────────────────────

/// Write a Float4 value to the debug buffer, tagged with a pixel coordinate.
/// The CPU-side overlay renderer draws this as a color overlay at (px, py).
/// \param px, py   Pixel coordinate (screen space).
/// \param value    Value to record (can be any float4 — color, vector, scalars).
void GP_DebugValue(UInt32 px, UInt32 py, Float4 value)
{
    UInt32 offset = GP_Debug_AllocRecord();
    GP_Debug_WriteRecord(offset, GP_DEBUG_RECORD_VALUE, UInt2(px, py), value, 0u);
}

/// Overload: write a Float3 (padded to float4 with w=0).
void GP_DebugValue3(UInt32 px, UInt32 py, Float3 value)
{
    GP_DebugValue(px, py, Float4(value, 0.0f));
}

/// Overload: write a single float.
void GP_DebugValue1(UInt32 px, UInt32 py, Float32 value)
{
    GP_DebugValue(px, py, Float4(value, 0.0f, 0.0f, 0.0f));
}

/// Assert a condition from shader code. If false, writes an error record with the line number.
/// The CPU reads this after the frame and logs a fatal assert with file/line info.
/// \param condition   The condition to check.
/// \param line        __LINE__ — pass the source line for diagnostics.
void GP_DebugAssert(bool condition, UInt32 line)
{
    if (!condition)
    {
        UInt32 offset = GP_Debug_AllocRecord();
        GP_Debug_WriteRecord(offset, GP_DEBUG_RECORD_ASSERT, UInt2(0u, 0u),
                             Float4(0, 0, 0, 0), line);
    }
}

/// Increment a named counter (identified by an integer ID). Used for counting
/// events like: culled instances, overdraw hits, cache misses.
void GP_DebugCounter(UInt32 counterID)
{
    UInt32 offset = GP_Debug_AllocRecord();
    GP_Debug_WriteRecord(offset, GP_DEBUG_RECORD_COUNTER, UInt2(0u, 0u),
                         Float4(0,0,0,0), counterID);
}

// ─── Convenience Macros ───────────────────────────────────────────────────────────────────────

/// Write a debug value only for the center pixel of the screen (focus pixel debugging).
/// Useful for isolating a single pixel's execution path in a compute shader.
#define GP_DEBUG_CENTER_PIXEL(value4) \
    if ((UInt2)GP_RENDER_RES / 2u == GP_THREAD_ID.xy) \
        GP_DebugValue(GP_RENDER_RES.x / 2u, GP_RENDER_RES.y / 2u, (value4))

/// Shader-side assert with line number captured automatically.
#define GP_SHADER_ASSERT(cond)  GP_DebugAssert((cond), (UInt32)(__LINE__))

/// Validate a normal is unit length (within tolerance). Fails silently in non-debug builds.
#define GP_DEBUG_VALIDATE_NORMAL(n) \
    GP_SHADER_ASSERT(abs(length(n) - 1.0f) < 0.01f)

/// Validate a UV is in [0,1] range.
#define GP_DEBUG_VALIDATE_UV(uv) \
    GP_SHADER_ASSERT(all((uv) >= 0.0f) && all((uv) <= 1.0f))

// ─── Wave Debug Utilities ─────────────────────────────────────────────────────────────────────
// Useful for diagnosing wave occupancy, divergence, and helper lane behavior.

#if GP_HAS_WAVE_INTRINSICS

/// Return the count of active (non-helper) lanes in the current wave.
UInt32 GP_WaveActiveLaneCount()
{
    return countbits(WaveActiveBallot(true).x);
}

/// True if this is the first active lane in the wave (useful for once-per-wave writes).
bool GP_IsFirstActiveLane()
{
    return WaveIsFirstLane();
}

/// Broadcast a float4 from lane 0 to all lanes.
Float4 GP_WaveBroadcast(Float4 value)
{
    return WaveReadLaneFirst(value);
}

/// Write wave lane count to debug buffer from lane 0 only (avoids N duplicate records).
void GP_DebugWaveOccupancy(UInt32 px, UInt32 py)
{
    if (WaveIsFirstLane())
    {
        UInt32 laneCount = WaveGetLaneCount();
        UInt32 activeLanes = countbits(WaveActiveBallot(true).x);
        GP_DebugValue(px, py, Float4((Float32)activeLanes / (Float32)laneCount,
                                     (Float32)WaveGetLaneIndex(), 0, 0));
    }
}

#endif // GP_HAS_WAVE_INTRINSICS

#endif // GP_SHADER_DEBUG_HLSLI
