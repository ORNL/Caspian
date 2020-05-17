#!/usr/bin/env zsh

# neurons, delay, fires
tst() {
    n="$1" #neurons
    dl="$2"
    f="$3"
    t=`expr $n + $n \* $dl + $f + 10`
    kw='Outputs'
    a=`bin/pass_bench sim $n 1 1 $t $f $dl | grep "$kw" - | cut -d ':' -f 2 | xargs`
    b="$a"
    c=`bin/pass_bench verilator $n 1 1 $t $f $dl | grep "$kw" - | cut -d ':' -f 2 | xargs`
    vv="Y"
    vf="Y"

    if ((a != c)); then
        vv="X"
    fi
    if ((a != b)); then
        vf="X"
    fi

    printf "%3d %3d %3d %5d | %5d %5d %5d %3s    %3s\n" $n $dl $f $t $a $b $c $vf $vv
}

printf "%3s %3s %3s %5s | %5s %5s %5s %s %s\n" "N" "Dly" "F" "Time" "Sim" "FPGA" "SVsim" "Match?" "Match?"


for n in {5..255..5}
do
    for d in 0 1 2 5 10 15
    do
        for f in 1 2 5 6 8 10
        do 
            tst $n $d $f
        done
    done
done
