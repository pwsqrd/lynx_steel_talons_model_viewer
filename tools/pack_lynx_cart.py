#!/usr/bin/env python3
# File name: filler.py
# Author: Programmer Unit
# Date: 13/2/2023
# Description: Given a filename to a *.lnx file, this script will create a lynx cartridge packed
#              with the game data and all the files in the input folder and create a C header file with the file offsets into the cartridge.
#              These files can then be loaded with lseek() and read() from the cartridge.
# License: -
import sys
import json
import os
import subprocess
import math  
import shutil

#TODO: Make blocksize come from this: #__BLOCKSIZE__

# 246kb cart is 1024
# 512kb cart is 2048
# 1MB cart is 4096 with bankswitching
CONFIG_CART_BLOCKSIZE = 2048
CONFIG_SHOW_CART_SPACE_HISTOGRAM = False

# Header configuration - Two-stage loader
# Cart layout:
#   Bytes 0-51:      Encrypted micro_loader (52 bytes) - loads 256 bytes to $0300
#   Bytes 52-307:    Stage2 loader binary (256 bytes) - handles multipage loading
#   Bytes 308-309:   Code size (2 bytes, little-endian)
#   Bytes 310+:      Code (starts immediately after header, loaded to $0400)
HEADER_SIZE = 310  # micro_loader(52) + stage2(256) + size(2)
STAGE2_LOADER_PATH = "../out/stage2_loader.bin"

# micro_loader.encrypted.bin - sets IODIR=$13 which includes AUDIN output bit
# This is critical for real hardware operation
MICRO_LOADER_BYTES = bytes([
    0xff, 0x58, 0x6f, 0x55, 0x63, 0x21, 0xc9, 0x6d,
    0xc3, 0x33, 0x30, 0x36, 0xca, 0xb5, 0x55, 0x9e,
    0xb4, 0x42, 0xd1, 0xf9, 0x2f, 0x93, 0xb7, 0xe8,
    0xe8, 0x28, 0x74, 0x10, 0x78, 0xa6, 0x47, 0xa3,
    0x5c, 0xf6, 0x9a, 0x8d, 0xe1, 0x92, 0x17, 0x99,
    0x98, 0xa3, 0x16, 0xda, 0x90, 0x76, 0x17, 0xa8,
    0xad, 0xaa, 0xed, 0x28
])

# Globals
ASSET_METADATA_FILE = 'metadata.json'

# Overlays (--overlays <dir> <name,name,...>): each <dir>/<name>.bin is packed
# after every other asset, so asset positions never depend on overlay sizes.
OVERLAY_DIR = None
OVERLAY_NAMES = []
# Address of g_overlayTable in the resident (--overlay_table 0xADDR). When
# given, the overlay cart positions are patched into the code image.
OVERLAY_TABLE_ADDR = None
CODE_LOAD_ADDR = 0x400
OVERLAY_TABLE_PLACEHOLDER = bytes([0xFF, 0xFF, 0xFF])
# Asset source folders (--asset_src <dir>), read for their config.json.
ASSET_SOURCE_DIR = "assets"

# Generated headers go to --gen_dir <dir>, which tools/build.py puts in the build
# directory and adds to the include path.
GEN_DIR = "gen"
OVERLAY_IDS_HEADER = "overlay_ids.h"

# Asset folders whose config.json declares "overlay": "<name>" are emitted into
# cart_files_<name>.h instead of the common cart_files.h.
COMMON_HEADER = "cart_files.h"
FOLDER_OVERLAY_MAP = {}

print(" - starting filler.py:")

#
# Run command and return stdout and stderr
#
def run_command(command):
    result = subprocess.run(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return result.stdout, result.stderr

#
# Creates the C struct arrays for the sprite assets
#
def create_arrays_for_sprite_assets(file_data):
    CART_BLOCK_SIZE = 2048

    folder_sprites = {}
    folder_counts = {}
    folder_declarations = ""

    hash_defines = ""

    for file_name, meta_data in file_data.items():
        if file_name == '__code__':
            continue

        if 'asset_type' not in meta_data or meta_data['asset_type'] != 'sprite':
            continue

        full_path = os.path.dirname(file_name)

        if full_path == "":
            folder_path = "root"
        else:
            folder_parts = full_path.split(os.path.sep)
            folder_path = folder_parts[-1] if len(folder_parts) > 0 else "root"

        if folder_path not in folder_sprites:
            folder_sprites[folder_path] = []
            folder_counts[folder_path] = 0

        c_folder_name = folder_path.replace("/", "_").replace("\\", "_").replace(".", "_")
        c_file_name = os.path.basename(file_name).replace("/", "_").replace("\\", "_").replace(".", "_")

        offset = meta_data['offset']
        block = offset // CART_BLOCK_SIZE
        pos = offset % CART_BLOCK_SIZE

        sprite_data = {
            'block': block,
            'pos': pos,
        }
        folder_sprites[folder_path].append(sprite_data)

        hash_defines += "#define {}_{}_IDX {}\n".format(c_folder_name.upper(), c_file_name.upper(), folder_counts[folder_path])
        folder_counts[folder_path] += 1

    for folder_path, count in folder_counts.items():
        if count == 0:
            continue

        c_folder_name = folder_path.replace("/", "_").replace("\\", "_").replace(".", "_").upper()

        struct_entries = []
        for sprite in folder_sprites[folder_path]:
            entry = "{{{}, {}}}".format(sprite['block'], sprite['pos'])
            struct_entries.append(entry)

        folder_declarations += "inline constexpr CartAsset {}_SPRITES[] = {{\n    {}\n}};\n".format(
            c_folder_name, ",\n    ".join(struct_entries))

        hash_defines += "#define {}_SPRITE_COUNT {}\n".format(c_folder_name, count)

    return hash_defines, folder_declarations

#
# Create CartAsset structs for non-sprite assets
#
def create_arrays_for_non_sprite_assets(file_data):
    CART_BLOCK_SIZE = 2048
    non_sprite_declarations = ""

    for file_name, meta_data in file_data.items():
        if file_name == '__code__':
            continue
        if 'asset_type' in meta_data and meta_data['asset_type'] == 'sprite':
            continue

        offset = meta_data['offset']
        block = offset // CART_BLOCK_SIZE
        pos = offset % CART_BLOCK_SIZE
        size = meta_data['length']

        c_file_name = os.path.basename(file_name).replace("/", "_").replace("\\", "_").replace(".", "_").upper()

        non_sprite_declarations += "inline constexpr CartAsset {} = {{{}, {}}};\n".format(
            c_file_name, block, pos)
        non_sprite_declarations += "#define {}_SIZE {}\n".format(c_file_name, size)

    return non_sprite_declarations

#
# Makes a C++ header file with defines for the offsets and lengths of the files
#
def create_header_file(file_data, header_file_path, guard="CART_FILES_H"):
    print("Creating header file: {}".format(header_file_path))

    hash_defines, declarations = create_arrays_for_sprite_assets(file_data)
    non_sprite_declarations = create_arrays_for_non_sprite_assets(file_data)

    with open(os.path.join(GEN_DIR, header_file_path), "w") as f:
        f.write("#ifndef {}\n".format(guard))
        f.write("#define {}\n\n".format(guard))
        f.write("#include \"sys/cart_types.h\"\n\n")
        f.write(hash_defines)
        f.write("\n")
        f.write(declarations)
        f.write("\n")
        f.write(non_sprite_declarations)
        f.write("\n#endif\n")

#
# Map each source asset folder that declares an overlay to that overlay name.
# The processed assets live in <out>/<folder>, the configs in <src>/<folder>.
#
def build_folder_overlay_map(source_root):
    global FOLDER_OVERLAY_MAP
    FOLDER_OVERLAY_MAP = {}
    for root, dirs, files in os.walk(source_root):
        rel_root = os.path.relpath(root, source_root)
        if 'config.json' not in files:
            continue
        with open(os.path.join(root, 'config.json'), 'r') as f:
            config = json.load(f)
        overlay = config.get('global', {}).get('overlay')
        if overlay:
            FOLDER_OVERLAY_MAP[rel_root] = overlay

def overlay_for_asset(asset_filename, asset_output_folder):
    rel = os.path.relpath(asset_filename, asset_output_folder)
    best = None
    for folder, overlay in FOLDER_OVERLAY_MAP.items():
        if rel.startswith(folder + os.sep) and (best is None or len(folder) > len(best[0])):
            best = (folder, overlay)
    return best[1] if best else None

#
# Write cart_files.h plus one cart_files_<overlay>.h per overlay that
# owns assets. Overlay binaries themselves are not listed: the resident finds
# them through g_overlayTable.
#
def create_header_files(file_data, asset_output_folder):
    os.makedirs(GEN_DIR, exist_ok=True)
    for name in os.listdir(GEN_DIR):
        if name.startswith("cart_files_") and name.endswith(".h"):
            os.remove(os.path.join(GEN_DIR, name))

    groups = {None: {}}
    for filename, metadata in file_data.items():
        if filename == '__code__' or metadata.get('cart_pack_type') == 'overlay':
            continue
        overlay = overlay_for_asset(filename, asset_output_folder)
        groups.setdefault(overlay, {})[filename] = metadata

    for overlay, group in groups.items():
        if overlay is None:
            create_header_file(group, COMMON_HEADER)
        else:
            c_name = overlay.upper().replace("-", "_")
            create_header_file(group, "cart_files_{}.h".format(overlay), "CART_FILES_{}_H".format(c_name))

def create_overlay_ids_header():
    print("Creating header file: {}".format(OVERLAY_IDS_HEADER))
    c_names = [name.upper().replace("-", "_") for name in OVERLAY_NAMES]
    with open(os.path.join(GEN_DIR, OVERLAY_IDS_HEADER), "w") as f:
        f.write("#ifndef OVERLAY_IDS_H\n")
        f.write("#define OVERLAY_IDS_H\n\n")
        f.write("#include \"sys/types.h\"\n\n")
        f.write("#define OVERLAY_LIST(X) {}\n\n".format(" ".join("X({})".format(n) for n in c_names)))
        f.write("enum : u8 {\n")
        for n in c_names:
            f.write("    OVERLAY_{},\n".format(n))
        f.write("    OVERLAY_COUNT\n")
        f.write("};\n\n#endif\n")

#
# Patch each overlay's {block, pos} into g_overlayTable inside the code image.
#
def patch_overlay_table(file_data):
    code = bytearray(file_data['__code__']['data'])
    base = HEADER_SIZE + OVERLAY_TABLE_ADDR - CODE_LOAD_ADDR
    print("Patching g_overlayTable at ${:04X}:".format(OVERLAY_TABLE_ADDR))
    for i, name in enumerate(OVERLAY_NAMES):
        at = base + i * 3
        if bytes(code[at:at + 3]) != OVERLAY_TABLE_PLACEHOLDER:
            print("ERROR: g_overlayTable[{}] at ${:04X} is not the placeholder, wrong table address?".format(i, OVERLAY_TABLE_ADDR + i * 3))
            sys.exit(1)
        offset = file_data[os.path.join(OVERLAY_DIR, name + ".bin")]['offset']
        block = offset // CONFIG_CART_BLOCKSIZE
        pos = offset % CONFIG_CART_BLOCKSIZE
        code[at:at + 3] = bytes([block, pos & 0xFF, pos >> 8])
        print("  {:<12} block {:3} pos {:4}".format(name, block, pos))
    file_data['__code__']['data'] = bytes(code)

#
# Recursively scan the folder for all metadata files and combine them.
#
def scan_asset_folder_for_metadata(folder):
    global ASSET_METADATA_FILE
    metadata = {}
    try:
        for root, dirs, files in os.walk(folder):
            for filename in files:
                    if filename == ASSET_METADATA_FILE:
                        file_path = os.path.join(root, filename)

                        with open(file_path, 'r') as f:
                            metadata.update(json.load(f))
    except Exception as e:
        print(f"Error: {e}")
    return metadata

#
# Recursively scan the folder for all asset files and add them to the file_data dictionary
#
def scan_asset_folder_for_files(folder, file_data):
    global ASSET_METADATA_FILE
    try:
        for root, dirs, files in os.walk(folder):
            dirs.sort()
            files.sort()
            for filename in files:
                # Skip the metadata file
                if filename == ASSET_METADATA_FILE:
                    continue
                    
                # Skip hidden files (files that start with a dot)
                if filename.startswith('.'):
                    continue

                file_path = os.path.join(root, filename)
                data = None
                with open(file_path, 'rb') as f:
                    data = f.read()

                if data is not None:
                    file_data[file_path] = {'data': data, 'offset': -1, 'length': len(data)}
    except Exception as e:
        print(f"Error: {e}")
    return file_data


#
# Add file to cart aligned to CONFIG_CART_BLOCKSIZE boundary 
# This means fast loading of the file, but it will waste space
#
def add_file_to_cart_aligned_to_boundary(metadata, buckets, filename):

    data = metadata['data']
    length = metadata['length']
    requiredBuckets = math.ceil(length / CONFIG_CART_BLOCKSIZE)  # Use math.ceil for proper ceiling division

    if requiredBuckets == 1:
        found = False

        # Try finding an empty bucket first (CONFIG_CART_BLOCKSIZE bytes free)
        for i, size in enumerate(buckets):
            if size == CONFIG_CART_BLOCKSIZE:
                metadata['offset'] = i * CONFIG_CART_BLOCKSIZE
                buckets[i] -= length
                found = True
                break

        # No empty bucket found, try finding a bucket with enough space
        if found is False:
            best_bucket = None
            min_remaining_space = float('inf')
            for i, size in enumerate(buckets):
                if length <= size and size - length < min_remaining_space:
                    best_bucket = i
                    min_remaining_space = size - length
            
            # If a suitable bucket was found, allocate the file to it
            if best_bucket is not None:
                metadata['offset'] = best_bucket * CONFIG_CART_BLOCKSIZE
                buckets[best_bucket] -= length
                found = True
    else:
        # we require more than one bucket, find requiredBuckets contiguous buckets
        found = False
        lastBucket = len(buckets) - requiredBuckets
        for i, size in enumerate(buckets):
            if i < lastBucket:
                if size == CONFIG_CART_BLOCKSIZE:
                    useThisBucket = True
                    for j in range(i+1, i+requiredBuckets):
                        if buckets[j] != CONFIG_CART_BLOCKSIZE:
                            useThisBucket = False
                            break

                    if useThisBucket:
                        metadata['offset'] = i * CONFIG_CART_BLOCKSIZE
                        for j in range(i, i+requiredBuckets):
                            buckets[j] -= length
                            length -= CONFIG_CART_BLOCKSIZE
                        found = True
                        break

    return found, buckets, metadata

#
# Data is packed tightly into the cart, but loading will be slow
# TODO: This function isn't working correctly, unaligned sprites aren't loading correctly
#
def add_file_to_cart_packed(metadata, buckets, filename):
    found = False
    length = metadata['length']
    requiredBuckets = math.ceil(length / CONFIG_CART_BLOCKSIZE)  # Use math.ceil for proper ceiling division

    # Iterate through buckets to find a suitable starting point
    for i in range(len(buckets)):
        # Check if we have enough contiguous buckets from this point
        if i + requiredBuckets > len(buckets):
            break

        # Check if first bucket has any space
        if buckets[i] > 0:
            firstBucketOffset = ((i+1)*CONFIG_CART_BLOCKSIZE) - buckets[i]
            firstBucketAvailable = buckets[i]
            
            # Calculate how much space we need in subsequent buckets
            remainingLength = length - min(length, firstBucketAvailable)
            subsequentBucketsNeeded = (remainingLength + CONFIG_CART_BLOCKSIZE - 1) // CONFIG_CART_BLOCKSIZE
            
            # Check if we have enough subsequent buckets that are completely empty
            enoughSpace = True
            for j in range(i+1, i+1+subsequentBucketsNeeded):
                if j >= len(buckets) or buckets[j] != CONFIG_CART_BLOCKSIZE:
                    enoughSpace = False
                    break
            
            if enoughSpace:
                # We found a suitable location, now allocate the space
                metadata['offset'] = firstBucketOffset
                
                # Allocate space in first bucket
                bytesToAllocateInFirst = min(length, firstBucketAvailable)
                buckets[i] -= bytesToAllocateInFirst
                remainingLength = length - bytesToAllocateInFirst
                
                # Allocate space in subsequent buckets
                j = i + 1
                while remainingLength > 0:
                    bytesToAllocate = min(remainingLength, CONFIG_CART_BLOCKSIZE)
                    buckets[j] -= bytesToAllocate
                    remainingLength -= bytesToAllocate
                    j += 1
                
                found = True
                break

    return found, buckets, metadata

#
# Analyze files and mark those larger than CONFIG_CART_BLOCKSIZE
#
def analyze_large_files(file_data):
    global CONFIG_CART_BLOCKSIZE
    total_files = len(file_data)
    large_files_count = 0
    
    print("\nAnalyzing file sizes:")
    for filename, metadata in file_data.items():
        if metadata['length'] > CONFIG_CART_BLOCKSIZE:
            metadata['exceeds_blocksize'] = True
            large_files_count += 1
            print(f"  - Large file: {filename} ({metadata['length']} bytes, {metadata['length']/CONFIG_CART_BLOCKSIZE:.2f} blocks)")
        else:
            metadata['exceeds_blocksize'] = False
            
    print(f"\nFile size summary:")
    print(f"  Total files: {total_files}")
    print(f"  Files larger than blocksize ({CONFIG_CART_BLOCKSIZE} bytes): {large_files_count}")
    print(f"  Percentage of large files: {(large_files_count / total_files) * 100:.1f}%")
    
    return file_data

#
# Load files from the folder
#
def create_cart_header(code_size):
    """Create 310-byte cart header with two-stage loader.

    Cart layout:
      Bytes 0-51:    Encrypted micro_loader (52 bytes)
      Bytes 52-307:  Stage2 loader binary (256 bytes)
      Bytes 308-309: Code size (2 bytes, little-endian)
      Bytes 310+:    Code starts here
    """
    global STAGE2_LOADER_PATH
    global MICRO_LOADER_BYTES
    global HEADER_SIZE

    script_dir = os.path.dirname(os.path.abspath(__file__))
    stage2_path = os.path.join(script_dir, STAGE2_LOADER_PATH)

    if not os.path.exists(stage2_path):
        print(f"ERROR: Stage2 loader not found at {stage2_path}")
        print("Run tools/build.py, which builds the stage2 loader.")
        sys.exit(1)

    with open(stage2_path, 'rb') as f:
        stage2_loader = f.read()

    if len(stage2_loader) > 256:
        print(f"ERROR: Stage2 loader too large ({len(stage2_loader)} bytes, max 256)")
        sys.exit(1)

    # Pad stage2 loader to exactly 256 bytes
    if len(stage2_loader) < 256:
        stage2_loader = stage2_loader + bytes(256 - len(stage2_loader))

    if code_size > 65535:
        print(f"ERROR: Code too large ({code_size} bytes, max 65535)")
        sys.exit(1)

    print(f"Code size: {code_size} bytes")
    print(f"Using proven micro_loader (52 bytes)")
    print(f"Stage2 loader: {len(stage2_loader)} bytes")

    # Build 310-byte header:
    # - Bytes 0-51: Encrypted micro_loader (proven working)
    # - Bytes 52-307: Stage2 loader (256 bytes)
    # - Bytes 308-309: Code size (little-endian)
    header = bytearray(HEADER_SIZE)
    header[0:52] = MICRO_LOADER_BYTES
    header[52:308] = stage2_loader
    header[308] = code_size & 0xFF         # size_lo
    header[309] = (code_size >> 8) & 0xFF  # size_hi

    return bytes(header)

def load_files(folder, gameData):
    global CONFIG_CART_BLOCKSIZE
    global HEADER_SIZE

    file_data = {}
    buckets = [CONFIG_CART_BLOCKSIZE] * 256

    # gameData is raw code - prepend the 256-byte header
    # Create header with encrypted loader + page count + padding
    header = create_cart_header(len(gameData))

    # Combine header + code
    full_cart_data = header + gameData
    file_data['__code__'] = {'data': full_cart_data, 'offset': -1, 'length': len(full_cart_data)}

    # Add all game assets
    file_data = scan_asset_folder_for_files(folder, file_data)
    meta_data = scan_asset_folder_for_metadata(folder)

    # Overlays go last, in OVERLAYS order (see OVERLAY_DIR)
    for name in OVERLAY_NAMES:
        path = os.path.join(OVERLAY_DIR, name + ".bin")
        if not os.path.exists(path):
            print("ERROR: overlay binary not found: {}".format(path))
            sys.exit(1)
        with open(path, 'rb') as f:
            data = f.read()
        file_data[path] = {'data': data, 'offset': -1, 'length': len(data), 'cart_pack_type': 'overlay'}

    # add meta_data to file_data
    for metadata_filename, mdata in meta_data.items():
        for asset_filename, fdata in file_data.items():
            if metadata_filename in asset_filename:
                #fdata.update(mdata)
                file_data[asset_filename].update(mdata)
                break

    # Mark files that exceed CONFIG_CART_BLOCKSIZE
    file_data = analyze_large_files(file_data)

    # Calculate total data size
    total_data_size = sum(metadata['length'] for metadata in file_data.values())
    total_files = len(file_data)
    remaining_data_size = total_data_size
    packed_data_size = 0
    packed_files = 0
    
    print(f"\nPacking process started - Total data size: {total_data_size} bytes ({total_data_size/1024:.2f} KB)")
    print(f"Total files to pack: {total_files}")
    print("=" * 50)
    
    # pack unaligned files first 
    print("\nPacking UNALIGNED files:")
    unaligned_count = 0
    unaligned_bytes = 0
    
    for filename, metadata in file_data.items():
        if 'cart_pack_type' not in metadata or metadata['cart_pack_type'] == 'unaligned':
            print(f"  - Processing: {filename} ({metadata['length']} bytes, {metadata['length']/1024:.2f} KB)")
            original_offset = metadata['offset']
            added, buckets, metadata = add_file_to_cart_packed(metadata, buckets, filename)
            
            if added:
                unaligned_count += 1
                unaligned_bytes += metadata['length']
                packed_data_size += metadata['length']
                packed_files += 1
                remaining_data_size -= metadata['length']
                progress_percent = (packed_data_size / total_data_size) * 100
                files_percent = (packed_files / total_files) * 100
            else:
                print("ERROR: Not enough space to pack files.")
                print(f"    ✗ Failed to add: {filename} ({metadata['length']} bytes, {metadata['length']/1024:.2f} KB)")
                exit(0)
    
    print(f"\nSuccessfully packed {unaligned_count} unaligned files ({unaligned_bytes} bytes, {unaligned_bytes/1024:.2f} KB)")
    print(f"Overall progress: {packed_data_size} bytes ({packed_data_size/1024:.2f} KB) - {(packed_data_size / total_data_size) * 100:.1f}% of total data")
    print(f"Files packed: {packed_files}/{total_files} ({(packed_files / total_files) * 100:.1f}%)")
    
    # pack aligned files
    print("\nPacking ALIGNED files:")
    aligned_count = 0
    aligned_bytes = 0
    
    for filename, metadata in file_data.items():
        if 'cart_pack_type' in metadata and metadata['cart_pack_type'] == 'aligned':
            print(f"  - Processing: {filename} ({metadata['length']} bytes, {metadata['length']/1024:.2f} KB)")
            original_offset = metadata['offset']
            added, buckets, metadata = add_file_to_cart_aligned_to_boundary(metadata, buckets, filename)
            
            if added:
                aligned_count += 1
                aligned_bytes += metadata['length']
                packed_data_size += metadata['length']
                packed_files += 1
                remaining_data_size -= metadata['length']
                progress_percent = (packed_data_size / total_data_size) * 100
                files_percent = (packed_files / total_files) * 100
                
                print(f"    ✓ Added at offset: {metadata['offset']} ({metadata['length']} bytes, {metadata['length']/1024:.2f} KB)")
                print(f"    ✓ Progress: {packed_data_size} bytes ({packed_data_size/1024:.2f} KB) - {progress_percent:.1f}% of total data")
                print(f"    ✓ Files packed: {packed_files}/{total_files} ({files_percent:.1f}%)")
                print(f"    ✓ Remaining: {remaining_data_size} bytes ({remaining_data_size/1024:.2f} KB)")
            else:
                print("ERROR: Not enough space to pack files.")
                print(f"    ✗ Failed to add: {filename} ({metadata['length']} bytes, {metadata['length']/1024:.2f} KB)")
                exit(0)
    
    print(f"\nSuccessfully packed {aligned_count} aligned files ({aligned_bytes} bytes, {aligned_bytes/1024:.2f} KB)")

    # pack overlays last so no asset position depends on an overlay's size
    for filename, metadata in file_data.items():
        if metadata.get('cart_pack_type') != 'overlay':
            continue
        added, buckets, metadata = add_file_to_cart_packed(metadata, buckets, filename)
        if not added:
            print("ERROR: Not enough space to pack overlay {} ({} bytes)".format(filename, metadata['length']))
            sys.exit(1)
        packed_data_size += metadata['length']
        packed_files += 1
        print(f"  - Overlay: {filename} ({metadata['length']} bytes) at offset {metadata['offset']}")
    print(f"Total packed: {unaligned_count + aligned_count} files ({packed_data_size} bytes, {packed_data_size/1024:.2f} KB)")
    print(f"Overall completion: {packed_files}/{total_files} files ({(packed_files / total_files) * 100:.1f}%)")
    print(f"Data packed: {packed_data_size}/{total_data_size} bytes ({(packed_data_size / total_data_size) * 100:.1f}%)")
    print("=" * 50)
    
    # Return the data and metadata for all the files
    return file_data

# Get the total number of bytes used by all of the files
def get_bytes_used(file_data):
    totalBytes = 0
    for metadata in file_data.values():
        totalBytes += metadata['length']
    return totalBytes

# Visualize cart space in terminal
def visualize_file_data(file_data):
    global CONFIG_CART_BLOCKSIZE

    # Calculate bucket usage
    bucket_sizes = [CONFIG_CART_BLOCKSIZE] * 256
    totalFreeCartSpaceBytes = (256 * CONFIG_CART_BLOCKSIZE) - get_bytes_used(file_data)
    for metadata in file_data.values():
        offset = metadata['offset']
        length = metadata['length']
        if offset != -1:
            bucket = offset // CONFIG_CART_BLOCKSIZE
            bucket_sizes[bucket] -= length

    # Print file details
    print("\nFile Details:")
    print("-" * 100)
    print(f"{'Filename':<40} {'Offset':<10} {'Start Bucket':<15} {'End Bucket':<15} {'Length (bytes)':<15}")
    print("-" * 100)
    
    # Sort files by offset for better visualization
    sorted_files = sorted([(filename, metadata) for filename, metadata in file_data.items() 
                           if metadata['offset'] != -1], key=lambda x: x[1]['offset'])
    
    for filename, metadata in sorted_files:
        offset = metadata['offset']
        length = metadata['length']
        start_bucket = offset // CONFIG_CART_BLOCKSIZE
        end_bucket = (offset + length - 1) // CONFIG_CART_BLOCKSIZE
        
        # Truncate filename if it's too long
        display_filename = filename
        if len(display_filename) > 38:
            display_filename = "..." + display_filename[-35:]
            
        print(f"{display_filename:<40} {offset:<10} {start_bucket:<15} {end_bucket:<15} {length:<15}")

    print(f"Total cart free space (bytes): {totalFreeCartSpaceBytes} bytes")
    print(f"Total cart free space (kb): {totalFreeCartSpaceBytes/1024} kb")

#
# Makes the bytearray for the final cart
#
def createCart(files, cartSize):
    cartridge = bytearray(cartSize)
    for i in range(len(cartridge)):
        cartridge[i] = 0

    for f, metadata in enumerate(files.values()):
        offset = metadata['offset']
        length = metadata['length']
        data = metadata['data']

        if len(data) != length:
            print("Error: File '{}' has length {} but data has length {}".format(metadata['filename'], length, len(data)))
            sys.exit(1)

        if offset != -1:
            data = metadata['data']
            for d in range(length):
                cartridge[offset + d] = data[d]

    return cartridge

# 
# Main function
# fname is a *.lnx file
#
def create_lynx_cartridge(fname, input_file_directory_name):
    global CONFIG_CART_BLOCKSIZE
    global CONFIG_SHOW_CART_SPACE_HISTOGRAM  

    blocksize = CONFIG_CART_BLOCKSIZE
    cartSize = blocksize * 256

    # Check if .lnx file exists, if not create a dummy one with 8KB of zeros
    lnx_file = fname
    fname = os.path.splitext(lnx_file)[0] 

    usingDummyLyx = False
    if not os.path.exists(lnx_file):
        print(f"No .lnx file found at {lnx_file}, creating a dummy 8KB file")
        usingDummyLyx = True
        with open(lnx_file, 'wb') as f:
            # Create an 8KB file of zeros with a 64-byte header
            dummy_header = bytearray(64)  # 64-byte header of zeros
            dummy_data = bytearray(8 * 1024 - 64)  # 8KB - header size
            f.write(dummy_header)
            f.write(dummy_data)
    
    with open(lnx_file, 'rb') as f:
        raw = f.read()
        data = raw

    # Remove the header
    gameData = data #data[64:]
    build_folder_overlay_map(ASSET_SOURCE_DIR)
    files = load_files(input_file_directory_name, gameData)
    if OVERLAY_TABLE_ADDR is not None:
        patch_overlay_table(files)

    # Print cart stats
    totalFreeCartSpaceBytes = cartSize - get_bytes_used(files)
    percentageUsed = round((get_bytes_used(files) / cartSize) * 100, 1)
    print("Total Cart Usage: {}% with {} bytes free.".format(percentageUsed, totalFreeCartSpaceBytes))

    # Show bucket usage if requested
    if CONFIG_SHOW_CART_SPACE_HISTOGRAM:
        visualize_file_data(files)

    # Create the cart bytes
    print("Creating cart with size: {} Kb".format(cartSize/1024))
    cart = createCart(files, cartSize)

    # Write out the cart then create a lynx cart from it
    with open(fname+'.lyx', 'wb') as f:
        f.write(cart)

    # Create the final lnx cart from the lyx
    #inname = fname +'.lyx'
    #outname = fname + "_packaged" +'.lnx'
    #makelnx_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "makelnx", "makelnx")
    #stdout, stderr = run_command(f"{makelnx_path} {inname} -o {outname}")
    #print(stdout)
    
    # Create header files in the generated header directory
    create_header_files(files, input_file_directory_name)
    if OVERLAY_DIR is not None:
        create_overlay_ids_header()

#
# Main
#
def main():
    global CONFIG_SHOW_CART_SPACE_HISTOGRAM  

    if len(sys.argv) < 2:
        print("Error: Not enough arguments. Please provide an asset folder path.")
        print("Usage: python pack_lynx_cart.py <asset folder path> [code_name] [--show_cart_layout]")
        print("         [--overlays <dir> <name,name,...>] [--overlay_table <hex addr>] [--gen_dir <dir>] [--asset_src <dir>]")
        print("e.g: python pack_lynx_cart.py assets game --show_cart_layout")
        return
        
    asset_folder = sys.argv[1]
    
    # Default code name is "game" if not specified
    filename = "game"
    if len(sys.argv) >= 3 and not sys.argv[2].startswith("--"):
        filename = sys.argv[2]
    
    # Check for show_cart_layout flag
    CONFIG_SHOW_CART_SPACE_HISTOGRAM = False
    for arg in sys.argv:
        if arg == "--show_cart_layout":
            CONFIG_SHOW_CART_SPACE_HISTOGRAM = True

    global OVERLAY_DIR, OVERLAY_NAMES, OVERLAY_TABLE_ADDR, GEN_DIR, ASSET_SOURCE_DIR
    if "--gen_dir" in sys.argv:
        GEN_DIR = sys.argv[sys.argv.index("--gen_dir") + 1]
    if "--asset_src" in sys.argv:
        ASSET_SOURCE_DIR = sys.argv[sys.argv.index("--asset_src") + 1]
    if "--overlays" in sys.argv:
        i = sys.argv.index("--overlays")
        OVERLAY_DIR = sys.argv[i + 1]
        names = sys.argv[i + 2] if i + 2 < len(sys.argv) and not sys.argv[i + 2].startswith("--") else ""
        OVERLAY_NAMES = [n for n in names.split(",") if n]
    if "--overlay_table" in sys.argv:
        OVERLAY_TABLE_ADDR = int(sys.argv[sys.argv.index("--overlay_table") + 1], 16)

    print("Input code file:", filename)
    print("Input asset folder:", asset_folder)
    create_lynx_cartridge(filename, asset_folder)

if __name__ == "__main__":
    main()

print(" - filler.py Done!")