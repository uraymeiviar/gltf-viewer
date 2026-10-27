"""MSFS 2020 shared index-accessor slices: exact reference and malformed-range checks."""
import base64
import copy
import json
import struct

import validate_asobo_glass as glass


def fixture(name, component=5123, declared=True, base=0):
    # Two disjoint quads, with opposite material colours. Clockwise package winding.
    positions = [(-1, -.6, 0), (-.1, -.6, 0), (-.1, .6, 0), (-1, .6, 0),
                 (.1, -.6, 0), (1, -.6, 0), (1, .6, 0), (.1, .6, 0)]
    indices = [0, 2, 1, 0, 3, 2, 4, 6, 5, 4, 7, 6]
    if base:
        positions = positions[:4]+[(0,0,0)]*(base-4)+positions[4:]
        indices[6:] = [0,2,1,0,3,2]
    data = b"".join(struct.pack("<3f", *p) for p in positions)
    position_bytes = len(data)
    data += b"".join(struct.pack("<H" if component == 5123 else "<I", i) for i in indices)
    doc = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}],
           "nodes": [{"mesh": 0}], "meshes": [{"primitives": []}],
           "extensionsUsed": ["KHR_materials_unlit"]+(["ASOBO_asset_optimized"] if declared else []),
           "buffers": [{"uri": "data:application/octet-stream;base64,"+base64.b64encode(data).decode(), "byteLength": len(data)}],
           "bufferViews": [{"buffer": 0, "byteLength": position_bytes}, {"buffer": 0, "byteOffset": position_bytes, "byteLength": len(data)-position_bytes}],
           "accessors": [{"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3", "min": [-1, -.6, 0], "max": [1, .6, 0]},
                         {"bufferView": 1, "componentType": component, "count": 12, "type": "SCALAR"}],
           "materials": [{"pbrMetallicRoughness": {"baseColorFactor": color}, "doubleSided": True,
                          "extensions": {"KHR_materials_unlit": {}}} for color in ([.7, .03, .01, 1], [.01, .6, .03, 1])]}
    for material in range(2):
        doc["meshes"][0]["primitives"].append({"attributes": {"POSITION": 0}, "indices": 1, "material": material,
                                             "extras": {"ASOBO_primitive": {"PrimitiveCount": 2, **({"StartIndex": 6} if material else {})}}})
    if base:
        doc["meshes"][0]["primitives"][1]["extras"]["ASOBO_primitive"]["BaseVertexIndex"] = base
    path = glass.OUT / ("ranges_"+name+".gltf")
    path.write_text(json.dumps(doc))
    return path, doc


def main():
    glass.OUT = glass.REPO / "build/review/pmdg"
    glass.OUT.mkdir(parents=True, exist_ok=True)
    # A 16-bit index addresses vertices beyond 65535 through the base vertex.
    for component in (5123, 5125):
        path, doc = fixture("base_"+str(component), component, base=65536)
        reference = copy.deepcopy(doc)
        raw = base64.b64decode(reference["buffers"][0]["uri"].split(",",1)[1])
        vertex_bytes = reference["bufferViews"][0]["byteLength"]
        raw = raw[:vertex_bytes]+struct.pack("<12I",0,2,1,0,3,2,65536,65538,65537,65536,65539,65538)
        reference["buffers"][0].update(uri="data:application/octet-stream;base64,"+base64.b64encode(raw).decode(),byteLength=len(raw))
        reference["bufferViews"][1]["byteLength"] = 48
        reference["accessors"][1].update(componentType=5125,count=6)
        reference["accessors"].append({"bufferView":1,"byteOffset":24,"componentType":5125,"count":6,"type":"SCALAR"})
        reference["meshes"][0]["primitives"][1]["indices"] = 2
        for primitive in reference["meshes"][0]["primitives"]:
            primitive.pop("extras")
        ref = path.with_name(path.stem+"_reference.gltf")
        ref.write_text(json.dumps(reference))
        glass.check(glass.same(glass.render(path,"base_vertex_"+str(component)),glass.render(ref,"base_vertex_reference_"+str(component))),
                    f"{component}: base vertex 65536 matches explicit uint32 indices byte-for-byte")
        instanced = copy.deepcopy(doc)
        # Same material, buffer and slice, with only base vertex differing: the
        # instancing key must keep these two groups separate.
        instanced["meshes"][0]["primitives"][1]["material"] = 0
        instanced["meshes"][0]["primitives"][1]["extras"]["ASOBO_primitive"]["StartIndex"] = 0
        instanced["nodes"] = [{"mesh":0,"translation":[(i%4-1.5)*2.3,(i//4-1)*1.5,0]} for i in range(12)]
        instanced["scenes"][0]["nodes"] = list(range(12))
        inst = path.with_name(path.stem+"_instanced.gltf")
        inst.write_text(json.dumps(instanced))
        on = glass.render(inst,f"base_instanced_{component}",["--camera-distance","14"])
        off = glass.render(inst,f"base_instanced_off_{component}",["--camera-distance","14","--disable-instancing"])
        glass.check(glass.same(on,off), f"{component}: instanced base-vertex draws match individual draws byte-for-byte")
        on_profile, off_profile = (json.loads(image.with_suffix(".json").read_text()) for image in (on,off))
        calls = lambda profile: max(frame["counters"]["rendererDrawCalls"] for frame in profile["frames"])
        glass.check(calls(on_profile) < calls(off_profile), f"{component}: base-vertex fixture actually executes merged instanced draws")
        # Malformed base values must never cause an out-of-bounds vertex fetch.
        for label, invalid in (("negative",-1),("fraction",.5),("past_vertices",65540),("last_vertex",65539),("huge",2**63)):
            doc["meshes"][0]["primitives"][1]["extras"]["ASOBO_primitive"]["BaseVertexIndex"] = invalid
            path.write_text(json.dumps(doc))
            without = copy.deepcopy(doc)
            without["meshes"][0]["primitives"].pop()
            ref.write_text(json.dumps(without))
            glass.check(glass.same(glass.render(path,f"base_invalid_{component}_{label}"),glass.render(ref,f"base_invalid_ref_{component}_{label}")),
                        f"{component} {label}: invalid base/rebased index omitted safely")
    for component in (5123, 5125):
        path, doc = fixture(str(component), component)
        reference = copy.deepcopy(doc)
        size = 2 if component == 5123 else 4
        reference["accessors"][1]["count"] = 6
        reference["accessors"].append({"bufferView": 1, "byteOffset": 6*size, "componentType": component, "count": 6, "type": "SCALAR"})
        reference["meshes"][0]["primitives"][1]["indices"] = 2
        for primitive in reference["meshes"][0]["primitives"]:
            primitive.pop("extras")
        ref = path.with_name(path.stem+"_reference.gltf")
        ref.write_text(json.dumps(reference))
        ranged = glass.render(path, "ranges_"+str(component))
        glass.check(glass.same(ranged, glass.render(ref, "ranges_ref_"+str(component))),
                    f"{component}: shared indices and explicit slice accessors match byte-for-byte")
    path, doc = fixture("undeclared", declared=False)
    plain = copy.deepcopy(doc)
    for primitive in plain["meshes"][0]["primitives"]:
        primitive.pop("extras")
    ref = path.with_name("ranges_plain.gltf")
    ref.write_text(json.dumps(plain))
    glass.check(glass.same(glass.render(path, "ranges_undeclared"), glass.render(ref, "ranges_plain")),
                "undeclared asset_optimized ignores primitive metadata")
    # Invalid slices must not turn into GPU reads beyond the accessor or wrap unsigned offsets.
    for name, start, count in (("negative", -1, 2), ("fraction", .5, 2), ("past_end", 13, 0),
                               ("too_many", 6, 3), ("huge", 2**63, 2), ("negative_count", 0, -1)):
        path, doc = fixture(name)
        doc["meshes"][0]["primitives"][1]["extras"]["ASOBO_primitive"].update(StartIndex=start, PrimitiveCount=count)
        path.write_text(json.dumps(doc))
        result = glass.render(path, "ranges_invalid_"+name)
        reference = copy.deepcopy(doc)
        reference["meshes"][0]["primitives"].pop()
        ref = path.with_name(path.stem+"_reference.gltf")
        ref.write_text(json.dumps(reference))
        glass.check(glass.same(result, glass.render(ref, "ranges_invalid_ref_"+name)),
                    name+": invalid slice omitted safely")
    if glass.failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
