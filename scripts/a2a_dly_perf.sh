#!/usr/bin/env zsh

tst() {
    n="$1" #neurons
    dl="Y"
    t="1000"
    rs="5"
    kw='Average Simulate'
    a="0"
    b="0"

    for s in {0..4}
    do
        x=`bin/all_to_all_bench sim $n $rs $t $s | grep "$kw" - | cut -d ':' -f 2 | xargs`
        y=`bin/all_to_all_bench ucaspian $n $rs $t $s | grep "$kw" - | cut -d ':' -f 2 | xargs`
        
        a=`echo "$a+$x" | bc -l`
        b=`echo "$b+$y" | bc -l`
    done

    a=`echo "$a/5.0" | bc -l`
    b=`echo "$b/5.0" | bc -l`

    printf "%3d %8.6f %8.6f\n" $n $a $b
}

printf "%3s %8s %8s\n" "N" "Sim" "FPGA"

for i in {8..64}
do
    tst $i
done

