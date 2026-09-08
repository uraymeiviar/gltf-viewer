#!python3

import argparse
import os
import pathlib
import subprocess
from helpers import cmake
import wget
import stat
import glob
import json
import shutil

# arguments
parser = argparse.ArgumentParser(description='Build and package the linux version')
cmake.add_cmake_args(parser)
args = parser.parse_args()
args.output_name = args.output_name.replace(" ", "_") if args.output_name != "" else args.app_name.replace(" ", "_")

# override the install dir for appdir build and run cmake generate, build and install
install_dir = pathlib.Path(args.install_dir)
appdir = (install_dir / ".." / "appdir").resolve()
args.install_dir = appdir / "usr"
cmake.run(args)

# install linuxdeploy
if not args.build_only:
    os.environ["QMAKE"] = f'{args.build_dir}/qt/bin/qmake'
    os.environ["LD_LIBRARY_PATH"] = f'{args.build_dir}/qt/lib'
    linuxdeploy_name = "linuxdeploy-x86_64.AppImage"
    deployment_dir = pathlib.Path(os.path.dirname(os.path.abspath(__file__)))
    download_dir = deployment_dir / '../..'
    linuxdeploy_path = download_dir / linuxdeploy_name
    if not linuxdeploy_path.exists():
        wget.download("https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage", out=str(download_dir))
        st = os.stat(str(linuxdeploy_path))
        os.chmod(str(linuxdeploy_path), st.st_mode | stat.S_IEXEC)

    if not (deployment_dir / "linuxdeploy-plugin-qt-x86_64.AppImage").exists():
        path = wget.download("https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage", out=str(download_dir))
        st = os.stat(str(path))
        os.chmod(str(path), st.st_mode | stat.S_IEXEC)

    if not (deployment_dir / "linuxdeploy-plugin-appimage-x86_64.AppImage").exists():
        path = wget.download("https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-x86_64.AppImage", out=str(download_dir))
        st = os.stat(str(path))
        os.chmod(str(path), st.st_mode | stat.S_IEXEC)

    if not (deployment_dir / "appimagetool-x86_64.AppImage").exists():
        path = wget.download("https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage", out=str(download_dir))
        st = os.stat(str(path))
        os.chmod(str(path), st.st_mode | stat.S_IEXEC)

    # Load the version and version name from the package json
    with open(args.package_json, 'r', encoding="utf-8") as package_json_file:
        package_json = json.load(package_json_file)

    # set environment variables for linuxdeploy
    version = "OpenSource"
        
    os.environ["VERSION"] = version
    os.environ["LD_LIBRARY_PATH"] += ":" + str(appdir / "usr/lib") + ":" + str(appdir / "usr/bin")
    os.environ["QML_SOURCES_PATHS"] = str(deployment_dir / '..' / "editor/qml")

    # Copy ssl lib
    lib_path = subprocess.run("ldconfig -p | grep libssl.so.1 | head -n1 | tr ' ' '\n' | grep /", check=True, shell=True, capture_output=True).stdout.decode("utf-8").strip()
    print(lib_path)
    shutil.copy(lib_path, appdir / "usr/lib/libssl.so.1")
    subprocess.run([linuxdeploy_path, "--appdir", appdir], check=True)
    subprocess.run([download_dir / "linuxdeploy-plugin-qt-x86_64.AppImage", "--appdir", appdir], check=True)
    # change to the install directory, where the artefacts should be built to
    os.makedirs(install_dir, exist_ok=True)
    os.chdir(install_dir)
    # Copy validator manually from build dir since fix_bundle and appImage break it
    shutil.copy(args.build_dir + "/bin/gltf_validator", appdir / "usr/bin/gltf_validator")
    subprocess.run([download_dir/ "linuxdeploy-plugin-appimage-x86_64.AppImage", "--appdir", appdir], check=True)
