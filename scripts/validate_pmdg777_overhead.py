"""Read-only PMDG 777 package audit and overhead rendering references."""
import copy
import json
from pathlib import Path

import numpy as np
import validate_msfs2020_materials as review

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "build/review/pmdg777"
SOURCE = Path(r"F:\msfs-addons\pmdg-aircraft-77w\SimObjects\Airplanes\PMDG 777-300ER\model.300ER\PMDG773ER_VC.gltf")


def prepare():
    OUT.mkdir(parents=True, exist_ok=True)
    doc = json.loads(SOURCE.read_text(encoding="utf-8-sig"))
    missing = []
    folders = [SOURCE.parent.parent / name for name in ("texture.vc", "texture.common", "texture.300ER", "texture.PMDG")]
    for collection in ("buffers", "images"):
        for item in doc.get(collection, []):
            uri = item.get("uri", "")
            if not uri or uri.startswith("data:"):
                continue
            path = SOURCE.parent / uri
            if not path.is_file():
                path = next((folder / Path(uri).name for folder in folders if (folder / Path(uri).name).is_file()), path)
            if not path.is_file():
                missing.append(uri)
            item["uri"] = str(path.resolve())
    raw = [Path(buffer["uri"]).read_bytes() for buffer in doc["buffers"]]
    slices, rebased = [], 0
    baked = copy.deepcopy(doc)
    extra = bytearray()
    for mi, mesh in enumerate(doc["meshes"]):
        for pi, primitive in enumerate(mesh["primitives"]):
            if "indices" not in primitive:
                continue
            meta = primitive.get("extras", {}).get("ASOBO_primitive", {})
            accessor = doc["accessors"][primitive["indices"]]
            view = doc["bufferViews"][accessor["bufferView"]]
            start, count = meta.get("StartIndex", 0), meta.get("PrimitiveCount", accessor["count"]//3)*3
            base = meta.get("BaseVertexIndex", 0)
            dtype = np.dtype({5121: "u1", 5123: "<u2", 5125: "<u4"}[accessor["componentType"]])
            values = np.ndarray((accessor["count"],), dtype=dtype, buffer=raw[view["buffer"]],
                                offset=view.get("byteOffset", 0)+accessor.get("byteOffset", 0),
                                strides=(view.get("byteStride", dtype.itemsize),))[start:start+count].astype(np.uint64)
            vertex_count = min(doc["accessors"][ai]["count"] for ai in primitive["attributes"].values())
            if start or count != accessor["count"] or base:
                slices.append(dict(mesh=mesh.get("name"), material=primitive.get("material"), start=start,
                                   count=count, base=base, vertices=vertex_count,
                                   max_index=int(values.max()) if len(values) else -1,
                                   valid=start+count<=accessor["count"] and (not len(values) or int(values.max())+base<vertex_count)))
            rebased += bool(base)
            # An explicit uint32 accessor bakes both slicing and base vertex so the
            # unchanged 8e932ca baseline can act as a reference.
            while len(extra)%4:
                extra.append(0)
            offset = len(extra)
            data = (values+base).astype("<u4").tobytes()
            extra.extend(data)
            baked["bufferViews"].append({"buffer": len(doc["buffers"]), "byteOffset": offset, "byteLength": len(data)})
            baked["accessors"].append({"bufferView": len(baked["bufferViews"])-1, "componentType": 5125,
                                       "count": count, "type": "SCALAR"})
            target = baked["meshes"][mi]["primitives"][pi]
            target["indices"] = len(baked["accessors"])-1
            for key in ("StartIndex", "PrimitiveCount", "BaseVertexIndex"):
                target.get("extras", {}).get("ASOBO_primitive", {}).pop(key, None)
    (OUT / "explicit_indices.bin").write_bytes(extra)
    baked["buffers"].append({"uri": str(OUT / "explicit_indices.bin"), "byteLength": len(extra)})
    (OUT / "vc.gltf").write_text(json.dumps(doc))
    (OUT / "vc_explicit_indices.gltf").write_text(json.dumps(baked))
    audit = dict(source=str(SOURCE), asset=doc["asset"], missing_images=missing,
                 ranged_primitives=len(slices), base_vertex_primitives=rebased,
                 invalid_ranges=[row for row in slices if not row["valid"]],
                 center_overhead=[row for row in slices if row["mesh"]=="Center_OVHD"], ranges=slices)
    (OUT / "audit.json").write_text(json.dumps(audit, indent=2))
    print(json.dumps({key:value for key,value in audit.items() if key!="ranges"}, indent=2), flush=True)
    return OUT / "vc.gltf"


def main():
    import sys
    model = prepare()
    if "--prepare" in sys.argv:
        return
    review.OUT = OUT
    flags = ["--camera-target", "0,2.85,30", "--camera-yaw", "180", "--camera-pitch", "-60",
             "--camera-distance", "1.6", "--exposure", "0", "--bloom", "off"]
    review.capture(model, "777_before", flags, review.BASE)
    review.capture(model, "777_after", flags)
    off = [arg for extension in ("ASOBO_material_glass", "ASOBO_material_kitty_glass", "ASOBO_material_windshield", "ASOBO_material_code")
           for arg in ("--asobo-disable", extension)]
    # Isolate geometry from the extreme-emission safety correction as well as
    # the newly supported glass types.
    geometry_flags = flags+["--msfs-emissive-scale", "0"]
    reference = review.capture(OUT / "vc_explicit_indices.gltf", "777_reference", geometry_flags, review.BASE)
    native = review.capture(model, "777_geometry_only", geometry_flags+off)
    if reference.read_bytes() != native.read_bytes():
        raise RuntimeError("777 native slicing/base vertex differs from explicit baseline indices")
    print("777 native geometry matches explicit baseline byte-for-byte", flush=True)
    review.capture(model, "777_no_emissive", flags+["--msfs-emissive-scale", "0"])
    review.capture(model, "777_default_bloom", [*flags, "--bloom", "on"])
    without_base = json.loads(model.read_text())
    for mesh in without_base["meshes"]:
        for primitive in mesh["primitives"]:
            primitive.get("extras", {}).get("ASOBO_primitive", {}).pop("BaseVertexIndex", None)
    no_base = OUT / "vc_ignore_base_vertex.gltf"
    no_base.write_text(json.dumps(without_base))
    review.capture(no_base,"777_slices_only_geometry",flags+["--msfs-emissive-scale","0"])
    preview_flags = flags+["--window-size","1600x1000","--exposure","2","--bloom","on"]
    review.capture(model,"777_authored_preview",preview_flags)
    for factor in (250,1000,3000):
        preview = json.loads(model.read_text())
        preview["materials"][28]["emissiveFactor"] = [factor]*3
        path = OUT / ("vc_CB_Labels_"+str(factor)+".gltf")
        path.write_text(json.dumps(preview))
        review.capture(path,"777_CB_Labels_"+str(factor)+"_preview",preview_flags)


if __name__ == "__main__":
    main()
