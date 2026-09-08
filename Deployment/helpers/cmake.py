import argparse
import subprocess
import pathlib
import os
import shutil
import multiprocessing
import json

# add args to the argparser that are used to configure cmake
def add_cmake_args(parser: argparse.ArgumentParser) -> argparse.ArgumentParser:
    group = parser.add_argument_group("cmake")
    group.add_argument('--cmake_root', default="..", help='specify the path to the cmake root directory')
    group.add_argument('--build_dir', default="../deploy", help='specify a custom build directory')
    group.add_argument('--package_json', required=True, help='Specify the path to a package json file containing version number and version name.')
    group.add_argument('--install_dir', default="../artefacts", help='specify a custom output directory for cmake to install the bundfle to')
    group.add_argument('--build_number', help="Incremental build number, used to identify different builds for the same release version")
    group.add_argument('--app_name', default="Gestaltor", help='Name of our application')
    group.add_argument('--output_name', default="Gestaltor", help='Name of the executable. Should be without spaces')
    group.add_argument('--generator', help='Specify the cmake generator (see cmake --help for a list)')
    group.add_argument('--msvc', action="store_true", help='Specify if msvc is used for building')
    group.add_argument('--ccache', action="store_true", help='Enable ccache')
    parser.add_argument('--package_only', action="store_true", help='Only packages an already provided build')
    parser.add_argument('--build_only', action="store_true", help='Do not package the build')
    parser.add_argument('--mac_arch', default="x86_64", help='The Mac architecture to build, arm64 or x86_64')
    return parser

def generate(args) -> pathlib.Path:
    cmake_root_path = pathlib.Path(args.cmake_root)
    build_dir_path = pathlib.Path(args.build_dir)
    appdir_path = pathlib.Path(args.install_dir)
    os.makedirs(build_dir_path, exist_ok=True)
    if not args.package_only: # Don't remove on package only
        if appdir_path.exists() and appdir_path.is_dir():
            shutil.rmtree(appdir_path)
    
        os.makedirs(appdir_path)

    # Load the version and version name from the package json
    with open(args.package_json, 'r', encoding="utf-8") as package_json_file:
        package_json = json.load(package_json_file)

    # run cmake generate
    cmake_command = ["cmake", 
        "-S", str(cmake_root_path), 
        "-B", str(build_dir_path),
        "-DCMAKE_BUILD_TYPE=Release", 
        f"-DCMAKE_INSTALL_PREFIX={appdir_path}",
        f"-DUX3D_PROJECT_NAME={args.app_name}",
        f"-DUX3D_OUTPUT_NAME={args.output_name}",
        "-DUX3D_GESTALTOR_BUNDLE_RESOURCES=TRUE",
        f"-DUX3D_ENABLE_CCACHE={args.ccache}",
        f"-DCMAKE_OSX_ARCHITECTURES={args.mac_arch}"
        ]

    if args.generator:
        cmake_command.append("-G")
        cmake_command.append(args.generator)

    if args.build_number:
        cmake_command.append(f'-DUX3D_BUILD_NUMBER={args.build_number}')

    if hasattr(args, "sign"):
        cmake_command.append(f'-DUX3D_CODESIGN_IDENTITY={args.sign}')

    if "version" in package_json:
        cmake_command.append("-DUX3D_VERSION={}".format(package_json["version"]))
    
    subprocess.run(cmake_command, check=True)

    return build_dir_path, appdir_path

def build(args, build_dir_path, threads=16, target="Gestaltor"):
    if (args.msvc):
        subprocess.run(["cmake", "--build", str(build_dir_path), "--config", "Release", "--target", target], check=True)
    else:
        subprocess.run(["cmake", "--build", str(build_dir_path), "--config", "Release", "--parallel", str(threads), "--target", target], check=True)

def install(args, build_dir_path):
    # run cmake install
    subprocess.run(["cmake", "--install", str(build_dir_path), "--component", "Runtime"], check=True)

def run(args) -> pathlib.Path:
    #We need to download qt6 for packaging
    builddir, appdir = generate(args)
    if not args.package_only:
        threads = multiprocessing.cpu_count()
        build(args, builddir, threads)
        install(args, builddir)
    return appdir
