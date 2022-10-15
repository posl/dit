/**
 * @file _reflect.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'reflect', 
 * @author Tsukasa Inada
 * @date 2022/09/21
 *
 * @note 
 * @note 
 */

#include "main.h"

#define BLANKS_NUM 3

#define REFLECT_FILE_P "/dit/tmp/reflect-report.prov"
#define REFLECT_FILE_R "/dit/tmp/reflect-report.real"


/** Data type for storing the results of option parse */
typedef struct {
    int target;    /** integer representing destination file */
    int blank;     /** how to handle blank lines */
} refl_opts;


static int __parse_opts(int argc, char **argv, refl_opts *opt);
static int __determine_reflect();


extern const char * const target_args[TARGETS_NUM];




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
    if (! ((argc == 1) && check_file_isempty(REFLECT_FILE_R))){
        int i;
        refl_opts opt;

        if ((i = __parse_opts(argc, argv, &opt)))
            return (i < 0);
    }
    else
        __determine_reflect();

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

    int flag;
    const struct option long_opts[] = {
        { "help",   no_argument,        NULL,  1    },
        { "blank",  required_argument, &flag, true  },
        { "target", required_argument, &flag, false },
        {  0,        0,                  0,    0    }
    };

    const char * const blank_args[BLANKS_NUM] = {
        "ignore",
        "preserve",
        "squeeze"
    };

    opt->target = '\0';
    opt->blank = 'i';

    int c, i, *ptr;
    const char * const *valid_args = NULL;

    while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) >= 0){
        switch (c){
            case 'd':
                assign_both_or_either(opt->target, 'h', 'b', 'd');
                break;
            case 'h':
                assign_both_or_either(opt->target, 'd', 'b', 'h');
                break;
            case 'p':
            case 's':
                opt->blank = c;
                break;
            case 1:
                erase_manual();
                return 1;

#if (BLANKS_NUM != TARGETS_NUM)
    #error "inconsistent with the definition of the macros"
#endif
            case 0:
                if (flag){
                    ptr = &(opt->blank);
                    valid_args = blank_args;
                }
                else {
                    ptr = &(opt->target);
                    valid_args = target_args;
                }
                if ((c = receive_expected_string(optarg, valid_args, BLANKS_NUM, 2)) >= 0){
                    *ptr = valid_args[c][0];
                    break;
                }
                xperror_invalid_optarg(c, long_opts[i].name, optarg);
                xperror_valid_args(valid_args, TARGETS_NUM);
            default:
                xperror_suggestion(true);
                return -1;
        }
    }

    if (opt->target)
        return 0;

    xperror_target_files();
    return -1;
}





static int __determine_reflect(){

}




/******************************************************************************
    * Functions that operate File-by-File
******************************************************************************/


int read_temporary_report(){

}




/******************************************************************************
    * Functions that operate Line-by-Line
******************************************************************************/


