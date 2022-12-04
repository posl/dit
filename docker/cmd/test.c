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
 * @param[in]  cmd_id  index number corresponding to one of the dit commands or negative integer
 *
 * @note if unit tests were performed in this function, it will not return to the caller.
 */
void test(int argc, char **argv, int cmd_id){
    assert(argc > 0);
    assert(argv);
    assert(cmd_id < CMDS_NUM);

    bool test_flag = false;
    void (* test_func)(void) = dit_test;

    if (cmd_id >= 0){
        void (* const test_funcs[CMDS_NUM])(void) = {
            cmd_test,
            config_test,
            convert_test,
            copy_test,
            erase_test,
            healthcheck_test,
            help_test,
            ignore_test,
            inspect_test,
            label_test,
            onbuild_test,
            optimize_test,
            reflect_test
        };

        test_flag = parse_opts(argc, argv);
        test_func = test_funcs[cmd_id];
    }
    else if (argc == 1)
        test_flag = (! strcmp(*argv, "test"));

    if (test_flag){
        test_func();
        exit(0);
    }
}


/**
 * @brief check if command line arguments contain an unit test option.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return bool  whether there was an unit test option
 *
 * @note the arguments are expected to be passed as-is from main function.
 * @note reset updated global variables so that it does not affect subsequent option parse.
 */
static bool parse_opts(int argc, char **argv){
    const char *short_opts = "+";

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

    optind = 0;
    opterr = 1;

    return (bool) flag;
}




/******************************************************************************
    * Utilities
******************************************************************************/


/**
 * @brief check if array of strings is in alphabetical order.
 *
 * @param[in]  reprs  array of expected strings
 * @param[in]  size  array size
 * @return bool  the resulting boolean
 *
 * @note even if there are duplications between strings, it is regarded as an error.
 */
bool check_if_alphabetical_order(const char * const *reprs, size_t size){
    assert(reprs);

    if (size > 1){
        const char * const *p_repr;
        const char *repr;

        p_repr = reprs;
        size--;

        do {
            assert((repr = *(p_repr++)));

            if (strcmp(repr, *p_repr) >= 0)
                return false;
        } while (--size);
    }

    return true;
}




/**
 * @brief check if the comparison result is correct.
 *
 * @param[in]  type  the type of comparison result
 * @param[in]  result  comparison result
 * @return bool  the resulting boolean
 */
bool comptest_result_check(int type, int result){
    assert((type >= 0) && (type < COMPTESTS_NUM));

    switch (type){
        case COMPTEST_EQUAL:
            return (! result);
        case COMPTEST_LESSER:
            return (result < 0);
        case COMPTEST_GREATER:
            return (result > 0);
    }
}




/**
 * @brief print looping progress information for the test.
 *
 * @param[in]  code_c  output code ('S' (success or failure), 'C' (equal, lesser or greater) or others)
 * @param[in]  type  output type that embody output code above
 * @param[in]  count  count value during a single loop
 *
 * @attention the newline character is not printed to give concrete information after this.
 */
void print_progress_test_loop(int code_c, int type, int count){
    assert(count >= 0);

    const char * const successful_reprs[2] = {
        "success",
        "failure"
    };

    const char * const comptest_reprs[COMPTESTS_NUM] = {
        "equal",
        "lesser",
        "greater"
    };

    const char *desc = NULL;

    switch (code_c){
        case 'S':
            desc = successful_reprs[(bool) type];
            break;
        case 'C':
            assert((type >= 0) && (type < COMPTESTS_NUM));
            desc = comptest_reprs[type];
    }

    if (desc)
        fprintf(stderr, "%9s case", desc);

    fprintf(stderr, "%4d:  ", count);
}


#endif // NDEBUG