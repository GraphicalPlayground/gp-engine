#!/usr/bin/env python3
"""Compiles a GLSL shader to SPIR-V and packages it as a 'GPVS' v1 bytecode file."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import platform
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

GPVS_MAGIC = 0x53565047  # 'GPVS' little-endian
GPVS_VERSION = 1

STAGE_BY_EXTENSION: dict[str, str] = {
    ".vert": "vert",
    ".vs": "vert",
    ".frag": "frag",
    ".fs": "frag",
    ".comp": "comp",
    ".cs": "comp",
    ".geom": "geom",
    ".gs": "geom",
    ".tesc": "tesc",
    ".tese": "tese",
    ".rgen": "rgen",
    ".rmiss": "rmiss",
    ".rchit": "rchit",
    ".rahit": "rahit",
    ".rint": "rint",
    ".rcall": "rcall",
    ".task": "task",
    ".mesh": "mesh",
}

DESCRIPTOR_TYPES: dict[str, int] = {
    "sampler": 0,
    "combined_image_sampler": 1,
    "sampled_image": 2,
    "storage_image": 3,
    "uniform_texel_buffer": 4,
    "storage_texel_buffer": 5,
    "uniform_buffer": 6,
    "storage_buffer": 7,
    "uniform_buffer_dynamic": 8,
    "storage_buffer_dynamic": 9,
    "input_attachment": 10,
}


def _version_sort_key(path: Path) -> tuple[tuple[int, int | str], ...]:
    parts: list[tuple[int, int | str]] = []
    for token in re.split(r"(\d+)", path.name):
        if not token:
            continue
        if token.isdigit():
            parts.append((1, int(token)))
        else:
            parts.append((0, token))
    return tuple(parts)


def _is_executable_file(path: Path) -> bool:
    return path.is_file() and (platform.system() == "Windows" or os.access(path, os.X_OK))


def find_sdk_tool(tool_name: str, explicit_path: str | None = None) -> str:
    if explicit_path:
        candidate = Path(explicit_path)
        if _is_executable_file(candidate):
            return str(candidate)
        raise FileNotFoundError(f"Specified {tool_name} path is not executable: {explicit_path}")

    found = shutil.which(tool_name)
    if found:
        return found

    system = platform.system()
    exe_name = f"{tool_name}.exe" if system == "Windows" else tool_name
    machine = platform.machine().lower()
    arch_dirs = {machine}
    if machine in ("amd64", "x86_64"):
        arch_dirs.add("x86_64")
    elif machine in ("arm64", "aarch64"):
        arch_dirs.update(("arm64", "aarch64"))

    candidates: list[Path] = []

    sdk_root = os.environ.get("VULKAN_SDK")
    if sdk_root:
        root = Path(sdk_root)
        candidates.extend(
            [
                root / "Bin" / exe_name,
                root / "bin" / exe_name,
                root / "macOS" / "bin" / exe_name,
                *(root / arch / "bin" / exe_name for arch in sorted(arch_dirs)),
            ]
        )

    if system == "Windows":
        sdk_bases = [Path(r"C:\VulkanSDK"), Path.home() / "VulkanSDK"]
        if local_app_data := os.environ.get("LOCALAPPDATA"):
            sdk_bases.append(Path(local_app_data) / "VulkanSDK")
        for base in sdk_bases:
            if base.is_dir():
                for version_dir in sorted(base.iterdir(), key=_version_sort_key, reverse=True):
                    if version_dir.is_dir():
                        candidates.append(version_dir / "Bin" / exe_name)
                        candidates.append(version_dir / "bin" / exe_name)
    elif system == "Darwin":
        base = Path.home() / "VulkanSDK"
        if base.is_dir():
            for version_dir in sorted(base.iterdir(), key=_version_sort_key, reverse=True):
                if version_dir.is_dir():
                    candidates.append(version_dir / "macOS" / "bin" / exe_name)
                    candidates.append(version_dir / "bin" / exe_name)
        candidates.extend(
            [
                Path("/opt/homebrew/bin") / exe_name,
                Path("/usr/local/bin") / exe_name,
            ]
        )
    else:  # Linux and other Unix-like systems
        for base in (Path.home() / "VulkanSDK", Path("/opt/vulkan-sdk")):
            if base.is_dir():
                for version_dir in sorted(base.iterdir(), key=_version_sort_key, reverse=True):
                    if version_dir.is_dir():
                        for arch in sorted(arch_dirs):
                            candidates.append(version_dir / arch / "bin" / exe_name)
                        candidates.append(version_dir / "bin" / exe_name)
        candidates.extend(
            [
                Path("/usr/local/bin") / exe_name,
                Path("/usr/bin") / exe_name,
            ]
        )

    for candidate in candidates:
        if _is_executable_file(candidate):
            return str(candidate)

    raise FileNotFoundError(
        f"Could not locate '{tool_name}'. Install the Vulkan SDK, add it to PATH, "
        f"set VULKAN_SDK, or pass --{tool_name}."
    )


def patch_spirv_entry_point(spirv_bytes: bytes, new_name: str) -> bytes:
    """Rewrites the first OpEntryPoint's name literal in a SPIR-V binary."""
    if len(spirv_bytes) % 4 != 0 or len(spirv_bytes) < 20:
        raise ValueError("Invalid SPIR-V binary length")

    words = list(struct.unpack(f"<{len(spirv_bytes) // 4}I", spirv_bytes))
    if words[0] != 0x07230203:
        raise ValueError("Invalid SPIR-V magic number")

    name_bytes = new_name.encode("utf-8") + b"\x00"
    while len(name_bytes) % 4 != 0:
        name_bytes += b"\x00"
    new_name_words = list(struct.unpack(f"<{len(name_bytes) // 4}I", name_bytes))

    idx = 5  # Skip 5-word SPIR-V header
    while idx < len(words):
        word0 = words[idx]
        opcode = word0 & 0xFFFF
        word_count = word0 >> 16
        if word_count <= 0:
            raise ValueError(f"Invalid SPIR-V instruction word count at word {idx}")
        if opcode == 15:  # OpEntryPoint
            # Layout: [word0, ExecutionModel, EntryPointId, NameString..., InterfaceIds...]
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


def build_gpvs(
    source_path: Path,
    stage: str,
    debug_name: str,
    bindings: list[tuple[int, str]],
    packed_globals_size: int,
    in_out_mask: int,
    glslc: str,
    spirv_val: str,
) -> bytes:
    with tempfile.TemporaryDirectory() as tmp_dir:
        spv_path = Path(tmp_dir) / f"{source_path.name}.spv"
        subprocess.run(
            [glslc, f"-fshader-stage={stage}", str(source_path), "-o", str(spv_path)],
            check=True,
        )
        raw_spirv = spv_path.read_bytes()

        # First pass with placeholder of exact 22-char length to compute final byte size, then compute CRC.
        placeholder = "main_00000000_00000000"
        sized_spirv = patch_spirv_entry_point(raw_spirv, placeholder)
        spirv_size = len(sized_spirv)
        spirv_crc = zlib.crc32(raw_spirv) & 0xFFFFFFFF
        entry_name = f"main_{spirv_size:08x}_{spirv_crc:08x}"
        final_spirv = patch_spirv_entry_point(raw_spirv, entry_name)
        if len(final_spirv) != spirv_size:
            raise RuntimeError("Patched SPIR-V size mismatch")

        patched_path = Path(tmp_dir) / f"{source_path.name}.patched.spv"
        patched_path.write_bytes(final_spirv)
        subprocess.run([spirv_val, str(patched_path)], check=True)

    digest = hashlib.sha256(source_path.read_bytes()).digest()
    hash_lo, hash_hi = struct.unpack("<QQ", digest[:16])

    out = bytearray()
    out.extend(struct.pack("<II", GPVS_MAGIC, GPVS_VERSION))

    # VulkanShaderHeader:
    out.extend(struct.pack("<I", len(bindings)))
    for desc_type, binding_name in bindings:
        out.extend(struct.pack("<I", desc_type))
        write_string(out, binding_name)
    out.extend(struct.pack("<I", 0))  # uniformBufferInfos count
    out.extend(struct.pack("<I", 0))  # inputAttachmentInfos count
    out.extend(struct.pack("<I", 0))  # globalSamplerInfos count
    out.extend(
        struct.pack(
            "<IIIIII",
            0,                   # numBoundUniformBuffers
            packed_globals_size, # packedGlobalsSize
            0,                   # inputAttachmentsMask
            in_out_mask,         # inOutMask
            0,                   # rayTracingPayloadType
            0,                   # rayTracingPayloadSize
        )
    )
    out.extend(struct.pack("<II", 0, 0))  # specialization constants & exclusions counts
    out.extend(struct.pack("<QQI", hash_lo, hash_hi, spirv_crc))
    out.extend(struct.pack("<BBBB", 0, 0, 0, 0))  # waveSize, usageFlags, rayGroupAnyHit, rayGroupIntersection
    write_string(out, debug_name)

    # ShaderResourceTable (empty: resourceTableBits + 8 empty arrays)
    out.extend(struct.pack("<9I", 0, 0, 0, 0, 0, 0, 0, 0, 0))

    # SPIR-V payload (sizeInBytes + words)
    if len(final_spirv) % 4 != 0:
        raise RuntimeError("SPIR-V byte size is not a multiple of 4")
    out.extend(struct.pack("<I", len(final_spirv)))
    out.extend(final_spirv)
    return bytes(out)


def parse_binding(spec: str) -> tuple[int, str]:
    if ":" not in spec:
        raise argparse.ArgumentTypeError(
            f"Invalid binding '{spec}'. Expected TYPE:NAME (e.g. uniform_buffer:Globals or 6:Globals)."
        )
    raw_type, name = spec.split(":", 1)
    if not name:
        raise argparse.ArgumentTypeError(f"Binding name cannot be empty in '{spec}'.")
    key = raw_type.strip().lower()
    if key in DESCRIPTOR_TYPES:
        return DESCRIPTOR_TYPES[key], name
    try:
        return int(raw_type, 0), name
    except ValueError as exc:
        valid = ", ".join(DESCRIPTOR_TYPES)
        raise argparse.ArgumentTypeError(
            f"Unknown descriptor type '{raw_type}'. Use an integer or one of: {valid}"
        ) from exc


def parse_int(value: str) -> int:
    try:
        return int(value, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"Invalid integer value: '{value}'") from exc


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compile a GLSL shader to SPIR-V and package it as a GPVS v1 bytecode file."
    )
    parser.add_argument("input", type=Path, help="Path to the input GLSL shader file.")
    parser.add_argument("output", type=Path, help="Path to the output .gpvs file.")
    parser.add_argument(
        "--stage",
        help="Shader stage for glslc (e.g. vert, frag, comp). Inferred from input extension if omitted.",
    )
    parser.add_argument(
        "--debug-name",
        help="Debug name stored in the GPVS header. Defaults to the input filename.",
    )
    parser.add_argument(
        "--binding",
        action="append",
        default=[],
        type=parse_binding,
        metavar="TYPE:NAME",
        help="Add a shader binding (repeatable), e.g. uniform_buffer:Globals or 6:Globals.",
    )
    parser.add_argument(
        "--packed-globals-size",
        type=parse_int,
        default=0,
        help="Packed globals size in bytes (default: 0).",
    )
    parser.add_argument(
        "--in-out-mask",
        type=parse_int,
        default=0,
        help="Shader input/output bitmask, decimal or hex (default: 0).",
    )
    parser.add_argument("--glslc", help="Explicit path to the glslc executable.")
    parser.add_argument("--spirv-val", help="Explicit path to the spirv-val executable.")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    input_path: Path = args.input.resolve()
    output_path: Path = args.output

    if not input_path.is_file():
        print(f"Error: input file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    stage = args.stage or STAGE_BY_EXTENSION.get(input_path.suffix.lower())
    if not stage:
        print(
            f"Error: could not infer shader stage from extension '{input_path.suffix}'. "
            "Pass --stage explicitly (e.g. --stage vert).",
            file=sys.stderr,
        )
        sys.exit(1)

    debug_name = args.debug_name or input_path.name
    glslc = find_sdk_tool("glslc", args.glslc)
    spirv_val = find_sdk_tool("spirv-val", args.spirv_val)

    gpvs_bytes = build_gpvs(
        source_path=input_path,
        stage=stage,
        debug_name=debug_name,
        bindings=args.binding,
        packed_globals_size=args.packed_globals_size,
        in_out_mask=args.in_out_mask,
        glslc=glslc,
        spirv_val=spirv_val,
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(gpvs_bytes)
    print(f"Compiled {input_path.name} -> {output_path} ({len(gpvs_bytes)} bytes)")


if __name__ == "__main__":
    main()
