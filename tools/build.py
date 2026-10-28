#!/usr/bin/env python3
"""
Builds the Lynx cart from the settings in src/project.json.

    python3 tools/build.py          build
    python3 tools/build.py clean    delete out/

project.json:
    name, manufacturer, cart_size   LNX header
    eeprom_93c46                    cart has a 93C46 (sys/eeprom.S)
    eeprom_sd_save                  ask the GameDrive to back it with an SD file
    sources                         resident game sources
    overlays                        overlay names in id order; overlay <name>
                                    is built from game/<name>/*.cpp

Steps:
  1. Stage2 loader, assets, and a first pack with placeholder code and
     overlays so the generated headers exist.
  2. Pin every resident symbol the overlays use, or the resident link strips it.
  3. Link the resident until its size stops changing. Asset cart positions
     depend on the code size and are compiled into the code.
  4. Link each overlay against the resident and ZX02-compress it.
  5. Final pack (patching overlay positions into g_overlayTable), disassembly,
     and the .lnx.
"""
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

SDK_BIN  = "../submodules/llvm-mos-sdk/bin/bin"
LLVM_BIN = "../submodules/llvm-mos/build/bin"
SDK_LIB  = "../submodules/llvm-mos-sdk/bin/mos-platform/common/lib"
BUILD    = "../out"
GEN      = f"{BUILD}/gen"
OVL      = f"{BUILD}/ovl"
PREPASS  = f"{BUILD}/prepass"
TOOLS    = "../tools"

CXX     = [f"{LLVM_BIN}/clang++", f"--config={SDK_BIN}/mos-lynx.cfg"]
NM      = f"{LLVM_BIN}/llvm-nm"
OBJCOPY = f"{LLVM_BIN}/llvm-objcopy"
OBJDUMP = f"{LLVM_BIN}/llvm-objdump"
LLVM_MC = f"{LLVM_BIN}/llvm-mc"
ZX02    = f"{TOOLS}/zx02/build/zx02"
MAKELNX = f"{TOOLS}/makelnx/makelnx"

CFLAGS = [
    "-O2", "-g", "-Wall", "-Wextra", "-Wno-incompatible-library-redeclaration", "-mlto-zp=0",
    "-I.", f"-I{GEN}", "-I../submodules/llvm-mos-sdk/mos-platform/lynx",
    "-ffunction-sections", "-fdata-sections",
]
CXXFLAGS = CFLAGS + ["-fno-exceptions", "-fno-rtti"]
ASFLAGS  = [f for f in CFLAGS if not f.startswith("-mlto-zp")]
LDFLAGS  = ["-nostartfiles", f"-Wl,-Map,{BUILD}/x.map", "-flto", "-Wl,--allow-multiple-definition", f"-L{SDK_LIB}"]

SDK_CPP = ["sys/sys.cpp", "sys/sprite.cpp", "sys/suzy_math.cpp", "sys/chipper/chipper_sfx.cpp", "sys/audio.cpp"]
SDK_ASM = ["sys/crt0.S", "sys/zx0.S", "sys/cart.S", "sys/cart_asm.S", "sys/eeprom.S",
           "sys/chipper/chipper.S", "sys/chipper/music_demo.S"]

OVERLAY_RAM_LIMIT = 0xC038
LNX_EEPROM_OFFSET  = 60
LNX_EEPROM_93C46   = 1
LNX_EEPROM_SD_SAVE = 64


def run(cmd, **kwargs):
    try:
        return subprocess.run(cmd, check=True, **kwargs)
    except subprocess.CalledProcessError:
        print(f"FAILED: {' '.join(cmd)}")
        sys.exit(1)


def step(title):
    print(f"=== {title} ===", flush=True)


def symbol(elf, name):
    out = run([NM, elf], capture_output=True, text=True).stdout
    for line in out.splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[2] == name:
            return fields[0]
    return None


def nm_names(args, objs):
    out = run([NM] + args + ["-j"] + objs, capture_output=True, text=True).stdout
    return {line for line in out.splitlines() if line and not line.endswith(":")}


def size(path):
    return os.path.getsize(path)


def build_stage2():
    obj = f"{BUILD}/stage2_loader.o"
    bin_ = f"{BUILD}/stage2_loader.bin"
    run([LLVM_MC, "-triple=mos", "-mcpu=mos65c02", "-filetype=obj", "-o", obj, "sys/loaders/stage2_loader.s"])
    run([OBJCOPY, "-O", "binary", "--only-section=.text", obj, bin_])
    if size(bin_) > 256:
        print(f"ERROR: stage2 loader is {size(bin_)} bytes, must be <= 256")
        sys.exit(1)
    print(f"Stage2 loader size: {size(bin_)} bytes")


def pack(overlays, *extra):
    cmd = ["python3", f"{TOOLS}/pack_lynx_cart.py", f"{BUILD}/assets", f"{BUILD}/code.bin", "--asset_src", "assets"]
    if overlays:
        cmd += ["--overlays", OVL, ",".join(overlays)]
    run(cmd + ["--gen_dir", GEN] + list(extra))


def write_pins(resident_cpp, overlay_sources):
    pins = f"{BUILD}/ovl_pins.rsp"
    if not overlay_sources:
        Path(pins).write_text("")
        return

    run(["rm", "-rf", PREPASS])
    os.makedirs(PREPASS)

    def compile_(src, flags):
        obj = f"{PREPASS}/{src.replace('/', '_')}.o"
        run(CXX + flags + ["-c", src, "-o", obj])
        return obj

    ovl_objs = [compile_(src, CXXFLAGS) for src in overlay_sources]
    res_objs = [compile_(src, CXXFLAGS) for src in resident_cpp] + [compile_(src, ASFLAGS) for src in SDK_ASM]

    shared = nm_names(["-u"], ovl_objs) & nm_names(["--defined-only"], res_objs)
    shared = run(["sort"], input="\n".join(shared), capture_output=True, text=True).stdout.split()
    Path(pins).write_text("".join(f"-Wl,-u,{name}\n" for name in shared))
    print(f"{len(shared)} resident symbols pinned for overlays")


def link_resident(resident_cpp, overlays):
    cmd = CXX + CXXFLAGS + LDFLAGS + [f"@{BUILD}/ovl_pins.rsp", "-T", "sys/lyx.ld"] + resident_cpp + SDK_ASM + ["-o", f"{BUILD}/t.elf"]
    prev = None
    for attempt in range(1, 5):
        step(f"Resident pass {attempt}")
        run(cmd)
        run([OBJCOPY, "-O", "binary", f"{BUILD}/t.elf", f"{BUILD}/code.bin"])
        code_size = size(f"{BUILD}/code.bin")
        if code_size == prev:
            return
        prev = code_size
        pack(overlays)
    print("ERROR: resident code size did not settle")
    sys.exit(1)


def build_overlay(name, base, zp):
    elf = f"{OVL}/{name}.elf"
    raw = f"{OVL}/{name}.raw"
    zx  = f"{OVL}/{name}.zx02"
    sources = sorted(str(p) for p in Path(f"game/{name}").glob("*.cpp"))

    run(CXX + CXXFLAGS + ["-nostartfiles", "-flto", f"-L{SDK_LIB}", "-T", "sys/lyx_overlay.ld",
                          f"-Wl,--just-symbols={BUILD}/t.elf", f"-Wl,-Map,{OVL}/{name}.map",
                          f"-Wl,--defsym,OVERLAY_BASE=0x{base}", f"-Wl,--defsym,OVERLAY_ZP_BASE=0x{zp}"]
        + sources + ["-o", elf])
    run([OBJCOPY, "-O", "binary", "--only-section=.overlay_header", "--only-section=.text",
         "--only-section=.rodata", "--only-section=.data", elf, raw])

    if os.path.exists(zx):
        os.remove(zx)
    run([ZX02, raw, zx], stdout=subprocess.DEVNULL)

    raw_size = size(raw)
    Path(f"{OVL}/{name}.bin").write_bytes(bytes([raw_size & 0xFF, raw_size >> 8]) + Path(zx).read_bytes())

    end = symbol(elf, "__overlay_bss_end")
    print(f"  {name:<12} {raw_size:5d} bytes, {size(f'{OVL}/{name}.bin'):5d} on cart, "
          f"RAM end ${end} (limit ${OVERLAY_RAM_LIMIT:04X})")


def build_cart(project, overlays):
    elf = f"{BUILD}/t.elf"
    with open(f"{BUILD}/disasm-full.txt", "wb") as f:
        run([OBJDUMP, "-d", "--source", "--line-numbers", elf], stdout=f)

    table = symbol(elf, "g_overlayTable")
    pack(overlays, "--show_cart_layout", *(["--overlay_table", table] if table else []))
    os.replace(f"{BUILD}/code.lyx", f"{BUILD}/a.lyx")

    lyx = Path(f"{BUILD}/a.lyx").read_bytes()
    Path(f"{BUILD}/a.o").write_bytes(lyx[:2048 + size(f"{BUILD}/code.bin")])

    lnx = f"{BUILD}/a.lnx"
    run([MAKELNX, f"{BUILD}/a.lyx", "-b0", project["cart_size"], "-g", project["name"],
         "-m", project["manufacturer"], "-o", lnx])
    eeprom = 0
    if project.get("eeprom_93c46"):
        eeprom |= LNX_EEPROM_93C46
    if project.get("eeprom_sd_save"):
        eeprom |= LNX_EEPROM_SD_SAVE
    with open(lnx, "r+b") as f:
        f.seek(LNX_EEPROM_OFFSET)
        f.write(bytes([eeprom]))
    Path(f"{BUILD}/{project['name']}.lnx").write_bytes(Path(lnx).read_bytes())


def main():
    os.chdir(ROOT / "src")
    if sys.argv[1:] == ["clean"]:
        shutil.rmtree(BUILD, ignore_errors=True)
        return
    if sys.argv[1:]:
        print(__doc__)
        sys.exit(1)

    if not all(os.path.exists(t) for t in (f"{LLVM_BIN}/clang", f"{SDK_BIN}/mos-lynx.cfg", ZX02, MAKELNX)):
        print("Toolchain not built. Run 'python3 setup.py' first")
        sys.exit(1)

    project = json.loads(Path("project.json").read_text())
    overlays = project.get("overlays", [])
    overlay_sources = [str(p) for o in overlays for p in sorted(Path(f"game/{o}").glob("*.cpp"))]
    resident_cpp = project["sources"] + SDK_CPP + (["sys/overlay.cpp"] if overlays else [])

    os.makedirs(OVL, exist_ok=True)
    step("Stage2 loader")
    build_stage2()

    step("Assets")
    run(["python3", f"{TOOLS}/process_assets.py"])

    step("Generate headers")
    for name in overlays:
        Path(f"{OVL}/{name}.bin").write_bytes(bytes(16))
    Path(f"{BUILD}/code.bin").write_bytes(bytes(256))
    pack(overlays)

    step("Pin resident symbols used by overlays")
    write_pins(resident_cpp, overlay_sources)

    link_resident(resident_cpp, overlays)

    if overlays:
        base = symbol(f"{BUILD}/t.elf", "__overlay_start")
        zp = symbol(f"{BUILD}/t.elf", "__zp_bss_resident_end")
        step(f"Overlays at ${base}, zero page ${zp}")
        for name in overlays:
            build_overlay(name, base, zp)

    step("Cart")
    build_cart(project, overlays)

    print()
    print("Build complete!")
    print("  a.o   - Raw BLL binary (for ComLynx/development)")
    print("  a.lnx - LNX format (for emulators and flash carts)")
    print(f"  {project['name']}.lnx - final cart")


if __name__ == "__main__":
    main()
