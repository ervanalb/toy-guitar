#!/bin/bash
set -euo pipefail

OUTPUT="$1"
shift
rm -f "$OUTPUT"

for input in "$@"
do 
    stem=$(basename -s .wav "$input")
    ffmpeg -y -i "$input" -filter_complex "compand=0.01:0.01:-65/-65|-30/-10|0/-5" -c:a pcm_u8 -ar 8820 tmp.wav
    echo "static const uint8_t audio_${stem}[] ROM = {"
    sox --norm --guard "tmp.wav" -t u8 -r 8820 - fade 0.05 0 0.05 | xxd -i
    echo -e "};\n"
done >> "$OUTPUT"
