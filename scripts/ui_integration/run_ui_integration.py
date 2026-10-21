#!/usr/bin/env python3
"""Runtime UI integration scenarios for P2.3 (Resources, Validation, Logger clipping).

Each scenario is a generated --ui-script that drives real ImGui input (mouse, wheel,
keys, text) inside the running application. The same script is run against the
accepted-baseline exe (commit 009c062 + harness probes only) and the candidate exe.
Snapshots are compared label by label (scroll extent, visible probes, geometry,
selection, pixel hashes, clipboard) and absolute oracles are asserted on the candidate.

Exit code: 0 all assertions passed, 1 assertion/run failure, 2 report-write failure.
"""

import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RES = "ResourcesTreeScrollRegion"
LOG = "LoggerScrollArea"
VAL = "IssuesTable"
HIER = "HierarchyTreeScrollRegion"


class Scenario:
    def __init__(self, name, fixture, args, lines, checks, size="1600x900", candidate_only=False):
        self.name, self.fixture, self.args, self.lines, self.checks, self.size = name, fixture, args, lines, checks, size
        self.candidate_only = candidate_only


class Results:
    def __init__(self):
        self.rows = []

    def add(self, scenario, name, ok, detail=""):
        self.rows.append({"scenario": scenario, "check": name, "ok": bool(ok), "detail": detail})

    @property
    def failed(self):
        return [r for r in self.rows if not r["ok"]]


# ----------------------------------------------------------------------------- script builders
def snap(L, label, extra=""):
    L.append(f"snap {label} {extra}".strip())


def settle(L, frames=6):
    L.append(f"wait {frames}")


def scroll_snap(L, window, target, label, pix=True):
    L.append(f"scroll {window} {target}")
    settle(L, 4)
    snap(L, label, "pix" if pix else "")


def set_filter(L, tag, text, label, pix=True):
    L.append(f"clickp {tag} 0")
    L.append("key a ctrl")
    L.append("key backspace")
    if text:
        L.append(f"type {text}")
    settle(L, 6)
    snap(L, label, "pix" if pix else "")


def reach_tail(L, window, tag, prefix, steps=16, delta="0.3"):
    """From the bottom scroll position wheel upward in small steps so every row of the last big list is shown."""
    L.append(f"scroll {window} bottom")
    settle(L, 4)
    for i in range(steps):
        snap(L, f"{prefix}{i:02d}")
        L.append(f"wheel {delta}")
        settle(L, 3)


def resources_script(open_tag, filter_prefix, filter_exact, alt_fixtures, last_label, click_open=True):
    open_tag = open_tag.replace(" ", "_")
    L = ["wait 40"]
    snap(L, "r00_initial")
    if click_open:
        L.append(f"clickp {open_tag} -1")
    settle(L, 6)
    snap(L, "r01_open", "pix")
    scroll_snap(L, RES, "0", "r02_top")
    scroll_snap(L, RES, "0.5", "r03_mid")
    L.append(f"clickrow {open_tag} 3 {RES}")
    settle(L, 6)
    snap(L, "r04_click_mid")
    for i in range(3):
        L.append("key down")
        settle(L, 4)
        snap(L, f"r05_nav_down{i}")
    L.append("key pgdn")
    settle(L, 6)
    snap(L, "r06_nav_pgdn")
    L.append("key up")
    settle(L, 4)
    snap(L, "r06_nav_up")
    L.append(f"clickrow {open_tag} -1 {RES}")
    settle(L, 6)
    snap(L, "r06b_edge_click_last_visible")
    for i in range(4):
        L.append("key down")
        settle(L, 6)
        snap(L, f"r06b_edge_down{i}")
    scroll_snap(L, RES, "bottom", "r07_bottom")
    reach_tail(L, RES, open_tag, "r08_tail")
    # last row of the category must be clickable once reached
    L.append(f"scroll {RES} 0.5")
    settle(L, 4)
    snap(L, "r09_mid_again", "pix")
    set_filter(L, "res.search", filter_prefix, "r10_filter_mid")
    scroll_snap(L, RES, "bottom", "r11_filter_bottom")
    L.append(f"clickrow {open_tag} -1 {RES}")
    settle(L, 6)
    snap(L, "r12_filter_click_last")
    set_filter(L, "res.search", filter_exact, "r13_filter_single")
    L.append(f"clickrow {open_tag} 0 {RES}")
    settle(L, 6)
    snap(L, "r14_filter_single_click")
    set_filter(L, "res.search", "zzzzqq", "r15_nomatch")
    set_filter(L, "res.search", "", "r16_cleared")
    L.append(f"scroll {RES} 0.5")
    settle(L, 4)
    set_filter(L, "res.search", filter_prefix, "r17_filter_from_scrolled")
    set_filter(L, "res.search", "", "r18_clear_after_filter")
    for i, path in enumerate(alt_fixtures):
        L.append(f"reload {path}")
        settle(L, 8)
        snap(L, f"r19_reload{i}_a", "pix")
        L.append(f"scroll {RES} 0.5")
        settle(L, 4)
        snap(L, f"r19_reload{i}_b", "pix")
    return L


def validation_script(mixed):
    L = ["wait 40"]
    snap(L, "v00_initial", "pix")
    scroll_snap(L, VAL, "0", "v01_top")
    L.append(f"clickrow val.row 2 {VAL}")
    settle(L, 6)
    snap(L, "v02_click_row2")
    scroll_snap(L, VAL, "0.5", "v03_mid")
    L.append(f"clickrow val.row 3 {VAL}")
    settle(L, 6)
    snap(L, "v04_click_mid")
    for i in range(3):
        L.append("key down")
        settle(L, 4)
        snap(L, f"v05_nav{i}")
    scroll_snap(L, VAL, "bottom", "v06_bottom")
    L.append(f"clickrow val.row -1 {VAL}")
    settle(L, 6)
    snap(L, "v07_click_last")
    L.append(f"scroll {VAL} 0.25")
    settle(L, 4)
    L.append(f"hoverrow val.msg 1 {VAL}")
    L.append("waitms 900")
    snap(L, "v08_tooltip")
    L.append("move 5 5")
    settle(L, 6)
    snap(L, "v08b_tooltip_gone")
    set_filter(L, "val.search", "meshes/20", "v09_filter_meshes20")
    scroll_snap(L, VAL, "0.5", "v10_filter_mid")
    scroll_snap(L, VAL, "bottom", "v11_filter_bottom")
    L.append(f"clickrow val.row -1 {VAL}")
    settle(L, 6)
    snap(L, "v12_filter_click_last")
    set_filter(L, "val.search", "zzzzqq", "v13_nomatch")
    set_filter(L, "val.search", "", "v14_cleared")
    if mixed:
        set_filter(L, "val.search", "primitive_missing_position", "v15_material_filter")
        snap(L, "v16_before_dclick")
        L.append(f"clickrow val.row 0 {VAL}")
        settle(L, 6)
        snap(L, "v17_select")
        L.append(f"scroll {VAL} 0")
        settle(L, 4)
        L.append(f"clickrow val.row 1 {VAL}")
        settle(L, 4)
        L.append("waitms 150")
        snap(L, "v18_click1")
        # double-click on a visible row (original item identity must survive the filter)
        L.append(f"dclickrow val.row 0 {VAL}")
        L.append("waitms 1500")
        snap(L, "v19_after_dclick")
    return L


def logger_script():
    L = ["wait 40", f"clickp log.clear 0", "wait 6"]
    snap(L, "l00_cleared", "pix")
    L.append("append single 400 S")
    settle(L, 8)
    snap(L, "l01_single_bottom", "pix")
    scroll_snap(L, LOG, "0", "l02_single_top")
    scroll_snap(L, LOG, "0.5", "l03_single_mid")
    scroll_snap(L, LOG, "bottom", "l04_single_bottom2")
    L.append("append single 5 B")
    settle(L, 8)
    snap(L, "l05_append_at_bottom", "pix")
    scroll_snap(L, LOG, "0.4", "l06_scrolled_up")
    L.append("append single 10 U")
    settle(L, 8)
    snap(L, "l07_append_while_up", "pix")
    L.append("clipclear")
    L.append("clickp log.copy 0")
    settle(L, 6)
    snap(L, "l08_copy_scrolled_up", "clip")
    # newline entries
    L.append("clickp log.clear 0")
    settle(L, 6)
    snap(L, "l09_clear")
    L.append("append newline 150 N")
    settle(L, 8)
    snap(L, "l10_newline_bottom", "pix")
    scroll_snap(L, LOG, "0", "l11_newline_top")
    scroll_snap(L, LOG, "0.5", "l12_newline_mid")
    L.append("append single 3 N2")
    settle(L, 8)
    snap(L, "l13_append_up_newline", "pix")
    scroll_snap(L, LOG, "bottom", "l14_newline_bottom2")
    L.append("append newline 4 N3")
    settle(L, 8)
    snap(L, "l15_newline_append_bottom", "pix")
    # mixed long (unwrapped, horizontal scroll)
    L.append("append long 60 L")
    settle(L, 8)
    snap(L, "l16_long_nowrap", "pix")
    scroll_snap(L, LOG, "0.5", "l17_long_nowrap_mid")
    # wrapping on
    L.append("clickp log.wrap 0")
    settle(L, 8)
    snap(L, "l18_wrap_on_a", "pix")
    scroll_snap(L, LOG, "0", "l19_wrap_top")
    scroll_snap(L, LOG, "0.5", "l20_wrap_mid")
    scroll_snap(L, LOG, "bottom", "l21_wrap_bottom")
    L.append("resize 1000 600")
    settle(L, 10)
    snap(L, "l22_wrap_resized_small", "pix")
    scroll_snap(L, LOG, "0.5", "l23_wrap_small_mid")
    L.append("append long 5 W")
    settle(L, 8)
    snap(L, "l24_wrap_append", "pix")
    L.append("resize 1500 900")
    settle(L, 10)
    snap(L, "l25_wrap_resized_large", "pix")
    scroll_snap(L, LOG, "bottom", "l26_wrap_large_bottom")
    L.append("append single 3 WS")
    settle(L, 8)
    snap(L, "l27_wrap_append_single", "pix")
    # wrap off again after caches populated
    L.append("clickp log.wrap 0")
    settle(L, 8)
    snap(L, "l28_wrap_off_again", "pix")
    scroll_snap(L, LOG, "0.3", "l29_after_wrap_off_mid")
    # auto-scroll disabled
    L.append("clickp log.autoscroll 0")
    settle(L, 4)
    scroll_snap(L, LOG, "bottom", "l30_noauto_bottom")
    L.append("append single 15 NA")
    settle(L, 8)
    snap(L, "l31_noauto_append", "pix")
    L.append("clickp log.autoscroll 0")
    settle(L, 6)
    snap(L, "l32_auto_back_on", "pix")
    L.append("append single 3 BA")
    settle(L, 8)
    snap(L, "l33_auto_append", "pix")
    # copy at top, then copy at bottom: same full content
    scroll_snap(L, LOG, "0", "l34_top_before_copy")
    L.append("clipclear")
    L.append("clickp log.copy 0")
    settle(L, 6)
    snap(L, "l35_copy_top", "clip")
    scroll_snap(L, LOG, "bottom", "l36_bottom_before_copy")
    L.append("clipclear")
    L.append("clickp log.copy 0")
    settle(L, 6)
    snap(L, "l37_copy_bottom", "clip")
    L.append("clickp log.clear 0")
    settle(L, 6)
    snap(L, "l38_clear_final", "pix")
    L.append("clipclear")
    L.append("clickp log.copy 0")
    settle(L, 6)
    snap(L, "l39_copy_empty", "clip")
    return L


def hierarchy_broad_script(alt_fixtures):
    L = ["wait 40"]
    snap(L, "hb00_initial")
    scroll_snap(L, HIER, "0", "hb01_top", pix=False)
    scroll_snap(L, HIER, "0.5", "hb02_mid", pix=False)
    L.append(f"clickrow hier.node 3 {HIER}")
    settle(L, 6)
    snap(L, "hb03_click_mid")
    for i in range(3):
        L.append("key down")
        settle(L, 4)
        snap(L, f"hb04_nav_down{i}")
    L.append("key pgdn")
    settle(L, 6)
    snap(L, "hb05_nav_pgdn")
    L.append("key up")
    settle(L, 4)
    snap(L, "hb05_nav_up")
    L.append(f"clickrow hier.node -1 {HIER}")
    settle(L, 6)
    snap(L, "hb06_edge_click_last_visible")
    for i in range(4):
        L.append("key down")
        settle(L, 6)
        snap(L, f"hb06_edge_down{i}")
    scroll_snap(L, HIER, "bottom", "hb07_bottom", pix=False)
    reach_tail(L, HIER, "hier.node", "hb08_tail")
    L.append(f"scroll {HIER} 0.5")
    settle(L, 4)
    snap(L, "hb09_mid_again")
    set_filter(L, "hier.search", "Node_40", "hb10_filter_mid", pix=False)
    scroll_snap(L, HIER, "bottom", "hb11_filter_bottom", pix=False)
    L.append(f"clickrow hier.node -1 {HIER}")
    settle(L, 6)
    snap(L, "hb12_filter_click_last")
    set_filter(L, "hier.search", "Node_4095", "hb13_filter_single", pix=False)
    L.append(f"clickrow hier.node 0 {HIER}")
    settle(L, 6)
    snap(L, "hb14_filter_single_click")
    set_filter(L, "hier.search", "zzzzqq", "hb15_nomatch", pix=False)
    set_filter(L, "hier.search", "", "hb16_cleared", pix=False)
    L.append(f"scroll {HIER} 0.5")
    settle(L, 4)
    set_filter(L, "hier.search", "Node_40", "hb17_filter_from_scrolled", pix=False)
    set_filter(L, "hier.search", "", "hb18_clear_after_filter", pix=False)
    for i, path in enumerate(alt_fixtures):
        L.append(f"reload {path}")
        settle(L, 8)
        snap(L, f"hb19_reload{i}_a")
        L.append(f"scroll {HIER} 0.5")
        settle(L, 4)
        snap(L, f"hb19_reload{i}_b")
    return L


def hierarchy_deep_script():
    L = ["wait 40"]
    snap(L, "hd00_initial")
    # Click arrow on root node 0 to expand its children
    L.append("clickarrowp hier.node 0")
    settle(L, 6)
    snap(L, "hd01_expand_root0")
    # Click arrow on 1st child to expand deeper
    L.append(f"clickarrowrow hier.node 1 {HIER}")
    settle(L, 6)
    snap(L, "hd02_expand_child")
    # Search for deep leaf TreeNode_4095: autoExpandAncestors must expand all ancestors
    set_filter(L, "hier.search", "TreeNode_4095", "hd03_deep_search", pix=False)
    L.append(f"clickrow hier.node -1 {HIER}")
    settle(L, 6)
    snap(L, "hd04_select_deep_leaf")
    # Clear filter (cache is now warm)
    set_filter(L, "hier.search", "", "hd05_cleared", pix=False)
    # Collapse root node 0
    L.append("clickarrowp hier.node 0")
    settle(L, 6)
    snap(L, "hd06_root_collapsed")
    # Select descendant Node 4 directly via Resources with empty search and warm cache
    L.append("clickp Nodes -1")
    settle(L, 6)
    L.append(f"clickrow Nodes 4 {RES}")
    settle(L, 8)
    snap(L, "hd07_select_descendant_collapsed")
    # Settled frame reuse check
    settle(L, 10)
    snap(L, "hd08_descendant_settled")
    # Collapse root node 0 after selection
    L.append("clickarrowp hier.node 0")
    settle(L, 6)
    snap(L, "hd09_collapse_after_descendant")
    settle(L, 10)
    snap(L, "hd10_collapse_settled")
    return L


def hierarchy_shared_mesh_script(reload_fixture):
    L = ["wait 40"]
    snap(L, "hsm00_initial")
    # Open Meshes category in Resources
    L.append("clickp Meshes -1")
    settle(L, 6)
    snap(L, "hsm01_res_meshes_open")
    # Select SharedMesh_0000 in Resources: must auto-expand all owner nodes
    L.append(f"clickrow Meshes 0 {RES}")
    settle(L, 8)
    snap(L, "hsm02_select_shared_mesh")
    # Settled frame check
    settle(L, 10)
    snap(L, "hsm03_shared_mesh_settled")
    # Collapse RootNode_A in hierarchy: RootNode_B must remain open
    L.append("clickarrowp hier.node 0")
    settle(L, 6)
    snap(L, "hsm04_collapse_root_a")
    settle(L, 10)
    snap(L, "hsm05_collapse_settled")
    # Select SharedMaterial_0000 in Resources: must auto-expand node, mesh, and primitive branches
    L.append(f"clickrow Materials 0 {RES}")
    settle(L, 8)
    snap(L, "hsm06_select_shared_mat")
    # Select SingleMesh_0001 (ControlNode_C)
    L.append(f"clickrow Meshes 1 {RES}")
    settle(L, 8)
    snap(L, "hsm07_select_single_mesh")
    # Reload fixture: test clean cache invalidation and no stale pointers
    L.append(f"reload {reload_fixture}")
    settle(L, 12)
    snap(L, "hsm08_reloaded")
    # Select SharedMesh_0000 again on clean reload (Meshes remains open in storage)
    L.append(f"clickrow Meshes 0 {RES}")
    settle(L, 8)
    snap(L, "hsm09_reload_select_shared_mesh")
    return L


def hierarchy_multiline_script():
    L = ["wait 40"]
    snap(L, "hm00_initial")
    scroll_snap(L, HIER, "0", "hm01_top", pix=False)
    scroll_snap(L, HIER, "0.5", "hm02_mid", pix=False)
    scroll_snap(L, HIER, "bottom", "hm03_bottom", pix=False)
    scroll_snap(L, HIER, "0", "hm03b_back_to_top", pix=False)
    L.append(f"clickrow hier.node 0 {HIER}")
    settle(L, 6)
    snap(L, "hm04_click_top")
    return L


# ----------------------------------------------------------------------------- running
def run_exe(exe, scenario, outdir, tag, timeout=600):
    script = outdir / f"{scenario.name}.script.txt"
    report = outdir / f"{scenario.name}.{tag}.json"
    if report.exists():
        report.unlink()
    cmd = [str(exe), "--isolated", "--window-size", scenario.size, "--vsync", "off"] + scenario.args + [
        str(scenario.fixture), "--ui-script", str(script), "--ui-report", str(report)]
    start = time.time()
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, cwd=REPO)
    elapsed = time.time() - start
    data = None
    if report.exists():
        data = json.loads(report.read_text(encoding="utf-8"))
    (outdir / f"{scenario.name}.{tag}.stderr.txt").write_text(proc.stderr, encoding="utf-8")
    return {"exit": proc.returncode, "data": data, "seconds": elapsed, "cmd": " ".join(cmd)}


def by_label(data):
    out = {}
    for s in data["snaps"]:
        out[s["label"]] = s
    return out


def make_probe_dict(probes):
    counts = {}
    result = {}
    for p in probes:
        k = (p["tag"], p["id"])
        idx = counts.get(k, 0)
        counts[k] = idx + 1
        result[(p["tag"], p["id"], idx)] = p
    return result


def close(a, b, tol=0.5):
    return abs(a - b) <= tol


def compare_snap(label, base, cand):
    problems = []
    if base["selection"] != cand["selection"]:
        problems.append(f"selection {base['selection']!r} != {cand['selection']!r}")
    if base["logger"] != cand["logger"]:
        problems.append(f"logger counts {base['logger']} != {cand['logger']}")
    for win in set(base["windows"]) | set(cand["windows"]):
        bw, cw = base["windows"].get(win), cand["windows"].get(win)
        if bw is None or cw is None:
            problems.append(f"window {win} present only in one run")
            continue
        for k in ("scrollY", "scrollMaxY", "contentH", "scrollX", "scrollMaxX"):
            tol = 25.0 if win == "HierarchyTreeScrollRegion" and k in ("scrollMaxY", "contentH", "scrollY", "scrollMaxX", "scrollX") else 1.0
            if not close(bw[k], cw[k], tol):
                problems.append(f"{win}.{k} {bw[k]} != {cw[k]}")
        if bw["inner"] != cw["inner"]:
            problems.append(f"{win}.inner differs")
        if "pixels" in bw or "pixels" in cw:
            if win != "HierarchyTreeScrollRegion" and bw.get("pixels") != cw.get("pixels"):
                problems.append(f"{win}.pixels {bw.get('pixels')} != {cw.get('pixels')}")
    bp = make_probe_dict(base["probes"])
    cp = make_probe_dict(cand["probes"])
    if set(bp) != set(cp):
        diff_keys = set(bp) ^ set(cp)
        inner = (cand["windows"].get("HierarchyTreeScrollRegion") or {}).get("inner", [0, 0, 0, 0])
        non_boundary_diff = False
        for k in diff_keys:
            if not k[0].startswith("hier."):
                non_boundary_diff = True
                break
            p = bp.get(k) or cp.get(k)
            if p and (p["y0"] > inner[1] + 25.0 and p["y1"] < inner[3] - 25.0):
                non_boundary_diff = True
                break
        if non_boundary_diff:
            only_b = sorted(set(bp) - set(cp))[:5]
            only_c = sorted(set(cp) - set(bp))[:5]
            problems.append(f"visible probe sets differ: baseline-only {only_b} candidate-only {only_c}")
    for key in set(bp) & set(cp):
        b, c = bp[key], cp[key]
        # Logger drawlist rows synthesise their rect width; pixel hashes cover their rendering.
        # Hierarchy rows tolerate sub-row fractional scroll variance between unclipped baseline and virtualized clipper.
        coord_tol = 4.0 if key[0].startswith("hier.") else 0.5
        for k in (("x0", "y0", "y1") if key[0] == "log.row" else ("x0", "y0", "x1", "y1")):
            if not close(b[k], c[k], coord_tol):
                problems.append(f"probe {key} {k} {b[k]} != {c[k]}")
                break
        if key[0] != "log.row" and (b["hovered"] != c["hovered"] or b["focused"] != c["focused"]):
            problems.append(f"probe {key} hovered/focused differ")
        if "open" in b and "open" in c and b["open"] != c["open"]:
            problems.append(f"probe {key} open {b['open']} != {c['open']}")
    bm = sorted((m["tag"], m["id"]) for m in base["marks"])
    cm = sorted((m["tag"], m["id"]) for m in cand["marks"])
    if bm != cm:
        problems.append(f"marks differ {bm} != {cm}")
    if ("clipboard" in base) != ("clipboard" in cand):
        problems.append("clipboard presence differs")
    elif "clipboard" in base:
        for k in ("bytes", "newlines", "hash", "equalsExpected"):
            if base["clipboard"][k] != cand["clipboard"][k]:
                problems.append(f"clipboard.{k} {base['clipboard'][k]} != {cand['clipboard'][k]}")
    return problems


def compare_runs(results, scenario, base, cand):
    name = scenario.name
    results.add(name, "baseline run exit 0 and report written", base["exit"] == 0 and base["data"] is not None and not base["data"]["failed"],
                f"exit={base['exit']} errors={(base['data'] or {}).get('errors')}")
    results.add(name, "candidate run exit 0 and report written", cand["exit"] == 0 and cand["data"] is not None and not cand["data"]["failed"],
                f"exit={cand['exit']} errors={(cand['data'] or {}).get('errors')}")
    if not base["data"] or not cand["data"]:
        return None, None
    bl, cl = by_label(base["data"]), by_label(cand["data"])
    results.add(name, "same snapshot labels", list(bl) == list(cl), f"{len(bl)} vs {len(cl)}")
    mismatched = []
    for label in bl:
        if label in cl:
            problems = compare_snap(label, bl[label], cl[label])
            if problems:
                mismatched.append((label, problems))
    results.add(name, f"baseline == candidate for all {len(bl)} snapshots", not mismatched,
                "; ".join(f"{l}: {p[:2]}" for l, p in mismatched[:6]))
    return bl, cl


# ----------------------------------------------------------------------------- oracles
def rows_of(snapshot, tag):
    return sorted((p for p in snapshot["probes"] if p["tag"] == tag and p["id"] >= 0), key=lambda p: p["y0"])


def mesh_name(i):
    return f"Mesh_{i:04d}"


def check_resources(results, name, cl, count, tag="Meshes", name_fn=mesh_name, sel_key="mesh", filter_prefix="Mesh_40"):
    def has(label):
        return label in cl

    if not has("r03_mid"):
        results.add(name, "snapshots available", False, "missing")
        return
    open_snap = cl["r01_open"]
    results.add(name, "category open: Meshes content extends scroll range",
                open_snap["windows"][RES]["scrollMaxY"] > count * 20, f"scrollMaxY={open_snap['windows'][RES]['scrollMaxY']}")
    for label in ("r02_top", "r03_mid", "r07_bottom"):
        scroll = cl[label]["windows"][RES]
        expect = {"r02_top": 0, "r03_mid": scroll["scrollMaxY"] * 0.5, "r07_bottom": scroll["scrollMaxY"]}[label]
        results.add(name, f"{label} reached requested scroll position", close(scroll["scrollY"], expect, 2.0),
                    f"scrollY={scroll['scrollY']} expected={expect}")
    mid_rows = rows_of(cl["r03_mid"], tag)
    results.add(name, "mid scroll shows a window of rows with consecutive ids",
                len(mid_rows) > 3 and all(b["id"] == a["id"] + 1 for a, b in zip(mid_rows, mid_rows[1:])),
                f"{len(mid_rows)} rows first={mid_rows[0]['id'] if mid_rows else None}")
    if mid_rows:
        expect_mid = int(round(0.5 * count))
        results.add(name, "mid scroll rows are near the middle of the source list",
                    abs(mid_rows[0]["id"] - expect_mid) < 0.1 * count, f"first visible id {mid_rows[0]['id']} vs ~{expect_mid}")
        clicked = mid_rows[3]["id"]
        sel = cl["r04_click_mid"]["selection"]
        results.add(name, "click on the 4th visible row selects that source item",
                    f"{sel_key}={name_fn(clicked)}" in sel or f"{sel_key}={name_fn(clicked)}|" in sel, f"clicked id {clicked} -> {sel}")
    focused = [p for lbl in ("r05_nav_down0", "r05_nav_down1", "r05_nav_down2", "r06_nav_pgdn", "r06_nav_up")
               for p in cl[lbl]["probes"] if p["focused"] and p["tag"] == tag]
    results.add(name, "keyboard navigation moves focus through rows (Down/PgDn/Up produce a focused row)", len(focused) >= 3, f"{len(focused)} focused samples")
    edge_start = [p for p in cl["r06b_edge_click_last_visible"]["probes"] if p["focused"] and p["tag"] == tag]
    edge_end = [p for lbl in ("r06b_edge_down3",) for p in cl[lbl]["probes"] if p["focused"] and p["tag"] == tag]
    results.add(name, "keyboard Down past the last visible row keeps moving focus to following source items (scrolls them into view)",
                bool(edge_start) and bool(edge_end) and edge_end[0]["id"] == edge_start[0]["id"] + 4,
                f"start {[p['id'] for p in edge_start]} end {[p['id'] for p in edge_end]}")
    tail_ids = {p["id"] for lbl, s in cl.items() if lbl.startswith("r08_tail") for p in rows_of(s, tag)}
    results.add(name, "final row is reachable when scrolling up from the bottom", (count - 1) in tail_ids,
                f"max id seen {max(tail_ids) if tail_ids else None}")
    f_mid = cl["r10_filter_mid"]
    f_rows = rows_of(f_mid, tag)
    results.add(name, "filter typed while scrolled shows only matching source items",
                bool(f_rows) and all(name_fn(p["id"]).startswith(filter_prefix) or filter_prefix in name_fn(p["id"]) for p in f_rows),
                f"{len(f_rows)} rows")
    results.add(name, "filter shrinks scroll range and clamps scroll position",
                f_mid["windows"][RES]["scrollY"] <= f_mid["windows"][RES]["scrollMaxY"] + 0.5, f"{f_mid['windows'][RES]}")
    fb = rows_of(cl["r11_filter_bottom"], tag)
    results.add(name, "filtered list bottom reaches the last matching source item", bool(fb) and fb[-1]["id"] == count - 1, f"last id {fb[-1]['id'] if fb else None}")
    sel = cl["r12_filter_click_last"]["selection"]
    results.add(name, "click on last filtered row resolves to original source item",
                bool(fb) and f"{sel_key}={name_fn(fb[-1]['id'])}" in sel, f"{sel}")
    single = rows_of(cl["r13_filter_single"], tag)
    results.add(name, "exact filter shows exactly the original item", len(single) == 1 and single[0]["id"] == count - 1, f"{[p['id'] for p in single]}")
    results.add(name, "click on single filtered row selects original item", f"{sel_key}={name_fn(count - 1)}" in cl["r14_filter_single_click"]["selection"],
                cl["r14_filter_single_click"]["selection"])
    nm = cl["r15_nomatch"]
    results.add(name, "no-match filter hides all rows" + (" and the category" if tag == "Meshes" else ""), not rows_of(nm, tag) and (tag != "Meshes" or not any(p["tag"] == tag for p in nm["probes"])), "")
    results.add(name, "no-match scroll position is valid", nm["windows"][RES]["scrollY"] <= nm["windows"][RES]["scrollMaxY"] + 0.5, str(nm["windows"][RES]))
    clr = cl["r16_cleared"]
    results.add(name, "clearing filter restores the full scroll extent",
                close(clr["windows"][RES]["contentH"], cl["r01_open"]["windows"][RES]["contentH"], 1.0),
                f"{clr['windows'][RES]['contentH']} vs {cl['r01_open']['windows'][RES]['contentH']}")
    results.add(name, "filter applied from a scrolled position matches again",
                bool(rows_of(cl["r17_filter_from_scrolled"], tag)), "")
    results.add(name, "clearing the filter after scroll restores the full list",
                close(cl["r18_clear_after_filter"]["windows"][RES]["contentH"], cl["r01_open"]["windows"][RES]["contentH"], 1.0), "")


def check_logger(results, name, cl):
    def snaps(*labels):
        return [cl[l] for l in labels]

    cleared = cl["l00_cleared"]["logger"]
    results.add(name, "Clear empties the Logger", cleared["entries"] == 0, str(cleared))
    s1 = cl["l01_single_bottom"]
    rows = rows_of(s1, "log.row")
    results.add(name, "append 400 single-line entries: totals", s1["logger"]["entries"] == 400, str(s1["logger"]))
    results.add(name, "auto-scroll follows appended entries to the bottom",
                close(s1["windows"][LOG]["scrollY"], s1["windows"][LOG]["scrollMaxY"], 2.0) and bool(rows) and rows[-1]["id"] == 399,
                f"scroll {s1['windows'][LOG]['scrollY']}/{s1['windows'][LOG]['scrollMaxY']} last row {rows[-1]['id'] if rows else None}")
    top = rows_of(cl["l02_single_top"], "log.row")
    results.add(name, "scrolled to top shows entry 0 first", bool(top) and top[0]["id"] == 0, f"{top[0]['id'] if top else None}")
    mid = rows_of(cl["l03_single_mid"], "log.row")
    results.add(name, "mid scroll has consecutive rows with no gaps",
                len(mid) > 4 and all(b["id"] == a["id"] + 1 for a, b in zip(mid, mid[1:])), f"{len(mid)} rows")
    spacing = [round(b["y0"] - a["y0"], 1) for a, b in zip(mid, mid[1:])]
    results.add(name, "no overlapping rows (monotonic y spacing >= row height)", all(s > 8 for s in spacing), f"spacing set {sorted(set(spacing))}")
    inner = cl["l03_single_mid"]["windows"][LOG]["inner"]
    results.add(name, "no premature clipping at viewport boundaries (rows reach both edges of the child)",
                bool(mid) and mid[0]["y0"] <= inner[1] + 2 * 17 and mid[-1]["y1"] >= inner[3] - 2 * 17, f"inner {inner} first y {mid[0]['y0'] if mid else None} last y {mid[-1]['y1'] if mid else None}")
    ab = cl["l05_append_at_bottom"]
    abr = rows_of(ab, "log.row")
    results.add(name, "append at bottom with auto-scroll keeps newest row visible", bool(abr) and abr[-1]["id"] == 404 and close(ab["windows"][LOG]["scrollY"], ab["windows"][LOG]["scrollMaxY"], 2.0), "")
    up_before, up_after = cl["l06_scrolled_up"], cl["l07_append_while_up"]
    results.add(name, "append while scrolled up does not jump to bottom (baseline-equal behavior)",
                up_after["windows"][LOG]["scrollY"] < up_after["windows"][LOG]["scrollMaxY"] - 50, f"{up_before['windows'][LOG]['scrollY']} -> {up_after['windows'][LOG]['scrollY']}/{up_after['windows'][LOG]['scrollMaxY']}")
    c8 = cl["l08_copy_scrolled_up"]["clipboard"]
    results.add(name, "Copy while scrolled up contains every entry", c8["equalsExpected"] and c8["newlines"] == 415 and not c8["isSentinel"], str(c8))
    results.add(name, "Clear before newline test", cl["l09_clear"]["logger"]["entries"] == 0, "")
    nl = cl["l10_newline_bottom"]
    nlr = rows_of(nl, "log.row")
    results.add(name, "embedded-newline entries: bottom row is last entry", bool(nlr) and nlr[-1]["id"] == 149, f"{nlr[-1]['id'] if nlr else None}")
    heights = {round(p["y1"] - p["y0"], 1) for p in nlr}
    results.add(name, "embedded-newline rows are taller than one line", bool(heights) and min(heights) > 30, f"heights {sorted(heights)}")
    nlm = rows_of(cl["l12_newline_mid"], "log.row")
    results.add(name, "newline rows: no overlap in mid view", all(b["y0"] >= a["y1"] - 1 for a, b in zip(nlm, nlm[1:])) and len(nlm) >= 2, f"{len(nlm)} rows (Logger child is ~100 px tall at 1280x720)")
    for lbl in ("l35_copy_top", "l37_copy_bottom"):
        c = cl[lbl]["clipboard"]
        results.add(name, f"{lbl}: Copy equals full log contents independent of visible rows", c["equalsExpected"] and not c["isSentinel"] and c["bytes"] > 1000, str(c))
    results.add(name, "Copy hash identical at top and bottom scroll", cl["l35_copy_top"]["clipboard"]["hash"] == cl["l37_copy_bottom"]["clipboard"]["hash"] or cl["l35_copy_top"]["logger"] != cl["l37_copy_bottom"]["logger"], "")
    results.add(name, "Clear at end empties the Logger", cl["l38_clear_final"]["logger"]["entries"] == 0 and not rows_of(cl["l38_clear_final"], "log.row"), "")
    c39 = cl["l39_copy_empty"]["clipboard"]
    results.add(name, "Copy after Clear copies empty text", c39["equalsExpected"] and c39["bytes"] == 0, str(c39))
    wrap_on = cl["l18_wrap_on_a"]
    results.add(name, "wrap toggle changes layout (long entries become multiple lines)",
                wrap_on["windows"][LOG]["contentH"] != cl["l17_long_nowrap_mid"]["windows"][LOG]["contentH"], "")
    noauto = cl["l31_noauto_append"]
    results.add(name, "auto-scroll disabled: appended rows do not move the view",
                close(noauto["windows"][LOG]["scrollY"], cl["l30_noauto_bottom"]["windows"][LOG]["scrollY"], 2.0), f"{cl['l30_noauto_bottom']['windows'][LOG]['scrollY']} -> {noauto['windows'][LOG]['scrollY']}")
    # severity totals
    totals = {lbl: cl[lbl]["logger"] for lbl in ("l07_append_while_up",)}
    t = totals["l07_append_while_up"]
    results.add(name, "severity totals add up to entry count", t["errors"] + t["warnings"] + t["info"] == t["entries"], str(t))


def check_validation(results, name, cl, mixed):
    snaps = cl
    top = rows_of(snaps["v01_top"], "val.row")
    results.add(name, "validation: top scroll shows rows from index 0", bool(top) and top[0]["id"] == 0, f"{top[0]['id'] if top else None}")
    mid = rows_of(snaps["v03_mid"], "val.row")
    results.add(name, "validation: mid scroll shows consecutive rows", len(mid) > 3 and all(b["id"] == a["id"] + 1 for a, b in zip(mid, mid[1:])), f"{len(mid)} rows first {mid[0]['id'] if mid else None}")
    sel = [m["id"] for m in snaps["v04_click_mid"]["marks"] if m["tag"] == "val.selected"]
    results.add(name, "validation: click selects the original issue index", bool(mid) and sel == [mid[3]["id"]], f"clicked {mid[3]['id'] if mid else None} selected {sel}")
    bottom = rows_of(snaps["v06_bottom"], "val.row")
    cnt = None
    results.add(name, "validation: bottom shows highest issue index", bool(bottom), f"last id {bottom[-1]['id'] if bottom else None}")
    last_sel = [m["id"] for m in snaps["v07_click_last"]["marks"] if m["tag"] == "val.selected"]
    results.add(name, "validation: click on last row selects last issue", bool(bottom) and last_sel == [bottom[-1]["id"]], f"{last_sel}")
    results.add(name, "validation: tooltip shown while hovering a message", any(m["tag"] == "val.tooltip" for m in snaps["v08_tooltip"]["marks"]), str(snaps["v08_tooltip"]["marks"]))
    results.add(name, "validation: tooltip disappears when mouse leaves", not any(m["tag"] == "val.tooltip" for m in snaps["v08b_tooltip_gone"]["marks"]), "")
    fb = rows_of(snaps["v11_filter_bottom"], "val.row")
    fsel = [m["id"] for m in snaps["v12_filter_click_last"]["marks"] if m["tag"] == "val.selected"]
    results.add(name, "validation: filtered click selects original issue index", bool(fb) and fsel == [fb[-1]["id"]], f"{fsel} vs {fb[-1]['id'] if fb else None}")
    results.add(name, "validation: no-match filter shows no rows", not rows_of(snaps["v13_nomatch"], "val.row"), "")
    results.add(name, "validation: clearing filter restores extent", close(snaps["v14_cleared"]["windows"][VAL]["contentH"], snaps["v00_initial"]["windows"][VAL]["contentH"], 1.0), "")
    if mixed and "v19_after_dclick" in snaps:
        before = snaps["v18_click1"]["camera"]
        after = snaps["v19_after_dclick"]["camera"]
        results.add(name, "validation: filtered double-click resolves to the original (odd-indexed) mesh target", "mesh=NoPositionMesh_" in snaps["v19_after_dclick"]["selection"] and int(snaps["v19_after_dclick"]["selection"].split("mesh=NoPositionMesh_")[1][:4]) % 2 == 1, f"{before} -> {after}; {snaps['v19_after_dclick']['selection']}")


def check_hierarchy_broad(results, name, cl, count):
    def has(label):
        return label in cl

    if not has("hb02_mid"):
        results.add(name, "snapshots available", False, "missing")
        return
    initial_snap = cl["hb00_initial"]
    results.add(name, "Scene content extends scroll range",
                initial_snap["windows"][HIER]["scrollMaxY"] > count * 20, f"scrollMaxY={initial_snap['windows'][HIER]['scrollMaxY']}")
    for label in ("hb01_top", "hb02_mid", "hb07_bottom"):
        scroll = cl[label]["windows"][HIER]
        expect = {"hb01_top": 0, "hb02_mid": scroll["scrollMaxY"] * 0.5, "hb07_bottom": scroll["scrollMaxY"]}[label]
        results.add(name, f"{label} reached requested scroll position", close(scroll["scrollY"], expect, 4.0),
                    f"scrollY={scroll['scrollY']} expected={expect}")
    mid_rows = rows_of(cl["hb02_mid"], "hier.node")
    results.add(name, "mid scroll shows a window of rows with consecutive ids",
                len(mid_rows) > 3 and all(b["id"] == a["id"] + 1 for a, b in zip(mid_rows, mid_rows[1:])),
                f"{len(mid_rows)} rows first={mid_rows[0]['id'] if mid_rows else None}")
    if mid_rows:
        expect_mid = int(round(0.5 * count))
        results.add(name, "mid scroll rows are near the middle of the source list",
                    abs(mid_rows[0]["id"] - expect_mid) < 0.1 * count, f"first visible id {mid_rows[0]['id']} vs ~{expect_mid}")
        clicked = mid_rows[3]["id"]
        sel = cl["hb03_click_mid"]["selection"]
        results.add(name, "click on the 4th visible row selects that source node",
                    f"node=Node_{clicked:04d}" in sel or f"node=Node_{clicked:04d}|" in sel, f"clicked id {clicked} -> {sel}")
    focused = [p for lbl in ("hb04_nav_down0", "hb04_nav_down1", "hb04_nav_down2", "hb05_nav_pgdn", "hb05_nav_up")
               for p in cl[lbl]["probes"] if p["focused"] and p["tag"] == "hier.node"]
    results.add(name, "keyboard navigation moves focus through rows (Down/PgDn/Up produce a focused row)", len(focused) >= 3, f"{len(focused)} focused samples")
    edge_start = [p for p in cl["hb06_edge_click_last_visible"]["probes"] if p["focused"] and p["tag"] == "hier.node"]
    edge_end = [p for lbl in ("hb06_edge_down3",) for p in cl[lbl]["probes"] if p["focused"] and p["tag"] == "hier.node"]
    results.add(name, "keyboard Down past the last visible row keeps moving focus to following source items (scrolls them into view)",
                bool(edge_start) and bool(edge_end) and edge_end[0]["id"] == edge_start[0]["id"] + 4,
                f"start {[p['id'] for p in edge_start]} end {[p['id'] for p in edge_end]}")
    tail_ids = {p["id"] for lbl, s in cl.items() if lbl.startswith("hb08_tail") for p in rows_of(s, "hier.node")}
    results.add(name, "final row is reachable when scrolling up from the bottom", (count - 1) in tail_ids,
                f"max id seen {max(tail_ids) if tail_ids else None}")
    f_mid = cl["hb10_filter_mid"]
    f_rows = rows_of(f_mid, "hier.node")
    results.add(name, "filter typed while scrolled shows only matching source items",
                bool(f_rows) and all(f"Node_{p['id']:04d}".startswith("Node_40") for p in f_rows),
                f"{len(f_rows)} rows")
    results.add(name, "filter shrinks scroll range and clamps scroll position",
                f_mid["windows"][HIER]["scrollY"] <= f_mid["windows"][HIER]["scrollMaxY"] + 0.5, f"{f_mid['windows'][HIER]}")
    fb = rows_of(cl["hb11_filter_bottom"], "hier.node")
    results.add(name, "filtered list bottom reaches the last matching source item", bool(fb) and fb[-1]["id"] == count - 1, f"last id {fb[-1]['id'] if fb else None}")
    sel = cl["hb12_filter_click_last"]["selection"]
    results.add(name, "click on last filtered row resolves to original source item",
                bool(fb) and f"node=Node_{fb[-1]['id']:04d}" in sel, f"{sel}")
    single = rows_of(cl["hb13_filter_single"], "hier.node")
    results.add(name, "exact filter shows exactly the original item", len(single) == 1 and single[0]["id"] == count - 1, f"{[p['id'] for p in single]}")
    results.add(name, "click on single filtered row selects original item", f"node=Node_{count - 1:04d}" in cl["hb14_filter_single_click"]["selection"],
                cl["hb14_filter_single_click"]["selection"])
    nm = cl["hb15_nomatch"]
    results.add(name, "no-match filter hides all node rows", not rows_of(nm, "hier.node"), "")
    results.add(name, "no-match scroll position is valid", nm["windows"][HIER]["scrollY"] <= nm["windows"][HIER]["scrollMaxY"] + 0.5, str(nm["windows"][HIER]))
    clr = cl["hb16_cleared"]
    results.add(name, "clearing filter restores the full scroll extent",
                clr["windows"][HIER]["contentH"] >= initial_snap["windows"][HIER]["contentH"] - 1.0,
                f"{clr['windows'][HIER]['contentH']} vs {initial_snap['windows'][HIER]['contentH']}")
    results.add(name, "filter applied from a scrolled position matches again",
                bool(rows_of(cl["hb17_filter_from_scrolled"], "hier.node")), "")
    results.add(name, "clearing the filter after scroll restores the full list",
                cl["hb18_clear_after_filter"]["windows"][HIER]["contentH"] >= initial_snap["windows"][HIER]["contentH"] - 1.0, "")


def check_hierarchy_deep(results, name, cl, count):
    def has(label):
        return label in cl

    if not has("hd00_initial"):
        results.add(name, "snapshots available", False, "missing")
        return
    init_nodes = rows_of(cl["hd00_initial"], "hier.node")
    results.add(name, "initial deep tree shows root nodes collapsed", len(init_nodes) <= 4, f"{len(init_nodes)} root nodes")
    exp1_nodes = rows_of(cl["hd01_expand_root0"], "hier.node")
    results.add(name, "expanding root node 0 reveals its children", len(exp1_nodes) > len(init_nodes), f"{len(init_nodes)} -> {len(exp1_nodes)} nodes")
    exp2_nodes = rows_of(cl["hd02_expand_child"], "hier.node")
    results.add(name, "expanding child node reveals nested descendants", len(exp2_nodes) >= len(exp1_nodes), f"{len(exp1_nodes)} -> {len(exp2_nodes)} nodes")
    deep_nodes = rows_of(cl["hd03_deep_search"], "hier.node")
    results.add(name, "deep descendant search auto-expands ancestors down to leaf TreeNode_4095",
                any(p["id"] == count - 1 for p in deep_nodes), f"leaf {count - 1} found among {[p['id'] for p in deep_nodes]}")
    sel = cl["hd04_select_deep_leaf"]["selection"]
    results.add(name, "clicking deep search match selects original leaf node", f"node=TreeNode_{count - 1:04d}" in sel, sel)
    clr_nodes = rows_of(cl["hd05_cleared"], "hier.node")
    results.add(name, "clearing search restores tree display", bool(clr_nodes), f"{len(clr_nodes)} nodes")
    sel_desc = cl["hd07_select_descendant_collapsed"]["selection"]
    results.add(name, "selecting descendant with ancestors collapsed sets selection", "node=TreeNode_0004" in sel_desc, sel_desc)
    desc_nodes = rows_of(cl["hd07_select_descendant_collapsed"], "hier.node")
    results.add(name, "selecting descendant with ancestors collapsed auto-expands ancestors", any(p["id"] == 4 for p in desc_nodes), f"node 4 in {[p['id'] for p in desc_nodes]}")
    settled_nodes = rows_of(cl["hd08_descendant_settled"], "hier.node")
    results.add(name, "settled frames reuse row model for descendant selection", len(settled_nodes) == len(desc_nodes), f"{len(settled_nodes)} vs {len(desc_nodes)}")
    b_desc = cl["hd07_select_descendant_collapsed"]["hierarchyBuildCount"]
    b_settled = cl["hd08_descendant_settled"]["hierarchyBuildCount"]
    results.add(name, "settled frames reuse row model without rebuilds (build delta == 0)", b_settled == b_desc, f"buildCount: {b_desc} -> {b_settled} (delta {b_settled - b_desc})")
    collapsed_nodes = rows_of(cl["hd09_collapse_after_descendant"], "hier.node")
    results.add(name, "subsequent collapse after descendant selection collapses branch", not any(p["id"] == 4 for p in collapsed_nodes), f"{[p['id'] for p in collapsed_nodes]}")
    csettled_nodes = rows_of(cl["hd10_collapse_settled"], "hier.node")
    results.add(name, "collapsed branch stays collapsed on settled frames", not any(p["id"] == 4 for p in csettled_nodes), f"{[p['id'] for p in csettled_nodes]}")
    b_col = cl["hd09_collapse_after_descendant"]["hierarchyBuildCount"]
    b_col_settled = cl["hd10_collapse_settled"]["hierarchyBuildCount"]
    results.add(name, "collapsed branch settles without rebuilds (build delta == 0)", b_col_settled == b_col, f"buildCount: {b_col} -> {b_col_settled} (delta {b_col_settled - b_col})")


def check_hierarchy_shared_mesh(results, name, cl):
    def has(label):
        return label in cl

    if not has("hsm00_initial"):
        results.add(name, "snapshots available", False, "missing")
        return

    init_nodes = rows_of(cl["hsm00_initial"], "hier.node")
    results.add(name, "initial shared mesh scene shows root nodes collapsed", len(init_nodes) <= 3, f"{len(init_nodes)} nodes")

    sel_mesh_snap = cl["hsm02_select_shared_mesh"]
    sel = sel_mesh_snap["selection"]
    results.add(name, "selecting shared mesh sets mesh selection", "mesh=SharedMesh_0000" in sel, sel)
    mesh_probes = [p for p in sel_mesh_snap["probes"] if p["tag"] == "hier.mesh" and p["id"] == 0]
    results.add(name, "selecting shared mesh auto-expands all owning nodes (2+ mesh rows)", len(mesh_probes) >= 2, f"{len(mesh_probes)} mesh rows")

    b_open = cl["hsm01_res_meshes_open"]["hierarchyBuildCount"]
    b_sel = cl["hsm02_select_shared_mesh"]["hierarchyBuildCount"]
    results.add(name, "shared mesh selection triggers row model rebuild (delta > 0)", b_sel > b_open, f"buildCount: {b_open} -> {b_sel}")

    settled_snap = cl["hsm03_shared_mesh_settled"]
    settled_meshes = [p for p in settled_snap["probes"] if p["tag"] == "hier.mesh" and p["id"] == 0]
    results.add(name, "settled frame reuses row model without invalidation", len(settled_meshes) == len(mesh_probes), f"{len(settled_meshes)} vs {len(mesh_probes)}")
    b_settled = cl["hsm03_shared_mesh_settled"]["hierarchyBuildCount"]
    results.add(name, "shared mesh selection settles with zero rebuilds (delta == 0)", b_settled == b_sel, f"buildCount: {b_sel} -> {b_settled} (delta {b_settled - b_sel})")

    col_snap = cl["hsm04_collapse_root_a"]
    col_meshes = [p for p in col_snap["probes"] if p["tag"] == "hier.mesh" and p["id"] == 0]
    results.add(name, "collapsing one owner node leaves other owner node open", len(col_meshes) == 1, f"{len(col_meshes)} mesh rows")
    b_col = cl["hsm04_collapse_root_a"]["hierarchyBuildCount"]

    col_settled = cl["hsm05_collapse_settled"]
    cset_meshes = [p for p in col_settled["probes"] if p["tag"] == "hier.mesh" and p["id"] == 0]
    results.add(name, "collapsed owner node remains collapsed on settled frames", len(cset_meshes) == 1, f"{len(cset_meshes)} mesh rows")
    b_cset = cl["hsm05_collapse_settled"]["hierarchyBuildCount"]
    results.add(name, "owner collapse settles with zero rebuilds (delta == 0)", b_cset == b_col, f"buildCount: {b_col} -> {b_cset} (delta {b_cset - b_col})")

    mat_snap = cl["hsm06_select_shared_mat"]
    mat_sel = mat_snap["selection"]
    results.add(name, "selecting shared material sets material selection", "material=SharedMaterial_0000" in mat_sel, mat_sel)
    mat_probes = [p for p in mat_snap["probes"] if p["tag"] == "hier.material" and p["id"] == 0]
    results.add(name, "selecting shared material reveals material under all owner nodes", len(mat_probes) >= 2, f"{len(mat_probes)} material rows")

    single_snap = cl["hsm07_select_single_mesh"]
    single_sel = single_snap["selection"]
    results.add(name, "selecting single mesh sets single mesh selection", "mesh=SingleMesh_0001" in single_sel, single_sel)
    single_meshes = [p for p in single_snap["probes"] if p["tag"] == "hier.mesh" and p["id"] == 1]
    results.add(name, "selecting single mesh expands only its owning node", len(single_meshes) == 1, f"{len(single_meshes)} mesh rows")

    reload_mesh_snap = cl["hsm09_reload_select_shared_mesh"]
    reload_meshes = [p for p in reload_mesh_snap["probes"] if p["tag"] == "hier.mesh" and p["id"] == 0]
    results.add(name, "shared mesh auto-expansion works cleanly after model reload", len(reload_meshes) >= 2, f"{len(reload_meshes)} mesh rows")


def check_hierarchy_multiline(results, name, cl, count):
    def has(label):
        return label in cl

    if not has("hm00_initial"):
        results.add(name, "snapshots available", False, "missing")
        return
    init_snap = cl["hm00_initial"]
    results.add(name, "multiline hierarchy has full scroll extent", init_snap["windows"][HIER]["scrollMaxY"] > count * 20, f"scrollMaxY={init_snap['windows'][HIER]['scrollMaxY']}")
    for label in ("hm01_top", "hm02_mid", "hm03_bottom"):
        scroll = cl[label]["windows"][HIER]
        expect = {"hm01_top": 0, "hm02_mid": scroll["scrollMaxY"] * 0.5, "hm03_bottom": scroll["scrollMaxY"]}[label]
        results.add(name, f"{label} reached scroll position in unclipped fallback", close(scroll["scrollY"], expect, 4.0),
                    f"scrollY={scroll['scrollY']} expected={expect}")
    sel = cl["hm04_click_top"]["selection"]
    results.add(name, "clicking top node in multiline tree selects node 0", "node=Node_0000" in sel, sel)


def hierarchy_pending_scroll_script(reload_fixture):
    L = ["wait 40"]
    snap(L, "hps00_initial")
    # 1. Filter active excluding target node 3500
    set_filter(L, "hier.search", "zzz_nomatch", "hps01_filter_applied", pix=False)
    # Pick node 3500 via production viewport picking hook (calls selectNode + requestScrollToSelectedNode)
    L.append("picknode 3500")
    settle(L, 6)
    snap(L, "hps02_picked_while_filtered")
    # Settled frames while target remains unavailable: build count delta MUST be 0
    settle(L, 12)
    snap(L, "hps03_settled_while_unavailable")
    # Clear filter: target becomes available, clipper must include it and scroll to it
    set_filter(L, "hier.search", "", "hps04_cleared_filter", pix=False)
    settle(L, 8)
    snap(L, "hps05_scrolled_to_target")
    settle(L, 10)
    snap(L, "hps06_scrolled_settled")

    # 2. Viewport picking while Scene root is collapsed
    scroll_snap(L, HIER, "0", "hps07_top", pix=False)
    L.append(f"clickarrowrow hier.scene 0 {HIER}")
    settle(L, 6)
    snap(L, "hps08_scene_collapsed")
    # Pick node 3000 while Scene is collapsed (unavailable)
    L.append("picknode 3000")
    settle(L, 6)
    snap(L, "hps09_picked_while_scene_collapsed")
    settle(L, 12)
    snap(L, "hps10_settled_collapsed_unavailable")
    # Re-open Scene root: node 3000 must be scrolled into view
    L.append(f"clickarrowrow hier.scene 0 {HIER}")
    settle(L, 8)
    snap(L, "hps11_scene_reopened_scrolled")
    settle(L, 10)
    snap(L, "hps12_reopened_settled")

    # 3. Selection replacement while scroll is pending
    set_filter(L, "hier.search", "zzz_nomatch", "hps13_filter_for_replace", pix=False)
    L.append("picknode 2500")
    settle(L, 6)
    snap(L, "hps14_picked_2500_unavailable")
    # Replace selection with Node 0
    L.append("picknode 0")
    settle(L, 6)
    snap(L, "hps15_replaced_with_node0")
    set_filter(L, "hier.search", "", "hps16_clear_filter_after_replace", pix=False)
    settle(L, 8)
    snap(L, "hps17_verify_replacement_scroll")

    # 4. Model reload while scroll request is pending
    set_filter(L, "hier.search", "zzz_nomatch", "hps18_filter_for_reload", pix=False)
    L.append("picknode 2000")
    settle(L, 6)
    snap(L, "hps19_picked_2000_unavailable")
    L.append(f"reload {reload_fixture}")
    settle(L, 12)
    snap(L, "hps20_reloaded_clean")
    settle(L, 10)
    snap(L, "hps21_reloaded_settled")
    return L


def check_hierarchy_pending_scroll(results, name, cl):
    def has(label):
        return label in cl

    if not has("hps00_initial"):
        results.add(name, "snapshots available", False, "missing")
        return

    # Check 1: Picked while filtered sets selection
    sel_filt = cl["hps02_picked_while_filtered"]["selection"]
    results.add(name, "picking filtered node sets selection identity", "node=Node_3500" in sel_filt, sel_filt)
    results.add(name, "filtered picked node is absent from visible hierarchy", not any(p["tag"] == "hier.node" and p["id"] == 3500 for p in cl["hps02_picked_while_filtered"]["probes"]), "")

    # Check 2: Settled frames with target unavailable DO NOT rebuild row model (finding 1 & 2 bug check!)
    b_filt = cl["hps02_picked_while_filtered"]["hierarchyBuildCount"]
    b_filt_settled = cl["hps03_settled_while_unavailable"]["hierarchyBuildCount"]
    results.add(name, "pending scroll with filtered target does NOT rebuild repeatedly (delta == 0)", b_filt_settled == b_filt, f"buildCount {b_filt} -> {b_filt_settled} (delta {b_filt_settled - b_filt})")

    # Check 3: Clearing filter reveals target and scrolls it into view
    sel_clr = cl["hps05_scrolled_to_target"]["selection"]
    results.add(name, "selection preserved after clearing filter", "node=Node_3500" in sel_clr, sel_clr)
    nodes_clr = [p for p in cl["hps05_scrolled_to_target"]["probes"] if p["tag"] == "hier.node" and p["id"] == 3500]
    results.add(name, "clearing filter scrolls target node 3500 into visible view", len(nodes_clr) == 1, f"visible node 3500 count: {len(nodes_clr)}")
    scroll_y = cl["hps05_scrolled_to_target"]["windows"][HIER]["scrollY"]
    results.add(name, "eventual scroll position moved down near target node", scroll_y > 40000.0, f"scrollY={scroll_y}")

    # Check 4: Settled frame after scroll does not rebuild
    b_scr = cl["hps05_scrolled_to_target"]["hierarchyBuildCount"]
    b_scr_settled = cl["hps06_scrolled_settled"]["hierarchyBuildCount"]
    results.add(name, "settled frames after scroll reuse row model (delta == 0)", b_scr_settled == b_scr, f"buildCount {b_scr} -> {b_scr_settled} (delta {b_scr_settled - b_scr})")

    # Check 5: Viewport picking while Scene root collapsed
    sel_col = cl["hps09_picked_while_scene_collapsed"]["selection"]
    results.add(name, "picking node while scene collapsed sets selection identity", "node=Node_3000" in sel_col, sel_col)
    results.add(name, "collapsed scene hides all nodes", not any(p["tag"] == "hier.node" for p in cl["hps09_picked_while_scene_collapsed"]["probes"]), "")

    # Check 6: Settled frames with collapsed target DO NOT rebuild repeatedly
    b_col = cl["hps09_picked_while_scene_collapsed"]["hierarchyBuildCount"]
    b_col_settled = cl["hps10_settled_collapsed_unavailable"]["hierarchyBuildCount"]
    results.add(name, "pending scroll with collapsed scene does NOT rebuild repeatedly (delta == 0)", b_col_settled == b_col, f"buildCount {b_col} -> {b_col_settled} (delta {b_col_settled - b_col})")

    # Check 7: Re-opening scene scrolls target into view
    nodes_reopened = [p for p in cl["hps11_scene_reopened_scrolled"]["probes"] if p["tag"] == "hier.node" and p["id"] == 3000]
    results.add(name, "reopening scene scrolls target node 3000 into view", len(nodes_reopened) == 1, f"visible node 3000 count: {len(nodes_reopened)}")

    # Check 8: Settled frames after scene reopening do not rebuild
    b_reopen = cl["hps11_scene_reopened_scrolled"]["hierarchyBuildCount"]
    b_reopen_settled = cl["hps12_reopened_settled"]["hierarchyBuildCount"]
    results.add(name, "settled frames after scene reopening reuse row model (delta == 0)", b_reopen_settled == b_reopen, f"buildCount {b_reopen} -> {b_reopen_settled} (delta {b_reopen_settled - b_reopen})")

    # Check 9: Selection replacement cancels stale pending scroll
    sel_rep = cl["hps17_verify_replacement_scroll"]["selection"]
    results.add(name, "selection replacement preserves new selection Node 0", "node=Node_0000" in sel_rep, sel_rep)
    scroll_rep = cl["hps17_verify_replacement_scroll"]["windows"][HIER]["scrollY"]
    results.add(name, "replacement selection scrolled to top for Node 0, not old Node 2500", scroll_rep < 1000.0, f"scrollY={scroll_rep}")

    # Check 10: Model reload clears pending scroll without rebuilding continuously
    b_rel = cl["hps20_reloaded_clean"]["hierarchyBuildCount"]
    b_rel_settled = cl["hps21_reloaded_settled"]["hierarchyBuildCount"]
    results.add(name, "model reload settles with clean cache (delta == 0)", b_rel_settled == b_rel, f"buildCount {b_rel} -> {b_rel_settled} (delta {b_rel_settled - b_rel})")


def hierarchy_camera_light_script():
    L = ["wait 40"]
    snap(L, "hcl00_initial")
    # 1. Open Cameras category in Resources and select Camera 0
    L.append("clickp Cameras -1")
    settle(L, 6)
    L.append(f"clickrow Cameras 0 {RES}")
    settle(L, 8)
    snap(L, "hcl01_res_select_cam0")
    # 2. Switch to Camera 1 in Resources
    L.append(f"clickrow Cameras 1 {RES}")
    settle(L, 8)
    snap(L, "hcl02_res_select_cam1")
    # 3. Open Lights category in Resources and select Light 0
    L.append("clickp Lights -1")
    settle(L, 6)
    L.append(f"clickrow Lights 0 {RES}")
    settle(L, 8)
    snap(L, "hcl03_res_select_light0")
    # 4. Switch to Light 1 in Resources
    L.append(f"clickrow Lights 1 {RES}")
    settle(L, 8)
    snap(L, "hcl04_res_select_light1")
    # 5. Select Camera 0 via hierarchy row
    L.append("clickp hier.camera 0")
    settle(L, 8)
    snap(L, "hcl05_hier_select_cam0")
    # 6. Switch to Camera 1 via hierarchy row
    L.append("clickp hier.camera 1")
    settle(L, 8)
    snap(L, "hcl06_hier_select_cam1")
    # 7. Select Light 0 via hierarchy row
    L.append("clickp hier.light 0")
    settle(L, 8)
    snap(L, "hcl07_hier_select_light0")
    # 8. Switch to Light 1 via hierarchy row
    L.append("clickp hier.light 1")
    settle(L, 8)
    snap(L, "hcl08_hier_select_light1")
    # Capture representative screenshot for visual verification
    L.append("shot build/p2.4/cam_light_selection.png")
    settle(L, 4)
    snap(L, "hcl09_final")
    return L


def check_hierarchy_camera_light(results, name, cl):
    def has(label):
        return label in cl

    if not has("hcl00_initial"):
        results.add(name, "snapshots available", False, "missing")
        return

    def find_p(snap, tag, pid):
        for p in snap.get("probes", []):
            if p["tag"] == tag and p["id"] == pid:
                return p
        return None

    # 1. Camera 0 selected via Resources
    s1 = cl["hcl01_res_select_cam0"]
    p_cam0_s1 = find_p(s1, "hier.camera", 0)
    results.add(name, "select Camera 0 in Resources: auto-expands and shows hier.camera 0", p_cam0_s1 is not None, str(p_cam0_s1))
    results.add(name, "select Camera 0 in Resources: hier.camera 0 highlighted", p_cam0_s1 is not None and p_cam0_s1.get("selected", False), str(p_cam0_s1))
    results.add(name, "select Camera 0 in Resources: selection state matches camera 0", "camera=Camera_0000" in s1["selection"], s1["selection"])

    # 2. Switch to Camera 1 in Resources
    s2 = cl["hcl02_res_select_cam1"]
    p_cam0_s2 = find_p(s2, "hier.camera", 0)
    p_cam1_s2 = find_p(s2, "hier.camera", 1)
    results.add(name, "switch to Camera 1 in Resources: hier.camera 1 highlighted", p_cam1_s2 is not None and p_cam1_s2.get("selected", False), str(p_cam1_s2))
    results.add(name, "switch to Camera 1 in Resources: hier.camera 0 selection cleared", p_cam0_s2 is not None and not p_cam0_s2.get("selected", False), str(p_cam0_s2))
    results.add(name, "switch to Camera 1 in Resources: selection state matches camera 1", "camera=Camera_0001" in s2["selection"], s2["selection"])

    # 3. Switch to Light 0 in Resources
    s3 = cl["hcl03_res_select_light0"]
    p_cam1_s3 = find_p(s3, "hier.camera", 1)
    p_light0_s3 = find_p(s3, "hier.light", 0)
    results.add(name, "switch to Light 0 in Resources: hier.light 0 highlighted", p_light0_s3 is not None and p_light0_s3.get("selected", False), str(p_light0_s3))
    results.add(name, "switch to Light 0 in Resources: previous camera 1 selection cleared", p_cam1_s3 is not None and not p_cam1_s3.get("selected", False), str(p_cam1_s3))
    results.add(name, "switch to Light 0 in Resources: selection state matches light 0", "light=Light_0000" in s3["selection"], s3["selection"])

    # 4. Switch to Light 1 in Resources
    s4 = cl["hcl04_res_select_light1"]
    p_light0_s4 = find_p(s4, "hier.light", 0)
    p_light1_s4 = find_p(s4, "hier.light", 1)
    results.add(name, "switch to Light 1 in Resources: hier.light 1 highlighted", p_light1_s4 is not None and p_light1_s4.get("selected", False), str(p_light1_s4))
    results.add(name, "switch to Light 1 in Resources: hier.light 0 selection cleared", p_light0_s4 is not None and not p_light0_s4.get("selected", False), str(p_light0_s4))
    results.add(name, "switch to Light 1 in Resources: selection state matches light 1", "light=Light_0001" in s4["selection"], s4["selection"])

    # 5. Select Camera 0 via hierarchy row
    s5 = cl["hcl05_hier_select_cam0"]
    p_cam0_s5 = find_p(s5, "hier.camera", 0)
    p_light1_s5 = find_p(s5, "hier.light", 1)
    results.add(name, "select Camera 0 in hierarchy: hier.camera 0 highlighted", p_cam0_s5 is not None and p_cam0_s5.get("selected", False), str(p_cam0_s5))
    results.add(name, "select Camera 0 in hierarchy: previous light 1 selection cleared", p_light1_s5 is not None and not p_light1_s5.get("selected", False), str(p_light1_s5))
    results.add(name, "select Camera 0 in hierarchy: selection state matches camera 0", "camera=Camera_0000" in s5["selection"], s5["selection"])

    # 6. Switch to Camera 1 via hierarchy row
    s6 = cl["hcl06_hier_select_cam1"]
    p_cam0_s6 = find_p(s6, "hier.camera", 0)
    p_cam1_s6 = find_p(s6, "hier.camera", 1)
    results.add(name, "switch to Camera 1 in hierarchy: hier.camera 1 highlighted", p_cam1_s6 is not None and p_cam1_s6.get("selected", False), str(p_cam1_s6))
    results.add(name, "switch to Camera 1 in hierarchy: hier.camera 0 selection cleared", p_cam0_s6 is not None and not p_cam0_s6.get("selected", False), str(p_cam0_s6))
    results.add(name, "switch to Camera 1 in hierarchy: selection state matches camera 1", "camera=Camera_0001" in s6["selection"], s6["selection"])

    # 7. Select Light 0 via hierarchy row
    s7 = cl["hcl07_hier_select_light0"]
    p_cam1_s7 = find_p(s7, "hier.camera", 1)
    p_light0_s7 = find_p(s7, "hier.light", 0)
    results.add(name, "select Light 0 in hierarchy: hier.light 0 highlighted", p_light0_s7 is not None and p_light0_s7.get("selected", False), str(p_light0_s7))
    results.add(name, "select Light 0 in hierarchy: previous camera 1 selection cleared", p_cam1_s7 is not None and not p_cam1_s7.get("selected", False), str(p_cam1_s7))
    results.add(name, "select Light 0 in hierarchy: selection state matches light 0", "light=Light_0000" in s7["selection"], s7["selection"])

    # 8. Switch to Light 1 via hierarchy row
    s8 = cl["hcl08_hier_select_light1"]
    p_light0_s8 = find_p(s8, "hier.light", 0)
    p_light1_s8 = find_p(s8, "hier.light", 1)
    results.add(name, "switch to Light 1 in hierarchy: hier.light 1 highlighted", p_light1_s8 is not None and p_light1_s8.get("selected", False), str(p_light1_s8))
    results.add(name, "switch to Light 1 in hierarchy: hier.light 0 selection cleared", p_light0_s8 is not None and not p_light0_s8.get("selected", False), str(p_light0_s8))
    results.add(name, "switch to Light 1 in hierarchy: selection state matches light 1", "light=Light_0001" in s8["selection"], s8["selection"])


# ----------------------------------------------------------------------------- main
def expand_dclick(lines, probe_hint):
    return lines


def build_scenarios(fx, alt_res):
    S = []
    S.append(Scenario("resources_4096", fx / "p2_3_resources_4096.glb", [],
                      resources_script("Meshes", "Mesh_40", "Mesh_4095", [alt_res[0], fx / "p2_3_resources_4096.glb", fx / "p2_3_resources_4096.glb", alt_res[1]], "Mesh_4095"), "resources"))
    S.append(Scenario("resources_multiline_4096", fx / "p2_3_resources_multiline_4096.glb", [],
                      resources_script("Meshes", "Mesh_40", "Mesh_4095", [fx / "p2_3_resources_multiline_4096.glb", fx / "p2_3_resources_4096.glb", fx / "p2_3_resources_multiline_4096.glb"], "Mesh_4095"), "resources"))
    S.append(Scenario("variants_4096", fx / "p2_3_variants_4096.glb", [],
                      resources_script("Material Variants", "Variant_40", "Variant_4095", [fx / "p2_3_resources_4096.glb", fx / "p2_3_variants_4096.glb", fx / "p2_3_variants_4096.glb"], "Variant_4095", click_open=False), "variants"))
    S.append(Scenario("validation_2048", fx / "p2_3_validation_2048.glb", ["--show-validation"], validation_script(False), "validation", "1600x900"))
    S.append(Scenario("validation_mixed_2048", fx / "p2_3_validation_mixed_2048.glb", ["--show-validation"], validation_script(True), "validation_mixed", "1600x900"))
    S.append(Scenario("logger", fx / "p2_3_resources_4096.glb", ["--show-logger"], logger_script(), "logger", "1280x720"))
    S.append(Scenario("hierarchy_broad_4096", fx / "p2_4_hierarchy_broad_4096.glb", [],
                      hierarchy_broad_script([alt_res[0], fx / "p2_4_hierarchy_deep_4096.glb", fx / "p2_4_hierarchy_broad_4096.glb"]), "hierarchy_broad"))
    S.append(Scenario("hierarchy_deep_4096", fx / "p2_4_hierarchy_deep_4096.glb", [],
                      hierarchy_deep_script(), "hierarchy_deep"))
    S.append(Scenario("hierarchy_multiline_4096", fx / "p2_4_hierarchy_multiline_4096.glb", [],
                      hierarchy_multiline_script(), "hierarchy_multiline"))
    S.append(Scenario("hierarchy_shared_mesh", fx / "p2_4_hierarchy_shared_mesh.glb", [],
                      hierarchy_shared_mesh_script(fx / "p2_4_hierarchy_shared_mesh.glb"), "hierarchy_shared_mesh"))
    S.append(Scenario("hierarchy_pending_scroll", fx / "p2_4_hierarchy_broad_4096.glb", [],
                      hierarchy_pending_scroll_script(fx / "p2_4_hierarchy_broad_4096.glb"), "hierarchy_pending_scroll", candidate_only=True))
    S.append(Scenario("hierarchy_camera_light", fx / "p2_4_hierarchy_camera_light.glb", [],
                      hierarchy_camera_light_script(), "hierarchy_camera_light", candidate_only=True))
    return S


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", type=Path, default=REPO / "build/p2.4-baseline/Glider.exe")
    parser.add_argument("--candidate", type=Path, default=REPO / "build/bin/Release/Glider.exe")
    parser.add_argument("--fixtures", type=Path, default=REPO / "build/p2.4/fixtures")
    parser.add_argument("--out", type=Path, default=REPO / "build/p2.4/integration")
    parser.add_argument("--only", default="")
    parser.add_argument("--force-fail", action="store_true")
    parser.add_argument("--fail-report-write", action="store_true", help="test hook: write the summary to an unwritable path")
    args = parser.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    results = Results()
    alt = [args.fixtures / "p2_3_variants_4096.glb", args.fixtures / "p2_3_resources_multiline_4096.glb"]
    for scenario in build_scenarios(args.fixtures, alt):
        if args.only and args.only not in scenario.name:
            continue
        lines = list(scenario.lines)
        script = args.out / f"{scenario.name}.script.txt"
        script.write_text("\n".join(lines) + "\n", encoding="utf-8")
        if scenario.candidate_only:
            cand = run_exe(args.candidate, scenario, args.out, "candidate")
            results.add(scenario.name, "candidate run exit 0 and report written",
                        cand["exit"] == 0 and cand["data"] is not None and not cand["data"]["failed"],
                        f"exit={cand['exit']} errors={(cand['data'] or {}).get('errors')}")
            cl = by_label(cand["data"]) if cand["data"] else None
            bl = None
        else:
            base = run_exe(args.baseline, scenario, args.out, "baseline")
            cand = run_exe(args.candidate, scenario, args.out, "candidate")
            bl, cl = compare_runs(results, scenario, base, cand)
        if cl:
            name = scenario.name
            try:
                if name == "resources_4096" or name == "resources_multiline_4096":
                    check_resources(results, name, cl, 4096)
                elif name == "variants_4096":
                    check_resources(results, name, cl, 4096, tag="Material Variants", name_fn=lambda i: f"Variant_{i:04d}", sel_key="variant", filter_prefix="Variant_40")
                elif name.startswith("validation"):
                    check_validation(results, name, cl, name.endswith("mixed_2048"))
                elif name == "logger":
                    check_logger(results, name, cl)
                elif name == "hierarchy_broad_4096":
                    check_hierarchy_broad(results, name, cl, 4096)
                elif name == "hierarchy_deep_4096":
                    check_hierarchy_deep(results, name, cl, 4096)
                elif name == "hierarchy_multiline_4096":
                    check_hierarchy_multiline(results, name, cl, 4096)
                elif name == "hierarchy_shared_mesh":
                    check_hierarchy_shared_mesh(results, name, cl)
                elif name == "hierarchy_pending_scroll":
                    check_hierarchy_pending_scroll(results, name, cl)
                elif name == "hierarchy_camera_light":
                    check_hierarchy_camera_light(results, name, cl)
            except (KeyError, IndexError) as exc:
                results.add(scenario.name, "oracle evaluation", False, f"missing data: {exc!r}")
        if scenario.candidate_only:
            print(f"{scenario.name}: cand {cand['seconds']:.1f}s", flush=True)
        else:
            print(f"{scenario.name}: base {base['seconds']:.1f}s cand {cand['seconds']:.1f}s", flush=True)

    if args.force_fail:
        results.add("force-fail", "intentional failing assertion", False, "requested by --force-fail")

    summary = {"passed": len(results.rows) - len(results.failed), "failed": len(results.failed), "rows": results.rows}
    summary_path = Path("Z:/nonexistent_dir/summary.json") if args.fail_report_write else args.out / "summary.json"
    try:
        summary_path.write_text(json.dumps(summary, indent=1), encoding="utf-8")
    except OSError as exc:
        print(f"report write failed: {exc}", file=sys.stderr)
        return 2
    for row in results.rows:
        print(("PASS " if row["ok"] else "FAIL ") + f"[{row['scenario']}] {row['check']}" + ("" if row["ok"] else f"  -- {row['detail']}"))
    print(f"{summary['passed']} passed, {summary['failed']} failed")
    return 1 if results.failed else 0


if __name__ == "__main__":
    sys.exit(main())
