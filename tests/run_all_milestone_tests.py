#!/usr/bin/env python3
"""
ZERO OS 9.0.0 — Unified Milestone Regression & Verification Runner
Runs all 9 test suites across Milestones 0.0.0 through 9.0.0.
"""

import sys
import subprocess

TESTS = [
    ("check_versions.py",  "tests/check_versions.py"),
    ("test_hardware.py",   "tests/test_hardware.py"),
    ("test_storage_net.py","tests/test_storage_net.py"),
    ("test_desktop.py",    "tests/test_desktop.py"),
    ("test_ecosystem.py",  "tests/test_ecosystem.py"),
    ("test_security.py",   "tests/test_security.py"),
    ("test_intelligence.py","tests/test_intelligence.py"),
    ("test_ultimate.py",   "tests/test_ultimate.py"),
    ("benchmark_suite.py", "tests/benchmark_suite.py"),
]

def main():
    print("=" * 70, flush=True)
    print("      ZERO OS 9.0.0 — COMPLETE SYSTEM-WIDE VERIFICATION", flush=True)
    print("=" * 70, flush=True)

    passed_count = 0
    failed_count = 0

    for name, path in TESTS:
        res = subprocess.run([sys.executable, "-u", path], capture_output=True, text=True)
        if res.returncode == 0:
            print(f"[PASS] {name:<24} (exit code 0)", flush=True)
            passed_count += 1
        else:
            print(f"[FAIL] {name:<24} (exit code {res.returncode})", flush=True)
            if res.stderr:
                print(res.stderr, flush=True)
            if res.stdout:
                print(res.stdout, flush=True)
            failed_count += 1

    print("=" * 70, flush=True)
    print(f"SUMMARY: {passed_count}/{len(TESTS)} suites passed, {failed_count} failures", flush=True)
    print("=" * 70, flush=True)

    if failed_count > 0:
        sys.exit(1)
    else:
        print("ALL ZERO OS SUBSYSTEMS FULLY OPERATIONAL AND VALIDATED (100% OK)", flush=True)
        sys.exit(0)

if __name__ == "__main__":
    main()
