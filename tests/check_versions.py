#!/usr/bin/env python3
"""Enforces the exact-ten-version policy and that the kernel's version matches the manifest."""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

EXPECTED = [
    ("0.0.0", "Genesis"), ("1.0.0", "Core Foundation"), ("2.0.0", "System Architecture"),
    ("3.0.0", "Hardware Foundation"), ("4.0.0", "Storage & Networking"), ("5.0.0", "ZERO Desktop"),
    ("6.0.0", "ZERO Ecosystem"), ("7.0.0", "ZERO Security"),
    ("8.0.0", "ZERO Intelligence"), ("9.0.0", "ZERO Ultimate"),
]


def main():
    errors = []
    m = json.load(open(os.path.join(ROOT, "configs", "versions.json")))
    got = [(v["version"], v["name"]) for v in m["versions"]]
    if len(got) != 10:
        errors.append(f"manifest has {len(got)} entries, must be exactly 10")
    if got != EXPECTED:
        errors.append(f"manifest entries/order differ from the official list: {got}")
    versions = [v["version"] for v in m["versions"]]
    if m["current"] not in versions:
        errors.append(f"current {m['current']!r} is not one of the ten official versions")
    else:
        ci = versions.index(m["current"])
        for i, v in enumerate(m["versions"]):
            want = "complete" if i < ci else "in-progress" if i == ci else "planned"
            if v["status"] != want:
                errors.append(f"{v['version']} status is {v['status']!r}, must be {want!r} while current is {m['current']}")

    h = open(os.path.join(ROOT, "kernel", "include", "zero", "version.h")).read()
    kv = re.search(r'#define ZERO_VERSION_STRING "([^"]+)"', h).group(1)
    kn = re.search(r'#define ZERO_CODENAME "([^"]+)"', h).group(1)
    if (kv, kn) != (m["current"], dict(got).get(m["current"])):
        errors.append(f"kernel version.h says {kv} {kn!r}, manifest current is {m['current']}")

    if errors:
        print("version manifest check FAILED:")
        for e in errors:
            print("  -", e)
        return 1
    print(f"version manifest check OK: exactly 10 official versions, current = {m['current']} {kn}, kernel matches")
    return 0


if __name__ == "__main__":
    sys.exit(main())
