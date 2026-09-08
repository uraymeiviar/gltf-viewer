#!python3

import argparse
from operator import sub
import os
import pathlib
import subprocess
from helpers import codesign_recursive_macos, cmake
import shutil
import re
import time

def print_status(message):
    OKCYAN = '\033[96m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    print('\n' + BOLD + OKCYAN + message + ENDC)

# arguments
parser = argparse.ArgumentParser(description='Build and package the macOS version')
cmake.add_cmake_args(parser)
parser.add_argument('-s', "--sign", help='specify a codesign identity (should be of type "Developer ID Application" or "Apple Distribution")')
parser.add_argument('--notarize', action='store_true', help='notarize the application and stamp the notarization ticket to it. This will take up to an hour to complete.')
parser.add_argument('-u', '--username', help="username of app store connect, required by notarization and app store. Usually corresponds to your company AppleID email address.")
parser.add_argument('-p', '--password', help="password of app store connect, required by notarization and app store. You should use a device specific password")
parser.add_argument('-t', '--team', help="The app store connect team ID.")
parser.add_argument('--headless', action='store_true', help="run this script in headless mode, remove all dependencies on the GUI of macOS")
parser.add_argument('--dmg', action='store_true', help="Create a .dmg file, necessary for releasing")
parser.add_argument('--appstore_connect_upload', action='store_true', help="Verify the bundle and upload to appstore connect for release.")
parser.add_argument('--auth_key', help="Auth key ID for use with App Store Connect. The .p8 file must be present in one of the following locations: './private_keys', '~/private_keys', '~/.private_keys', '~/.appstoreconnect/private_keys'")
parser.add_argument('--api_issuer', help="App Store Connect API issuer ID, required with --appstore_connect_upload")
args = parser.parse_args()
args.output_name = args.output_name if args.output_name != "" else args.app_name

# Always deploy with the Xcode generator to ensure the resulting bundle is properly codesigned and self contained
args.generator = "Xcode"

# check input
if args.notarize:
    if not args.sign:
        raise RuntimeError("Notarization is only possible with a signed application. Specify an identity with '--sign'")
    # check if the provided identity is suitable for notarization
    codesign_identities = subprocess.check_output("security find-identity -v -p codesigning", shell=True)
    codesign_identities = list(filter(lambda line: args.sign.encode('utf-8') in line, codesign_identities.splitlines()))
    if not codesign_identities:
        raise RuntimeError("Provided codesign identity not found in 'security find-identity -v -p codesigning'")
    if not b"Developer ID" in codesign_identities[0]:
        raise RuntimeError("Provided codesign identity is not of type 'Develper ID', which is required for successfull notarization")
    if not args.username and args.password:
        raise RuntimeError("Username and password are required for notarization")
    subprocess.check_output(["xcrun", "altool", "--list-providers", "-u", args.username, "-p", args.password])


if args.appstore_connect_upload:
    if not args.build_number:
        raise RuntimeError("App Store Connect upload requires the --build_number argument. \n\n{}".format())
    if not args.auth_key:
        raise RuntimeError(f"App Store Connect upload requires the --auth_key argument. \n\n{parser.format_usage()}")
    if not args.api_issuer:
        raise RuntimeError(f"App Store Connect upload requires the --api_issuer argument. \n\n{parser.format_usage()}")

if args.notarize:
    if not args.username or not args.password or not args.team:
        raise RuntimeError(f"Notarization requires you to specify the '--username', '--password' and '--team' arguments. \n\n{parser.format_usage()}")

print_status("CMake Generate")
build_dir, artefacts_dir = cmake.generate(args)

if not args.package_only:
    print_status("Xcode Build")
    subprocess.run(["xcodebuild", "archive", "-project", build_dir / "Gestaltor.xcodeproj", "-quiet", "-scheme", "Gestaltor", "-archivePath", artefacts_dir / "Gestaltor.xcarchive"], check=True)

if args.appstore_connect_upload:
    # Upload the xcarchive to app store connect with the new xcodebuild based tool
    # https://developer.apple.com/videos/play/wwdc2021/10204/ ~ from 17:30
    print_status("Upload the archive to App Store Connect")
    subprocess.run([
        "xcodebuild", 
        "-exportArchive", 
        "-archivePath", artefacts_dir / "Gestaltor.xcarchive", 
        "-exportOptionsPlist", "ExportMacAppStore.plist", 
        "-exportPath", artefacts_dir,
        ], check=True)

    # Auth keys must be in one of the following locations: './private_keys', '~/private_keys', '~/.private_keys', '~/.appstoreconnect/private_keys'
    subprocess.run([
        "xcrun", 
        "altool",
        "--upload-app",
        "-f", artefacts_dir / "Gestaltor.pkg",
        "--type", "macos",
        "--apiIssuer", args.api_issuer,
        "--apiKey", args.auth_key,
    ])

app_dir = artefacts_dir / "Gestaltor.xcarchive/Products/Applications/{}.app".format(args.output_name)

# Sign manually if only packaging
if args.sign and args.package_only:
    print_status("Sign the patched application")
    # sign the executable 
    subprocess.run(["codesign", "--timestamp",'--entitlements', 'entitlements.plist', "--force", "--options", "runtime",  "-s", args.sign, app_dir / "Contents"/ "MacOS" / args.output_name])
    # renew codesign for the whole app
    subprocess.run(["codesign", "--timestamp",'--entitlements', 'entitlements.plist', "--force", "--options", "runtime",  "-s", args.sign, app_dir])
    print("Success")

if args.notarize:
    # create .zip containing application
    print_status("Preparing app for submission to notarization")
    subprocess.check_output(["ditto", "-c", "-k", "--keepParent", app_dir, artefacts_dir / "{}.zip".format(args.output_name)])

    print_status("")

    # submit .zip to notarization
    print_status("Submitting the app for notarization")
    notarization_command = ["xcrun", "notarytool", "submit", artefacts_dir / f"{args.output_name}.zip", "--apple-id", args.username, "--password", args.password, "--team-id", args.team, "--wait"]
    notary_output = subprocess.check_output(notarization_command).decode("utf-8")
    if not "status: Accepted" in notary_output:
        submissionId = re.search("id: (.+)", notary_output).group(1)
        print(submissionId)
        submissionLog = subprocess.check_output(["xcrun", "notarytool", "log", submissionId, "--apple-id", args.username, "--password", args.password, "--team-id", args.team]).decode("utf-8")
        raise RuntimeError(f"Errors in notarization submission: \n{notary_output} \n\n {submissionLog}")

    print_status("Notarization successfull.")

    # staple the notarization ticket to the app
    print_status("Stapling notarization ticket to .app")
    subprocess.run(["xcrun", "stapler", "staple", app_dir])

    # remove intermediate files
    os.remove(artefacts_dir / "{}.zip".format(args.output_name))

# cleanup
shutil.rmtree(artefacts_dir / "lib", ignore_errors=True)
shutil.rmtree(artefacts_dir / "include", ignore_errors=True)

# create a disk image
if args.dmg:
    print_status("\nCreate .dmg")
    dmg_dir = artefacts_dir / "dmg"
    os.makedirs(dmg_dir, exist_ok=True)
    shutil.copytree(app_dir,  dmg_dir / f"{args.output_name}.app", symlinks=True)
    os.symlink("/Applications", dmg_dir / "Applications")
    subprocess.run(["hdiutil", "create", "-volname", args.output_name, "-srcfolder", dmg_dir, "-ov", "-format", "UDZO", artefacts_dir / f"{args.output_name}.dmg"])
    # Cleanup
    shutil.rmtree(dmg_dir)
    # Remove other artefacts (we only want to create the .dmg)
    if (artefacts_dir/ f"{args.output_name}.app").exists() and (artefacts_dir/ f"{args.output_name}.app").is_dir():
        shutil.rmtree(artefacts_dir / f"{args.output_name}.app")
    shutil.rmtree(artefacts_dir / "Gestaltor.xcarchive")

if not args.headless:
    subprocess.call(["open", artefacts_dir])
