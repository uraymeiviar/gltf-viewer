"""Extreme authored emission must remain finite in half-float scene/decal targets."""
import json
from PIL import Image
import numpy as np

import validate_asobo_glass as glass
import validate_asobo_emissive as emission
import validate_asobo_decal as decals


def dark_lit(material):
    material["pbrMetallicRoughness"] = {"baseColorFactor":[0,0,0,1],"metallicFactor":0,"roughnessFactor":1}
    material.setdefault("extensions", {})["KHR_materials_specular"] = {"specularFactor":0}


def main():
    out = glass.REPO / "build/review/pmdg777"
    out.mkdir(parents=True, exist_ok=True)
    glass.OUT = emission.EMIS_DIR = decals.OUT = out
    # Power-of-two source ratios and unit scale make the finite reference exact
    # in float32, including bloom/TAA's additional half-float quantization.
    high = [8388608,16777216,4194304]
    # 13000 leaves headroom for the five-level RGBA16F additive bloom chain.
    safe = [6500,13000,3250]
    flags = ["--exposure","-15","--clear-color","0,0,0","--bloom","off","--msfs-emissive-scale","1"]
    for kind in ("forward", "decal", "stacked_decal"):
        models = []
        for label, factor in (("high",high),("safe",safe)):
            if kind == "forward":
                path = emission.make_quad_fixture("extreme_"+kind+label,factor,is_asobo=True,half_extent=1)
                doc = json.loads(path.read_text())
                dark_lit(doc["materials"][0])
            else:
                path = decals.write_fixture("extreme_"+kind+label,decal_count=2 if kind=="stacked_decal" else 1,
                                            factors=decals.property_factors("emissiveBlendFactor"),decal_z=0.0001)
                doc = json.loads(path.read_text())
                dark_lit(doc["materials"][0])
                doc["materials"][0].pop("emissiveTexture",None)
                doc["materials"][0]["emissiveFactor"] = [0,0,0]
                for material in doc["materials"][1:]:
                    material["emissiveFactor"] = factor
                    material.pop("emissiveTexture",None)
                    material["pbrMetallicRoughness"]["baseColorFactor"] = [1,1,1,.5]
            doc["extensionsUsed"].append("KHR_materials_specular")
            path.write_text(json.dumps(doc))
            models.append(path)
        for label, extra in (("none",[]),("bloom",["--bloom","on"]),("taa",["--post-aa","taa"])):
            a = glass.render(models[0],"extreme_"+kind+label,flags+extra)
            b = glass.render(models[1],"extreme_safe_"+kind+label,flags+extra)
            glass.check(glass.same(a,b),kind+" "+label+": extreme HDR matches finite hue-preserving reference byte-for-byte")
            pixels = np.asarray(Image.open(a).convert("RGB"))
            glass.check(float(pixels.mean())>1,kind+" "+label+": visible image, no overflow-to-black")
    if glass.failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
