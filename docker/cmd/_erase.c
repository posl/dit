/**
 * @file _erase.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'erase', that 
 * @author Tsukasa Inada
 * @date 2022/09/09
 *
 * @note 
 */

#include "main.h"

#define LOG_FILE_D "/dit/tmp/change-log.dock"
#define LOG_FILE_H "/dit/tmp/change-log.hist"


/** Data type for storing the results of option parse */
typedef struct {
    bool reset;            /** whether to reset log-file */
    enum {
        force,
        interactive,
        selective
    } mode;                /** how to confirm whether to remove the hit lines */
    bool verbose;          /** whether to display deleted lines on screen */
    char **BC_args;        /** array for storing option arguments for '-B' or '-C' */
    unsigned int B_end;    /** index representing the end of option arguments for '-B' */
    unsigned int C_end;    /** index representing the end of option arguments for '-C' */
    unsigned int times;    /** option argument for '-N' */
    unsigned int limit;    /** the maximum number of lines to remove */
} erase_opts;


static int __parse_opts(int argc, char **argv, erase_opts *opt);




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
int erase(int argc, char **argv){
    int i;
    erase_opts opt;

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
static int __parse_opts(int argc, char **argv, erase_opts *opt){
    optind = 1;
    opterr = 0;

    const char *short_opts = ":rfi::nvB:C:N:L:";
    const struct option long_opts[] = {
        { "reset",       no_argument,       NULL, 'r' },
        { "force",       no_argument,       NULL, 'f' },
        { "interactive", optional_argument, NULL, 'i' },
        { "selective",   no_argument,       NULL, 'n' },
        { "verbose",     no_argument,       NULL, 'v' },
        { "begin-with",  required_argument, NULL, 'B' },
        { "contain",     required_argument, NULL, 'C' },
        { "times",       required_argument, NULL, 'N' },
        { "lines-limit", required_argument, NULL, 'L' },
        { "help",        no_argument,       NULL,  1  },
        {  0,             0,                 0,    0  }
    };
    int long_index = -1;

    opt->reset = false;
    opt->mode = interactive;
    opt->verbose = false;
    opt->BC_args = (char **) malloc(sizeof(char *) * (argc - 1));
    opt->B_end = 0;
    opt->C_end = 0;
    opt->times = 0;
    opt->limit = -1;

    int c, i;
    const char *err_msg;

    while ((c = getopt_long(argc, argv, short_opts, long_opts, &long_index)) != -1){
        switch (c){
            case 'r':
                opt->reset = true;
                break;
            case 'f':
                opt->mode = force;
                break;
            case 'i':
                if (! optarg)
                    opt->mode = interactive;
                else if ((i = receive_yes_or_no(optarg)))
                    opt->mode = (i == 'y') ? force : selective;
                else {
                    err_msg = "Valid arguments are:\n  - no arguments\n  - strings representing [Y/n]\n";
                    goto invalid_arg;
                }
                break;
            case 'n':
                opt->mode = selective;
                break;
            case 'v':
                opt->verbose = true;
                break;
            case 1:
                erase_usage();
                return 1;
            case 'B':
                opt->B_end++;
            case 'C':
                if (optarg){
                    if (opt->BC_args)
                        opt->BC_args[opt->C_end++] = optarg;
                    break;
                }
            case 'N':
            case 'L':
                if (optarg){
                    if ((i = receive_positive_integer(optarg)) >= 0){
                        if (c == 'N')
                            opt->times = i;
                        else
                            opt->limit = i;
                        break;
                    }
                    else {
                        err_msg = "Valid arguments are:\n  - '0'\n  - positive integer\n";
                        goto invalid_arg;
                    }
                }
            case ':':
                if (argv[--optind][1] == '-'){
                    i = 5;
                    while ((long_opts[i].val != optopt) && (++i <= 8));
                    if (i <= 8)
                        fprintf(stderr, "erase: '--%s' requires an argument\n", long_opts[i].name);
                }
                else
                    fprintf(stderr, "erase: '-%c' requires an argument\n", optopt);
                goto err_exit;
            case '\?':
                if (argv[--optind][1] == '-')
                    fprintf(stderr, "erase: unrecognized option '%s'\n", argv[optind]);
                else
                    fprintf(stderr, "erase: invalid option '-%c'\n", optopt);
            default:
                goto err_exit;
        }
        long_index = -1;
    }

    if (! opt->C_end){
        free(opt->BC_args);
        opt->BC_args = NULL;
        if (! opt->times)
            opt->times = 1;
    }
    return 0;

invalid_arg:
    fprintf(stderr, "erase: invalid argument '%s' for ", optarg);
    if (long_index >= 0)
        fprintf(stderr, "'--%s'\n", long_opts[long_index].name);
    else
        fprintf(stderr, "'-%c'\n", c);
    fputs(err_msg, stderr);

err_exit:
    fputs("Try 'dit erase --help' for more information.\n", stderr);
    return -1;
}