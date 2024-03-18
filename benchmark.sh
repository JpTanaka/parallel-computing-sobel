make
for images in {1..36..5}; do
    for size in 100 640 1500; do
        for N in {1..3..1}; do
            for n in {1..5..2}; do
            salloc -N $N -n $n mpirun ./sobelf $images $size $size $N
            done
        done
    done
done