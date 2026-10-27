"""Focused smoke checks for editable quality presets, telemetry UI and wildcard paths.

Does not run the regression suites. Uses isolated fixture assets and optional UI captures.
"""
import base64
import json
from pathlib import Path
import struct
import os
import shutil

import numpy as np
from PIL import Image
import harness_process as subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/review/quality_controls"
EXE = ROOT / "build/bin/Release/Glider.exe"


def fixture():
    model_dir = OUT / "assets/model"
    texture_dir = OUT / "assets/texture.FSS"
    nested = OUT / "assets/other/deep"
    for directory in (model_dir,texture_dir,nested):
        directory.mkdir(parents=True,exist_ok=True)
    pixels = np.empty((2048,2048,4),dtype=np.uint8)
    pixels[:,:,0] = np.arange(2048,dtype=np.uint16)[None,:]//8
    pixels[:,:,1] = np.arange(2048,dtype=np.uint16)[:,None]//8
    pixels[:,:,2] = 100; pixels[:,:,3] = 255
    Image.fromarray(pixels).save(texture_dir/"preset.png")
    Image.fromarray(pixels[::8,::8]).save(nested/"recursive.png")
    Image.fromarray(pixels[::8,::8]).save(OUT/"assets/other/immediate.png")
    positions = (-1,-1,0, 1,-1,0, 1,1,0, -1,1,0)
    normals = (0,0,1)*4
    uv = (0,0, 1,0, 1,1, 0,1)
    data = struct.pack("<32f",*positions,*normals,*uv)
    doc = {"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
        "nodes":[{"mesh":0,"name":"Quality fixture"}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"mode":6,"material":0}]}],
        "materials":[{"doubleSided":True,"pbrMetallicRoughness":{"baseColorTexture":{"index":0},"metallicFactor":0,"roughnessFactor":.8}}],
        "images":[{"uri":"preset.png"}],"textures":[{"source":0,"sampler":0}],"samplers":[{"minFilter":9987}],
        "buffers":[{"uri":"data:application/octet-stream;base64,"+base64.b64encode(data).decode(),"byteLength":len(data)}],
        "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":48},{"buffer":0,"byteOffset":96,"byteLength":32}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[-1,-1,0],"max":[1,1,0]},
            {"bufferView":1,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":4,"type":"VEC2"}]}
    model = model_dir/"fixture.gltf"
    model.write_text(json.dumps(doc),encoding="utf-8")
    for uri in ("recursive.png","immediate.png"):
        doc["images"][0]["uri"] = uri
        (model_dir/uri.replace(".png",".gltf")).write_text(json.dumps(doc),encoding="utf-8")
    return model


def capture(model,label,preset,search,relative_model=False):
    png,report,log = (OUT/f"{label}.{ext}" for ext in ("png","json","log"))
    model_arg = model.relative_to(ROOT).as_posix() if relative_model else str(model)
    with log.open("wb") as stream:
        result = subprocess.run([str(EXE),model_arg,"--isolated","--quality-preset",preset,"--search-path",search,
            "--vsync","off","--capture-viewport",str(png),"--capture-frame","12","--profile-report",str(report),"--exit-after-profile"],
            cwd=ROOT,stdout=stream,stderr=stream,timeout=180)
    text = log.read_text(encoding="utf-8",errors="replace")
    assert result.returncode == 0 and png.exists(), (label,result.returncode,text)
    assert "GL automation errors: 0" in text and "[SearchPaths] Resolved" in text, (label,text)
    assert "Cannot resolve image" not in text and "using error image" not in text, (label,text)
    data = json.loads(report.read_text(encoding="utf-8"))
    print("PASS",label,data["startup"]["profileTextureMaximum"],data["viewport"]["antiAliasing"],flush=True)
    return data


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    model = fixture()
    for preset,cap,aa in (("low",512,"none"),("med",1024,"msaa2"),("high",4096,"msaa2+taa"),("ultra",0,"msaa4+taa")):
        data = capture(model,preset,preset,"../texture*")
        assert data["startup"]["profileTextureMaximum"] == cap and data["viewport"]["antiAliasing"] == aa
        assert data["startup"]["autoQuality"] is False
        if preset in ("low","med"): assert data["startup"]["reducedTextures"] > 0
    capture(model.parent/"immediate.gltf","wildcard_immediate","low","../*")
    capture(model.parent/"recursive.gltf","wildcard_recursive","low","../**")
    # A model opened by a relative path must resolve relative search paths the same way.
    capture(model,"relative_model","low","../texture*",relative_model=True)
    script = OUT/"settings_ui.txt"
    script.write_text("\n".join(("wait 8",f"shot {(OUT/'settings_search.png').as_posix()}",
        "clickp settings.tab.quality 0","wait 8",f"shot {(OUT/'settings_profiles.png').as_posix()}",
        "clickp quality.tab 1","wait 8","clickp quality.use 1","waitms 800",f"shot {(OUT/'low_live.png').as_posix()}",
        "clickp quality.tab 4","wait 8","clickp quality.use 4","waitms 800",f"shot {(OUT/'ultra_live.png').as_posix()}",
        "clickp settings.close 0","wait 8",f"shot {(OUT/'statistics.png').as_posix()}",
        "clickp statistics.tab.textures 0","wait 8",f"shot {(OUT/'texture_memory.png').as_posix()}",
        "clickp statistics.tab.meshes 0","wait 8",f"shot {(OUT/'mesh_memory.png').as_posix()}","")),encoding="utf-8")
    with (OUT/"settings_ui.log").open("wb") as stream:
        result = subprocess.run([str(EXE),str(model),"--isolated","--quality-preset","high","--search-path","../texture*",
            "--show-statistics","--show-search-paths","--show-telemetry","--ui-script",str(script),"--ui-report",str(OUT/"settings_ui.json")],
            cwd=ROOT,stdout=stream,stderr=stream,timeout=180)
    ui = json.loads((OUT/"settings_ui.json").read_text(encoding="utf-8"))
    assert result.returncode == 0 and not ui["failed"], ui
    print("PASS Settings tabs / live Low -> Ultra texture refresh / statistics screenshots",flush=True)
    # VRAM/FPS in the status bar must open (and focus) the Statistics panel.
    def status_click(label,probe):
        script = OUT/f"{label}.txt"
        script.write_text("\n".join(("wait 10",f"clickp {probe} 0","wait 8",
            "clickp statistics.tab.textures 0","wait 8",
            f"shot {(OUT/(label+'.png')).as_posix()}","")),encoding="utf-8")
        with (OUT/f"{label}.log").open("wb") as stream:
            result = subprocess.run([str(EXE),str(model),"--isolated","--show-telemetry",
                "--ui-script",str(script),"--ui-report",str(OUT/f"{label}.json")],
                cwd=ROOT,stdout=stream,stderr=stream,timeout=180)
        ui = json.loads((OUT/f"{label}.json").read_text(encoding="utf-8"))
        assert result.returncode == 0 and not ui["failed"], ui
    status_click("status_fps","status.fps")
    status_click("status_vram","status.vram")
    print("PASS status bar FPS/VRAM click opens the Statistics panel",flush=True)
    data_dir = OUT/"persistent_settings"
    if data_dir.exists():
        shutil.rmtree(data_dir)
    data_dir.mkdir(parents=True)
    env = {**os.environ,"GLIDER_DATA_DIR":str(data_dir)}
    def read_ini(path):
        sections = {}
        current = None
        for line in path.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith(";") or line.startswith("#"):
                continue
            if line.startswith("[") and line.endswith("]"):
                current = sections.setdefault(line[1:-1], {})
            elif current is not None and "=" in line:
                key, value = line.split("=", 1)
                current[key.strip()] = value.strip()
        return sections
    def persistent_ui(label,preset,commands):
        script = OUT/f"{label}.txt"
        script.write_text("wait 8\nclickp settings.tab.quality 0\nwait 8\nclickp quality.tab 1\nwait 8\n"+f"shot {(OUT/(label+'_before.png')).as_posix()}\nwait 2\n"+commands+"\nwait 8\n",encoding="utf-8")
        with (OUT/f"{label}.log").open("wb") as stream:
            result = subprocess.run([str(EXE),str(model),"--quality-preset",preset,"--search-path","../texture*",
                "--show-search-paths","--ui-script",str(script),"--ui-report",str(OUT/f"{label}.json")],
                cwd=ROOT,env=env,stdout=stream,stderr=stream,timeout=180)
        assert result.returncode == 0 and not json.loads((OUT/f"{label}.json").read_text())["failed"]
        return read_ini(data_dir/"settings.ini")
    saved = persistent_ui("edit_profile","high","clickp quality.texture.max 1\nwait 4\nclickp quality.texture.option 1024")
    assert saved["QualityProfiles"]["profile0.textureMaxSize"] == "1024", saved
    assert "profile1.textureMaxSize" not in saved["QualityProfiles"], saved
    restored = persistent_ui("reset_profile","low","clickp quality.reset 1")
    assert "profile0.textureMaxSize" not in restored.get("QualityProfiles", {}), restored
    assert "profile1.textureMaxSize" not in restored.get("QualityProfiles", {}), restored
    print("PASS saved profile override and reset (single settings.ini in a private directory)",flush=True)
    print("PASS settings.ini stores only non-default values",flush=True)


if __name__ == "__main__":
    main()
