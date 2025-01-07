#!/bin/sh

# Converts icons to hex arrays in c to embed them in the executable

OUTPUT="icon_data.h"

printf "/* This file contains the image data for the various icons */\n" > $OUTPUT;

# meta will contain the offsets and sizes for the icons
meta=""

offset=0
# Write one big array containing all images to $OUTPUT
printf "const unsigned char icon_data[] = {\n" >> $OUTPUT
for icon in icons/*.png; do
    printf "Embedding icon $icon\n"
    printf "0x$(hexdump -e '16/1 "%02x " "\n"' $icon | xargs | sed -e 's/ /, 0x/g'),\n" >> $OUTPUT

    name=${icon#"icons/"}
    name=${name%.*}
    name=$(printf "$name\n" | sed -e 's/-/_/g')
    size=$(du -b $icon | cut -f1)
    meta="${meta}\n #define icon_offset_$name $offset\n #define icon_size_$name $size"
    offset=$((offset + size))
done
printf "};\n" >> $OUTPUT

# Put meta contents at the end (with line breaks)
printf "%b" "$meta" >> $OUTPUT
