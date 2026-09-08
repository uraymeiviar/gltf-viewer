#/bin/python3

import argparse
from PIL import Image, ImageFilter
import os
import tempfile
import math
import itertools
import subprocess

parser = argparse.ArgumentParser(description='Convert .png to .ico or .icns')
parser.add_argument('input', help='path to the .png that is used as base for conversion')
parser.add_argument('output', help='path to the output file')
args = parser.parse_args()

def convert_icns(png, output):
    # load the image
    im = Image.open(png)
    assert im.size[0] == im.size[1] and im.size[0] >= 1024, "Input image does not match resolution requirements. Should be at least 1024 px, square and power of two"
    full_resolution = im.size[0]
    
    # create the iconset (images from full resolution to 16x16)
    inconset_path = tempfile.gettempdir() + "/tmp.iconset"
    os.makedirs(tempfile.gettempdir() + "/tmp.iconset", exist_ok=True)
    iconset_resolutions = [ 512, 256, 128, 32, 16]
    iconset_scales = [1, 2]
    for res, scale in itertools.product(iconset_resolutions, iconset_scales):
        im_resized = im.resize((res * scale, res * scale), Image.ANTIALIAS)
        icon_filename = "icon_{}x{}.png".format(res, res) if scale == 1 else "icon_{}x{}@{}x.png".format(res, res, scale)
        im_resized.save(inconset_path + "/" + icon_filename)
    
    # convert the iconset to a .icns file
    subprocess.call(["iconutil", "--convert", "icns", "--output", output, inconset_path])

if os.path.splitext(args.output)[-1] == ".icns":
    convert_icns(args.input, args.output)
else:
    raise NotImplementedError("No conversion function for '.ico' available")
