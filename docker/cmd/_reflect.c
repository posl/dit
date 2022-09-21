/**
 * @file _reflect.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'reflect', 
 * @author Tsukasa Inada
 * @date 2022/09/21
 */

#include "main.h"


/** Data type for storing the results of option parse */
typedef struct {
    int target;    /** integer representing destination file */
    int blank;     /** how to handle blank lines */
} refl_opts;


static int __parse_opts(int argc, char **argv, refl_opts *opt);




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief 
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note treated like a normal main function.
 */
int reflect(int argc, char **argv){
    int i;
    refl_opts opt;

    if ((i = __parse_opts(argc, argv, &opt)))
        return (i > 0) ? 0 : 1;

    return 0;
}


/**
 * @brief parse optional arguments.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @param[out] opt  variable to store the results of option parse
 * @return int  0 (parse success), 1 (normally exit) or -1 (error exit)
 *
 * @note the arguments are expected to be passed as-is from main function.
 */
static int __parse_opts(int argc, char **argv, refl_opts *opt){
    const char *short_opts = "dhps";

    const struct option long_opts[] = {
        { "help",   no_argument,       NULL, 1 },
        { "target", required_argument, NULL, 2 },
        { "blank",  required_argument, NULL, 3 },
        {  0,        0,                 0,   0 }
    };

    const char * const blank_args[] = {
        "ignore",
        "preserve",
        "squeeze"
    };

    opt->target = '\0';
    opt->blank = 'i';

    int c, i;
    char *valid_args = NULL;

    while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) != -1){
        switch (c){
            case 'd':
            case 'h':
                opt->target = c;
                break;
            case 'p':
            case 's':
                opt->blank = c;
                break;
            case 1:
                erase_manual();
                return 1;
            case 2:
                if (optarg){
                    if ((c = receive_target_file(optarg))){
                        if (c != 'b'){
                            opt->target = c;
                            break;
                        }
                        else
                            fputs("reflect: destination file must be limited to one\n", stderr);
                    }
                    valid_args = "- 'dockerfile'\n  - 'history-file'\n";
                    goto err_exit;
                }
            case 3:
                if (optarg){
                    if ((c = receive_expected_string(optarg, blank_args, 3, 2)) >= 0){
                        opt->blank = blank_args[c][0];
                        break;
                    }
                    valid_args = "- 'ignore'\n  - 'preserve'\n  - 'squeeze'\n";
                }
            default:
                goto err_exit;
        }
    }
    return 0;

err_exit:
    if (valid_args){
        if (c <= 0)
            fprintf(stderr, "reflect: invalid argument '%s' for '--%s'\n", optarg, long_opts[i].name);
        fprintf(stderr, "Valid arguments are:\n  %s", valid_args);
    }
    fputs("Try 'dit reflect --help' for more information.\n", stderr);
    return -1;
}