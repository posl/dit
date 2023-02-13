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
    uid_t chown_uid;      /** uid of the new file or directory */
    gid_t chown_gid;      /** gid of the new file or directory */
} copy_opts;


static int parse_opts(int argc, char **argv, copy_opts *opt);
static int do_copy(int argc, char **argv, copy_opts *opt);

/*
static int copy_to_tmp_dir();
static int move_from_tmp_dir();

static int reflect_copy_instr();
*/



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
    const char *errdesc = NULL, *first_arg = NULL;

    if ((i = parse_opts(argc, argv, &opt)))
        i = (i < 0) ? FAILURE : SUCCESS;
    else {
        i = FAILURE;
        argc -= optind;
        argv += optind;

        if (argc <= 0)
            errdesc = "file";
        else if (argc == 1){
            errdesc = "destination file";
            first_arg = *argv;
        }
        else
            i = do_copy(argc, argv, &opt);
    }

    if (i){
        if (errdesc)
            xperror_missing_args(errdesc, first_arg);
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
    opt->chown_uid = 0;
    opt->chown_gid = 0;

    int c, i;
    char *tmp, *p_colon;
    struct passwd *passwd;
    struct group *group;

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
                if (! optarg)
                    goto errexit;
                for (tmp = optarg, p_colon = NULL; *tmp; tmp++){
                    if (isspace((unsigned  char) *tmp))
                        goto errexit;
                    if (*tmp == ':'){
                        if (p_colon)
                            goto errexit;
                        p_colon = tmp;
                    }
                }
                if (optarg == p_colon)
                    opt->chown_uid = 0;
                else {
                    if (p_colon)
                        *p_colon = '\0';
                    if ((c = receive_positive_integer(optarg, NULL)) < 0)
                        c = (passwd = getpwnam(optarg)) ? passwd->pw_uid : -1;
                    if (p_colon)
                        *p_colon = ':';
                    if (c < 0)
                        goto errexit;
                    opt->chown_uid = c;
                }
                if (! ((tmp = p_colon) && *(++tmp)))
                    opt->chown_gid = opt->chown_uid;
                else {
                    if ((c = receive_positive_integer(tmp, NULL)) < 0)
                        c = (group = getgrnam(tmp)) ? group->gr_gid : -1;
                    if (c < 0)
                        goto errexit;
                    opt->chown_gid = c;
                }
                opt->chown_arg = optarg;
                break;
            default:
                return ERROR_EXIT;
        }

    return SUCCESS;

errexit:
    xperror_invalid_arg('O', 1, long_opts[i].name, optarg);
    return ERROR_EXIT;
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