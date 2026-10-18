#!python3

import argparse
import subprocess
import shutil
import os
import requests
from pathlib import Path
from helpers import cmake


def print_status(message):
    OKCYAN = "\033[96m"
    ENDC = "\033[0m"
    BOLD = "\033[1m"
    print("\n" + BOLD + OKCYAN + message + ENDC)


parser = argparse.ArgumentParser(description="Build and package the linux version")
cmake.add_cmake_args(parser)
args = parser.parse_args()
args.output_name = (
    args.output_name.replace(" ", "_")
    if args.output_name != ""
    else args.app_name.replace(" ", "_")
)


cmake_root_path = Path(args.cmake_root)
build_dir_path = Path(args.build_dir)
application_output_path = Path("packages/ux3d.gestaltor/data")
deployment_dir = Path(os.path.dirname(os.path.abspath(__file__)))

package_name = "GestaltorAutomated"

print_status("Prepare Python Package")
shutil.rmtree(application_output_path, True)
os.makedirs(application_output_path, exist_ok=True)
shutil.copytree(
    "../automated/python/deployment/",
    application_output_path,
    symlinks=True,
    dirs_exist_ok=True,
)

shutil.copytree(
    "../automated/python/helper_operations/",
    application_output_path / "GestaltorAutomated",
    symlinks=True,
    dirs_exist_ok=True,
)

print_status("CMake Generate")
build_dir, artefacts_dir = cmake.generate(args)

print_status("CMake Build")
cmake.build(args, build_dir_path=build_dir, target="core_operations")

print_status("Copy Files")

build_folder = "bin"

for file_path in (build_dir_path / f"{build_folder}").rglob("*.so"):
    shutil.copy(file_path, application_output_path / f"{package_name}")
    print(str(file_path))

print("binary dir: "+str( (build_dir_path / f"{build_folder}")))
print("deployment_dir: "+str(deployment_dir))
print("application_output_path: "+str(application_output_path))

# todo: fix usd

#shutil.copytree(build_dir_path / f"{build_folder}/usd", application_output_path / f"{package_name}/usd", dirs_exist_ok=True)


