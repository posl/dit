BEGIN {
    X = -1
}

NR == 1 {
    X = (((getline N < "/dit/tmp/last-history-number") > 0) && (N ~ /^[0-9]+$/) && (N < $1)) ? 0 : 1

    print $1 > "/dit/tmp/last-history-number"

    if (X == 0){
        $1 = ""
        sub(/[ \t]*/, "", $0)
        print $0 > "/dit/tmp/last-command-line"
    }
    else {
        print " dit: invalid number of history" > "/dev/stderr"
        exit 1
    }
}

NR != 1 {
    print $0 >> "/dit/tmp/last-command-line"
}

END {
    if (X < 0){
        print 0 > "/dit/tmp/last-history-number"
        exit 1
    }
}