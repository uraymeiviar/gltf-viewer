"""Read-only PMDG package audit, with review variants isolated under build/review/pmdg."""
import copy
import json
from pathlib import Path

import numpy as np
from PIL import Image

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "build/review/pmdg"


def main():
    doc = json.loads((OUT / "vc.gltf").read_text())
    raw = Path(doc["buffers"][0]["uri"]).read_bytes()
    types = {5120: "i1", 5121: "u1", 5122: "<i2", 5123: "<u2", 5125: "<u4", 5126: "<f4"}
    sizes = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}

    def read(index):
        accessor = doc["accessors"][index]
        view = doc["bufferViews"][accessor["bufferView"]]
        dtype = np.dtype(types[accessor["componentType"]])
        size = sizes[accessor["type"]]
        return np.ndarray((accessor["count"], size), dtype=dtype, buffer=raw,
                          offset=view.get("byteOffset", 0)+accessor.get("byteOffset", 0),
                          strides=(view.get("byteStride", size*dtype.itemsize), dtype.itemsize)).astype(float)

    agreement, triangles = 0, 0
    for mesh in doc["meshes"]:
        for primitive in mesh["primitives"]:
            attrs = primitive["attributes"]
            if primitive.get("mode", 4) != 4 or "NORMAL" not in attrs or "indices" not in primitive:
                continue
            positions, normals = read(attrs["POSITION"]), read(attrs["NORMAL"])[:, :3]
            indices = read(primitive["indices"]).astype(int).reshape(-1, 3)
            points = positions[indices]
            dots = np.einsum("ij,ij->i", np.cross(points[:, 1]-points[:, 0], points[:, 2]-points[:, 0]), normals[indices].sum(1))
            valid = abs(dots) > 1e-10
            agreement += int((dots[valid] > 0).sum())
            triangles += int(valid.sum())
    stats = {"asset": doc["asset"], "triangles": triangles, "triangles_agreeing_with_normals": agreement,
             "note": "No PMDG source PNGs supplied. Histograms alone cannot identify the package transfer function.", "textures": {}}
    ranges = []
    for mesh in doc["meshes"]:
        for primitive in mesh["primitives"]:
            meta = primitive.get("extras", {}).get("ASOBO_primitive", {})
            accessor_count = doc["accessors"][primitive["indices"]]["count"]
            start, count = meta.get("StartIndex", 0), meta.get("PrimitiveCount", accessor_count//3)*3
            if start or count != accessor_count:
                ranges.append({"mesh": mesh.get("name"), "material": primitive.get("material"),
                               "accessor_count": accessor_count, "start": start, "index_count": count,
                               "valid": start <= accessor_count and count <= accessor_count-start})
    stats["primitive_ranges"] = {"count": len(ranges), "all_valid": all(item["valid"] for item in ranges),
                                 "old_indices": sum(item["accessor_count"] for item in ranges),
                                 "new_indices": sum(item["index_count"] for item in ranges), "ranges": ranges}
    identity = copy.deepcopy(doc)
    images = set()
    for material in doc["materials"]:
        info = material.get("occlusionTexture")
        if info:
            texture = doc["textures"][info["index"]]
            images.add(texture["extensions"]["MSFT_texture_dds"]["source"])
    for index in sorted(images):
        path = Path(doc["images"][index]["uri"])
        if not path.exists():
            continue
        image = Image.open(path).convert("RGBA")
        pixels = np.asarray(image)
        red = pixels[:, :, 0].astype(float)/255
        stats["textures"][path.name] = {
            "dimensions": list(image.size),
            "quantiles_RGB_8bit": [np.quantile(pixels[:, :, channel], [0, .1, .5, .9, 1]).tolist() for channel in range(3)],
            "mean_R": float(red.mean()), "mean_R_squared": float(np.mean(red*red)),
            "mean_absolute_R_vs_R_squared": float(np.mean(red-red*red)),
        }
        # The baseline still squares R; pre-encode sqrt(R) to isolate identity AO.
        # PNG replaces only the test copy's image URI (G/B/A stay unchanged).
        converted = pixels.copy()
        converted[:, :, 0] = np.round(np.sqrt(red)*255).astype(np.uint8)
        output = OUT / ("identity_ao_"+str(index)+".png")
        Image.fromarray(converted).save(output)
        identity["images"][index]["uri"] = str(output)
    # Keep the old whole-accessor geometry for the isolated AO hypothesis test.
    for mesh in identity["meshes"]:
        for primitive in mesh["primitives"]:
            meta = primitive.get("extras", {}).get("ASOBO_primitive", {})
            meta.pop("StartIndex", None)
            meta.pop("PrimitiveCount", None)
    (OUT / "vc_identity_ao.gltf").write_text(json.dumps(identity))
    # Explicit alternative configuration, used only to diagnose authored overlap.
    single = copy.deepcopy(doc)
    hidden_names = {"bl_OVHD_LL_Panel_Option", "bl_OVHD_LL_Pulse", "bl_OVHD_LL_position_pulse",
                    "bl_OVHD_aux_bat_option_noaux", "bl_OVHD_recirc_single", "bl_OVHD_galley_nine",
                    "bl_OVHD_ignition_off", "bl_Probe_Heat_Label_Option_OFF", "bl_pressurization_old",
                    "00_aux_fuel_panel_option_show", "Smoke_panel_Options_Move"}

    def hide(index):
        single["nodes"][index].pop("mesh", None)
        for child in single["nodes"][index].get("children", []):
            hide(child)

    hidden = []
    for index, node in enumerate(single["nodes"]):
        if node.get("name") in hidden_names:
            hidden.append(node["name"])
            hide(index)
    stats["review_only_hidden_nodes"] = hidden
    (OUT / "vc_single_options.gltf").write_text(json.dumps(single))
    (OUT / "m2020_texture_audit.json").write_text(json.dumps(stats, indent=2))
    print("Wrote audit and two review-only variants; input package unchanged.")


if __name__ == "__main__":
    main()
