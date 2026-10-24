# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""The FSS E195 assets the harnesses use, made loadable with all their textures.

The project's glTF files name their images relatively; the textures live in sibling folders
(TEXTURE_FOLDERS) that Glider only searches through settings.ini, which --isolated ignores.
textured_copy() writes a copy whose buffer and image URIs are absolute, so every binary, including
baselines older than --search-path, loads the asset fully textured.
"""

import json
from pathlib import Path

E195_DIR = Path(r"F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model")
COCKPIT = E195_DIR / "E195-Cockpit-LOD00.gltf"
EXTERIOR = E195_DIR / "E195-Exterior-LOD00.gltf"
# The fallback chain of the project's default texture folder (texture/texture.cfg), as MSFS resolves it, then
# texture.FSS, which the user's settings.ini also lists.
TEXTURE_FOLDERS = ("texture", "texture.E195", "texture.E195.cockpit", "texture.E195.ext", "texture.E195.cabin",
                   "texture.E195.GroundObjects", "texture.FSS")


def textured_copy(model, out_dir):
    """Writes <out_dir>/<model stem>_abs.gltf with absolute buffer and image URIs; returns its path, or None
    when the asset is missing. Raises when an image is found in none of the texture folders."""
    model = Path(model)
    if not model.exists():
        return None
    doc = json.loads(model.read_text(encoding="utf-8"))
    for buffer in doc.get("buffers", []):
        if "uri" in buffer and not buffer["uri"].startswith("data:"):
            buffer["uri"] = (model.parent / buffer["uri"]).as_posix()
    missing = []
    for image in doc.get("images", []):
        uri = image.get("uri")
        if not uri or uri.startswith("data:"):
            continue
        local = model.parent / uri
        found = local if local.exists() else next(
            (model.parent.parent / folder / uri for folder in TEXTURE_FOLDERS if (model.parent.parent / folder / uri).exists()), None)
        if found is None:
            missing.append(uri)
        else:
            image["uri"] = found.as_posix()
    if missing:
        raise FileNotFoundError(f"{model.name}: {len(missing)} images in none of {TEXTURE_FOLDERS}, e.g. {missing[0]}")
    out = Path(out_dir) / f"{model.stem}_abs.gltf"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc), encoding="utf-8")
    return out
