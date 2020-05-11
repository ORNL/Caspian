#!/usr/bin/env zsh

tst() {
    n="$1" #neurons
    s="$2" #seed
    dl="N"
    t="200"
    kw='Counts'
    a=`bin/all_to_all_bench sim $n 1 $t $s $dl | grep "$kw" - | cut -d ':' -f 2 | xargs`
    b=`bin/all_to_all_bench ucaspian $n 1 $t $s $dl | grep "$kw" - | cut -d ':' -f 2 | xargs`
    c=`bin/all_to_all_bench verilator $n 1 $t $s $dl | grep "$kw" - | cut -d ':' -f 2 | xargs`
    v="Y"

    if ((a != c)); then
        v="X"
    elif ((a != b)); then
        v="X"
    fi

    printf "%3d %3d | %5d %5d %5d %4s\n" $n $s $a $b $c $v
}

printf "%3s %3s | %5s %5s %5s %s\n" "N" "S" "Sim" "FPGA" "SVsim" "Correct?"

for i in {8..32}
do
    for j in {0..9}
    do
        tst $i $j
    done
done

