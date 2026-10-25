# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Validation harness for external URI portability checks:
- Warnings (URI_ABSOLUTE_PATH) for absolute filesystem paths (Windows drive, file:///, UNC, POSIX)
- Infos (URI_EXTERNAL_URL) for external HTTP/HTTPS URLs
- No warnings/infos for relative paths, data: URIs, or bufferView references
"""

import json
import sys
from pathlib import Path

import harness_process as subprocess

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "uri_warnings"

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    check(EXE.exists(), f"Glider binary exists: {EXE}")
    if not EXE.exists():
        print("RESULT: FAIL (missing binary)")
        sys.exit(1)

    # -------------------------------------------------------------
    # 1. Primary Fixture: relative path, Windows drive, file:///, UNC, https:// URL, data: URI
    # -------------------------------------------------------------
    print("[Primary Fixture: buffer data: URI, image relative, drive, file:///, UNC, https://]")
    primary_fixture = OUT / "fixture.gltf"
    primary_report = OUT / "report.json"

    primary_data = {
        "asset": {"version": "2.0", "generator": "validate_uri_warnings"},
        "buffers": [
            {"byteLength": 4, "uri": "data:application/octet-stream;base64,AAAA"}
        ],
        "images": [
            {"uri": "textures/relative.png"},
            {"uri": "C:/textures/drive.png"},
            {"uri": "file:///C:/textures/file_uri.png"},
            {"uri": r"\\server.invalid\share\unc.png"},
            {"uri": "https://example.com/remote.png"}
        ]
    }
    primary_fixture.write_text(json.dumps(primary_data, indent=2), encoding="utf-8")
    primary_report.unlink(missing_ok=True)

    cmd = [
        str(EXE),
        "--isolated",
        "--validate-file",
        str(primary_fixture),
        "--validation-report",
        str(primary_report)
    ]
    res = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, timeout=30)
    check(res.returncode == 0, f"Glider exit code 0 (got {res.returncode})")
    check(primary_report.exists(), f"Validation report generated: {primary_report.name}")

    if primary_report.exists():
        rep = json.loads(primary_report.read_text(encoding="utf-8"))
        errors = rep.get("errors", 0)
        warnings = rep.get("warnings", 0)
        infos = rep.get("infos", 0)
        issues = rep.get("issues", [])

        check(errors == 0, f"Error count is 0 (got {errors})")
        check(warnings == 3, f"Warning count is exactly 3 (got {warnings})")
        check(infos == 1, f"Info count is exactly 1 (got {infos})")

        issues_by_ptr = {issue.get("pointer"): issue for issue in issues}

        # Verify absolute path warnings with exact pointers
        w_drive = issues_by_ptr.get("/images/1/uri")
        check(
            w_drive is not None
            and w_drive.get("code") == "URI_ABSOLUTE_PATH"
            and w_drive.get("severity") == "Warning",
            "Drive path /images/1/uri has Warning URI_ABSOLUTE_PATH"
        )

        w_file = issues_by_ptr.get("/images/2/uri")
        check(
            w_file is not None
            and w_file.get("code") == "URI_ABSOLUTE_PATH"
            and w_file.get("severity") == "Warning",
            "file:/// URI /images/2/uri has Warning URI_ABSOLUTE_PATH"
        )

        w_unc = issues_by_ptr.get("/images/3/uri")
        check(
            w_unc is not None
            and w_unc.get("code") == "URI_ABSOLUTE_PATH"
            and w_unc.get("severity") == "Warning",
            "UNC path /images/3/uri has Warning URI_ABSOLUTE_PATH"
        )

        # Verify external URL info with exact pointer
        i_url = issues_by_ptr.get("/images/4/uri")
        check(
            i_url is not None
            and i_url.get("code") == "URI_EXTERNAL_URL"
            and i_url.get("severity") == "Info",
            "https:// URL /images/4/uri has Info URI_EXTERNAL_URL"
        )

        # Verify relative path and data: URI produce no issues
        check("/images/0/uri" not in issues_by_ptr, "Relative path /images/0/uri produces no issue")
        check("/buffers/0/uri" not in issues_by_ptr, "data: URI /buffers/0/uri produces no issue")

    # -------------------------------------------------------------
    # 2. Extended Fixture: POSIX paths, Windows backslash, buffer URIs, http:// URL, bufferView image
    # -------------------------------------------------------------
    print("[Extended Fixture: POSIX paths, backslash drive, buffer absolute/URL/relative, http:// URL, bufferView image]")
    ext_fixture = OUT / "fixture_extended.gltf"
    ext_report = OUT / "report_extended.json"

    ext_data = {
        "asset": {"version": "2.0", "generator": "validate_uri_warnings_ext"},
        "buffers": [
            {"byteLength": 4, "uri": "D:/buffers/data.bin"},
            {"byteLength": 4, "uri": "https://example.com/remote.bin"},
            {"byteLength": 4, "uri": "relative/buffer.bin"}
        ],
        "bufferViews": [
            {"buffer": 0, "byteLength": 4}
        ],
        "images": [
            {"uri": "/home/user/posix_texture.png"},
            {"uri": r"E:\textures\backslash.png"},
            {"uri": "http://example.com/remote.png"},
            {"bufferView": 0, "mimeType": "image/png"}
        ]
    }
    ext_fixture.write_text(json.dumps(ext_data, indent=2), encoding="utf-8")
    ext_report.unlink(missing_ok=True)

    cmd_ext = [
        str(EXE),
        "--isolated",
        "--validate-file",
        str(ext_fixture),
        "--validation-report",
        str(ext_report)
    ]
    res_ext = subprocess.run(cmd_ext, cwd=REPO, capture_output=True, text=True, timeout=30)
    check(res_ext.returncode == 0, f"Glider exit code 0 on extended fixture (got {res_ext.returncode})")
    check(ext_report.exists(), f"Extended validation report generated: {ext_report.name}")

    if ext_report.exists():
        rep = json.loads(ext_report.read_text(encoding="utf-8"))
        check(rep.get("warnings") == 3, f"Extended fixture warning count is 3 (got {rep.get('warnings')})")
        check(rep.get("infos") == 2, f"Extended fixture info count is 2 (got {rep.get('infos')})")

        issues_by_ptr = {issue.get("pointer"): issue for issue in rep.get("issues", [])}

        b_abs = issues_by_ptr.get("/buffers/0/uri")
        check(
            b_abs is not None and b_abs.get("code") == "URI_ABSOLUTE_PATH",
            "Buffer absolute path /buffers/0/uri has Warning URI_ABSOLUTE_PATH"
        )

        b_url = issues_by_ptr.get("/buffers/1/uri")
        check(
            b_url is not None and b_url.get("code") == "URI_EXTERNAL_URL",
            "Buffer external URL /buffers/1/uri has Info URI_EXTERNAL_URL"
        )

        check("/buffers/2/uri" not in issues_by_ptr, "Buffer relative path /buffers/2/uri produces no issue")

        img_posix = issues_by_ptr.get("/images/0/uri")
        check(
            img_posix is not None and img_posix.get("code") == "URI_ABSOLUTE_PATH",
            "POSIX absolute path /images/0/uri has Warning URI_ABSOLUTE_PATH"
        )

        img_bs = issues_by_ptr.get("/images/1/uri")
        check(
            img_bs is not None and img_bs.get("code") == "URI_ABSOLUTE_PATH",
            "Backslash drive path /images/1/uri has Warning URI_ABSOLUTE_PATH"
        )

        img_http = issues_by_ptr.get("/images/2/uri")
        check(
            img_http is not None and img_http.get("code") == "URI_EXTERNAL_URL",
            "http:// URL /images/2/uri has Info URI_EXTERNAL_URL"
        )

        check("/images/3/uri" not in issues_by_ptr, "bufferView image /images/3 produces no issue")

    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
