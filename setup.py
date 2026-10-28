#!/usr/bin/env python3
"""
One-time setup: installs host prerequisites, fetches submodules and builds
the toolchain. Safe to re-run; finished steps are skipped.

    python3 setup.py          full setup
    python3 setup.py --no-install
                                    skip installing system packages

Supported hosts (Linux only for now; on Windows use WSL2):
    Arch / Manjaro / CachyOS        pacman
    Debian / Ubuntu                 apt
    Fedora                          dnf

Steps:
  1. Install any missing prerequisites (cmake, ninja, clang, libc++, OpenSSL, make, gcc, git).
  2. git submodule update --init --recursive
  3. make -f Makefile.toolchain (llvm-mos, llvm-mos-sdk, sprpck, lynxenc, zx02, makelnx)
"""
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent

TOOLS = ["git", "cmake", "ninja", "clang", "clang++", "make", "gcc"]

# Package lists per package manager. Listing everything is fine; the
# managers skip what is already installed.
PACKAGES = {
    "pacman": ["git", "cmake", "ninja", "clang", "libc++", "openssl", "make", "gcc"],
    "apt":    ["git", "cmake", "ninja-build", "clang", "libc++-dev", "libc++abi-dev", "libssl-dev",
               "build-essential"],
    "dnf":    ["git", "cmake", "ninja-build", "clang", "libcxx-devel", "libcxxabi-devel", "openssl-devel",
               "make", "gcc"],
}

INSTALL_CMD = {
    "pacman": ["sudo", "pacman", "-S", "--needed", "--noconfirm"],
    "apt":    ["sudo", "apt-get", "install", "-y"],
    "dnf":    ["sudo", "dnf", "install", "-y"],
}


def run(cmd, **kwargs):
    print("+ " + " ".join(cmd), flush=True)
    try:
        return subprocess.run(cmd, check=True, **kwargs)
    except (subprocess.CalledProcessError, FileNotFoundError):
        print(f"FAILED: {' '.join(cmd)}")
        sys.exit(1)


def step(title):
    print(f"=== {title} ===", flush=True)


def linux_distro():
    try:
        text = Path("/etc/os-release").read_text()
    except OSError:
        return set()
    info = dict(line.split("=", 1) for line in text.splitlines() if "=" in line)
    ids = info.get("ID", "") + " " + info.get("ID_LIKE", "")
    return set(ids.replace('"', "").split())


def package_manager():
    if platform.system() != "Linux":
        return None
    ids = linux_distro()
    if ids & {"arch", "manjaro", "cachyos", "endeavouros"}:
        return "pacman"
    if ids & {"debian", "ubuntu"}:
        return "apt"
    if ids & {"fedora", "rhel"}:
        return "dnf"
    return None


def has_libcxx():
    """True if clang++ can link a program against libc++ (needed on Linux)."""
    if platform.system() != "Linux":
        return True
    if not shutil.which("clang++"):
        return False
    test = subprocess.run(["clang++", "-stdlib=libc++", "-x", "c++", "-", "-o", os.devnull],
                          input="#include <string>\nint main() { std::string s; return 0; }\n",
                          text=True, capture_output=True)
    return test.returncode == 0


def has_openssl():
    """True if gcc can build against OpenSSL (needed by lynxenc)."""
    if not shutil.which("gcc"):
        return False
    test = subprocess.run(["gcc", "-x", "c", "-", "-o", os.devnull, "-lssl", "-lcrypto"],
                          input="#include <openssl/evp.h>\nint main(void) { return 0; }\n",
                          text=True, capture_output=True)
    return test.returncode == 0


def missing_prereqs():
    missing = [t for t in TOOLS if not shutil.which(t)]
    if not has_libcxx():
        missing.append("libc++")
    if not has_openssl():
        missing.append("openssl")
    return missing


def install_prereqs(allow_install):
    step("Checking prerequisites")
    missing = missing_prereqs()
    if not missing:
        print("All prerequisites found")
        return

    print(f"Missing: {', '.join(missing)}")
    pm = package_manager()
    if not allow_install or pm is None:
        if pm is None:
            print("Unsupported host for automatic install. Install these yourself and re-run")
            print("with --no-install. Only Linux is supported for now; on Windows use WSL2.")
        sys.exit(1)

    step(f"Installing prerequisites ({pm})")
    if pm == "apt":
        run(["sudo", "apt-get", "update"])
    run(INSTALL_CMD[pm] + PACKAGES[pm])

    still_missing = missing_prereqs()
    if still_missing:
        print(f"Still missing after install: {', '.join(still_missing)}")
        sys.exit(1)


def fetch_submodules():
    step("Fetching submodules")
    run(["git", "-C", str(ROOT), "submodule", "update", "--init", "--recursive"])


def build_toolchain():
    step("Building toolchain (llvm-mos takes a long time the first time)")
    run(["make", "-C", str(ROOT), "-f", "Makefile.toolchain"])


def main():
    args = sys.argv[1:]
    if args not in ([], ["--no-install"]):
        print(__doc__)
        sys.exit(1)

    install_prereqs(allow_install=not args)
    fetch_submodules()
    build_toolchain()
    print("\nSetup complete. Build the game with: python3 tools/build.py")


if __name__ == "__main__":
    main()
