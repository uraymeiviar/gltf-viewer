# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.10 Sun Source, legacy parity, Auto ambient CLI/pipe and Manual parity. Serial only."""
import argparse
import ctypes
import json
import os
from pathlib import Path

if os.name == "nt":
    # Keep the harness below half the logical CPUs (12 on the task host); use a pointer-sized pseudo-handle.
    core_limit = min(ctypes.sizeof(ctypes.c_size_t) * 8, max(1, (os.cpu_count() or 1) // 2))
    assert ctypes.windll.kernel32.SetProcessAffinityMask(ctypes.c_void_p(-1), ctypes.c_size_t((1 << core_limit) - 1)), ctypes.WinError()

import validate_night as night
from validate_asobo_common import EXE, REPO, check, finish, same
from validate_sun import lambert

OUT = REPO / "build/sun_source_ambient/validation"


def parity(baseline):
    model = night.write("source_lambert", lambert())
    for environment in ("hdr", "sky"):
        for sun in ("off", "on"):
            for mode in ("manual", "hdr"):
                for clock in ("off", "on"):
                    label = f"{environment}_{sun}_{mode}_{clock}"
                    # Ambient is explicitly off on BOTH binaries: sky's new default fill is intentional.
                    flags = ["--environment", environment, "--sun", sun, "--sun-mode", mode,
                             "--time-of-day-mode", clock, "--sky-time-slicing", "off", "--ambient", "off"]
                    now, _ = night.render(model, f"legacy_now_{label}", flags)
                    ref, _ = night.render(model, f"legacy_ref_{label}", flags, exe=baseline)
                    check(same(now, ref), f"legacy {label}: byte-identical to {baseline.name} (same flags)")
    for environment in ("hdr", "sky"):
        flags = ["--environment", environment, "--ambient", "on", "--ambient-color", "0.8,0.5,1",
                 "--ambient-intensity", "350", "--sky-time-slicing", "off"]
        now, _ = night.render(model, f"manual_now_{environment}", flags)
        ref, _ = night.render(model, f"manual_ref_{environment}", flags, exe=baseline)
        check(same(now, ref), f"Manual ambient {environment}: byte-identical to {baseline.name}")
    now, _ = night.render(None, "default_now", [])
    ref, _ = night.render(None, "default_ref", [], exe=baseline)
    check(same(now, ref), "HDR defaults: byte-identical")


def sources():
    model = night.write("source_lambert", lambert())
    session = night.Session(model, ["--sky-time-slicing", "off"])
    try:
        for choice in ("off", "hdr", "manual", "clock"):
            session.env("sun_source", choice)
            session.capture(f"source_hdr_{choice}", frames=16)
            source = session.call("get_state")["sun"]["source"]
            check(source["effective"] == choice and source["options"] == ["off", "hdr", "manual", "clock"],
                  f"HDR {choice}: source/options {source}")
            session.env("environment", "sky")
            session.capture(f"source_sky_from_{choice}", frames=16)
            source = session.call("get_state")["sun"]["source"]
            check(source["effective"] in ("manual", "clock") and source["options"] == ["manual", "clock"],
                  f"sky from {choice}: valid source/options {source}")
            session.env("sun_source", "manual")
            session.env("environment", "hdr")
            session.capture(f"source_return_{choice}", frames=16)
            source = session.call("get_state")["sun"]["source"]
            check(source["effective"] == choice, f"return to HDR restores {choice}: {source}")
        session.env("sun_source", "hdr")
        session.env("time", "08:00")
        session.capture("hdr_clock_08", frames=8)
        a = session.call("get_state")["sun"]
        session.env("time", "17:00")
        session.capture("hdr_clock_17", frames=8)
        b = session.call("get_state")["sun"]
        check(a["direction"] == b["direction"] and a["illuminance"] == b["illuminance"] and a["active"],
              "From HDR is locked to panorama with a running clock (direction and illuminance unchanged)")
        check(same(OUT / "hdr_clock_08.png", OUT / "hdr_clock_17.png"),
              "From HDR with the clock at 08:00 / 17:00: byte-identical sphere captures")
        for key, value, expected in (("time_of_day_mode", True, "clock"), ("sun_mode", "hdr", "hdr"),
                                     ("sun_source", "manual", "manual"), ("sun", False, "off"),
                                     ("time_of_day_mode", True, "clock")):
            session.env(key, value)
            source = session.call("get_state")["sun"]["source"]["effective"]
            check(source == expected, f"legacy pipe {key}={value} -> {source}, expected {expected}")
        errors = session.close()
        check(errors == 0, f"source session: {errors} GL errors")
    except BaseException:
        session.process.kill()
        session.process.wait(30)
        raise
    for environment in ("hdr", "sky"):
        for source in (("off", "hdr", "manual", "clock") if environment == "hdr" else ("manual", "clock")):
            flags = ["--environment", environment, "--sky-time-slicing", "off", "--sun-source", source,
                     "--ambient", "auto", "--ambient-mode", "auto", "--ambient-fill", "0.035"]
            cli, _ = night.render(model, f"cli_{environment}_{source}", flags)
            session = night.Session(model, ["--environment", environment, "--sky-time-slicing", "off"])
            try:
                for key, value in (("sun_source", source), ("ambient", "auto"), ("ambient_mode", "auto"), ("ambient_fill", 0.035)):
                    session.env(key, value)
                piped = session.capture(f"pipe_{environment}_{source}", frames=24)
                check(same(cli, piped), f"CLI = pipe: {environment}/{source}, Auto ambient (byte-identical)")
                check(session.close() == 0, f"CLI/pipe {environment}/{source}: zero GL errors")
            except BaseException:
                session.process.kill()
                session.process.wait(30)
                raise
    # The intentionally changed ambiguous HDR ordering: the last source-selecting flag wins.
    for label, flags, expected in (
        ("clock_then_hdr", ["--time-of-day-mode", "on", "--sun-mode", "hdr"], "hdr"),
        ("hdr_then_clock", ["--sun-mode", "hdr", "--time-of-day-mode", "on"], "clock"),
        ("clock_then_off", ["--time-of-day-mode", "on", "--sun-source", "off"], "off")):
        png, report = night.render(model, label, flags)
        check(report["sun"]["source"]["effective"] == expected, f"CLI {label} -> {expected}")
        check(same(png, OUT / f"cli_hdr_{expected}.png"),
              f"CLI {label}: byte-identical to explicit --sun-source {expected}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", type=Path, required=True)
    parser.add_argument("--only", choices=("parity", "sources"))
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    night.OUT = OUT
    if args.only != "sources":
        parity(args.baseline_exe.resolve())
    if args.only != "parity":
        sources()
    check(all(night.gl_errors_zero(log) for _, log in night.gl_logs), f"zero GL errors in {len(night.gl_logs)} captures")
    return finish()


if __name__ == "__main__":
    raise SystemExit(main())
