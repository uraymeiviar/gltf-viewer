#!/usr/bin/env python3
"""Generate deterministic GLB inputs for flat-list, validation, and hierarchy UI checks."""

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


def hierarchy_broad_document(count: int, multiline_index: int = -1) -> tuple[dict, bytes]:
    """A broad scene with count root nodes, each having a mesh, primitive and material."""
    node_names = [f"Node_{index:04d}" for index in range(count)]
    if 0 <= multiline_index < count:
        node_names[multiline_index] += "\nsecond line"
    document = {
        "asset": {"version": "2.0", "generator": "Glider P2.4 UI hierarchy clipping fixture"},
        "scene": 0,
        "scenes": [{"name": "BroadScene", "nodes": list(range(count))}],
        "nodes": [
            {
                "name": node_names[index],
                "mesh": index,
            }
            for index in range(count)
        ],
        "meshes": [
            {
                "name": f"Mesh_{index:04d}",
                "primitives": [{"attributes": {"POSITION": 0}, "material": index}],
            }
            for index in range(count)
        ],
        "materials": [
            {
                "name": f"Material_{index:04d}",
                "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.8, 0.8, 1.0]},
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


def hierarchy_deep_document(count: int, branching: int = 4) -> tuple[dict, bytes]:
    """A hierarchical tree structure of count nodes with branching factor branching."""
    nodes = []
    for i in range(count):
        nodes.append({
            "name": f"TreeNode_{i:04d}",
            "children": [],
        })
    root_nodes = []
    for r in range(min(branching, count)):
        root_nodes.append(r)
    next_child = len(root_nodes)
    queue = list(root_nodes)
    while queue and next_child < count:
        parent_idx = queue.pop(0)
        num_children = min(branching, count - next_child)
        for _ in range(num_children):
            nodes[parent_idx]["children"].append(next_child)
            queue.append(next_child)
            next_child += 1

    meshes = []
    materials = []
    mesh_idx = 0
    for i, node in enumerate(nodes):
        if not node["children"]:
            node["mesh"] = mesh_idx
            meshes.append({
                "name": f"Mesh_{mesh_idx:04d}",
                "primitives": [{"attributes": {"POSITION": 0}, "material": mesh_idx}],
            })
            materials.append({
                "name": f"Material_{mesh_idx:04d}",
                "pbrMetallicRoughness": {"baseColorFactor": [0.5, 0.5, 0.8, 1.0]},
            })
            mesh_idx += 1
            del node["children"]

    document = {
        "asset": {"version": "2.0", "generator": "Glider P2.4 UI hierarchy clipping fixture"},
        "scene": 0,
        "scenes": [{"name": "DeepScene", "nodes": root_nodes}],
        "nodes": nodes,
        "meshes": meshes,
        "materials": materials,
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


def hierarchy_shared_mesh_document() -> tuple[dict, bytes]:
    """A scene with multiple nodes sharing the same mesh to test shared-mesh auto-expansion."""
    # Node 0 (RootNode_A) has Mesh 0
    # Node 1 (RootNode_B) has Mesh 0
    # Node 2 (ControlNode_C) has Mesh 1
    document = {
        "asset": {"version": "2.0", "generator": "Glider P2.4 UI shared mesh fixture"},
        "scene": 0,
        "scenes": [{"name": "SharedMeshScene", "nodes": [0, 1, 2]}],
        "nodes": [
            {"name": "RootNode_A", "mesh": 0},
            {"name": "RootNode_B", "mesh": 0},
            {"name": "ControlNode_C", "mesh": 1},
        ],
        "meshes": [
            {
                "name": "SharedMesh_0000",
                "primitives": [{"attributes": {"POSITION": 0}, "material": 0}],
            },
            {
                "name": "SingleMesh_0001",
                "primitives": [{"attributes": {"POSITION": 0}, "material": 1}],
            },
        ],
        "materials": [
            {
                "name": "SharedMaterial_0000",
                "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.2, 0.2, 1.0]},
            },
            {
                "name": "SingleMaterial_0001",
                "pbrMetallicRoughness": {"baseColorFactor": [0.2, 0.8, 0.2, 1.0]},
            },
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


def hierarchy_camera_light_document() -> tuple[dict, bytes]:
    """A scene with 2 cameras and 2 punctual lights to test camera/light selection, switching and clearing."""
    document = {
        "asset": {"version": "2.0", "generator": "Glider P2.4 UI camera/light fixture"},
        "scene": 0,
        "scenes": [{"name": "CameraLightScene", "nodes": [0, 1, 2, 3]}],
        "extensionsUsed": ["KHR_lights_punctual"],
        "extensions": {
            "KHR_lights_punctual": {
                "lights": [
                    {"name": "Light_0000", "type": "directional", "color": [1.0, 1.0, 1.0], "intensity": 10.0},
                    {"name": "Light_0001", "type": "point", "color": [1.0, 0.0, 0.0], "intensity": 20.0},
                ]
            }
        },
        "cameras": [
            {"name": "Camera_0000", "type": "perspective", "perspective": {"yfov": 0.7, "znear": 0.1}},
            {"name": "Camera_0001", "type": "orthographic", "orthographic": {"xmag": 1.0, "ymag": 1.0, "znear": 0.1, "zfar": 100.0}},
        ],
        "nodes": [
            {"name": "CamNode_0000", "camera": 0},
            {"name": "CamNode_0001", "camera": 1},
            {"name": "LightNode_0000", "extensions": {"KHR_lights_punctual": {"light": 0}}},
            {"name": "LightNode_0001", "extensions": {"KHR_lights_punctual": {"light": 1}}},
        ],
    }
    return document, b""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output_directory", type=Path)
    parser.add_argument("--resource-count", type=int, default=4096)
    parser.add_argument("--validation-count", type=int, default=2048)
    parser.add_argument("--variant-count", type=int, default=4096)
    parser.add_argument("--hierarchy-count", type=int, default=4096)
    args = parser.parse_args()

    if min(args.resource_count, args.validation_count, args.variant_count, args.hierarchy_count) < 1:
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

    hier_broad_path = args.output_directory / f"p2_4_hierarchy_broad_{args.hierarchy_count}.glb"
    hier_broad_doc, hier_broad_bin = hierarchy_broad_document(args.hierarchy_count)
    write_glb(hier_broad_path, hier_broad_doc, hier_broad_bin)

    hier_deep_path = args.output_directory / f"p2_4_hierarchy_deep_{args.hierarchy_count}.glb"
    hier_deep_doc, hier_deep_bin = hierarchy_deep_document(args.hierarchy_count)
    write_glb(hier_deep_path, hier_deep_doc, hier_deep_bin)

    hier_multi_path = args.output_directory / f"p2_4_hierarchy_multiline_{args.hierarchy_count}.glb"
    hier_multi_doc, hier_multi_bin = hierarchy_broad_document(args.hierarchy_count, args.hierarchy_count - 6)
    write_glb(hier_multi_path, hier_multi_doc, hier_multi_bin)

    hier_shared_path = args.output_directory / "p2_4_hierarchy_shared_mesh.glb"
    hier_shared_doc, hier_shared_bin = hierarchy_shared_mesh_document()
    write_glb(hier_shared_path, hier_shared_doc, hier_shared_bin)

    hier_cam_light_path = args.output_directory / "p2_4_hierarchy_camera_light.glb"
    hier_cam_light_doc, hier_cam_light_bin = hierarchy_camera_light_document()
    write_glb(hier_cam_light_path, hier_cam_light_doc, hier_cam_light_bin)

    for path in (resource_path, multiline_path, variants_path, validation_path, mixed_path,
                 hier_broad_path, hier_deep_path, hier_multi_path, hier_shared_path, hier_cam_light_path):
        print(f"Wrote {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
