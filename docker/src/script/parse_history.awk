BEGIN {
    EXIT_FLAG = -1
}

NR == 1 {
    EXIT_FLAG = (((getline PREV_NUM < "/dit/tmp/last-history-number") > 0) && ($1 > PREV_NUM)) ? 0 : 1

    print $1 > "/dit/tmp/last-history-number"

    if (EXIT_FLAG == 0){
        $1 = ""
        sub(/[ \t]*/, "", $0)
        print $0 > "/dit/tmp/last-command-line"
    }
    else
        exit 1
}

NR != 1 {
    print $0 >> "/dit/tmp/last-command-line"
}

END {
    if (EXIT_FLAG < 0){
        print 0 > "/dit/tmp/last-history-number"
        exit 1
    }
}