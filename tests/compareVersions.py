#!python3

import argparse
import subprocess
from pathlib import Path
import json
from PIL import Image
from PIL import ImageChops
import os
import glob
import copy
import evaluation



parser = argparse.ArgumentParser(description='Generate screenshots for certification')
parser.add_argument("--old", help="Path to the old Gestaltor executable")
parser.add_argument("--new", help="Path to the new Gestaltor executable")
parser.add_argument("--assets", help="Path to directory containing the assets to test", default=".")
parser.add_argument("--threshold", help="Threshold for SSIM to include into report.", default="1")
args = parser.parse_args()

asset_path = Path(args.assets)

all_assets = sorted(asset_path.rglob('*.gltf'))
all_assets.extend(sorted(asset_path.rglob('*.glb')))

os.makedirs("Diff", exist_ok=True)

gestaltor_path_old = Path(args.old)
gestaltor_path_new = Path(args.new)

names = set()

for asset in all_assets:
    scenario_args = []
    scenario_args.append("-i")
    scenario_args.append(str(asset.absolute()))
    scenario_args.append("--height")
    scenario_args.append("1024")
    scenario_args.append("--width")
    scenario_args.append("1024")
    scenario_args.append("-c")
    scenario_args.append("0")
    scenario_args_new = copy.deepcopy(scenario_args)

    # Only create screenshot if name was not already used
    name = asset.stem
    if name in names:
        continue
    names.update([name])
    namePath = "GestaltorOld/{}.png".format(name)
    scenario_args.append("-o")
    scenario_args.append(str(Path(namePath).absolute()))

    namePath = "GestaltorNew/{}.png".format(name)
    scenario_args_new.append("-o")
    scenario_args_new.append(str(Path(namePath).absolute()))

    scenario_args.insert(0, str(gestaltor_path_old.absolute()))
    scenario_args_new.insert(0, str(gestaltor_path_new.absolute()))

    
    print("Generate Screenshot: {}".format(name))
    subprocess.run(scenario_args)
    subprocess.run(scenario_args_new)

evaluation.main(["--old", "GestaltorOld", "--new", "GestaltorNew", "--name", "Test", "--output", "Results", "--threshold", args.threshold])
    


