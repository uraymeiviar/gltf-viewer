"""Generate deterministic, distinct 8K textures and 3.6M actual (unshared) vertices.

Default output is build/review/heavy/heavy.gltf. Uses numpy/Pillow; never launches Glider.
Textures are PNG RGBA with stripes, gradients and distinct per-material colours.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def make(out, vertices=3_600_000, textures=16, size=8192):
    out.mkdir(parents=True, exist_ok=True)
    vertices = (vertices // (textures * 3)) * textures * 3
    per_mesh = vertices // textures
    views, accessors, meshes, nodes = [], [], [], []
    with (out / "geometry.bin").open("wb") as f:
        for i in range(textures):
            triangles = per_mesh // 3
            t = np.arange(triangles, dtype=np.float32)
            x, y = (t % 300) / 300, (t // 300) / 300
            pos = np.empty((triangles, 3, 3), dtype="<f4")
            pos[:, :, 2] = (i % 3) * .001
            pos[:, :, 0] = x[:, None] + np.array([0, 1/300, 0])
            pos[:, :, 1] = y[:, None] + np.array([0, 0, 1/300])
            normal = np.tile(np.array([0, 0, 1], dtype="<f4"), (per_mesh, 1))
            uv = pos.reshape(-1, 3)[:, :2].copy()
            first = len(accessors)
            for data, kind in ((pos, "VEC3"), (normal, "VEC3"), (uv, "VEC2")):
                offset = f.tell()
                f.write(data.tobytes())
                views.append(dict(buffer=0, byteOffset=offset, byteLength=data.nbytes, target=34962))
                a = dict(bufferView=len(views)-1, componentType=5126, count=per_mesh, type=kind)
                if data is pos:
                    a.update(min=pos.reshape(-1, 3).min(axis=0).tolist(), max=pos.reshape(-1, 3).max(axis=0).tolist())
                accessors.append(a)
            meshes.append(dict(primitives=[dict(attributes=dict(POSITION=first, NORMAL=first+1, TEXCOORD_0=first+2), material=i)]))
            nodes.append(dict(mesh=i, translation=[(i % 4)*1.1, (i // 4)*1.1, 0]))
    for i in range(textures):
        path = out / f"texture_{i:02d}.png"
        if path.exists():
            with Image.open(path) as old:
                if old.size == (size, size):
                    continue
        pixels = np.empty((size, size, 4), dtype=np.uint8)
        pixels[:, :, 0] = (np.arange(size, dtype=np.uint32)[None, :] // 32 + i*13) % 256
        pixels[:, :, 1] = (np.arange(size, dtype=np.uint32)[:, None] // 32 + i*29) % 256
        pixels[:, :, 2] = 30+i*11
        pixels[:, :, 3] = 255
        Image.fromarray(pixels).save(path, compress_level=1)
        print(path, flush=True)
    doc = dict(asset=dict(version="2.0", generator="Glider heavy load fixture"), scene=0,
               scenes=[dict(nodes=list(range(textures)))], nodes=nodes, meshes=meshes,
               buffers=[dict(uri="geometry.bin", byteLength=(out/"geometry.bin").stat().st_size)],
               bufferViews=views, accessors=accessors,
               images=[dict(uri=f"texture_{i:02d}.png") for i in range(textures)],
               textures=[dict(source=i, sampler=0) for i in range(textures)],
               samplers=[dict(minFilter=9987, magFilter=9729)],
               materials=[dict(pbrMetallicRoughness=dict(baseColorTexture=dict(index=i), metallicFactor=0, roughnessFactor=.8)) for i in range(textures)])
    (out/"heavy.gltf").write_text(json.dumps(doc), encoding="utf-8")
    print(f"{vertices:,} vertices, {textures} distinct {size}x{size} textures: {out/'heavy.gltf'}", flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", type=Path, default=Path(__file__).resolve().parents[1]/"build/review/heavy")
    p.add_argument("--vertices", type=int, default=3_600_000)
    p.add_argument("--textures", type=int, default=16)
    p.add_argument("--size", type=int, default=8192)
    a = p.parse_args()
    make(a.out, a.vertices, a.textures, a.size)
