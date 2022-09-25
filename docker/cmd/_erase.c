/**
 * @file _erase.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'erase', that 
 * @author Tsukasa Inada
 * @date 2022/09/09
 */

#include "main.h"

#define LID 4
#define NID 5


/** Data type for storing the results of option parse */
typedef struct {
    int target;        /** integer representing the files to be edited */
    bool reset;        /** whether to reset log-file */
    bool verbose;      /** whether to display deleted lines on screen */
    int response;      /** the response to confirmation that it is okay to remove the hit lines */
    char **BC_args;    /** array for storing option arguments for '-B' or '-C' */
    int B_end;         /** index number representing the end of option arguments for '-B' */
    int C_end;         /** index number representing the end of option arguments for '-C' */
    int lines;         /** the maximum number of lines to remove */
    int times;         /** option argument for '-N' */
} erase_opts;


static int __parse_opts(int argc, char **argv, erase_opts *opt);


extern const char * const response_reprs[RESPONSES_NUM];
extern const char * const target_files_reprs[TARGETS_NUM];




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

    if ((i = __parse_opts(argc, argv, &opt))){
        if (i > 0)
            return 0;

        xperror_suggestion(true);
    }

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
    const char *short_opts = "dhrvyB:C:L:N:";

    const struct option long_opts[] = {
                { "reset",       no_argument,       NULL, 'r' },
                { "verbose",     no_argument,       NULL, 'v' },
                { "begin-with",  required_argument, NULL, 'B' },
                { "contain",     required_argument, NULL, 'C' },
        [LID] = { "lines",       required_argument, NULL, 'L' },
        [NID] = { "times",       required_argument, NULL, 'N' },
                { "help",        no_argument,       NULL,  1  },
                { "target",      required_argument, NULL,  2  },
                { "response",    required_argument, NULL,  3  },
                {  0,             0,                 0,    0  }
    };

    opt->target = '\0';
    opt->reset = false;
    opt->verbose = false;
    opt->response = '\0';
    opt->BC_args = (char **) malloc(sizeof(char *) * (argc - 1));
    opt->B_end = 0;
    opt->C_end = 0;
    opt->lines = -1;
    opt->times = 0;

    int c, i, size;
    int *ptr = NULL;
    const char * const *valid_args = NULL;

    while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) >= 0){
        switch (c){
            case 'd':
            case 'h':
                opt->target = c;
                break;
            case 'r':
                opt->reset = true;
                break;
            case 'v':
                opt->verbose = true;
                break;
            case 'y':
                opt->response = 'Y';
                break;
            case 'B':
                opt->B_end++;
            case 'C':
                if (opt->BC_args)
                    opt->BC_args[opt->C_end++] = optarg;
                break;
            case 'L':
                ptr = &(opt->lines);
                i = LID;
            case 'N':
                if (! ptr){
                    ptr = &(opt->times);
                    i = NID;
                }
                if ((c = receive_positive_integer(optarg)) >= 0){
                    *ptr = c;
                    ptr = NULL;
                    break;
                }
                INVALID_NUMBER(long_opts[i].name, optarg);
                return -1;
            case 1:
                erase_manual();
                return 1;
            case 2:
                ptr = &(opt->target);
                valid_args = target_files_reprs;
            case 3:
                if (! ptr){
                    ptr = &(opt->response);
                    valid_args = response_reprs;
                }
                size = c ^ 1;
                if ((c = receive_expected_string(optarg, valid_args, size, c)) >= 0){
                    *ptr = valid_args[c][0];
                    ptr = NULL;
                    break;
                }
                INVALID_OPTARG(c, long_opts[i].name, optarg);
                xperror_valid_args(valid_args, size);
            default:
                return -1;
        }
    }

    if (opt->target){
        if (! opt->C_end){
            free(opt->BC_args);
            opt->BC_args = NULL;

            if (! opt->times)
                opt->times = 1;
        }
        return 0;
    }
    xperror_target_file(NULL);
    return -1;
}