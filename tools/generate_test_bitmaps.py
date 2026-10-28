from PIL import Image
import os

# Define the size of the largest image
MAX_SIZE = 7

# Define the colors to use
COLORS = [
    (255, 0, 0),    # Red
    (0, 255, 0),    # Green
    (0, 0, 255),    # Blue
    (255, 255, 0),  # Yellow
    (255, 0, 255),  # Magenta
    (0, 255, 255),  # Cyan
    (128, 0, 0),    # Maroon
    (0, 128, 0),    # Dark Green
    (0, 0, 128),    # Navy
    (128, 128, 0),  # Olive
    (128, 0, 128),  # Purple
    (0, 128, 128),  # Teal
    (255, 128, 0),  # Orange
    (128, 255, 0),  # Lime
    (0, 128, 255)   # Sky Blue/home/muncher/Documents/development/git/lynx_sprite_optimisation_test/tools/convert_folder_to_shared_palette.py
]

imageSizeList = [ 8, 16, 32, 64, 128 ]
outputFolder = "test_bitmaps/"
# Loop through each size

for imageSize in imageSizeList:
    index = 0
    for size in range(0, MAX_SIZE+1):
        # Calculate the size of the bitmap/home/muncher/Documents/development/git/lynx_sprite_optimisation_test/tools/convert_folder_to_shared_palette.py

        bitmap_size = imageSize  #2**size
        
        for j in range(0, 1):
            # Create a new indexed mode image with the given size
            image = Image.new("P", (bitmap_size, bitmap_size))

            # Generate a random color 
            # Generate a unique color for this image
            color = COLORS[index % len(COLORS)]
            index = index + 1
            
            # Set the color palette for the image
            palette = []
            for i in range(256):
                palette.extend(color)
            image.putpalette(palette)
            
            # Save the image
            filename = f"bitmap_{bitmap_size}_{size}.bmp"
            path = outputFolder + str(imageSize)  + "/"

            # Create the folder if it doesn't exist
            if not os.path.exists(path):
                os.makedirs(path)

            image.save(path+filename)

