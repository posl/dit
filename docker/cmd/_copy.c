/**
 * @file _copy.c
 *
 * Copyright (c) 2023 Tsukasa Inada
 *
 * @brief Described the dit command 'copy', that imitates COPY/ADD instruction.
 * @author Tsukasa Inada
 * @date 2023/02/08
 */

#include "main.h"
#include "srcglob.h"

#define COPY_TMP_DIR "/dit/tmp/copy.d"


/** Data type for storing the results of option parse */
typedef struct {
    bool verbose;         /** whether to display an added instruction on screen */
    bool extract_flag;    /** whether to expand the tar archive like the ADD instruction */
    char *chown_arg;      /** the argument to set in the chown flag of the COPY instruction */
} copy_opts;


static int parse_opts(int argc, char **argv, copy_opts *opt);
static bool parse_owner(const char *target);
static int do_copy(int argc, char **argv, copy_opts *opt);

/*
static int copy_to_tmp_dir();
static int move_from_tmp_dir();

static int reflect_copy_instr();
*/


/** owner information of the files to be newly created */
static uid_t uid = 0;
static gid_t gid = 0;




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief copy files from the host environment and reflect this as COPY/ADD instruction.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note treated like a normal main function.
 */
int copy(int argc, char **argv){
    int i;
    copy_opts opt;
    const char *errdesc = NULL;

    if ((i = parse_opts(argc, argv, &opt)))
        i = (i < 0) ? FAILURE : SUCCESS;
    else {
        i = FAILURE;
        argc -= optind;
        argv += optind;

        if (argc <= 0)
            errdesc = "source";
        else if (argc == 1)
            errdesc = "destination";
        else
            i = do_copy(argc, argv, &opt);
    }

    if (i){
        if (errdesc)
            xperror_missing_args(errdesc);
        xperror_suggestion(true);
    }
    return i;
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
 * @note the argument of '--chown' matches that of the COPY instruction.
 */
static int parse_opts(int argc, char **argv, copy_opts *opt){
    assert(opt);

    const char *short_opts = "vX";

    const struct option long_opts[] = {
        { "verbose", no_argument,       NULL, 'v' },
        { "extract", no_argument,       NULL, 'X' },
        { "help",    no_argument,       NULL,  1  },
        { "chown",   required_argument, NULL,  0  },
        {  0,         0,                 0,    0  }
    };

    opt->verbose = false;
    opt->extract_flag = false;
    opt->chown_arg = NULL;

    int c, i;

    while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) >= 0)
        switch (c){
            case 'v':
                opt->verbose = true;
                break;
            case 'X':
                opt->extract_flag = true;
                break;
            case 1:
                copy_manual();
                return NORMALLY_EXIT;
            case 0:
                if (optarg){
                    if ((! *optarg) || strchrcmp(optarg, ':')){
                        uid = 0;
                        gid = 0;
                        opt->chown_arg = NULL;
                        break;
                    }
                    if (parse_owner(optarg)){
                        opt->chown_arg = optarg;
                        break;
                    }
                }
                xperror_invalid_arg('O', 1, long_opts[i].name, optarg);
            default:
                return ERROR_EXIT;
        }

    return SUCCESS;
}


/**
 * @brief parse the target string as the argument to set in the chown flag of the COPY instruction.
 *
 * @param[in]  target  target string
 * @return bool  successful or not
 *
 * @note neither 'UID_MAX' nor 'GID_MAX' were defined.
 */
static bool parse_owner(const char *target){
    assert(target);

    char *brk;
    int tmp;
    struct passwd *passwd;
    struct group *group;

    if ((brk = strchr(target, ':')))
        *(brk++) = '\0';

    if (! *target)
        uid = 0;
    else if ((tmp = receive_positive_integer(target, NULL)) >= 0)
        uid = tmp;
    else if ((passwd = getpwnam(target)))
        uid = passwd->pw_uid;
    else
        goto errexit;

    if (! (brk && *brk))
        gid = uid;
    else if ((tmp = receive_positive_integer(brk, NULL)) >= 0)
        gid = tmp;
    else if ((group = getgrnam(brk)))
        gid = group->gr_gid;
    else
        goto errexit;

    return true;

errexit:
    if (brk)
        *(--brk) = ':';
    return false;
}




/**
 * @brief 
 *
 * @param[in]  argc  the number of non-optional arguments
 * @param[in]  argv  array of strings that are non-optional arguments
 * @param[in]  opt  variable to store the results of option parse
 * @return int  command's exit status
 */
static int do_copy(int argc, char **argv, copy_opts *opt){
    assert(opt);

    return SUCCESS;
}




/******************************************************************************
    * Copy Part
******************************************************************************/



//




/******************************************************************************
    * Reflect Part
******************************************************************************/



//




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/


void copy_test(void){
    fputs("copy test\n", stdout);
}


#endif // NDEBUG