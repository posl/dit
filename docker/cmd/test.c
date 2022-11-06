/**
 * @file test.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the interface and utility functions for unit tests.
 * @author Tsukasa Inada
 * @date 2022/11/06
 */

#include "main.h"


#ifndef NDEBUG


static bool parse_opts(int argc, char **argv);




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief perform unit tests if instructed to do so.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @param[in]  cmd_id  index number corresponding to one of the dit commands
 *
 * @note always unit test commonly used functions before unit testing the dit command.
 * @note if unit tests were performed in this function, it will not return to the caller.
 */
void test(int argc, char **argv, int cmd_id){
    assert(argc > 0);
    assert(argv);
    assert((cmd_id >= 0) && (cmd_id < CMDS_NUM));

    if (parse_opts(argc, argv)){
        int (* const test_funcs[CMDS_NUM])(void) = {
            config_test,
            convert_test,
            cp_test,
            erase_test,
            healthcheck_test,
            help_test,
            ignore_test,
            inspect_test,
            label_test,
            onbuild_test,
            optimize_test,
            reflect_test,
            setcmd_test
        };

        int exit_status;
        exit_status = (dit_test() || test_funcs[cmd_id]()) ? 1 : 0;

        exit(exit_status);
    }
}


/**
 * @brief check if command line arguments contain a unit test option.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return bool  whether there was unit test option
 *
 * @note the arguments are expected to be passed as-is from main function.
 * @note reset updated global variables so that it does not affect subsequent option parse.
 */
static bool parse_opts(int argc, char **argv){
    const char *short_opts = "";

    int flag = false;
    const struct option long_opts[] = {
        { "test",       no_argument, &flag, true },
        { "unit-tests", no_argument, &flag, true },
        {  0,            0,            0,    0   }
    };

    opterr = 0;

    while (getopt_long(argc, argv, short_opts, long_opts, NULL) >= 0)
        if (flag)
            break;

    optind = (opterr = 1);

    return (bool) flag;
}


#endif // NDEBUG