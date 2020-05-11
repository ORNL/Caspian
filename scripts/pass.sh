#!/usr/bin/env zsh

tst() {
    n="$1" #neurons
    dl="N"
    t=`expr $n + 1000`
    f="1000"
    rs="5"
    kw='Simulate Time'
    a=`bin/benchmark sim $n 1 $rs $t $f | grep "$kw" - | cut -d ':' -f 2 | xargs`
    b=`bin/benchmark ucaspian $n 1 $rs $t $f | grep "$kw" - | cut -d ':' -f 2 | xargs`

    printf "%3d %8.6f %8.6f\n" $n $a $b
}

printf "%3s %8s %8s\n" "N" "Sim" "FPGA"

for i in {5..255..5}
do
    tst $i
done

