#!/usr/bin/env python3
"""Consistency check between the settings registry, the command-line parsers and the docs.

Runs `Glider.exe --isolated --agent-guide <file>` (no window, no GL context) and compares:

  1. every registered setting key (non-compatibility) with docs/CLI_REFERENCE.md;
  2. every `--flag` string literal in the CLI parsers (CommandLine*.cpp, Application.cpp, RenderQuality.cpp)
     with docs/CLI_REFERENCE.md, the other docs/*.md files and the agent guide;
  3. every `cli` flag the registry declares with a parser for it.

A key or flag that CLI_REFERENCE.md lacks but another docs/*.md file or the agent guide (hand-written text or the
generated settings table) mentions is reported as a note; one that appears in no doc text except the generated table is a gap. Usage:

  python scripts/check_settings_docs.py [--exe path] [--strict]

Exit code 0 unless a real gap exists (undocumented flag, undocumented key, or a declared flag without a parser);
--strict also fails on keys and flags that CLI_REFERENCE.md lacks but another doc or the agent guide has.
"""

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_EXE = ROOT / "build" / "bin" / "Release" / "Glider.exe"
CLI_DOC = ROOT / "docs" / "CLI_REFERENCE.md"
AGENT_GUIDE_SRC = ROOT / "resources" / "agent" / "GLIDER_AGENT_GUIDE.md"
PARSER_FILES = sorted((ROOT / "source" / "imgui" / "application").glob("CommandLine*.cpp")) + [
    ROOT / "source" / "imgui" / "application" / "Application.cpp",
    ROOT / "source" / "imgui" / "RenderQuality.cpp",
]
FLAG_LITERAL = re.compile(r'"(--[a-z0-9][a-z0-9-]*[a-z0-9])"')  # whole-literal flags; prefix tests like "--ibl-" are skipped
FLAG_IN_TEXT = re.compile(r"--[a-z0-9][a-z0-9-]*")
ROW_SPLIT = re.compile(r"(?<!\\)\|")


def run_guide(exe: Path) -> str:
    with tempfile.TemporaryDirectory() as tmp:
        target = Path(tmp) / "guide.md"
        env = {"GLIDER_NO_FOCUS": "1"}
        import os
        full_env = dict(os.environ)
        full_env.update(env)
        result = subprocess.run([str(exe), "--isolated", "--agent-guide", str(target)], capture_output=True, text=True,
                                timeout=120, env=full_env)
        if result.returncode != 0 or not target.exists():
            sys.exit(f"--agent-guide failed (exit {result.returncode}): {result.stderr.strip()}")
        return target.read_text(encoding="utf-8")


def parse_settings(guide: str):
    """Rows of the generated settings table: [(key, scope, cli, help)]."""
    rows = []
    in_table = False
    for line in guide.splitlines():
        if line.startswith("## Settings reference"):
            in_table = True
            continue
        if not in_table or not line.startswith("| `"):
            continue
        cells = [c.strip() for c in ROW_SPLIT.split(line.strip().strip("|"))]
        if len(cells) < 7:
            continue
        key = cells[0].strip("`")
        cli = cells[5].replace("\\|", "|").strip("`")
        rows.append((key, cells[1], cli, cells[6]))
    return rows


def flags_in_parsers():
    flags = {}
    for path in PARSER_FILES:
        for flag in FLAG_LITERAL.findall(path.read_text(encoding="utf-8", errors="replace")):
            flags.setdefault(flag, path.name)
    return flags


def mentions(text: str, token: str) -> bool:
    return re.search(r"(?<![A-Za-z0-9_-])" + re.escape(token) + r"(?![A-Za-z0-9_-])", text) is not None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--strict", action="store_true", help="also fail on keys documented only in the generated table")
    args = parser.parse_args()
    if not args.exe.exists():
        sys.exit(f"{args.exe} not found (build first)")

    guide = run_guide(args.exe)
    cli_doc = CLI_DOC.read_text(encoding="utf-8")
    hand_guide = AGENT_GUIDE_SRC.read_text(encoding="utf-8")
    other_docs = "\n".join(p.read_text(encoding="utf-8", errors="replace")
                           for p in sorted((ROOT / "docs").glob("*.md")) if p != CLI_DOC)

    settings = parse_settings(guide)
    canonical = [row for row in settings if not row[3].startswith("Compatibility key")]
    parsed = flags_in_parsers()
    failures = 0

    # 1. registry keys in CLI_REFERENCE.md
    seen = set()
    table_only, undocumented = [], []
    for key, scope, _cli, _help in canonical:
        if key in seen:
            continue
        seen.add(key)
        if mentions(cli_doc, key):
            continue
        if mentions(hand_guide, key) or mentions(other_docs, key):
            table_only.append((key, scope))
        else:
            undocumented.append((key, scope))
    print(f"registry: {len(seen)} canonical keys ({len(settings)} rows incl. compatibility keys)")
    print(f"  in CLI_REFERENCE.md: {len(seen) - len(table_only) - len(undocumented)}")
    if table_only:
        print(f"  not in CLI_REFERENCE.md, but in another doc or the hand-written guide ({len(table_only)}):")
        for key, scope in table_only:
            print(f"    {key} ({scope})")
        if args.strict:
            failures += len(table_only)
    if undocumented:
        print(f"  UNDOCUMENTED (generated table only) ({len(undocumented)}):")
        for key, scope in undocumented:
            print(f"    {key} ({scope})")
        failures += len(undocumented)

    # 2. parsed flags documented (CLI_REFERENCE.md > another doc / the agent guide > nowhere)
    guide_text = hand_guide + "\n" + guide
    nowhere, elsewhere = [], []
    for flag in sorted(parsed):
        if mentions(cli_doc, flag):
            continue
        (elsewhere if mentions(other_docs, flag) or mentions(guide_text, flag) else nowhere).append(flag)
    print(f"cli parsers: {len(parsed)} flags in {', '.join(sorted({p.name for p in PARSER_FILES}))}")
    print(f"  in CLI_REFERENCE.md: {len(parsed) - len(elsewhere) - len(nowhere)}")
    if elsewhere:
        print(f"  not in CLI_REFERENCE.md, but in another doc or the agent guide ({len(elsewhere)}):")
        for flag in elsewhere:
            print(f"    {flag}")
        if args.strict:
            failures += len(elsewhere)
    if nowhere:
        print(f"  UNDOCUMENTED FLAGS ({len(nowhere)}):")
        for flag in nowhere:
            print(f"    {flag} ({parsed[flag]})")
        failures += len(nowhere)

    # 3. declared registry flags have a parser
    unparsed = []
    for key, scope, cli, _help in canonical:
        for flag in FLAG_IN_TEXT.findall(cli):
            if flag not in parsed:
                unparsed.append((key, scope, flag))
    if unparsed:
        print(f"  REGISTRY FLAGS WITHOUT A PARSER ({len(unparsed)}):")
        for key, scope, flag in unparsed:
            print(f"    {flag} (key {key}, {scope})")
        failures += len(unparsed)

    print("PASS" if failures == 0 else f"FAIL: {failures} gap(s)")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
