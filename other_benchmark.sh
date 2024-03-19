# have an idea of the impact of SendRecv, set nb_processes to 2 and compare both original and MPI approach
make
for images in {1..36..5}; do
    for size in 100 640 1500; do
        for n in {2}; do
            max_N=$((n < 3 ? n : 3))  
            for ((N = 1; N <= max_N; N++)); do
                for use_mpi in 0 1; do
                salloc -N $N -n $n mpirun ./sobelf $images $size $size $N $use_mpi 0 0
                done
            done
        done
    done
done