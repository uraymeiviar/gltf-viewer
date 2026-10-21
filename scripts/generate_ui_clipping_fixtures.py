#!/usr/bin/env python3
"""Generate deterministic GLB inputs for flat-list and validation UI checks."""

import argparse
import json
import struct
from pathlib import Path


def write_glb(path: Path, document: dict, binary: bytes = b"") -> None:
    json_chunk = json.dumps(document, separators=(",", ":"), ensure_ascii=True).encode("utf-8")
    json_chunk += b" " * ((-len(json_chunk)) % 4)
    chunks = [(0x4E4F534A, json_chunk)]
    if binary:
        binary += b"\0" * ((-len(binary)) % 4)
        chunks.append((0x004E4942, binary))

    total_length = 12 + sum(8 + len(chunk) for _, chunk in chunks)
    with path.open("wb") as output:
        output.write(struct.pack("<III", 0x46546C67, 2, total_length))
        for chunk_type, chunk in chunks:
            output.write(struct.pack("<II", len(chunk), chunk_type))
            output.write(chunk)


def resource_document(count: int, multiline_index: int = -1) -> tuple[dict, bytes]:
    names = [f"Mesh_{index:04d}" for index in range(count)]
    if 0 <= multiline_index < count:
        names[multiline_index] += "\nsecond line"
    document = {
        "asset": {"version": "2.0", "generator": "Glider P2.3 UI clipping fixture"},
        "scene": 0,
        "scenes": [{"nodes": []}],
        "meshes": [
            {
                "name": names[index],
                "primitives": [{"attributes": {"POSITION": 0}}],
            }
            for index in range(count)
        ],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": 1,
                "type": "VEC3",
                "min": [0, 0, 0],
                "max": [0, 0, 0],
            }
        ],
        "bufferViews": [{"buffer": 0, "byteLength": 12}],
        "buffers": [{"byteLength": 12}],
    }
    return document, struct.pack("<fff", 0.0, 0.0, 0.0)


def variants_document(count: int) -> tuple[dict, bytes]:
    document, binary = resource_document(32)
    document["extensionsUsed"] = ["KHR_materials_variants"]
    document["extensions"] = {
        "KHR_materials_variants": {
            "variants": [{"name": f"Variant_{index:04d}"} for index in range(count)]
        }
    }
    return document, binary


def validation_document(count: int) -> dict:
    return {
        "asset": {"version": "2.0", "generator": "Glider P2.3 UI clipping fixture"},
        "scene": 0,
        "scenes": [{"nodes": []}],
        "meshes": [
            {"name": f"InvalidMesh_{index:04d}", "primitives": []}
            for index in range(count)
        ],
    }


def mixed_validation_document(count: int) -> dict:
    """Interleaved MESH_NO_PRIMITIVES and PRIMITIVE_MISSING_POSITION errors; all target meshes, so a code filter selects every other source row."""
    document = validation_document(count)
    for index, mesh in enumerate(document["meshes"]):
        if index % 2:
            mesh["name"] = f"NoPositionMesh_{index:04d}"
            mesh["primitives"] = [{"attributes": {}}]
    return document

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output_directory", type=Path)
    parser.add_argument("--resource-count", type=int, default=4096)
    parser.add_argument("--validation-count", type=int, default=2048)
    parser.add_argument("--variant-count", type=int, default=4096)
    args = parser.parse_args()

    if min(args.resource_count, args.validation_count, args.variant_count) < 1:
        parser.error("fixture counts must be positive")

    args.output_directory.mkdir(parents=True, exist_ok=True)
    resource_path = args.output_directory / f"p2_3_resources_{args.resource_count}.glb"
    resource_doc, resource_bin = resource_document(args.resource_count)
    write_glb(resource_path, resource_doc, resource_bin)

    multiline_path = args.output_directory / f"p2_3_resources_multiline_{args.resource_count}.glb"
    multiline_doc, multiline_bin = resource_document(args.resource_count, args.resource_count - 6)
    write_glb(multiline_path, multiline_doc, multiline_bin)

    variants_path = args.output_directory / f"p2_3_variants_{args.variant_count}.glb"
    variants_doc, variants_bin = variants_document(args.variant_count)
    write_glb(variants_path, variants_doc, variants_bin)

    validation_path = args.output_directory / f"p2_3_validation_{args.validation_count}.glb"
    write_glb(validation_path, validation_document(args.validation_count))

    mixed_path = args.output_directory / f"p2_3_validation_mixed_{args.validation_count}.glb"
    write_glb(mixed_path, mixed_validation_document(args.validation_count))

    for path in (resource_path, multiline_path, variants_path, validation_path, mixed_path):
        print(f"Wrote {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
