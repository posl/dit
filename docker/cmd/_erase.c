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
    unsigned int lines;    /** the maximum number of lines to remove */
    unsigned int times;    /** option argument for '-N' */
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
    const char *short_opts = "finrvB:C:L:N:";

    const struct option long_opts[] = {
        { "force",       no_argument,       NULL, 'f' },
        { "interactive", no_argument,       NULL, 'i' },
        { "selective",   no_argument,       NULL, 'n' },
        { "reset",       no_argument,       NULL, 'r' },
        { "verbose",     no_argument,       NULL, 'v' },
        { "begin-with",  required_argument, NULL, 'B' },
        { "contain",     required_argument, NULL, 'C' },
        { "lines",       required_argument, NULL, 'L' },
        { "times",       required_argument, NULL, 'N' },
        { "help",        no_argument,       NULL,  1  },
        {  0,             0,                 0,    0  }
    };

    opt->reset = false;
    opt->mode = interactive;
    opt->verbose = false;
    opt->BC_args = (char **) malloc(sizeof(char *) * (argc - 1));
    opt->B_end = 0;
    opt->C_end = 0;
    opt->lines = -1;
    opt->times = 0;

    int c, i;
    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1){
        switch (c){
            case 'f':
                opt->mode = force;
                break;
            case 'i':
                opt->mode = interactive;
                break;
            case 'n':
                opt->mode = selective;
                break;
            case 'r':
                opt->reset = true;
                break;
            case 'v':
                opt->verbose = true;
                break;
            case 1:
                erase_manual();
                return 1;
            case 'B':
                opt->B_end++;
            case 'C':
                if (optarg){
                    if (opt->BC_args)
                        opt->BC_args[opt->C_end++] = optarg;
                    break;
                }
            case 'L':
            case 'N':
                if (optarg){
                    if ((i = receive_positive_integer(optarg)) >= 0){
                        if (c == 'L')
                            opt->lines = i;
                        else
                            opt->times = i;
                        break;
                    }
                    fputs("erase: invalid number of ", stderr);
                    fprintf(stderr, "%s: %s", ((c == 'L') ? "lines limit" : "times to go back"), optarg);
                }
            default:
                fputs("Try 'dit erase --help' for more information.\n", stderr);
                return -1;
        }
    }

    if (! opt->C_end){
        free(opt->BC_args);
        opt->BC_args = NULL;
        if (! opt->times)
            opt->times = 1;
    }
    return 0;
}