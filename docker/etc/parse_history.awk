
#
# take the history of the most recently executed command line as input and record its contents
#

BEGIN {
    N = 0
    HNF = "/dit/srv/last-history-number"
    CLF = "/dit/srv/last-command-line"
}

NR == 1 {
    N = $1

    if (((getline M < HNF) > 0) && (M ~ /^[+-]?[0-9]+$/)){
        if (N == (M + 1)){
            $1 = ""
            sub(/[ \t]*/, "", $0)
            print $0 > CLF
            next
        }
    }
    else
        printf "dit: invalid history number: '%s'\n", M > "/dev/stderr"
    exit 1
}

NR != 1 {
    print $0 >> CLF
}

END {
    close(HNF)
    print N > HNF

    if (! N)
        exit 1
}