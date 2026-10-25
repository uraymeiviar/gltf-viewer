# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A4: glass reflection/refraction, windshield SSR, clear colours and disabled paths.

Run with Anaconda Python. Artifacts and measurements go to build/review/glass_*.
Uses synthetic geometry only; aircraft comparisons are recorded in the A4 report.
"""
import base64
import json
import os
import re
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image
import harness_process as subprocess

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "build/review"
EXE = REPO / "build/bin/Release/Glider.exe"
BASELINE = REPO / "build/baselines/ed36dd8.exe"
GLASS = "ASOBO_material_glass_v2"
WINDSHIELD = "ASOBO_material_windshield_v3"
failures = []


def check(ok, label):
    print(("PASS " if ok else "FAIL ") + label, flush=True)
    if not ok:
        failures.append(label)


def fixture(name, extension=GLASS, width=0.0, normal=(0, 0, 1), tint=(1, 1, 1), alignment=False):
    # Checkerboard is opaque and unlit: scene-copy errors cannot hide behind lighting.
    data = bytearray()
    views, accessors, meshes = [], [], []
    materials = [{"pbrMetallicRoughness": {"baseColorFactor": [*tint, 0.02],
                  "metallicFactor": 0, "roughnessFactor": 0.08},
                  "alphaMode": "BLEND", "doubleSided": True}]
    if extension:
        materials[0]["extensions"] = {extension: {"glassWidth": width} if extension == GLASS else {}}
    for level in (0.025, 0.35):
        materials.append({"pbrMetallicRoughness": {"baseColorFactor": [level]*3+[1]},
                          "extensions": {"KHR_materials_unlit": {}}})

    def quad(x0, y0, x1, y1, z, material, n=(0, 0, 1)):
        positions = [(x0,y0,z),(x1,y0,z),(x1,y1,z),(x0,y1,z)]
        first = len(accessors)
        for values in (positions, [n]*4):
            offset = len(data)
            data.extend(b"".join(struct.pack("<3f", *v) for v in values))
            views.append({"buffer": 0, "byteOffset": offset, "byteLength": 48})
            acc = {"bufferView": len(views)-1, "componentType": 5126, "count": 4, "type": "VEC3"}
            if values is positions:
                acc.update(min=[x0,y0,z], max=[x1,y1,z])
            accessors.append(acc)
        # No index buffer: keep buffer alignment trivially valid.
        meshes.append({"primitives": [{"attributes": {"POSITION": first, "NORMAL": first+1},
                                      "mode": 6, "material": material}]})

    quad(-1,-1,1,1,0,0,normal)
    for y in range(-8, 8):
        for x in range(-8, 8):
            quad(x*0.3,y*0.3,(x+1)*0.3,(y+1)*0.3,-0.5,1+(x+y)%2)
    if alignment:
        # Neutral zero-width glass makes C = B exactly: no reflection/tint/coverage
        # differences can disguise a scene-copy lookup or missing-draw error.
        materials[0]["pbrMetallicRoughness"]["baseColorFactor"] = [1,1,1,0]
        materials[0]["extensions"]["KHR_materials_unlit"] = {}
        materials[0]["extensions"]["ASOBO_material_draw_order"] = {"drawOrderOffset": 0}
        decal = "ASOBO_material_geometry_decal"
        materials.append({"pbrMetallicRoughness": {"baseColorFactor": [0.1,0.8,0.1,1],
                          "metallicFactor": 0, "roughnessFactor": 1},
                          "extensions": {decal: {}}, "doubleSided": True})
        # A small E-shaped decal tests the accumulated labels, not just opaque geometry.
        for rect in [(-0.5,-0.4,-0.43,0.4),(-0.5,0.33,0.1,0.4),
                     (-0.5,-0.035,0.0,0.035),(-0.5,-0.4,0.1,-0.33)]:
            quad(*rect,-0.4999,3)
        # Ordinary transparent draws must also appear in the copy.
        materials.append({"pbrMetallicRoughness": {"baseColorFactor": [0.1,0.2,0.7,1]},
                          "alphaMode": "BLEND", "extensions": {"KHR_materials_unlit": {}},
                          "doubleSided": True})
        quad(0.3,-0.4,0.55,0.4,-0.25,4)
    doc = {"asset": {"version": "2.0"}, "scene": 0,
           "scenes": [{"nodes": list(range(len(meshes)))}],
           "nodes": [{"mesh": i} for i in range(len(meshes))], "meshes": meshes,
           "materials": materials, "accessors": accessors, "bufferViews": views,
           "buffers": [{"byteLength": len(data), "uri": "data:application/octet-stream;base64,"+
                        base64.b64encode(data).decode()}],
           "extensionsUsed": ["KHR_materials_unlit"] + ([extension] if extension else [])}
    if alignment:
        doc["extensionsUsed"] += ["ASOBO_material_geometry_decal", "ASOBO_material_draw_order"]
    path = OUT / ("glass_"+name+".gltf")
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def render(model, name, flags=(), exe=EXE, env=None):
    png = OUT / ("glass_"+name+".png")
    png.unlink(missing_ok=True)
    args = [str(exe), str(model), "--isolated", "--window-size", "1000x800",
            "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none",
            "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0",
            "--camera-distance", "4", "--exposure", "-3", "--capture-viewport", str(png), "--capture-frame", "24",
            "--profile-report", str(png.with_suffix(".json")), "--exit-after-profile", *flags]
    result = subprocess.run(args, cwd=REPO, capture_output=True, timeout=300, env=env)
    (OUT / ("glass_"+name+".log")).write_bytes(result.stdout+result.stderr)
    if result.returncode or not png.exists():
        raise RuntimeError(f"{name}: exit {result.returncode}; see log")
    if exe == EXE:
        log = (result.stdout+result.stderr).decode("utf-8",errors="replace")
        if "GL automation errors: 0" not in log or re.search(r"GL error (?:0x|during)|GL error \(0x",log):
            raise RuntimeError(f"{name}: GL diagnostics failed; see log")
        if not (env and env.get("GLIDER_TEST_GLASS_COPY_FAILURE")) and "Glass copy fallback:" in log:
            raise RuntimeError(f"{name}: unexpected glass copy fallback; see log")
    return png


def pixels(path):
    a = np.asarray(Image.open(path).convert("RGB"), dtype=float)
    h,w = a.shape[:2]
    return a[h//2-60:h//2+60,w//2-60:w//2+60]


def same(a,b):
    return a.read_bytes() == b.read_bytes()


def ssr_fixture(name="ssr", attenuation=1.0, windshield=True, occluded=False):
    """Tilted physical pane and an orange emissive box on the camera side below it."""
    data, views, accessors, meshes = bytearray(), [], [], []
    def face(points, normal, material):
        first = len(accessors)
        for values in (points, [normal]*4):
            offset = len(data)
            data.extend(b"".join(struct.pack("<3f", *v) for v in values))
            views.append(dict(buffer=0, byteOffset=offset, byteLength=48))
            a = dict(bufferView=len(views)-1, componentType=5126, count=4, type="VEC3")
            if values is points:
                a.update(min=np.min(points,axis=0).tolist(), max=np.max(points,axis=0).tolist())
            accessors.append(a)
        meshes.append({"primitives":[{"attributes":{"POSITION":first,"NORMAL":first+1},"mode":6,"material":material}]})
    face([(-1.3,-.65,-.375),(1.3,-.65,-.375),(1.3,.65,.375),(-1.3,.65,.375)],(0,-.5,.8660254),0)
    lo,hi = (-.5,-1.2,.4),(.5,-.8,.85)
    corners=[(x,y,z) for z in (lo[2],hi[2]) for y in (lo[1],hi[1]) for x in (lo[0],hi[0])]
    for indices,n in [([4,5,7,6],(0,0,1)),([0,2,3,1],(0,0,-1)),([2,6,7,3],(0,1,0)),
                      ([0,1,5,4],(0,-1,0)),([1,3,7,5],(1,0,0)),([0,4,6,2],(-1,0,0))]:
        face([corners[i] for i in indices],n,1)
    if occluded:
        # A thin overhanging lip lies along the reflected ray, but does not hide the
        # emissive box from the camera. The reflection must stop at the lip.
        face([(-.8,-.4,.1),(-.8,-.4,.9),(.8,-.4,.9),(.8,-.4,.1)],(0,1,0),2)
    pane={"pbrMetallicRoughness":{"baseColorFactor":[1,1,1,0],"metallicFactor":0,"roughnessFactor":.05},"alphaMode":"BLEND","doubleSided":True}
    if windshield: pane["extensions"]={WINDSHIELD:{"ssrAttenuation":attenuation}}
    doc={"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":list(range(len(meshes)))}],
         "nodes":[{"mesh":i} for i in range(len(meshes))],"meshes":meshes,
         "materials":[pane,{"pbrMetallicRoughness":{"baseColorFactor":[0,0,0,1],"metallicFactor":0},"emissiveFactor":[1,.12,.005],"doubleSided":True},
                      {"pbrMetallicRoughness":{"baseColorFactor":[0,0,0,1],"metallicFactor":0,"roughnessFactor":1},"doubleSided":True}],
         "buffers":[{"byteLength":len(data),"uri":"data:application/octet-stream;base64,"+base64.b64encode(data).decode()}],
         "bufferViews":views,"accessors":accessors,"extensionsUsed":[WINDSHIELD] if windshield else []}
    path=OUT/("ssr_"+name+".gltf")
    path.write_text(json.dumps(doc),encoding="utf-8")
    return path


def check_ssr():
    OUT.mkdir(parents=True,exist_ok=True)
    model=ssr_fixture()
    zero=ssr_fixture("zero",0)
    # SSR is on by default; these checks also pass --ssr on explicitly.
    flags=["--clear-color","0.015,0.02,0.025","--exposure","0","--interior-sky-reflection","0","--ssr","on"]
    metrics={}
    default=render(model,"ssr3_default")
    check(same(default,render(model,"ssr3_explicit_on",["--ssr","on"])),"SSR is on by default")
    aa_cases=[("none",[]),("msaa4",["--msaa","4"]),("taa",["--post-aa","taa"]),("msaa4_taa",["--msaa","4","--post-aa","taa"])]
    aa_cases += [(f"msaa{msaa}_{post}",["--msaa",str(msaa),"--post-aa",post])
                 for msaa in (0,2,4,8) for post in ("none","fxaa","taa")
                 if (msaa,post) not in [(0,"none"),(4,"none"),(0,"taa"),(4,"taa")]]
    blocked=ssr_fixture("ssr2_occluded",occluded=True)
    for label,aa in aa_cases:
        on=render(model,"ssr_"+label+"_on",flags+aa)
        off=render(model,"ssr_"+label+"_off",flags+aa+["--ssr","off"])
        z=render(zero,"ssr_"+label+"_zero",flags+aa)
        a,b=np.asarray(Image.open(on).convert("RGB"),dtype=float),np.asarray(Image.open(off).convert("RGB"),dtype=float)
        # Only the top two thirds: the box itself is below the pane.
        delta=(a-b)[:int(a.shape[0]*.65)]
        orange=(delta[:,:,0]>8)&(delta[:,:,0]>delta[:,:,1]*1.5)
        metrics[label]={"orange_reflection_pixels":int(orange.sum()),"max_red_delta":float(delta[:,:,0].max())}
        check(orange.sum()>30,label+": orange box appears in windshield reflection")
        # Interior of the smooth reflected box top, excluding geometric silhouette.
        cy,cx=a.shape[0]//2,a.shape[1]//2
        plane=(a-b)[cy-22:cy-8,cx-25:cx+25,0]
        lap=4*plane[1:-1,1:-1]-plane[:-2,1:-1]-plane[2:,1:-1]-plane[1:-1,:-2]-plane[1:-1,2:]
        hf=float(np.abs(lap).mean()/max(1.0,plane.mean()))
        metrics[label]["smooth_plane_high_frequency"]=hf
        metrics[label]["smooth_plane_mean"]=float(plane.mean())
        check(plane.mean()>1 and hf<0.65,label+f": smooth reflected plane has low pixel flicker ({hf:.3f})")

        check(same(off,z),label+": attenuation zero matches SSR off")
        check(a.max()>20 and a.mean()>1,label+": no black image; GL diagnostics passed")
        blocked_on=render(blocked,"ssr2_occluded_"+label+"_on",flags+aa)
        blocked_off=render(blocked,"ssr2_occluded_"+label+"_off",flags+aa+["--ssr","off"])
        ba=np.asarray(Image.open(blocked_on).convert("RGB"),dtype=float)
        bb=np.asarray(Image.open(blocked_off).convert("RGB"),dtype=float)
        bd=(ba-bb)[:int(ba.shape[0]*.65)]
        check(not np.any((bd[:,:,0]>8)&(bd[:,:,0]>bd[:,:,1]*1.5)),label+": thin lip prevents box reflection")

    # Non-windshield fixtures match the pre-HDR baseline within the linear-blending delta (with bloom off).
    for label,m in [("plain",fixture("ssr_plain",None)),("glass",fixture("ssr_glass")),("khr",OUT/"glass_khr.gltf")]:
        if not m.exists(): continue
        cur_p=render(m,"ssr_baseline_"+label,["--bloom","off"])
        ref_p=render(m,"ssr_baseline_"+label+"_ref",exe=REPO/"build/baselines/c68a9ae.exe")
        ca=np.asarray(Image.open(cur_p).convert("RGB"),dtype=float)
        ra=np.asarray(Image.open(ref_p).convert("RGB"),dtype=float)
        d=np.abs(ca-ra)
        check(d.mean()<0.7 and d.max()<=6,label+f": matches c68a9ae within linear-blending delta (MAE {d.mean():.3f}/255, max {d.max():.0f}/255)")
    for quality in ("low","high"):
        path=render(model,"ssr_quality_"+quality,flags+["--ssr-quality",quality])
        check(not same(path,render(model,"ssr_quality_"+quality+"_off",flags+["--ssr","off"])),quality+": SSR reflects the box")
    import copy
    for location,translation in [("inside",[0,0,0]),("outside",[10,0,0])]:
        doc=json.loads(model.read_text())
        volume=copy.deepcopy(doc["meshes"][0])
        volume["primitives"][0]["material"]=len(doc["materials"])
        doc["materials"].append({"extensions":{"ASOBO_material_environment_occluder":{}}})
        doc["extensionsUsed"].append("ASOBO_material_environment_occluder")
        doc["meshes"].append(volume)
        doc["scenes"][0]["nodes"].append(len(doc["nodes"]))
        doc["nodes"].append({"mesh":len(doc["meshes"])-1,"scale":[3,3,3],"translation":translation})
        path=OUT/("ssr_occluder_"+location+".gltf")
        path.write_text(json.dumps(doc),encoding="utf-8")
        a=render(path,"ssr_occluder_"+location+"_dark",["--ssr","off","--exposure","0","--interior-sky-reflection","0"])
        b=render(path,"ssr_occluder_"+location+"_light",["--ssr","off","--exposure","0","--interior-sky-reflection","1"])
        check(same(a,b)==(location=="outside"),location+": occluder bounds gate interior fallback")
    # A clear scene through neutral refraction must respond to the same scene setting.
    clear_glass=fixture("ssr_clear_glass",alignment=True)
    doc=json.loads(clear_glass.read_text());doc["scenes"][0]["nodes"]=[0]
    clear_glass.write_text(json.dumps(doc),encoding="utf-8")
    a=pixels(render(clear_glass,"ssr_behind_red",["--clear-color","0.4,0.02,0.01"]))
    b=pixels(render(clear_glass,"ssr_behind_blue",["--clear-color","0.01,0.02,0.4"]))
    check(a[:,:,0].mean()>b[:,:,0].mean()+10 and b[:,:,2].mean()>a[:,:,2].mean()+10,"behind copy follows the two clear colours")
    for samples in (0,2,4,8):
        for post in ("none","fxaa","taa"):
            label=f"msaa{samples}_{post}"
            aa=["--msaa",str(samples),"--post-aa",post]
            for name,c in [("red","0.4,0.02,0.01"),("blue","0.01,0.02,0.4")]:
                path=render(model,"ssr_clear_"+label+"_"+name,aa+["--clear-color",c,"--ssr","off"])
                a=np.asarray(Image.open(path).convert("RGB"))
                expected=np.round(np.array([float(v) for v in c.split(',')])*255)
                check(np.abs(a[20,20].astype(float)-expected).max()<=1,label+": "+name+" clear colour")
    (OUT/"ssr_metrics.json").write_text(json.dumps(metrics,indent=2))
    if failures: raise SystemExit(1)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    metrics = {}
    # At zero thickness, all features behind neutral glass must retain their pixels.
    aligned = fixture("alignment", alignment=True)
    for name, flags in (("none",[]),("msaa2",["--aa","msaa2"]),
                        ("msaa4",["--aa","msaa4"]),("msaa8",["--aa","msaa8"]),
                        ("scale_half",["--render-scale","0.5"])):
        on = pixels(render(aligned,"alignment_"+name+"_on",flags))
        off = pixels(render(aligned,"alignment_"+name+"_off",flags+["--asobo-disable",GLASS]))
        delta = np.abs(on-off)
        costs = {(dx,dy):float(np.abs(on[4:-4,4:-4] -
                 off[4+dy:116+dy,4+dx:116+dx]).mean())
                 for dy in range(-3,4) for dx in range(-3,4)}
        shift = min(costs,key=costs.get)
        metrics["alignment_"+name] = {"mean_abs_delta":float(delta.mean()),
            "max_delta":float(delta.max()),"changed_pixels":int(np.any(delta>0,axis=2).sum()),
            "best_shift":list(shift)}
        check(shift == (0,0) and delta.mean() < (1.5 if name.startswith("msaa") else 0.01),
              f"zero-width {name}: shift {shift}, MAE {delta.mean():.6f}, max {delta.max():.0f}/255")
    # Raw RGB alone hid the black-viewport bug: ImGui blends the captured alpha again.
    # A complete background replacement must be opaque, also on the very first render.
    lit = fixture("msaa_lit",normal=(0.7071,0,0.7071))
    reference = None
    for aa in ("none","msaa2","msaa4","msaa8"):
        first = OUT / ("glass_msaa_"+aa+"_first.png")
        path = render(lit,"msaa_"+aa,["--aa",aa,"--capture-first-frame",str(first)])
        image = np.asarray(Image.open(path).convert("RGBA"))
        h,w = image.shape[:2]
        center = image[h//2-60:h//2+60,w//2-60:w//2+60]
        initial = np.asarray(Image.open(first).convert("RGBA"))
        ih,iw = initial.shape[:2]
        initial = initial[ih//2-40:ih//2+40,iw//2-40:iw//2+40]
        check(center[:,:,3].min()==255 and initial[:,:,3].min()==255,
              aa+": complete glass colour is opaque in first and steady viewport images")
        if reference is None: reference=center[:,:,:3].astype(float)
        delta=float(np.abs(center[:,:,:3].astype(float)-reference).mean())
        metrics["lit_"+aa+"_mean_delta_from_none"]=delta
        check(delta<2, f"{aa}: lit glass interior MAE {delta:.6f}/255 against none")
    for fault in ("allocation","multisample","resolve","capture"):
        env=dict(os.environ,GLIDER_TEST_GLASS_COPY_FAILURE=fault)
        path=render(lit,"fallback_"+fault,["--aa","msaa8"],env=env)
        rgba=np.asarray(Image.open(path).convert("RGBA"))
        h,w=rgba.shape[:2]
        center=rgba[h//2-60:h//2+60,w//2-60:w//2+60]
        check(pixels(path).mean()>10 and center[:,:,3].min()==255,
              fault+": injected failure stays visible with opaque viewport alpha")
        log=(OUT/("glass_fallback_"+fault+".log")).read_text(encoding="utf-8",errors="replace")
        check(log.count("Glass copy fallback:")==1,fault+": fallback logs once")
    # Tilt the shading normal, retaining identical raster coverage/checkerboard pixels.
    # This isolates the Fresnel change from camera projection and visible sky changes.
    for extension, prefix in ((GLASS,"v2"),(WINDSHIELD,"v3")):
        for angle, normal in (("head",(0,0,1)),("grazing",(0.985,0,0.174))):
            model = fixture(prefix+"_"+angle, extension, normal=normal)
            plain_angle = fixture(prefix+"_"+angle+"_plain", None, normal=normal)
            on = render(model,prefix+"_"+angle+"_on")
            off = render(model,prefix+"_"+angle+"_off",["--asobo-disable",extension])
            off_no_bloom = render(model,prefix+"_"+angle+"_off_nb",["--asobo-disable",extension,"--bloom","off"])
            baseline = render(model,prefix+"_"+angle+"_baseline",["--asobo-disable",extension],BASELINE)
            d_base = np.abs(np.asarray(Image.open(off_no_bloom).convert("RGB"),dtype=float) -
                            np.asarray(Image.open(baseline).convert("RGB"),dtype=float))
            check(same(off, render(plain_angle, prefix+"_"+angle+"_plain")) and d_base.mean() < 0.7 and d_base.max() <= 6,
                  f"{prefix} {angle} switch off matches plain byte-for-byte and ed36dd8 within linear-blending delta (MAE {d_base.mean():.3f}/255)")
            delta = float(np.mean(pixels(on)-pixels(off)))
            metrics[prefix+"_"+angle+"_brightness_delta"] = delta
        head = metrics[prefix+"_head_brightness_delta"]
        grazing = metrics[prefix+"_grazing_brightness_delta"]
        check(grazing > 2, f"{prefix} grazing reflection visible: mean delta {grazing:.3f}/255")
        check(0 <= head < 8 and head < grazing*0.3, f"{prefix} head-on reflection weaker: {head:.3f} vs {grazing:.3f}/255")
    thin = fixture("width_zero",normal=(0.7071,0,0.7071))
    thick = fixture("width_large",width=3.0,normal=(0.7071,0,0.7071))
    a,b = render(thin,"width_zero"),render(thick,"width_large")
    diff = np.abs(pixels(a)-pixels(b))
    metrics["width_mean_abs_delta"] = float(diff.mean())
    check(diff.mean()>5, f"width shifts checkerboard: mean absolute delta {diff.mean():.3f}/255")
    check(same(render(thin,"width_zero_off",["--asobo-disable",GLASS]),
               render(thick,"width_large_off",["--asobo-disable",GLASS])), "width has no effect with switch off")
    tinted = render(fixture("tinted",normal=(0.7071,0,0.7071),tint=(0.3,1,0.3)),"tinted")
    check(pixels(tinted)[:,:,0].mean() < pixels(a)[:,:,0].mean()-2,
          "base colour tints the copied background")
    # Rough glass_v2 blurs the refracted background (lower high-frequency energy than smooth).
    smooth_glass = fixture("rough_005", width=0.5, normal=(0.7071,0,0.7071))
    s_doc = json.loads(smooth_glass.read_text())
    s_doc["materials"][0]["pbrMetallicRoughness"]["roughnessFactor"] = 0.05
    smooth_glass.write_text(json.dumps(s_doc), encoding="utf-8")
    frosted_glass = fixture("rough_090", width=0.5, normal=(0.7071,0,0.7071))
    r_doc = json.loads(frosted_glass.read_text())
    r_doc["materials"][0]["pbrMetallicRoughness"]["roughnessFactor"] = 0.90
    frosted_glass.write_text(json.dumps(r_doc), encoding="utf-8")
    ps = pixels(render(smooth_glass, "rough_005"))
    pr = pixels(render(frosted_glass, "rough_090"))
    hf_smooth = float(np.abs(np.diff(ps, axis=0)).mean() + np.abs(np.diff(ps, axis=1)).mean())
    hf_rough = float(np.abs(np.diff(pr, axis=0)).mean() + np.abs(np.diff(pr, axis=1)).mean())
    metrics["refraction_hf_smooth"] = hf_smooth
    metrics["refraction_hf_rough"] = hf_rough
    check(hf_rough < hf_smooth * 0.75,
          f"rough glass_v2 blurs refraction: HF {hf_rough:.3f} < {hf_smooth:.3f}")
    # Live ASOBO parameter edit via --set-asobo-param matches authored fixture byte-for-byte.
    param_thick = render(thin, "param_width_large", ["--set-asobo-param", "0", GLASS, "glassWidth", "3.0"])
    check(same(param_thick, b), "--set-asobo-param glassWidth=3.0 matches width_large fixture byte-for-byte")
    # Exercise the pre-existing KHR transmission copy path with no ASOBO declaration.
    khr = fixture("khr",None)
    doc = json.loads(khr.read_text())
    doc["materials"][0]["extensions"] = {"KHR_materials_transmission": {"transmissionFactor":0.8}}
    doc["extensionsUsed"].append("KHR_materials_transmission")
    khr.write_text(json.dumps(doc),encoding="utf-8")
    BASELINE_399 = REPO / "build/baselines/399c88b.exe"
    for label, model, flags in (("khr",khr,[]),
                                ("plain",fixture("plain",None),[]),
                                ("asobo_off",thick,["--asobo","off"])):
        cur = render(model,label,flags+["--bloom","off"])
        base_img = render(model,label+"_baseline",flags,BASELINE)
        d_ed = np.abs(np.asarray(Image.open(cur).convert("RGB"),dtype=float) -
                      np.asarray(Image.open(base_img).convert("RGB"),dtype=float))
        check(d_ed.mean() < 0.7 and d_ed.max() <= 6,
              f"{label} matches ed36dd8 within linear-blending delta (MAE {d_ed.mean():.3f}/255, max {d_ed.max():.0f}/255)")
        if BASELINE_399.exists():
            base399_img = render(model,label+"_baseline399",flags,BASELINE_399)
            d_399 = np.abs(np.asarray(Image.open(cur).convert("RGB"),dtype=float) -
                           np.asarray(Image.open(base399_img).convert("RGB"),dtype=float))
            check(d_399.mean() < 0.7 and d_399.max() <= 6,
                  f"{label} matches 399c88b within linear-blending delta (MAE {d_399.mean():.3f}/255, max {d_399.max():.0f}/255)")
    (OUT/"glass_metrics.json").write_text(json.dumps(metrics,indent=2),encoding="utf-8")
    print(json.dumps(metrics,indent=2))
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    if "--ssr" not in sys.argv:
        main()
    check_ssr()
