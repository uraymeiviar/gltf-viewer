"""Run the requested material checks serially, stopping all launches on 0xc0000142."""
import contextlib
from pathlib import Path
import runpy
import sys
import traceback

import harness_process

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "build/review/pmdg"
original_run = harness_process.run


class InitializationFailure(BaseException):
    pass


def guarded_run(*args, **kwargs):
    result = original_run(*args, **kwargs)
    if result.returncode in (0xc0000142, -1073741502):
        raise InitializationFailure("0xc0000142: no more Glider launches permitted")
    return result


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    harness_process.run = guarded_run
    results = {}
    for name in ("validate_msfs2020_materials", "validate_asobo_primitive_ranges", "validate_asobo_glass", "validate_asobo_decal",
                 "validate_asobo_support", "validate_asobo_emissive"):
        print("START", name, flush=True)
        code = 0
        with (OUT / ("m2020_"+name+".log")).open("w", encoding="utf-8") as log:
            with contextlib.redirect_stdout(log), contextlib.redirect_stderr(log):
                sys.argv = [str(REPO / "scripts" / (name+".py"))]
                try:
                    runpy.run_path(sys.argv[0], run_name="__main__")
                except InitializationFailure:
                    traceback.print_exc()
                    print("BLOCKED 0xc0000142", file=sys.__stdout__, flush=True)
                    raise
                except SystemExit as error:
                    code = error.code or 0
                except Exception:
                    traceback.print_exc()
                    code = 1
        results[name] = code
        print("RESULT", name, code, flush=True)
    import json
    (OUT / "m2020_check_results.json").write_text(json.dumps(results, indent=2))
    raise SystemExit(int(any(results.values())))


if __name__ == "__main__":
    main()
