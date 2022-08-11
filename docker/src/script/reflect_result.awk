BEGIN {
    PHASE = 0
    EXPECTS[0] = "Docker"
    EXPECTS[1] = "history-"
    TO_FILES[1] = "Dockerfile.draft"
    TO_FILES[2] = ".cmd_history"
}

{
    if ((PHASE <= 1) && ($0 == (" < " EXPECTS[PHASE] "file >")))
        PHASE++
    else if ((PHASE >= 1) && ($0 != ""))
        print $0 >> ("/dit/share/" TO_FILES[PHASE])
}