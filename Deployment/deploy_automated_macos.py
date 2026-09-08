#!python3

import argparse
from operator import sub
import copy
import shutil
from helpers import cmake
import glob

def print_status(message):
    OKCYAN = '\033[96m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    print('\n' + BOLD + OKCYAN + message + ENDC)

# arguments
parser = argparse.ArgumentParser(description='Build and package the macOS version')
cmake.add_cmake_args(parser)
parser.add_argument('-s', "--sign", help='specify a codesign identity (should be of type "Developer ID Application" or "Apple Distribution")')
parser.add_argument('--archs', nargs='+', default=["x86_64", "arm64"], help='Specify the architectures to be built')
args = parser.parse_args()

args.generator = "Xcode"

for arch in args.archs:
    arch_args = copy.deepcopy(args)

    arch_args.mac_arch = arch
    arch_args.build_dir = arch_args.build_dir + "/" + arch
    arch_args.install_dir = arch_args.install_dir + f"/GestaltorAutomated.macos.{arch}"

    print_status("CMake Generate")
    build_dir, artefacts_dir = cmake.generate(arch_args)

    print_status("CMake Build")
    cmake.build(arch_args, build_dir_path=build_dir, target="core_operations")

    # Copy the python module
    shutil.copytree("../automated/python/deployment/", artefacts_dir, symlinks=True, dirs_exist_ok=True)
    libs = glob.glob(str(build_dir / "bin" / "Release") + "/*.so")
    for lib in libs:
        shutil.copy(lib, artefacts_dir / "GestaltorAutomated") 
    