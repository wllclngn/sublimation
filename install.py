#!/usr/bin/env python3
"""
sublimation installer

Builds and installs libsublimation (C23 adaptive sorting library)
with Python, Rust, and Go bindings.

Usage:
    ./install.py              # Build and install (default)
    ./install.py build        # Build only
    ./install.py clean        # Clean build artifacts
    ./install.py uninstall    # Remove installed files
    ./install.py test         # Run test suite
    ./install.py status       # Show build/install status
    ./install.py bench        # Run benchmarks
    ./install.py fuzz         # Build and run libFuzzer differential target
"""

import argparse
import multiprocessing
import os
import sys
import shutil
import subprocess
from pathlib import Path
from datetime import datetime


# CONFIGURATION

SCRIPT_DIR = Path(__file__).parent.resolve()
BUILD_DIR = SCRIPT_DIR / "build"
SRC_DIR = SCRIPT_DIR / "src"
INSTALL_PREFIX = Path("/usr/local")
LIB_NAME = "sublimation"


# LOGGING

def _timestamp() -> str:
    return datetime.now().strftime("[%H:%M:%S]")


def log_debug(msg: str) -> None:
    print(f"{_timestamp()} [DEBUG]  {msg}")


def log_info(msg: str) -> None:
    print(f"{_timestamp()} [INFO]   {msg}")


def log_warn(msg: str) -> None:
    print(f"{_timestamp()} [WARN]   {msg}")


def log_error(msg: str) -> None:
    print(f"{_timestamp()} [ERROR]  {msg}")


# COMMAND EXECUTION

def run_cmd(cmd: list, cwd: Path | None = None) -> int:
    print(f">>> {' '.join(str(c) for c in cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    return result.returncode


def run_cmd_capture(cmd: list, cwd: Path | None = None) -> tuple[int, str, str]:
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)
    return result.returncode, result.stdout, result.stderr


# DEPENDENCY CHECKS

def check_gcc() -> bool:
    ret, stdout, _ = run_cmd_capture(["gcc", "--version"])
    if ret != 0:
        return False
    # Check for C23 support (GCC 13+)
    first_line = stdout.split("\n")[0]
    log_info(f"gcc: {first_line}")
    return True


def check_pkg(name: str) -> bool:
    ret, _, _ = run_cmd_capture(["which", name])
    return ret == 0


def check_deps() -> bool:
    ok = True

    if not check_gcc():
        log_error("gcc not found (need GCC 13+ for C23)")
        ok = False

    if not check_pkg("ar"):
        log_error("ar not found (install binutils)")
        ok = False

    # Optional: Rust bindings
    if check_pkg("rustc"):
        log_info("rustc found (Rust bindings: enabled)")
    else:
        log_warn("rustc not found (Rust bindings: skipped)")

    # Optional: Go bindings
    if check_pkg("go"):
        log_info("go found (Go bindings: enabled)")
    else:
        log_warn("go not found (Go bindings: skipped)")

    # Optional: Python bindings (ctypes is stdlib, just need python3)
    if check_pkg("python3"):
        log_info("python3 found (Python bindings: enabled)")
    else:
        log_warn("python3 not found (Python bindings: skipped)")

    return ok


# BUILD

def cmd_build(args, source_dir: Path) -> bool:
    log_info("CHECKING DEPENDENCIES")

    if not check_deps():
        return False

    print()
    log_info("BUILDING libsublimation [C23]")

    BUILD_DIR.mkdir(exist_ok=True)

    jobs = multiprocessing.cpu_count()
    log_info(f"Using {jobs} parallel jobs")

    # C23 core: compile to .o, archive to .a and .so
    # Flags from C23_CPP23_SAFETY_REFERENCE.md
    c_flags = [
        "-std=c2x", "-O2", "-march=native",
        "-Wall", "-Wextra", "-Wpedantic",
        "-Wshadow", "-Wconversion", "-Wsign-conversion", "-Wcast-qual",
        "-Wformat=2", "-Wformat-security", "-Wnull-dereference",
        "-Wdouble-promotion", "-Wrestrict", "-Wcast-align",
        "-fPIC", "-flto=auto", "-fvisibility=hidden",
        "-fstack-protector-strong", "-fstack-clash-protection", "-fcf-protection",
        "-D_FORTIFY_SOURCE=3",
        "-pthread",
    ]

    if args.debug:
        c_flags = [f for f in c_flags if not f.startswith("-O")]
        c_flags.extend(["-O0", "-g3", "-DDEBUG",
                        "-fsanitize=address,undefined",
                        "-fno-omit-frame-pointer"])
        log_info("Build type: Debug (ASan + UBSan)")
    else:
        log_info("Build type: Release")

    # Find all .c source files
    if not SRC_DIR.exists():
        log_warn("src/ directory not found (nothing to build yet)")
        log_info("Project scaffolding only. Create src/ to begin.")
        return True

    c_sources = list(SRC_DIR.glob("**/*.c"))
    if not c_sources:
        log_warn("No .c source files found in src/")
        return True

    # Compile each .c to .o
    objects = []
    for src in c_sources:
        obj = BUILD_DIR / src.with_suffix(".o").name
        cmd = ["gcc"] + c_flags + ["-I", str(SRC_DIR / "include"), "-I", str(SRC_DIR), "-c", str(src), "-o", str(obj)]
        ret = run_cmd(cmd)
        if ret != 0:
            log_error(f"Compile failed: {src.name}")
            return False
        objects.append(obj)

    # Static library
    static_lib = BUILD_DIR / f"lib{LIB_NAME}.a"
    ret = run_cmd(["ar", "rcs", str(static_lib)] + [str(o) for o in objects])
    if ret != 0:
        log_error("Static library archive failed")
        return False
    log_info(f"Built: {static_lib} ({static_lib.stat().st_size} bytes)")

    # Shared library
    shared_lib = BUILD_DIR / f"lib{LIB_NAME}.so"
    ld_harden = ["-Wl,-z,relro", "-Wl,-z,now", "-Wl,-z,noexecstack"]
    cmd = ["gcc"] + c_flags + ["-shared", "-o", str(shared_lib)] + [str(o) for o in objects] + ["-lpthread", "-lm"] + ld_harden
    ret = run_cmd(cmd)
    if ret != 0:
        log_error("Shared library link failed")
        return False
    log_info(f"Built: {shared_lib} ({shared_lib.stat().st_size} bytes)")

    print()

    # Rust bindings (optional)
    # Link against the shared library: libsublimation.a contains LTO GIMPLE
    # objects that rustc/lld can't consume directly. .so is a real ELF and
    # works without requiring a non-LTO static build.
    rust_dir = source_dir / "bindings" / "rust"
    rust_src = rust_dir / "bench_direct.rs"
    if rust_src.exists() and check_pkg("rustc"):
        log_info("BUILDING Rust bindings")
        ret = run_cmd([
            "rustc", "-O", "--edition", "2021",
            "-L", str(BUILD_DIR), "-l", "dylib=sublimation",
            "-l", "dylib=pthread", "-l", "dylib=m",
            "-C", f"link-arg=-Wl,-rpath,{BUILD_DIR}",
            str(rust_src), "-o", str(BUILD_DIR / "bench_rust_direct"),
        ])
        if ret != 0:
            log_warn("Rust bindings build failed (continuing)")
        else:
            log_info("Rust bindings built")
        print()

    # Go bindings (optional)
    go_dir = source_dir / "bindings" / "go"
    if go_dir.exists() and check_pkg("go"):
        log_info("BUILDING Go bindings")
        ret = run_cmd(["go", "build", "./..."], cwd=go_dir)
        if ret != 0:
            log_warn("Go bindings build failed (continuing)")
        else:
            log_info("Go bindings built")
        print()

    log_info("BUILD COMPLETE")
    return True


def cmd_install(args, source_dir: Path) -> bool:
    if not cmd_build(args, source_dir):
        return False

    prefix = Path(args.prefix) if args.prefix else INSTALL_PREFIX
    lib_dir = prefix / "lib"
    include_dir = prefix / "include" / LIB_NAME

    print()
    log_info("INSTALLING")

    static_lib = BUILD_DIR / f"lib{LIB_NAME}.a"
    shared_lib = BUILD_DIR / f"lib{LIB_NAME}.so"

    if not static_lib.exists() and not shared_lib.exists():
        log_warn("No libraries built (nothing to install)")
        return True

    # Install libraries
    if static_lib.exists():
        log_info(f"Installing {lib_dir / static_lib.name}")
        ret = run_cmd(["sudo", "install", "-Dm644", str(static_lib), str(lib_dir / static_lib.name)])
        if ret != 0:
            log_error("Failed to install static library")
            return False

    if shared_lib.exists():
        log_info(f"Installing {lib_dir / shared_lib.name}")
        ret = run_cmd(["sudo", "install", "-Dm755", str(shared_lib), str(lib_dir / shared_lib.name)])
        if ret != 0:
            log_error("Failed to install shared library")
            return False
        run_cmd(["sudo", "ldconfig"])

    # Install headers
    header_dir = SRC_DIR / "include"
    if header_dir.exists():
        headers = list(header_dir.glob("**/*.h"))
        for h in headers:
            rel = h.relative_to(header_dir)
            dest = include_dir / rel
            log_info(f"Installing {dest}")
            ret = run_cmd(["sudo", "install", "-Dm644", str(h), str(dest)])
            if ret != 0:
                log_error(f"Failed to install header: {h.name}")
                return False

    print()
    log_info("SUCCESS. Installation complete.")
    log_info(f"Library: {lib_dir}/lib{LIB_NAME}.{{a,so}}")
    log_info(f"Headers: {include_dir}/")
    return True


def cmd_clean(args, source_dir: Path) -> bool:
    log_info("CLEANING")

    if BUILD_DIR.exists():
        log_info(f"Removing {BUILD_DIR}")
        shutil.rmtree(BUILD_DIR)

    # Rust clean
    rust_dir = source_dir / "bindings" / "rust"
    if (rust_dir / "target").exists():
        log_info("Cleaning Rust bindings")
        run_cmd(["cargo", "clean"], cwd=rust_dir)

    # Go clean
    go_dir = source_dir / "bindings" / "go"
    if go_dir.exists() and check_pkg("go"):
        log_info("Cleaning Go bindings")
        run_cmd(["go", "clean"], cwd=go_dir)

    log_info("Clean complete")
    return True


def cmd_uninstall(args, source_dir: Path) -> bool:
    prefix = Path(args.prefix) if args.prefix else INSTALL_PREFIX
    lib_dir = prefix / "lib"
    include_dir = prefix / "include" / LIB_NAME

    log_info("UNINSTALLING")

    files = [
        lib_dir / f"lib{LIB_NAME}.a",
        lib_dir / f"lib{LIB_NAME}.so",
    ]

    removed = False
    for f in files:
        if f.exists():
            log_info(f"Removing {f}")
            run_cmd(["sudo", "rm", "-f", str(f)])
            removed = True

    if include_dir.exists():
        log_info(f"Removing {include_dir}")
        run_cmd(["sudo", "rm", "-rf", str(include_dir)])
        removed = True

    if not removed:
        log_warn("No installed files found")
    else:
        run_cmd(["sudo", "ldconfig"])
        log_info("Uninstall complete")

    return True


def cmd_test(args, source_dir: Path) -> bool:
    if not cmd_build(args, source_dir):
        return False

    print()
    log_info("RUNNING TESTS")

    test_dir = source_dir / "tests"
    if not test_dir.exists():
        log_warn("tests/ directory not found")
        return True

    test_sources = list(test_dir.glob("*.c"))
    if not test_sources:
        log_warn("No test files found")
        return True

    static_lib = BUILD_DIR / f"lib{LIB_NAME}.a"
    passed = 0
    failed = 0

    for test_src in sorted(test_sources):
        test_bin = BUILD_DIR / f"test_{test_src.stem}"
        cmd = [
            "gcc", "-std=c2x", "-O2", "-march=native",
            "-I", str(SRC_DIR / "include"), "-I", str(SRC_DIR),
            str(test_src), str(static_lib),
            "-lpthread", "-lm",
            "-o", str(test_bin),
        ]
        if args.debug:
            cmd.insert(3, "-fsanitize=address,undefined")
            cmd.insert(3, "-g3")
        ret = run_cmd(cmd)
        if ret != 0:
            log_error(f"COMPILE FAIL: {test_src.name}")
            failed += 1
            continue

        ret = run_cmd([str(test_bin)])
        if ret == 0:
            log_info(f"PASS: {test_src.stem}")
            passed += 1
        else:
            log_error(f"FAIL: {test_src.stem}")
            failed += 1

    print()
    log_info(f"Results: {passed} passed, {failed} failed")
    return failed == 0


def cmd_status(args, source_dir: Path) -> bool:
    log_info("STATUS")
    print()

    # Build status
    static_lib = BUILD_DIR / f"lib{LIB_NAME}.a"
    shared_lib = BUILD_DIR / f"lib{LIB_NAME}.so"

    print(f"  Static lib:  {static_lib}")
    if static_lib.exists():
        print(f"               built ({static_lib.stat().st_size} bytes)")
    else:
        print(f"               not built")

    print(f"  Shared lib:  {shared_lib}")
    if shared_lib.exists():
        print(f"               built ({shared_lib.stat().st_size} bytes)")
    else:
        print(f"               not built")
    print()

    # Install status
    prefix = Path(args.prefix) if args.prefix else INSTALL_PREFIX
    lib_dir = prefix / "lib"
    include_dir = prefix / "include" / LIB_NAME

    installed_a = (lib_dir / f"lib{LIB_NAME}.a").exists()
    installed_so = (lib_dir / f"lib{LIB_NAME}.so").exists()
    installed_h = include_dir.exists()

    print(f"  Installed .a:  {'yes' if installed_a else 'no'}")
    print(f"  Installed .so: {'yes' if installed_so else 'no'}")
    print(f"  Headers:       {'yes' if installed_h else 'no'}")
    print()

    # Bindings status
    rust_dir = source_dir / "bindings" / "rust"
    go_dir = source_dir / "bindings" / "go"
    py_dir = source_dir / "bindings" / "python"

    print(f"  Rust bindings:   {'present' if rust_dir.exists() else 'not found'}")
    print(f"  Go bindings:     {'present' if go_dir.exists() else 'not found'}")
    print(f"  Python bindings: {'present' if py_dir.exists() else 'not found'}")
    print()

    return True


def cmd_bench(args, source_dir: Path) -> bool:
    if not cmd_build(args, source_dir):
        return False

    print()
    log_info("RUNNING BENCHMARKS")

    bench_dir = source_dir / "tests" / "bench"
    if not bench_dir.exists():
        log_warn("tests/bench/ directory not found")
        return True

    bench_sources = list(bench_dir.glob("*.c"))
    if not bench_sources:
        log_warn("No benchmark files found")
        return True

    static_lib = BUILD_DIR / f"lib{LIB_NAME}.a"

    for bench_src in sorted(bench_sources):
        bench_bin = BUILD_DIR / f"bench_{bench_src.stem}"
        cmd = [
            "gcc", "-std=c2x", "-O2", "-march=native", "-flto=auto",
            "-I", str(SRC_DIR / "include"), "-I", str(SRC_DIR),
            str(bench_src), str(static_lib),
            "-lpthread", "-lm",
            "-o", str(bench_bin),
        ]
        ret = run_cmd(cmd)
        if ret != 0:
            log_error(f"COMPILE FAIL: {bench_src.name}")
            continue

        print()
        log_info(f"BENCH: {bench_src.stem}")
        run_cmd([str(bench_bin)])
        print()

    return True


def cmd_fuzz(args, source_dir: Path) -> bool:
    if not cmd_build(args, source_dir):
        return False

    print()
    log_info("BUILDING FUZZER")

    static_lib = BUILD_DIR / f"lib{LIB_NAME}.a"
    if not static_lib.exists():
        log_error("libsublimation.a not found -- build first")
        return False

    fuzz_src = source_dir / "tests" / "fuzz_sort.c"
    if not fuzz_src.exists():
        log_error("tests/fuzz_sort.c not found")
        return False

    fuzz_bin = BUILD_DIR / "fuzz_sort"
    cmd = [
        "clang",
        "-fsanitize=fuzzer,address,undefined",
        "-O2",
        "-I", str(SRC_DIR / "include"), "-I", str(SRC_DIR),
        str(fuzz_src), str(static_lib),
        "-lpthread", "-lm",
        "-o", str(fuzz_bin),
    ]
    ret = run_cmd(cmd)
    if ret != 0:
        log_error("Fuzzer compile failed")
        return False

    log_info(f"Built: {fuzz_bin}")
    print()
    log_info("RUNNING FUZZER (60s)")

    ret = run_cmd([str(fuzz_bin), "-max_len=800000", "-max_total_time=60"],
                  cwd=str(BUILD_DIR))
    if ret != 0:
        log_error("Fuzzer found a crash!")
        return False

    log_info("Fuzzer completed -- no crashes")
    return True


# MAIN

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build and install libsublimation (C23 adaptive sorting library)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Commands:
  (default)   Build and install
  build       Build only
  clean       Clean build artifacts
  uninstall   Remove installed files
  test        Run test suite
  status      Show build/install status
  bench       Run benchmarks
  fuzz        Run libFuzzer differential sort target (60s)

Examples:
  ./install.py                    # Build and install to /usr/local
  ./install.py build              # Build only
  ./install.py build --debug      # Debug build
  ./install.py test               # Build and run tests
  ./install.py bench              # Build and run benchmarks
  ./install.py status             # Check status
  ./install.py uninstall          # Remove installed files
"""
    )

    parser.add_argument("command", nargs="?", default="install",
                       choices=["install", "build", "clean", "uninstall", "test", "status", "bench", "fuzz"],
                       help="Command to run (default: install)")
    parser.add_argument("--debug", action="store_true",
                       help="Build with debug symbols")
    parser.add_argument("--prefix", type=str, default=None,
                       help="Installation prefix (default: /usr/local)")

    args = parser.parse_args()

    source_dir = SCRIPT_DIR

    print()
    log_info("sublimation installer")
    log_info(f"Source: {source_dir}")
    print()

    commands = {
        "install": cmd_install,
        "build": cmd_build,
        "clean": cmd_clean,
        "uninstall": cmd_uninstall,
        "test": cmd_test,
        "status": cmd_status,
        "bench": cmd_bench,
        "fuzz": cmd_fuzz,
    }

    success = commands[args.command](args, source_dir)
    return 0 if success else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nInterrupted by user.")
        sys.exit(130)
