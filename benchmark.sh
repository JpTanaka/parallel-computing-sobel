make
for images in {1..46..5}; do
    for width in {10..10000..500}; do
        for height in {10..10000..500}; do
            salloc -N 1 -n 2 mpirun ./sobelf $images $width $height
        done
    done
done