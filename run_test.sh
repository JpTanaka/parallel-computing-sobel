#!/bin/bash

make

INPUT_DIR=images/original
OUTPUT_DIR=images/processed
mkdir $OUTPUT_DIR 2>/dev/null
input_file="perf.log"


if [ -f "$input_file" ]; then
    rm "$input_file"
    touch "$input_file"    
fi 

for i in $INPUT_DIR/*gif ; do
    DEST=$OUTPUT_DIR/`basename $i .gif`-sobel.gif
    echo "Running test on $i -> $DEST"
    salloc -N 1 -n 4 mpirun ./sobelf $i $DEST
done


if [ ! -f "$input_file" ]; then
    echo "Error: File $input_file not found."
    exit 1
fi

temp_file=$(mktemp)
sort -k3,3 -n "$input_file" > "$temp_file"
mv "$temp_file" "$input_file"
gnuplot plot.gp > plot.png