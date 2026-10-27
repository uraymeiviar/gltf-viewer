"""GPU picking must address the same rebased vertices as visible draws."""
import base64
import copy
import json
import struct

import harness_process as subprocess
import validate_asobo_primitive_ranges as ranges


def main():
    glass = ranges.glass
    out = glass.REPO / "build/review/pmdg777"
    out.mkdir(parents=True, exist_ok=True)
    glass.OUT = out
    path, doc = ranges.fixture("picking", base=65536)
    primitives = doc["meshes"][0]["primitives"]
    doc["meshes"] = [{"primitives": [primitive]} for primitive in primitives]
    doc["nodes"] = [{"name": "LeftQuad", "mesh": 0}, {"name": "RightQuad", "mesh": 1}]
    doc["scenes"][0]["nodes"] = [0, 1]
    path.write_text(json.dumps(doc))
    reference = copy.deepcopy(doc)
    vertex_bytes = reference["bufferViews"][0]["byteLength"]
    raw = base64.b64decode(reference["buffers"][0]["uri"].split(",",1)[1])[:vertex_bytes]
    raw += struct.pack("<12I",0,2,1,0,3,2,65536,65538,65537,65536,65539,65538)
    reference["buffers"][0].update(uri="data:application/octet-stream;base64,"+base64.b64encode(raw).decode(),byteLength=len(raw))
    reference["bufferViews"][1]["byteLength"] = 48
    reference["accessors"][1].update(componentType=5125,count=6)
    reference["accessors"].append({"bufferView":1,"byteOffset":24,"componentType":5125,"count":6,"type":"SCALAR"})
    reference["meshes"][1]["primitives"][0]["indices"] = 2
    for mesh in reference["meshes"]:
        mesh["primitives"][0].pop("extras")
    ref = out / "ranges_picking_reference.gltf"
    ref.write_text(json.dumps(reference))
    script = out / "base_vertex_picking.txt"
    script.write_text("wait 8\nclick 620 400\nwait 6\nsnap right\nclick 390 400\nwait 6\nsnap left\nwait 2\n")
    selections = []
    for label, model in (("native",path),("reference",ref)):
        report = out / ("base_vertex_picking_"+label+".json")
        result = subprocess.run([str(glass.EXE),str(model),"--isolated","--window-size","1000x800",
                                 "--aa","none","--hide-grid","--camera-target","0,0,0","--camera-yaw","0",
                                 "--camera-pitch","0","--camera-distance","4","--ui-script",str(script),
                                 "--ui-report",str(report)],cwd=glass.REPO,capture_output=True,timeout=120)
        log = result.stdout+result.stderr
        (out/("base_vertex_picking_"+label+".log")).write_bytes(log)
        if result.returncode:
            raise RuntimeError(label+": picking run failed")
        data = json.loads(report.read_text())
        if data.get("failed", True):
            raise RuntimeError(label+": UI script failed")
        glass.check(bool(data["snaps"]) and all(snap.get("glErrors", -1)==0 for snap in data["snaps"]),
                    label+": UI snapshot GL error counters are zero")
        selected = [(snap["label"],snap["selection"]) for snap in data["snaps"]]
        print(label, selected, flush=True)
        selections.append(selected)
        glass.check(any(label=="right" and "RightQuad" in selection for label,selection in selected), "right pixel picks the rebased RightQuad")
        glass.check(any(label=="left" and "LeftQuad" in selection for label,selection in selected), "left pixel picks the unbased LeftQuad")
    glass.check(selections[0]==selections[1], "native and explicit-index reference picking selections match exactly")
    if glass.failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
