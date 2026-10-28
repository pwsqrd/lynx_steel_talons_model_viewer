import os
import subprocess
from PIL import Image
import json
import shutil

COLOR_MAGENTA = (255, 0, 255)
OUTPUT_METADATA_FILENAME = "metadata.json"

default_config = {
        'asset_type': "sprite",
        'cart_pack_type': "unaligned",      # or aligned to page boundaries
        'scale_x' : 1.0,
        'scale_y' : 1.0,
        'size_x'  : 0,
        'size_y'  : 0,
        'scale' : 1.0,
        'bpp': 4,
        'packed': True,
        'sprite_type': 'normal',
        'action_point': (0, 0),
        'process': 'none'
    }

def move_magenta_to_first_palette_entry(im):
    colors = im.palette.colors #get colors from palette
    if(COLOR_MAGENTA not in colors): #check there is pink (could be possible that there is not)
        print("Magenta not found\n")
        return im

    print("Magenta \n")

    first = list(colors.keys())[list(colors.values()).index(0)] #colors is a dict so I get the key of the first index
    pink_index = colors[COLOR_MAGENTA]
    colors[first] = colors[COLOR_MAGENTA] 
    colors[COLOR_MAGENTA] = 0 #change the value of the colors
    newcolors = {}
    newcolors[COLOR_MAGENTA] = colors.pop(COLOR_MAGENTA)
    for key in colors:
        newcolors[key] = colors[key] #reorder the dict so it is in order
    colors = newcolors
    newcolors = []
    for key in colors: 
        for c in key:
            newcolors.append(c) #make it a list

    # Swap 0 and pink_index palette indexes
    for j in range(im.size[1]):
        for i in range(im.size[0]): 
            index = im.getpixel((i, j))  

            if index == pink_index:
                index = 0
            elif index == 0:
                index = pink_index

            im.putpixel((i, j), index)

    # Sway 0 and pink_index palettes
    pal = im.getpalette()
    temp = pal[0:3] 
    pal[0:3] = pal[pink_index*3:pink_index*3+3]
    pal[pink_index*3:pink_index*3+3] = temp
    im.putpalette(pal)

    return im

def get_action_point_string(x, y):
    return f'-a{x:03d}{y:03d}'  # 3-digit hex

def argb_to_hex(a, r, g, b):
    
    num = g << 8 | b << 4 | r
    val = hex(num)

    return val


def export_palette_to_c_array(palette, output_folder):
    print("Exporting palette to C array...")
    data = palette.tobytes()
    palette_colors = [(data[i], data[i+1], data[i+2]) for i in range(0, len(data), 3)]
    
    # Downsample each RGB value to 4 bits
    downsampled_palette = []
    for color in palette_colors:
        downsampled_color = tuple((c//16 for c in color))
        downsampled_palette.append(downsampled_color)

    # Get the parent folder name from the output path
    parent_folder = os.path.basename(os.path.normpath(output_folder))
    palette_filename = os.path.join(output_folder, f"{parent_folder}.pal")

    # MIKEY-native layout: 16 green nibbles followed by 16 blue+red bytes,
    # so sys_SetPalette can copy straight into MIKEY.palette[0..31].
    green_bytes = bytearray(16)
    br_bytes    = bytearray(16)
    for index, color in enumerate(downsampled_palette[:16]):
        r = color[0] & 0xF
        g = color[1] & 0xF
        b = color[2] & 0xF
        green_bytes[index] = g
        br_bytes[index]    = (b << 4) | r
    with open(palette_filename, 'wb') as f:
        f.write(green_bytes)
        f.write(br_bytes)

    # Export palette to file as C array
    numExported = 0
    c_array = "unsigned short palette[16] = {"
    for color in downsampled_palette:
        if (numExported<16):
            c_array += argb_to_hex(0,color[0],color[1],color[2]) 
            if (numExported <= 14):
                c_array += ", "

        numExported+=1
    c_array += "};"
    return c_array



def get_related_json_files(json_dir, root_dir):
    json_files = []
    root_dir = os.path.abspath(root_dir)

    # Traverse up the directory hierarchy from the directory of the given JSON file
    while True:
        json_files += [os.path.join(json_dir, f) for f in os.listdir(json_dir) if f.endswith('.json')]
        if os.path.abspath(json_dir) == root_dir:
            break

        json_dir = os.path.dirname(json_dir)

    return json_files


def get_config(json):
    global default_config
    config = default_config
    if 'scale' in json:
        config ['scale_x'] = json['scale']
        config ['scale_y'] = json['scale']
    if 'scale_x' in json:
        config ['scale_x'] = json['scale_x']
    if 'scale_y' in json:
        config ['scale_y'] = json['scale_y']
    if 'bpp' in json  :
        config ['bpp'] = json['bpp']
    if 'packed' in json    :
        config ['packed'] = json['packed']
    if 'sprite_type' in json:
        config ['sprite_type'] = json['sprite_type']
    if 'action_point' in json      :
        config ['action_point'] = json['action_point']
    if 'asset_type' in json:
        config ['asset_type'] = json['asset_type']
    if 'cart_pack_type' in json:
        config ['cart_pack_type'] = json['cart_pack_type']
    if 'size_x' in json:
        config ['size_x'] = json['size_x']
    if 'size_y' in json:
        config ['size_y'] = json['size_y']
    if 'process' in json:
        config ['process'] = json['process']

    return config

def update_config(config, new_config):
    r = config
    r.update(new_config)
    return r

def get_json_config_options(file, input_folder):
    global default_config
    config = default_config.copy()

    path = os.path.dirname(file)
    config_files = get_related_json_files(path, input_folder)

    # iterate over config files in reverse order so that the most specific config file is processed first
    for config_file in reversed(config_files):
        with open(config_file) as f:
            data = json.load(f)
            if 'global' in data:
                global_settings = data['global']
                global_config = get_config(global_settings)   
                config = update_config(config,global_config)
            if 'images' in data:
                images_settings = data['images']

                # Iterate through the images in  images_settings
                for image in images_settings:
                    if 'filename' in image:
                        if image['filename'] == os.path.basename(file):
                            image_config = get_config(image)   
                            config = update_config(config,image_config)
                            break

    return config

def remove_output_folder_from_keys(output_metadata, output_folder):
    new_output_metadata = {}
    parent_folder = os.path.basename(os.path.normpath(output_folder))  # Get the immediate parent folder name
    for key, value in output_metadata.items():
        if key.startswith(output_folder):
            new_key = key.replace(output_folder, '', 1)  # Remove the absolute output_folder path
            new_key = new_key.lstrip('/')  # Remove leading slash if present
            new_key = os.path.join(parent_folder, new_key)  # Prepend the immediate parent folder
        else:
            new_key = key
        new_output_metadata[new_key] = value
    return new_output_metadata


def write_output_metadata(output_metadata, output_folder, c_array):
    print("Writing output metadata...")

    output_metadata = remove_output_folder_from_keys(output_metadata, output_folder)

    output_metadata['c_palette'] = c_array
    with open(os.path.join(output_folder, OUTPUT_METADATA_FILENAME), 'w') as f:
        json.dump(output_metadata, f, indent=4)

"""
    Quantizes an RGB image to 16 colors, keeping magenta (255, 0, 255) separate at palette index 0.
    Prevents magenta from blending with other colors.
    
    Args:
        image: A PIL Image object in RGB mode.
    
    Returns:
        A PIL Image object in 'P' mode with a 16-color palette.
    """
def quantize_with_magenta(image):
    # Ensure image is in RGB mode
    image = image.convert("RGB")
    
    # Step 1: Create a mask for magenta pixels (255, 0, 255)
    pixels = image.getdata()
    mask_data = [255 if p == (255, 0, 255) else 0 for p in pixels]
    mask = Image.new("L", image.size)
    mask.putdata(mask_data)
    
    # Step 2: Replace magenta pixels with a neutral color (e.g., black) for quantization
    non_magenta_image = image.copy()
    non_magenta_image.paste((0, 0, 0), mask=mask)
    
    # Step 3: Quantize to 15 colors (excluding magenta) using LIBIMAGEQUANT
    quantized_img = non_magenta_image.quantize(
        colors=15,
        method=Image.LIBIMAGEQUANT,  # High-quality quantization
        kmeans=1,                    # Improves palette selection
        dither=Image.FLOYDSTEINBERG  # Matches GIMP's dithering
    )
    
    # Step 4: Extract the 15-color palette (45 RGB values)
    palette = quantized_img.getpalette()[:15 * 3]
    
    # Step 5: Create a 16-color palette with magenta at index 0
    new_palette = [255, 0, 255] + palette + [0] * (768 - 48)  # 768 is full palette size
    
    # Step 6: Shift quantized pixel indices up by 1 (0-14 becomes 1-15)
    shifted_img = quantized_img.point(lambda x: x + 1)
    
    # Step 7: Create final indexed image with the new palette
    final_img = Image.new("P", image.size)
    final_img.putpalette(new_palette)
    final_img.putdata(shifted_img.getdata())
    
    # Step 8: Restore magenta pixels to index 0
    magenta_img = Image.new("P", image.size)
    magenta_img.putpalette(new_palette)
    magenta_img.paste(0, mask=mask)
    
    # Combine quantized image with magenta pixels
    final_img.paste(magenta_img, (0, 0), mask)
    
    return final_img

def bmp_to_16_colors(input_folder, output_folder, global_config):

    output_metadata = {}

    # Create output folder if it doesn't exist
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)

    # Load all BMP and PCX files in the input folder
    # NOTE:
    # PCX must be in RGB mode
    print("Scanning Image files...")
    bmp_files = []
    try:
        for root, dirs, files in os.walk(input_folder):
            for file in files:
                if file.endswith('.bmp') or file.endswith('.pcx'):
                    bmp_files.append(os.path.join(root, file))
    except Exception as e:
        print(f"Error: {e}")


    print("Loading BMP files...")
    #images = [Image.open(f) for f in bmp_files]
    images = []
    for f in bmp_files:
        with Image.open(f) as img:
            images.append(img.copy())  # Copy the image to keep it in memory after closing

    # Check if images list is empty
    if not images:
        print("Error: No images loaded. Exiting.")
        return

    # Append all images into a single image
    print("Packing images into a single image...")
    widths, heights = zip(*(i.size for i in images))
    total_width = sum(widths)
    max_height = max(heights)
    appended_image = Image.new('RGB', (total_width, max_height))
    x_offset = 0
    for im in images:
        appended_image.paste(im, (x_offset, 0))
        x_offset += im.size[0]
    print("Packing finished: final image size is {}x{}".format(appended_image.size[0], appended_image.size[1]))

    # Convert the image to 16 colors
    print("Downsampling image to 16 colors...")
    #appended_image  = appended_image.quantize(kmeans=1, dither=Image.Dither.NONE, method=Image.Quantize.MEDIANCUT, colors=16)


    #appended_image.save("PACKED_FILE.bmp", "BMP") 
    # if file exists:
    if os.path.exists("PACKED_FILE_16_COLOR_USER.bmp"):
        appended_image = Image.open("PACKED_FILE_16_COLOR_USER.bmp")
        #appended_image.load("PACKED_FILE_16_COLOR_USER.bmp", "BMP")
    else:
        appended_image = quantize_with_magenta(appended_image)
    #appended_image.save("PACKED_FILE_DOWNSAMPLED.bmp", "BMP") 

    # Pen0 is transparent, so we move magenta to the first palette entry
    if global_config['magenta_is_transparent'] == True:
        print("Moving magenta to first palette entry...")
        appended_image = move_magenta_to_first_palette_entry(appended_image)

    if global_config['show_combined_image'] == True:
        print("Saving large image...")
        appended_image.save(os.path.join(output_folder, 'TOTAL_IMAGE.bmp'), 'BMP')

    # Separate the image back into individual files and save them in the output folder
    x = 0
    print("Cropping and converting images...")
    numImages = len(images)
    for i, im in enumerate(images):
        print(f"    image : {i+1}/{numImages}", end='\r')        
        separated_image = appended_image.crop((x, 0, x + im.size[0], im.size[1]))

        # Get the relative path from input folder and join with output folder
        rel_path = os.path.relpath(bmp_files[i], input_folder)
        imageName = os.path.join(output_folder, rel_path)

        # create folder for imageName if it doesnt exist
        if not os.path.exists(os.path.dirname(imageName)):
            os.makedirs(os.path.dirname(imageName))

        base, ext = os.path.splitext(bmp_files[i])

        config = get_json_config_options(bmp_files[i], input_folder)



        # width and height
        width, height = separated_image.size
        new_width = int(round(width * config['scale_x']))
        new_height = int(round(height * config['scale_y']))
        if new_width <= 0 or new_height <= 0:
            print("Error: Output image dimensions is zero!")
            print("imageName:", imageName)
            print("width:", width)
            print("height:", height)
            print("scale_x:", config['scale_x'])
            print("scale_y:", config['scale_y'])
            print("new_width:", new_width)
            print("new_height:", new_height)
            print("skipping image...")
            continue


        # bpp
        bpp = config['bpp']
        if bpp == 4:
            bpp = "-s4"
        elif bpp == 3:
            bpp = "-s3"
        elif bpp == 2:
            bpp = "-s2"
        elif bpp == 1:
            bpp = "-s1"
        else:
            print("Error: Invalid bpp value for image: " + imageName)
            print("bpp:", bpp)
            print("Skipping image...")
            continue
        
        # packed
        packed = config['packed']
        if packed == False:
            packed = "-u"
        else:
            packed = ""

        # type
        # type 0 = 4bit raw,  1 = 8bit raw, 2 = SPS, 3 = PCX (3 is default)
        # type 4 = 1bit raw type 5 = PI1 (Atari ST), 6 = MS Windows BMP 
        type = None
        ext = ext.lower()
        if (ext == ".bmp"):
            type = "-t6"
        elif (ext == ".pcx"):
            type = "-t3"
        else:
            print("Error: Invalid image type for image: " + imageName)
            print("BMP and PCX are the only supported image types.")
            print("Skipping ...")
            continue

        # action point
        action_point = config['action_point']
        actionPoint = get_action_point_string(action_point[0], action_point[1])

        # if scaled, resize the image.
        separated_image = separated_image.resize((new_width, new_height))

        # transpose
        #if config ['process'] == "split_into_columns":
        #    separated_image = separated_image .transpose(Image.ROTATE_90)
        #    new_width, new_height = separated_image.size

        config['size_x'] = new_width ;
        config['size_y'] = new_height;

        byte_offsets = [0]
        packed_sprites = bytearray()
        current_offset = 0

        # split the image into columns of one pixel width:
        # used for walls and sprites.
        if config ['process'] == "split_into_columns" or config ['process'] == "split_into_columns_with_y_offset":

            y_offset = 0

            # Look for an offsets.json file in the same directory as the image
            input_path = os.path.dirname(bmp_files[i])
            input_filename = os.path.basename(bmp_files[i])
            input_filename = os.path.splitext(input_filename)[0]
            offsets_file = os.path.join(input_path, "offsets.json")

            # Check if the offsets file exists
            if os.path.exists(offsets_file):
                with open(offsets_file, 'r') as f:
                    json_offsets = json.load(f)
                    y_offset = json_offsets[input_filename]

            # Chop up the rows into indvidual sprites
            for index in range(0, new_height):

                column = separated_image.crop((0, index, new_width, index + 1))
                columnNameImage = "temp" + os.path.splitext(imageName)[1]
                column.save(columnNameImage)
                spriteName = os.path.splitext(columnNameImage)[0] + ".spr"
                command = "../submodules/sprpck/src/sprpck " + actionPoint + " " + bpp + " " + packed + " " + type + " " + columnNameImage + " " + spriteName 
                subprocess.run(command.split(), stdout=subprocess.PIPE)

                if global_config['remove_palette_files'] == True:
                    paletteName = os.path.splitext(columnNameImage)[0] + ".pal"
                    if os.path.exists(paletteName):
                        os.remove(paletteName)
                if global_config['remove_downsampled_images'] == True:
                    if os.path.exists(columnNameImage):
                        os.remove(columnNameImage)
                if os.path.exists(spriteName):
                    with open(spriteName, "rb") as f:
                        sprite_data = f.read()
                        if index < (new_height - 1):
                            current_offset += len(sprite_data)
                            byte_offsets.append(current_offset)
                        packed_sprites.extend(sprite_data)

                    os.remove(spriteName)

            # write the packed sprites to a single file
            packed_sprite_name = os.path.splitext(imageName)[0] + ".scaler"
            with open(packed_sprite_name, "wb") as f:

                # scalers store their y offset as the first byte
                if config ['process'] == "split_into_columns_with_y_offset":
                    pixel_y_offset = bytes([y_offset & 0xFF]) 
                    f.write(pixel_y_offset)

                for val in byte_offsets:
                    f.write(val.to_bytes(2, byteorder='little')) 
                f.write(packed_sprites)

            config['size_x'] = new_width;
            config['size_y'] = new_height;
            output_metadata[packed_sprite_name] = config

        else:
            separated_image.save(imageName)
            config['size_x'] = new_width ;
            config['size_y'] = new_height;

            spriteName = os.path.splitext(imageName)[0] + ".spr"
            output_metadata[spriteName] = config

            command = "../submodules/sprpck/src/sprpck " + actionPoint + " " + bpp + " " + packed + " " + type + " " + imageName + " " + spriteName 
            print("Command is:\n")
            print(command)

            result = subprocess.run(command.split(), stdout=subprocess.PIPE)
            print(result.stdout.decode())
        
        # delete seperated image
        if global_config['remove_downsampled_images'] == True:
            if os.path.exists(imageName):
                os.remove(imageName)

        if global_config['remove_palette_files'] == True:
            paletteName = os.path.splitext(imageName)[0] + ".pal"
            if os.path.exists(paletteName):
                os.remove(paletteName)

        x += im.size[0]
    

    print("Exporting palette...")
    c_array = export_palette_to_c_array(appended_image.palette, output_folder)
    print(c_array)
    write_output_metadata(output_metadata, output_folder, c_array)


if __name__ == '__main__':
    import sys
    global_config = {
            'magenta_is_transparent': True,
            'remove_palette_files': True,
            'remove_downsampled_images': True,
            'show_combined_image' : False 
        }

    if len(sys.argv) < 3:
        print('Usage: python bmp_converter.py <input_folder> <output_folder>')

        # set cwd to this folder
        script_dir = os.path.dirname(os.path.abspath(__file__))
        os.chdir(script_dir)
        print(f"Changed directory to: {os.getcwd()}")
        #"python3 convert_folder_to_shared_palette.py ../lynx_assets/gfx/game out/gfx/game"
        input_folder = "../lynx_assets/gfx/game"
        output_folder = "out/gfx/game"
        bmp_to_16_colors(input_folder, output_folder,global_config )
        print("Done!")
    else:
        input_folder = sys.argv[1]
        output_folder = sys.argv[2]

        
        if (len(sys.argv) > 3):
            for i in range(3, len(sys.argv)):
                if (sys.argv[i] == "-transparent"):
                    global_config['magenta_is_transparent'] = True
                if (sys.argv[i] == "-rp"):
                    global_config['remove_palette_files'] = False
                if (sys.argv[i] == "-ri"):
                    global_config['remove_downsampled_images'] = False


        bmp_to_16_colors(input_folder, output_folder, global_config )
        print("Done!")

