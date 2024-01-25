set terminal png
set logscale x
set logscale y
set xlabel "Number of pixels"
set ylabel "Execution time (s)"
plot "perf.log" using ($1*$2):3 title "Sequential" with linespoints
