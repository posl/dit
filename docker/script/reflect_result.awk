BEGIN {
    PHASE = 0
    EXPECTED_STRINGS[0] = "Docker"
    EXPECTED_STRINGS[1] = "history-"
    OUTPUT_FILES[1] = "Dockerfile.draft"
    OUTPUT_FILES[2] = ".cmd_history"
    ACT_CHANGES[1] = 0
    ACT_CHANGES[2] = 0
}

{
    if ((PHASE <= 1) && ($0 == (" < " EXPECTED_STRINGS[PHASE] "file >")))
        PHASE++
    else if ((PHASE >= 1) && ($0 != "")){
        print $0 >> ("/dit/share/" OUTPUT_FILES[PHASE])
        ACT_CHANGES[PHASE]++
    }
}

END {
    CHANGE_IN_DOCK = ACT_CHANGES[1]
    CHANGE_IN_HIST = ACT_CHANGES[2]

    if (((getline PROV_REPORT < "/dit/tmp/change-report.prov") > 0) && (PROV_REPORT ~ /^[0-9]+ [0-9]+$/)){
        split(PROV_REPORT, PROV_CHANGES, " ")
        CHANGE_IN_DOCK += PROV_CHANGES[1]
        CHANGE_IN_HIST += PROV_CHANGES[2]
    }

    if (CHANGE_IN_DOCK > 0)
        print CHANGE_IN_DOCK >> "/dit/tmp/change-log.dock"
    if (CHANGE_IN_HIST > 0)
        print CHANGE_IN_HIST >> "/dit/tmp/change-log.hist"

    printf ("[dock:+%u hist:+%u] \n", CHANGE_IN_DOCK, CHANGE_IN_HIST) > "/dit/tmp/change-report.act"
}