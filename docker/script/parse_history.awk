BEGIN {
    N = 0
    HNF = "/dit/tmp/last-history-number"
    CLF = "/dit/tmp/last-command-line"
}

NR == 1 {
    N = $1

    if (((getline M < HNF) > 0) && (M ~ /^[0-9]+$/)){
        if (N == (M + 1)){
            $1 = ""
            sub(/[ \t]*/, "", $0)
            print $0 > CLF
            next
        }
    }
    else
        print "dit: cannot get the valid history number" > "/dev/stderr"
    exit 1
}

NR != 1 {
    print $0 >> CLF
}

END {
    print N > HNF
    if (N == 0)
        exit 1
}