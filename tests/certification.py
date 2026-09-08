#!python3

import argparse
import subprocess
from pathlib import Path
import json
from PIL import Image
from PIL import ImageChops
import os



parser = argparse.ArgumentParser(description='Generate screenshots for certification')
parser.add_argument("--executable", help="Path to the Gestaltor executable")
parser.add_argument("--rep", help="Path to the certification repository")
parser.add_argument("--scenarios", help="Path to json files with scenarios", default="certificationConfig.json")
args = parser.parse_args()

f = open(args.scenarios)
data = json.load(f)

os.makedirs("Diff", exist_ok=True)

gestaltor_path = Path(args.executable)
cert_path = Path(args.rep)

defaultScenario = data["defaultScenario"]

for scenario in data["scenarios"]:
    combined = defaultScenario.copy()
    for item in scenario:
        combined[item] = scenario[item]
    scenario_args = [gestaltor_path]
    for item in combined:
        if item == "model":
            scenario_args.append("-i")
            scenario_args.append(str((cert_path / combined[item]).absolute()))
        elif item == "lighting":
            scenario_args.append("--ibl")
            scenario_args.append(str((cert_path / combined[item]).absolute()))
        elif item == "name":
            continue
        else:
            scenario_args.append("--{}".format(item))
            scenario_args.append(str(combined[item]))

    name = combined["model"]
    end = name.rfind(".")
    start = name.rfind("/") + 1
    name = name[start:end]
    namePath = ""
    gltfName = name
    if "name" in combined:
        name = combined["name"]
        name = "{}".format(name)
        namePath = "UX3D/c-{}.png".format(name)
        scenario_args.append("-o")
        scenario_args.append(str(Path(namePath).absolute()))
    else:

        name = "{}-{}".format(name, combined["camera"])
        namePath = "UX3D/c-{}.png".format(name)
        scenario_args.append("-o")
        scenario_args.append(str(Path(namePath).absolute()))
    print("Generate Screenshot: {}".format(name))
    print(*scenario_args)
    subprocess.run(scenario_args)
    referenceName = "rr-{}{}".format(name, ".png")
    im1 = Image.open(cert_path / "models" / gltfName / referenceName).convert('RGB')
    im2 = Image.open(namePath)
    edge = (im2.width - 1024) /2
    im2 = im2.crop((edge, 0, 1024 + edge, 1024))
    diff = ImageChops.difference(im2, im1)
    diff.save("Diff/d-{}{}".format(name, ".png"))
    


