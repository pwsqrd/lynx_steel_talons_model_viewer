#!/usr/bin/env python3

import json
import os
import subprocess
import sys

def run_command(cmd):
    """Run a shell command and print its output."""
    print(f"Running: {cmd}")
    result = subprocess.run(cmd, shell=True, check=False)
    if result.returncode != 0:
        print(f"Command failed with exit code {result.returncode}")
        return False
    return True

def process_directory(src_dir, dst_dir, compress=False, zx02_path=None):
    """Process files from source directory to destination directory with optional compression."""
    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir, exist_ok=True)
        print(f"Created directory: {dst_dir}")

    for item in os.listdir(src_dir):
        if item == 'config.json':
            continue

        src_path = os.path.join(src_dir, item)
        dst_path = os.path.join(dst_dir, item)

        if os.path.isdir(src_path):
            process_directory(src_path, dst_path, compress, zx02_path)
        else:
            if compress:
                with open(src_path, 'rb') as f:
                    orig_size = len(f.read())
                print(f"  Compressing: {item}")
                zx02_compress_file(src_path, dst_path, zx02_path)
                with open(dst_path, 'rb') as f:
                    comp_size = len(f.read())
                ratio = (1 - comp_size / orig_size) * 100 if orig_size > 0 else 0
                print(f"    {orig_size} -> {comp_size} bytes ({ratio:.1f}% reduction)")
            else:
                print(f"  Copying: {item}")
                with open(src_path, 'rb') as src, open(dst_path, 'wb') as dst:
                    dst.write(src.read())

def zx02_compress_file(input_path, output_path, zx02_path):
    """
    Compress a binary file using zx02 and save to output_path.
    Format: [uncompressed_size_lo][uncompressed_size_hi][zx02_data...]
    """
    import tempfile

    with open(input_path, 'rb') as infile:
        data = infile.read()
        if not data:
            with open(output_path, 'wb') as outfile:
                outfile.write(bytes([0, 0]))
            return

    file_length = len(data)

    with tempfile.NamedTemporaryFile(delete=True, suffix='.zx02') as tmp:
        tmp_path = tmp.name

    if os.path.exists(tmp_path):
        os.remove(tmp_path)

    try:
        result = subprocess.run(
            [zx02_path, input_path, tmp_path],
            capture_output=True,
            text=True
        )
        if result.returncode != 0:
            print(f"    zx02 failed: {result.stderr}")
            raise RuntimeError(f"zx02 compression failed for {input_path}")

        with open(tmp_path, 'rb') as compressed_file:
            compressed_data = compressed_file.read()

        with open(output_path, 'wb') as outfile:
            outfile.write(bytes([file_length & 0xFF, (file_length >> 8) & 0xFF]))
            outfile.write(compressed_data)

    finally:
        if os.path.exists(tmp_path):
            os.remove(tmp_path)

# Suzy SPRCTL0 / SPRCTL1 bits, matching Sprctl0 / Sprctl1 in sys/suzy.h.
SPRCTL0_BPP = {1: 0x00, 2: 0x40, 3: 0x80, 4: 0xC0}
SPRCTL0_TYPE = {
    'background': 0x00, 'backnoncoll': 0x01, 'bshadow': 0x02, 'boundary': 0x03,
    'normal': 0x04, 'noncoll': 0x05, 'xor': 0x06, 'shadow': 0x07,
}
SPRCTL1_LITERAL = 0x80

def add_sprite_header(spr_path, meta):
    """
    Prepend the 4-byte sys::sprite::Sprite header [width, height, sprctl0, sprctl1]
    so it ends up inside the compressed asset.
    """
    width = int(meta.get('size_x', 0))
    height = int(meta.get('size_y', 0))
    if not (0 < width < 256 and 0 < height < 256):
        raise RuntimeError(f"{spr_path}: sprite size {width}x{height} does not fit the header")
    sprite_type = meta.get('sprite_type', 'normal')
    if sprite_type not in SPRCTL0_TYPE:
        raise RuntimeError(f"{spr_path}: unknown sprite_type '{sprite_type}'")
    ctl0 = SPRCTL0_BPP[int(meta.get('bpp', 4))] | SPRCTL0_TYPE[sprite_type]
    ctl1 = 0 if meta.get('packed', True) else SPRCTL1_LITERAL
    with open(spr_path, 'rb') as f:
        data = f.read()
    with open(spr_path, 'wb') as f:
        f.write(bytes([width, height, ctl0, ctl1]))
        f.write(data)

def find_asset_folders(base_dir):
    """
    Recursively find all asset folders.
    Returns a list of (folder_path, relative_path, asset_type) tuples.
    - Folders with config.json use their asset_type setting
    - Folders without config.json but with files are treated as 'data'
    """
    asset_folders = []
    processed_dirs = set()

    for root, dirs, files in os.walk(base_dir):
        if root == base_dir:
            continue

        # Check if any parent was already processed as an asset folder
        skip = False
        for processed in processed_dirs:
            if root.startswith(processed + os.sep):
                skip = True
                break
        if skip:
            continue

        if 'config.json' in files:
            config_path = os.path.join(root, 'config.json')
            try:
                with open(config_path, 'r') as f:
                    config = json.load(f)
                asset_type = config.get('global', {}).get('asset_type', 'data')
            except (json.JSONDecodeError, IOError) as e:
                print(f"Warning: Could not read config at {config_path}: {e}")
                asset_type = 'data'

            rel_path = os.path.relpath(root, base_dir)
            asset_folders.append((root, rel_path, asset_type))
            processed_dirs.add(root)
            dirs.clear()
        elif files:
            # Folder has files but no config.json - treat as data
            rel_path = os.path.relpath(root, base_dir)
            asset_folders.append((root, rel_path, 'data'))
            processed_dirs.add(root)
            dirs.clear()

    return asset_folders

def clean_output_directory(out_dir):
    """Remove all files and directories in the output directory."""
    if os.path.exists(out_dir):
        print("Cleaning out old assets...")
        for root, dirs, files in os.walk(out_dir, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))
            for name in dirs:
                os.rmdir(os.path.join(root, name))

def build_zx02(tools_dir):
    """Build zx02 compressor if not already built."""
    zx02_dir = os.path.join(tools_dir, "zx02")
    zx02_exe = os.path.join(zx02_dir, "build", "zx02")

    if not os.path.exists(zx02_dir):
        print(f"Error: zx02 directory not found at {zx02_dir}")
        print("  Please run: git clone https://github.com/dmsc/zx02 tools/zx02")
        return None

    if not os.path.exists(zx02_exe):
        print("Building zx02 compressor...")
        old_dir = os.getcwd()
        os.chdir(zx02_dir)
        result = subprocess.run(["make"], capture_output=True, text=True)
        os.chdir(old_dir)
        if result.returncode != 0:
            print(f"Error building zx02: {result.stderr}")
            return None

    return zx02_exe

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    tools_dir = script_dir  # Script is now in tools/
    assets_dir = os.path.normpath(os.path.join(script_dir, "..", "src", "assets"))
    out_dir = os.path.normpath(os.path.join(script_dir, "..", "out", "assets"))

    if not os.path.exists(tools_dir):
        print(f"Error: Tools directory not found at {tools_dir}")
        return 1

    zx02_path = build_zx02(tools_dir)
    if not zx02_path:
        return 1

    os.chdir(tools_dir)
    print(f"Changed directory to: {os.getcwd()}")
    print("Transform assets into Lynx compatible format for cart packing.")

    clean_output_directory(out_dir)

    asset_folders = find_asset_folders(assets_dir)

    if not asset_folders:
        print("No asset folders found (folders with config.json)")
        return 0

    sprite_count = sum(1 for _, _, t in asset_folders if t == 'sprite')
    data_count = len(asset_folders) - sprite_count
    print(f"Found {len(asset_folders)} asset folder(s): {sprite_count} sprite, {data_count} data")

    success = True
    for src_path, rel_path, asset_type in asset_folders:
        dst_path = os.path.normpath(os.path.join(out_dir, rel_path))
        os.makedirs(dst_path, exist_ok=True)

        if asset_type == 'sprite':
            print(f"\nProcessing sprite folder: {rel_path}")
            cmd = f"python3 convert_folder_to_shared_palette.py {src_path} {dst_path}"
            if not run_command(cmd):
                success = False
                continue
            with open(os.path.join(dst_path, 'metadata.json'), 'r') as f:
                metadata = json.load(f)
            sprite_meta = {os.path.basename(k): v for k, v in metadata.items() if k.endswith('.spr')}
            for item in sorted(os.listdir(dst_path)):
                if item.endswith('.spr'):
                    spr_path = os.path.join(dst_path, item)
                    add_sprite_header(spr_path, sprite_meta[item])
                    zx02_compress_file(spr_path, spr_path, zx02_path)
        else:
            print(f"\nProcessing data folder: {rel_path}")
            process_directory(src_path, dst_path, compress=True, zx02_path=zx02_path)

    if success:
        print("\nAll asset processing completed successfully!")
    else:
        print("\nSome assets failed to process.")

    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
