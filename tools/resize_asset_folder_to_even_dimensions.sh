#!/bin/bash

# Directory to process (current directory by default)
DIR="."

# Find all .pcx files recursively and process them
find "$DIR" -type f -iname "*.pcx" | while read -r file; do
    echo "Processing: $file"

    # Get current dimensions
    dimensions=$(magick identify -format "%w %h" "$file" 2>/dev/null)
    if [ $? -ne 0 ]; then
        echo "Error: Failed to identify $file. Skipping."
        continue
    fi

    read width height <<< "$dimensions"

    # Calculate next even dimensions
    new_width=$(( (width + 1) / 2 * 2 ))  # Round up to next even
    new_height=$(( (height + 1) / 2 * 2 )) # Round up to next even

    # Only resize if dimensions need changing
    if [ "$width" -ne "$new_width" ] || [ "$height" -ne "$new_height" ]; then
        echo "Resizing $file from ${width}x${height} to ${new_width}x${new_height}"
        magick "$file" -filter point -resize "${new_width}x${new_height}!" "$file" 2>/dev/null
        if [ $? -ne 0 ]; then
            echo "Error: Failed to resize $file."
        else
            echo "Successfully resized $file."
        fi
    else
        echo "No resize needed for $file (already ${width}x${height})."
    fi
done
