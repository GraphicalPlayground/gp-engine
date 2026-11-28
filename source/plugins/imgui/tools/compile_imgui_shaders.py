#!/usr/bin/env python3
# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com
"""Compiles the plugin/imgui GLSL shaders to SPIR-V, packages them as 'GPVS' v1 blobs and embeds them in C++.

The output is `private/renderer/ImGuiShaderBytecode.inl`: two constexpr byte arrays plus their 128-bit content hashes.
Embedding the bytecode means the plugin has no content files to deploy or mount through the VFS. Linking the module
is all it takes to render ImGui.

Usage (requires the Vulkan SDK: glslc + spirv-val on PATH, in %VULKAN_SDK%/Bin, or under C:/VulkanSDK/<ver>/Bin):
    python source/plugins/imgui/tools/compile_imgui_shaders.py
"""

from pathlib import Path
import hashlib
import os
import shutil
import struct
import subprocess
import tempfile
import zlib

GPVS_MAGIC = 0x53565047  # 'GPVS' little-endian
GPVS_VERSION = 1

VK_DESCRIPTOR_TYPE_SAMPLER = 0
VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE = 2
VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6

COPYRIGHT = (
    "// Copyright (c) - Graphical Playground. All rights reserved.\n"
    "// For more information, see https://graphical-playground/legal\n"
    "// mailto:support AT graphical-playground DOT com\n"
)


def find_sdk_tool(tool_name: str) -> str:
    found = shutil.which(tool_name)
    if found:
        return found
    sdk_root = os.environ.get("VULKAN_SDK")
    if sdk_root:
        for sub in ("Bin", "bin"):
            for suffix in (".exe", ""):
                candidate = Path(sdk_root) / sub / f"{tool_name}{suffix}"
                if candidate.is_file():
                    return str(candidate)
    base = Path(r"C:\VulkanSDK")
    if base.is_dir():
        for version_dir in sorted(base.iterdir(), reverse=True):
            candidate = version_dir / "Bin" / f"{tool_name}.exe"
            if candidate.is_file():
                return str(candidate)
    raise FileNotFoundError(f"Could not locate {tool_name}")


def patch_spirv_entry_point(spirv_bytes: bytes, new_name: str) -> bytes:
    """Rewrites the first OpEntryPoint's name literal (the Vulkan RHI expects 'main_<size>_<crc>')."""
    words = list(struct.unpack(f"<{len(spirv_bytes) // 4}I", spirv_bytes))
    assert words[0] == 0x07230203, "Invalid SPIR-V magic number"

    name_bytes = new_name.encode("utf-8") + b"\x00"
    while len(name_bytes) % 4 != 0:
        name_bytes += b"\x00"
    new_name_words = list(struct.unpack(f"<{len(name_bytes) // 4}I", name_bytes))

    idx = 5
    while idx < len(words):
        word0 = words[idx]
        opcode = word0 & 0xFFFF
        word_count = word0 >> 16
        assert word_count > 0
        if opcode == 15:  # OpEntryPoint
            name_start = idx + 3
            name_end = name_start
            while name_end < idx + word_count:
                w = words[name_end]
                name_end += 1
                if (w & 0xFF) == 0 or (w & 0xFF00) == 0 or (w & 0xFF0000) == 0 or (w & 0xFF000000) == 0:
                    break
            old_name_word_count = name_end - name_start
            new_word_count = word_count - old_name_word_count + len(new_name_words)
            new_word0 = (new_word_count << 16) | opcode
            patched = words[:idx] + [new_word0, words[idx + 1], words[idx + 2]] + new_name_words + words[name_end:]
            return struct.pack(f"<{len(patched)}I", *patched)
        idx += word_count

    raise RuntimeError("OpEntryPoint not found in SPIR-V")


def write_string(buf: bytearray, text: str) -> None:
    encoded = text.encode("utf-8")
    buf.extend(struct.pack("<I", len(encoded)))
    buf.extend(encoded)


def build_gpvs(source_path, stage, debug_name, bindings, packed_globals_size, in_out_mask, glslc, spirv_val):
    with tempfile.TemporaryDirectory() as tmp_dir:
        spv_path = Path(tmp_dir) / f"{source_path.name}.spv"
        subprocess.run([glslc, "-O", f"-fshader-stage={stage}", str(source_path), "-o", str(spv_path)], check=True)
        raw_spirv = spv_path.read_bytes()

        sized_spirv = patch_spirv_entry_point(raw_spirv, "main_00000000_00000000")
        spirv_size = len(sized_spirv)
        spirv_crc = zlib.crc32(raw_spirv) & 0xFFFFFFFF
        final_spirv = patch_spirv_entry_point(raw_spirv, f"main_{spirv_size:08x}_{spirv_crc:08x}")
        assert len(final_spirv) == spirv_size

        patched_path = Path(tmp_dir) / f"{source_path.name}.patched.spv"
        patched_path.write_bytes(final_spirv)
        subprocess.run([spirv_val, str(patched_path)], check=True)

    digest = hashlib.sha256(source_path.read_bytes()).digest()
    hash_lo, hash_hi = struct.unpack("<QQ", digest[:16])

    out = bytearray()
    out.extend(struct.pack("<II", GPVS_MAGIC, GPVS_VERSION))
    out.extend(struct.pack("<I", len(bindings)))
    for desc_type, binding_name in bindings:
        out.extend(struct.pack("<I", desc_type))
        write_string(out, binding_name)
    out.extend(struct.pack("<I", 0))  # uniformBufferInfos
    out.extend(struct.pack("<I", 0))  # inputAttachmentInfos
    out.extend(struct.pack("<I", 0))  # globalSamplerInfos
    out.extend(struct.pack("<IIIIII", 0, packed_globals_size, 0, in_out_mask, 0, 0))
    out.extend(struct.pack("<II", 0, 0))  # specialization constants & exclusions
    out.extend(struct.pack("<QQI", hash_lo, hash_hi, spirv_crc))
    out.extend(struct.pack("<BBBB", 0, 0, 0, 0))
    write_string(out, debug_name)
    out.extend(struct.pack("<9I", 0, 0, 0, 0, 0, 0, 0, 0, 0))  # empty ShaderResourceTable
    assert len(final_spirv) % 4 == 0
    out.extend(struct.pack("<I", len(final_spirv)))
    out.extend(final_spirv)

    blob = bytes(out)
    blob_digest = hashlib.sha256(blob).digest()
    blob_lo, blob_hi = struct.unpack("<QQ", blob_digest[:16])
    return blob, blob_lo, blob_hi


def emit_array(name: str, blob: bytes) -> str:
    lines = [f"inline constexpr gp::UInt8 {name}[{len(blob)}u] = {{"]
    for offset in range(0, len(blob), 16):
        chunk = ", ".join(f"0x{b:02x}" for b in blob[offset:offset + 16])
        lines.append(f"    {chunk},")
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    plugin_dir = Path(__file__).resolve().parents[1]
    shader_dir = plugin_dir / "shaders"
    out_path = plugin_dir / "private" / "renderer" / "ImGuiShaderBytecode.inl"

    glslc = find_sdk_tool("glslc")
    spirv_val = find_sdk_tool("spirv-val")

    vs_blob, vs_lo, vs_hi = build_gpvs(
        shader_dir / "ImGui.vert", "vert", "ImGui.vert",
        [(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, "Globals")], 16, 0x7, glslc, spirv_val,
    )
    ps_blob, ps_lo, ps_hi = build_gpvs(
        shader_dir / "ImGui.frag", "frag", "ImGui.frag",
        [(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, "sTexture"), (VK_DESCRIPTOR_TYPE_SAMPLER, "sSampler")], 0, 0x1,
        glslc, spirv_val,
    )

    text = (
        COPYRIGHT
        + "\n"
        + "// AUTO-GENERATED by source/plugins/imgui/tools/compile_imgui_shaders.py - DO NOT EDIT BY HAND.\n"
        + "// Regenerate after modifying source/plugins/imgui/shaders/ImGui.vert or ImGui.frag.\n"
        + "\n#pragma once\n\n"
        + "#include \"CoreMinimal.hpp\"   // IWYU pragma: keep\n\n"
        + "namespace gp::imgui::detail\n{\n\n"
        + "/// @brief 'GPVS' v1 Vulkan vertex shader blob compiled from `shaders/ImGui.vert`.\n"
        + emit_array("kImGuiVertexShaderBytecode", vs_blob) + "\n\n"
        + "/// @brief Low 64 bits of the SHA-256 content hash of `kImGuiVertexShaderBytecode`.\n"
        + f"inline constexpr gp::UInt64 kImGuiVertexShaderHashLow = 0x{vs_lo:016x}ull;\n\n"
        + "/// @brief High 64 bits of the SHA-256 content hash of `kImGuiVertexShaderBytecode`.\n"
        + f"inline constexpr gp::UInt64 kImGuiVertexShaderHashHigh = 0x{vs_hi:016x}ull;\n\n"
        + "/// @brief 'GPVS' v1 Vulkan pixel shader blob compiled from `shaders/ImGui.frag`.\n"
        + emit_array("kImGuiPixelShaderBytecode", ps_blob) + "\n\n"
        + "/// @brief Low 64 bits of the SHA-256 content hash of `kImGuiPixelShaderBytecode`.\n"
        + f"inline constexpr gp::UInt64 kImGuiPixelShaderHashLow = 0x{ps_lo:016x}ull;\n\n"
        + "/// @brief High 64 bits of the SHA-256 content hash of `kImGuiPixelShaderBytecode`.\n"
        + f"inline constexpr gp::UInt64 kImGuiPixelShaderHashHigh = 0x{ps_hi:016x}ull;\n\n"
        + "}   // namespace gp::imgui::detail\n"
    )
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(text, encoding="utf-8", newline="\n")
    print(f"Wrote {out_path} (VS {len(vs_blob)} bytes, PS {len(ps_blob)} bytes)")


if __name__ == "__main__":
    main()
