#!/usr/bin/env python3
import os
import subprocess
import sys


def main():
    if len(sys.argv) != 3:
        print("usage: compare_conformance_traces.py <native-mesa-executable> <manifestgl-executable>", file=sys.stderr)
        return 2

    native_env = os.environ.copy()
    native_env.pop("LD_LIBRARY_PATH", None)
    native_env["LIBGL_ALWAYS_SOFTWARE"] = "1"

    try:
        expected = subprocess.run(
            [sys.argv[1]], env=native_env, capture_output=True, text=True, timeout=45
        )
        actual = subprocess.run(
            [sys.argv[2]], capture_output=True, text=True, timeout=45
        )
    except subprocess.TimeoutExpired as exc:
        print(f"Conformance executable timed out: {exc.cmd}", file=sys.stderr)
        return 1

    if expected.returncode or actual.returncode:
        print(f"Mesa exit code: {expected.returncode}\n{expected.stderr}", file=sys.stderr)
        print(f"ManifestGL exit code: {actual.returncode}\n{actual.stderr}", file=sys.stderr)
        return 1

    if not expected.stdout.strip() or not actual.stdout.strip():
        print("Conformance executable did not produce a behavior trace", file=sys.stderr)
        return 1

    if expected.stdout != actual.stdout:
        print("Mesa behavior trace:\n" + expected.stdout)
        print("ManifestGL behavior trace:\n" + actual.stdout)
        return 1

    print(expected.stderr.strip())
    print(f"Mesa and ManifestGL traces match ({len(actual.stdout.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
