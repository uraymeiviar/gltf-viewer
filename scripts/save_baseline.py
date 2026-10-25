# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Saves the current Release build as the regression baseline for the checked-out commit.

Run it right after committing (and building), so build/baselines/<short sha>.exe is the
binary of that commit. scripts/run_regression.py compares later builds with it.

Refuses when tracked files differ from HEAD (the binary would not match the commit) or
when the binary is older than the newest tracked source change, unless --force is given.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
BASELINES = REPO / "build" / "baselines"

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--force", action="store_true", help="save even if the tree or build looks out of date")
args = parser.parse_args()


def git(*command):
    return subprocess.run(["git", *command], cwd=REPO, capture_output=True, text=True, check=True).stdout.strip()


sha = git("rev-parse", "--short", "HEAD")
dirty = [line for line in git("status", "--porcelain", "--untracked-files=no").splitlines() if line.strip()]
if not EXE.exists():
    sys.exit(f"no build at {EXE}; build first: cmake --build build --config Release --target Glider")
if dirty and not args.force:
    sys.exit("tracked files differ from HEAD, so the build would not match the commit:\n  " + "\n  ".join(dirty[:10])
             + "\ncommit (or stash) first, rebuild, then rerun; --force overrides")
commit_time = int(git("log", "-1", "--format=%ct"))
if EXE.stat().st_mtime < commit_time - 3600 and not args.force:
    sys.exit(f"{EXE.name} is older than commit {sha}; rebuild first (--force overrides)")

BASELINES.mkdir(parents=True, exist_ok=True)
target = BASELINES / f"{sha}.exe"
shutil.copy2(EXE, target)
if os.name == "nt":
    # The copy inherits build/'s Low integrity label (agent sandboxes label the workspace), and Windows
    # runs a Low-labelled exe as a Low-integrity process that prompts on start. Give it the normal
    # Medium label, as CMake's post-build step does for Glider.exe.
    subprocess.run(["icacls", str(target), "/setintegritylevel", "M"], stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, check=False)
print(f"saved {target.relative_to(REPO)}")
