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
    vv="Y"
    vf="Y"

    if ((a != c)); then
        vv="X"
    fi
    if ((a != b)); then
        vf="X"
    fi

    printf "%3d %3d | %5d %5d %5d %3s    %3s\n" $n $s $a $b $c $vf $vv
}

printf "%3s %3s | %5s %5s %5s %s %s\n" "N" "S" "Sim" "FPGA" "SVsim" "Match?" "Match?"

for i in {2..64}
do
    for j in {0..99}
    do
        tst $i $j
    done
done

