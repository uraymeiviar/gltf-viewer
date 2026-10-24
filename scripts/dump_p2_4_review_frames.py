import json
import harness_process as subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
FINAL = REPO / "build" / "p2.4" / "final"
FINAL.mkdir(parents=True, exist_ok=True)
FIX_DIR = REPO / "build" / "p2.4" / "fixtures"

DUMPS = [
    (
        "shared_mesh_owners",
        FIX_DIR / "p2_4_hierarchy_shared_mesh.glb",
        """wait 40
clickp Meshes -1
wait 8
clickrow Meshes 0 ResourcesTreeScrollRegion
wait 12
snap preshot
wait 4
shot build/p2.4/final/shared_mesh_owners.png
wait 4
""",
        lambda snap: (
            any(p["tag"] == "hier.scene" and p.get("open", False) for p in snap["probes"]),
            len([p for p in snap["probes"] if p["tag"] == "hier.mesh" and p["id"] == 0]) >= 2,
            "mesh=SharedMesh_0000" in snap.get("selection", ""),
        )
    ),
    (
        "selection_expansion",
        FIX_DIR / "p2_4_hierarchy_shared_mesh.glb",
        """wait 40
clickrow Materials 0 ResourcesTreeScrollRegion
wait 12
snap preshot
wait 4
shot build/p2.4/final/selection_expansion.png
wait 4
""",
        lambda snap: (
            any(p["tag"] == "hier.scene" and p.get("open", False) for p in snap["probes"]),
            len([p for p in snap["probes"] if p["tag"] == "hier.material" and p["id"] == 0]) >= 2,
            "material=SharedMaterial_0000" in snap.get("selection", ""),
        )
    ),
    (
        "multiline_fallback",
        FIX_DIR / "p2_4_hierarchy_multiline_4096.glb",
        """wait 40
scroll HierarchyTreeScrollRegion 0.1
wait 8
snap preshot
wait 4
shot build/p2.4/final/multiline_fallback.png
wait 4
""",
        lambda snap: (
            len([p for p in snap["probes"] if p["tag"] == "hier.node"]) > 0,
        )
    ),
]

for name, fixture, script_text, validator in DUMPS:
    script_file = FINAL / f"{name}.script.txt"
    script_file.write_text(script_text, encoding="utf-8")
    report_file = FINAL / f"{name}.report.json"
    cmd = [
        str(EXE),
        str(fixture),
        "--isolated",
        "--window-size", "1600x900",
        "--ui-script", str(script_file),
        "--ui-report", str(report_file),
    ]
    print(f"Running dump for {name}...")
    res = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, timeout=60)
    if res.returncode != 0:
        print(f"FAILED {name}: returncode {res.returncode}")
        print(res.stderr)
        sys.exit(1)

    if not report_file.exists():
        print(f"FAILED {name}: report file missing")
        sys.exit(1)

    rep = json.loads(report_file.read_text(encoding="utf-8"))
    if rep.get("failed"):
        print(f"FAILED {name}: ui scenario recorded failure: {rep.get('errors')}")
        sys.exit(1)

    snaps = {s["label"]: s for s in rep.get("snaps", [])}
    if "preshot" not in snaps:
        print(f"FAILED {name}: preshot snapshot missing")
        sys.exit(1)

    snap = snaps["preshot"]
    val_results = validator(snap)
    if not all(val_results):
        print(f"FAILED {name}: validation conditions failed: {val_results}")
        print(f"Selection: {snap.get('selection')}")
        print(f"Probes: {[(p['tag'], p['id'], p.get('open')) for p in snap.get('probes', [])]}")
        sys.exit(1)

    out_img = FINAL / f"{name}.png"
    if not out_img.exists() or out_img.stat().st_size == 0:
        print(f"FAILED {name}: image not produced")
        sys.exit(1)
    print(f"SUCCESS {name}: validated and captured {out_img} ({out_img.stat().st_size} bytes)")

print("All frame dumps captured and validated successfully.")
