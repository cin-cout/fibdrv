set title "perf"
set xlabel "nth fibonacci"
set ylabel "time (ns)"
set terminal png font " Times_New_Roman,12 "
set output "time.png"
set key left

plot\
"iterative.txt" using 1:2 with linespoints linewidth 2 title "iterative", \
"fast_doubling.txt" using 1:2 with linespoints linewidth 2 title "fast doubling"
