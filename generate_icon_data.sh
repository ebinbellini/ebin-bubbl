#!/bin/bash

# Converts icons to hex arrays in c to embed them in the executable

OUTPUT="icon_data.h"

echo "/* This file contains the image data for the various icons */" > $OUTPUT;

# meta will contain the offsets and sizes for the icons
meta=""

offset=0
# Write one big array containing all images to $OUTPUT
echo "const unsigned char icon_data[] = {" >> $OUTPUT
for icon in icons/*.png; do
    echo "Embedding icon $icon"
    echo "0x$(hexdump -e '16/1 "%02x " "\n"' $icon | xargs | sed -e 's/ /, 0x/g')," >> $OUTPUT

    name=${icon#"icons/"}
    name=${name%.*}
    name=$(echo $name | sed -e 's/-/_/g')
    size=$(du -b $icon | cut -f1)
    meta="${meta}\n #define icon_offset_$name $offset\n #define icon_size_$name $size"
    offset=$((offset + size))
done
echo "};" >> $OUTPUT

# Put meta contents at the end (with line breaks)
echo -e $meta >> $OUTPUT
