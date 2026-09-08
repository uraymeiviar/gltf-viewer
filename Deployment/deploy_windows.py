#!python3

import argparse
import subprocess
import shutil
import os
import requests
from pathlib import Path
from helpers import cmake

parser = argparse.ArgumentParser(description='Build and package the windows version')
cmake.add_cmake_args(parser)
parser.add_argument("-s", "--sign", action="store_true", help="sign the application using the Windows signtool")
parser.add_argument('--no_build', action='store_true', help="Do not run build. Useful for signing/notarization only")
args = parser.parse_args()
args.output_name = args.output_name.replace(" ", "_") if args.output_name != "" else args.app_name.replace(" ", "_")
args.msvc = True

if args.msvc and args.generator is not None:
	print("Warning: You need to call vcvars.bat before running this script with a different generator")

cmake_root_path = Path(args.cmake_root)
build_dir_path = Path(args.build_dir)
should_sign = args.sign
application_output_path = Path("packages/ux3d.gestaltor/data")


redistDownload = requests.get("https://aka.ms/vs/16/release/vc_redist.x64.exe")
if redistDownload.status_code != requests.codes.ok:
    sys.exit("Failed to download VC_redist.x64.exe")

with open(application_output_path / "VC_redist.x64.exe",'wb') as f:
    f.write(redistDownload.content)

if not args.no_build:
	cmake.run(args)
	build_folder = "bin/Release"
	if args.generator is not None:
		build_folder = "bin"
	shutil.copy(build_dir_path / f'{build_folder}/{args.output_name}.exe', application_output_path)
	for file_path in (build_dir_path / f"{build_folder}").rglob("*.dll"):
		shutil.copy(file_path, application_output_path)

	for file_path in (build_dir_path / f"{build_folder}").rglob("*.exe"):
		shutil.copy(file_path, application_output_path)

	shutil.copytree(build_dir_path / f"{build_folder}/usd", application_output_path / "usd", dirs_exist_ok=True)

	subprocess.run([ build_dir_path / "qt/bin/windeployqt.exe", application_output_path / "{}.exe".format(args.output_name), "-qmldir={}/editor/qml".format(str(cmake_root_path)) ], check=True)

ssl_dir = os.environ.get("SSL_DIR")
if ssl_dir:
	ssl_dir_path = Path(ssl_dir)
	shutil.copy(ssl_dir_path / "libssl-1_1-x64.dll", application_output_path)
	shutil.copy(ssl_dir_path / "libcrypto-1_1-x64.dll", application_output_path)

# if should_sign:
# 	subprocess.run([ "signtool", "sign", "/a", "/fd", "SHA256", "/td", "SHA256" , "/tr", "http://timestamp.digicert.com", str(application_output_path / "{}.exe".format(args.output_name)) ], check=True)
# 	for dll_path in application_output_path.rglob("*.dll"):
# 		subprocess.run([ "signtool", "sign", "/a", "/fd", "SHA256", "/td", "SHA256" , "/tr", "http://timestamp.digicert.com", str(dll_path) ], check=True)

installerName = "{}_Installer.exe".format(args.output_name)

subprocess.run([ build_dir_path / "qt/binarycreator.exe", "--offline-only", "-c", "config/config.xml", "-p", "packages", installerName ], check=True)
# if should_sign:
	# subprocess.run([ "signtool", "sign", "/a", "/fd", "SHA256", "/td", "SHA256", "/tr", "http://timestamp.digicert.com", installerName ], check=True)
