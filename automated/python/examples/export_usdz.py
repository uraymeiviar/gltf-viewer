#!/usr/bin/python
# This example shows how to load and export a glTF to USDZ.

import os
import glob
import sys
from shutil import rmtree

from GestaltorAutomated import core_operations as ga

python_script_dir = os.path.dirname(os.path.realpath(__file__))
temp_dir = python_script_dir+"\\output"

invalid_files = []

# Either specify input folder here or via commandline argument:
input_folder = 'assets'
if len(sys.argv) > 1:
    input_folder = sys.argv[1]


def log(message, new_line=True):
    if new_line == False:
        print(message, end='')
        print(message, file=logFile, end='')
    else:
        print(message)
        print(message, file=logFile)


def load_save(input_filepath, output_filepath):
    log(input_filepath)
    gltf_handle = ga.create_handle()
    # load glTF
    try:
        log("Load: ", False)
        ga.call(gltf_handle, "load_file", {"filepath": input_filepath})
    except Exception:
        log("Error")
    else:
        log("Ok")

    # save glTF
    try:
        log("Save: ", False)
        ga.call(gltf_handle, "export_usdz", {
                "filepath": output_filepath, "overwrite": True})
    except Exception as e:
        log(f'Error: {e}')
    else:
        log("Ok")


logFile = open(python_script_dir+"\\log.txt", 'w')
files = glob.glob(input_folder + '/**/*.gltf', recursive=True)
files += glob.glob(input_folder + '/**/*.glb', recursive=True)

rmtree(temp_dir, True)

counter = 0
for filename in files:
    os.makedirs(temp_dir, exist_ok=True)  # prepare temporary output directory
    output_filepath = temp_dir + f'\\{filename}.usdz'

    # load and save the glTF
    load_save(filename, output_filepath)
    counter = counter+1
