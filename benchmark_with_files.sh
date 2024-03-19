make
INPUT_DIR=images/original
OUTPUT_DIR=images/processed
mkdir $OUTPUT_DIR 2>/dev/null

for use_mpi in 0 1; do
for use_omp in 0 1; do
for use_cuda in 0 1; do
    for n in 2; do
        max_N=$((n < 3 ? n : 3))  
        for ((N = 1; N <= max_N; N++)); do
            for i in $INPUT_DIR/*gif ; do
                filename=$(basename "$i" .gif)
                
                DEST="$OUTPUT_DIR/$filename""_$use_mpi""_$use_omp""_$use_cuda-sobel.gif"
                echo "Running test on $i -> $DEST"
                salloc -N $N -n $n mpirun ./sobelf 0 0 0 $N $use_mpi $use_omp $use_cuda $i $DEST
            done
        done
    done
done
done
done