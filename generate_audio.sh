#!/bin/bash
set -euo pipefail

OUTPUT="$1"
shift
rm -f "$OUTPUT"

for input in "$@"
do 
    stem=$(basename -s .wav "$input")
    echo "static const uint8_t audio_${stem}[] ROM = {"
    sox --norm --guard "$input" -t u8 -r 8820 - fade 0.05 0 0.05 | xxd -i
    echo -e "};\n"
done >> "$OUTPUT"
