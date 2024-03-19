#!/bin/bash

make

INPUT_DIR=images/original
OUTPUT_DIR=images/processed
mkdir $OUTPUT_DIR 2>/dev/null


if [ -f "$input_file" ]; then
    rm "$input_file"
    touch "$input_file"    
fi 

for i in $INPUT_DIR/*gif ; do
    DEST=$OUTPUT_DIR/`basename $i .gif`-sobel.gif
    echo "Running test on $i -> $DEST"
    salloc -N 1 -n 2 mpirun ./sobelf $i $DEST
done
