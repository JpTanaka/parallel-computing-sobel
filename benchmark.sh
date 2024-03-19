make
for images in {1..36..5}; do
    for size in 100 640 1500; do
        for n in {1..9..2}; do
            max_N=$((n < 3 ? n : 3))  
            for ((N = 1; N <= max_N; N++)); do
                for use_mpi in 0; do
                for use_omp in 0 1; do
                for use_cuda in 0; do
                salloc -N $N -n $n mpirun ./sobelf $images $size $size $N $use_mpi $use_omp $use_cuda
                done
                done
                done
            done
        done
    done
done