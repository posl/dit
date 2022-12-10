/**
 * @file _ignore.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'ignore', that edits the ignore-file used from the dit command 'convert'.
 * @author Tsukasa Inada
 * @date 2022/11/26
 *
 * @note In the ignore-file, a JSON object that associates each command name with its details is stored.
 */

#include "main.h"

#define IGNORE_FILE_BASE_D "/dit/etc/ignore.base.dock"
#define IGNORE_FILE_BASE_H "/dit/etc/ignore.base.hist"

#define IGNORE_FILE_D "/dit/var/ignore.json.dock"
#define IGNORE_FILE_H "/dit/var/ignore.json.hist"

#ifdef NDEBUG
#define IG_WRITER_FLAG  YYJSON_WRITE_NOFLAG
#else
#define IG_WRITER_FLAG  YYJSON_WRITE_PRETTY
#endif

#define IG_INITIAL_LONG_OPTS_MAX 16


/** Data type for storing the results of option parse */
typedef struct {
    int target_c;                /** character representing the target ignore-file ('d', 'h' or 'b') */
    bool unset_flag;             /** whether to only remove the contents of the ignore-file */
    bool print_flag;             /** whether to print the contents of the ignore-file */
    int reset_flag;              /** whether to reset the ignore-file */
    bool additional_settings;    /** whether to accept the additional settings in the arguments */
    char *nothing;               /** string meaning no arguments in the additional settings */
} ig_opts;


/** Data type that is applied to the elements of the array of long options */
typedef struct {
    const char *name;    /** long option name */
    size_t name_len;     /** the length of long option name */
    int colons;          /** the number of colons meaning whether the long option has an argument */
} long_opt_info;


/** Data type that is applied to the elements of the array of optargs */
typedef struct {
    const char *name;    /** option name */
    size_t name_len;     /** the length of option name */
    const char *arg;     /** the argument of an option */
    size_t arg_len;      /** the length of the argument of an option */
} optarg_info;


/** Data type for storing the results of parsing the additional settings */
typedef struct {
    const char *cmd_name;        /** command name */
    const char *short_opts;      /** short options */

    long_opt_info *long_opts;    /** array of long options */
    size_t long_opts_num;        /** the current number of long options */
    size_t long_opts_max;        /** the current maximum length of the array */

    yyjson_mut_doc *optargs;     /** JSON data representing the arguments to be specified for the options */

    char **first_args;           /** array of the first non-optional arguments */
    size_t first_args_num;       /** the current number of the first non-optional arguments */
} additional_settings;


static int parse_opts(int argc, char **argv, ig_opts *opt);
static int ignore_contents(int argc, char **argv, ig_opts *opt);

static bool parse_additional_settings(int argc, char **argv, additional_settings *data, const char *nothing);
static int parse_short_opts(const char *target);
static int parse_long_opts(const char *target, additional_settings *data);
static int append_long_opt(additional_settings *data, const char *name, size_t name_len, int colons);
static int parse_optarg(const char *target, additional_settings *data, optarg_info *p_info);
static bool append_optarg(additional_settings *data, const optarg_info *p_info, const char *nothing);

static void display_ignore_set(const yyjson_doc *idoc, int argc, char **argv, yyjson_write_err *write_err);
static bool edit_ignore_set(yyjson_mut_doc *mdoc, int argc, char **argv, bool unset_flag);
static bool append_ignore_set(yyjson_mut_doc *mdoc, const additional_settings *data, const char *nothing);

static bool check_if_contained(const char *target, yyjson_val *ival, yyjson_arr_iter *p_arr_iter);


extern const char * const target_args[ARGS_NUM];


/** 2D array of ignore-file name (axis 1: whether it is a base-file, axis 2: whether to target 'd' or 'h') */
static const char * const ignore_files[2][2] = {
    { IGNORE_FILE_H,      IGNORE_FILE_D,     },
    { IGNORE_FILE_BASE_H, IGNORE_FILE_BASE_D }
};

/** array of keys pointing to each detailed condition */
static const char * const additional_settings_keys[4] = {
    "short_opts",  // length = 10
    "long_opts",   // length = 9
    "optargs",     // length = 7
    "first_args"   // length = 10
};




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief edit the ignore-file used from the dit command 'convert'.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note treated like a normal main function.
 */
int ignore(int argc, char **argv){
    int i, exit_status = FAILURE;
    ig_opts opt;

    if (! (i = parse_opts(argc, argv, &opt))){
        argc -= optind;
        argv += optind;
        exit_status = ignore_contents(argc, argv, &opt);
    }
    else if (i > 0)
        exit_status = SUCCESS;

    if (exit_status)
        xperror_suggestion(true);
    return exit_status;
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
static int parse_opts(int argc, char **argv, ig_opts *opt){
    assert(opt);

    const char *short_opts = "dhnprA";

    int flag;
    const struct option long_opts[] = {
        { "unset",               no_argument,        NULL, 'n'   },
        { "print",               no_argument,        NULL, 'p'   },
        { "reset",               no_argument,        NULL, 'r'   },
        { "additional-settings", no_argument,        NULL, 'A'   },
        { "help",                no_argument,        NULL,  1    },
        { "target",              required_argument, &flag, true  },
        { "same-as-nothing",     required_argument, &flag, false },
        {  0,                     0,                  0,    0    }
    };

    opt->target_c = '\0';
    opt->unset_flag = false;
    opt->print_flag = false;
    opt->reset_flag = false;
    opt->additional_settings = false;
    opt->nothing = NULL;

    int c, i;
    while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) >= 0)
        switch (c){
            case 'd':
                assign_both_or_either(opt->target_c, 'h', 'b', 'd');
                break;
            case 'h':
                assign_both_or_either(opt->target_c, 'd', 'b', 'h');
                break;
            case 'n':
                opt->unset_flag = true;
                break;
            case 'p':
                opt->print_flag = true;
                break;
            case 'r':
                opt->reset_flag = true;
                break;
            case 'A':
                opt->additional_settings = true;
                break;
            case 1:
                ignore_manual();
                return POSSIBLE_ERROR;
            case 0:
                if (flag){
                    if ((c = receive_expected_string(optarg, target_args, ARGS_NUM, 2)) >= 0){
                        opt->target_c = *(target_args[c]);
                        break;
                    }
                }
                else {
                    if (optarg && (! strchr(optarg, '='))){
                        opt->nothing = optarg;
                        break;
                    }
                    c = 1;
                }
                xperror_invalid_arg('O', c, long_opts[i].name, optarg);
                if (c < 0)
                    xperror_valid_args(target_args, ARGS_NUM);
            default:
                return ERROR_EXIT;
        }

    assert(opt->reset_flag == ((bool) opt->reset_flag));

    if (opt->nothing){
        for (char *tmp = opt->nothing; *tmp; tmp++)
            *tmp = toupper(*tmp);
    }
    else
        opt->nothing = "NONE";

    if (opt->target_c)
        return SUCCESS;

    xperror_target_files();
    return ERROR_EXIT;
}




/**
 * @brief function that contains some the individual functions that handle the ignore-file
 *
 * @param[in]  argc  the number of non-optional arguments
 * @param[out] argv  array of strings that are non-optional arguments
 * @param[out] opt  variable to store the results of option parse
 * @return int  command's exit status
 *
 * @note when appending ignored commands with detailed conditions, argument parsing is done first.
 */
static int ignore_contents(int argc, char **argv, ig_opts *opt){
    assert(opt);
    assert((opt->target_c == 'd') || (opt->target_c == 'h') || (opt->target_c == 'b'));

    bool success = true;
    additional_settings data = {0};
    int offset = 1, exit_status = FAILURE;
    const char *file_name, *errmsg;

    yyjson_doc *idoc;
    yyjson_read_err read_err;
    yyjson_mut_doc *mdoc;
    yyjson_write_err write_err;

    if (argc > 0){
        if (opt->additional_settings){
            if ((argc == 1) || opt->unset_flag || opt->print_flag){
                argc = 1;
                opt->additional_settings = false;
            }
            else
                success = parse_additional_settings(argc, argv, &data, opt->nothing);
        }
    }
    else if (! opt->reset_flag)
        opt->print_flag = true;

    if (success){
        if (opt->target_c == 'h')
            offset = 0;
        exit_status = SUCCESS;

        do {
            file_name = ignore_files[opt->reset_flag][offset];

            if ((idoc = yyjson_read_file(file_name, 0, NULL, &read_err))){
                write_err.msg = NULL;

                if (opt->print_flag){
                    if (opt->target_c == 'b')
                        print_target_repr(offset);
                    display_ignore_set(idoc, argc, argv, &write_err);
                }
                else {
                    file_name = ignore_files[0][offset];

                    if (argc > 0){
                        success = false;

                        if ((mdoc = yyjson_doc_mut_copy(idoc, NULL))){
                            success =
                                (! opt->additional_settings) ?
                                edit_ignore_set(mdoc, argc, argv, opt->unset_flag) :
                                append_ignore_set(mdoc, &data, opt->nothing);

                            if (success)
                                yyjson_mut_write_file(file_name, mdoc, IG_WRITER_FLAG, NULL, &write_err);
                            yyjson_mut_doc_free(mdoc);
                        }

                        if (! success)
                            exit_status = FAILURE;
                    }
                    else {
                        assert(opt->reset_flag);
                        yyjson_write_file(file_name, idoc, IG_WRITER_FLAG, NULL, &write_err);
                    }
                }

                errmsg = write_err.msg;
                yyjson_doc_free(idoc);
            }
            else
                errmsg = read_err.msg;

            if (errmsg){
                exit_status = FAILURE;
                xperror_message(errmsg, file_name);
            }

            if (offset && (opt->target_c == 'b'))
                offset = 0;
            else
                break;
        } while (true);
    }

    if (data.long_opts)
        free(data.long_opts);
    if (data.optargs)
        yyjson_mut_doc_free(data.optargs);

    return exit_status;
}




/******************************************************************************
    * Argument Parser
******************************************************************************/


/**
 * @brief parse non-optional arguments as the additional settings.
 *
 * @param[in]  argc  the number of non-optional arguments
 * @param[in]  argv  array of strings that are non-optional arguments
 * @param[out] data  variable to store the results of parsing the additional settings
 * @param[in]  nothing  string meaning no arguments in the additional settings
 * @return int  successful or not
 */
static bool parse_additional_settings(int argc, char **argv, additional_settings *data, const char *nothing){
    assert(argc > 1);
    assert(argv);
    assert(data);
    assert(nothing);

    assert(! data->cmd_name);
    assert(! data->short_opts);
    assert(! data->long_opts);
    assert(! data->long_opts_max);
    assert(! data->long_opts_num);
    assert(! data->optargs);
    assert(! data->first_args);
    assert(! data->first_args_num);

    int phase = 0;
    const char *errdesc = NULL;
    yyjson_mut_doc *mdoc;
    yyjson_mut_val *mobj;
    optarg_info info[--argc];
    size_t optargs_num = 0;

    if (! (data->cmd_name = *argv))
        goto errexit;

    do {
        if (! *(++argv))
            goto errexit;

        switch (phase){
            case 0:
                switch (parse_short_opts(*argv)){
                    case SUCCESS:
                        data->short_opts = *argv;
                        phase = 1;
                        continue;
                    case POSSIBLE_ERROR:
                        phase = 2;
                        break;
                    case UNEXPECTED_ERROR:
                        errdesc = "short opts";
                    default:
                        goto errexit;
                }
            case 1:
                assert((phase == 1) || (phase == 2));
                if (phase == 1)
                    switch (parse_long_opts(*argv, data)){
                        case SUCCESS:
                            continue;
                        case POSSIBLE_ERROR:
                            phase = 2;
                            break;
                        case UNEXPECTED_ERROR:
                            errdesc = "long opts";
                        default:
                            goto errexit;
                    }
            case 2:
                assert(phase == 2);
                if (strchrcmp(*argv, '=')){
                    phase = 4;
                    continue;
                }
                if (! ((mdoc = yyjson_mut_doc_new(NULL)) && (mobj = yyjson_mut_obj(mdoc))))
                    goto errexit;
                data->optargs = mdoc;
                data->optargs->root = mobj;
                phase = 3;
            case 3:
                switch (parse_optarg(*argv, data, (info + optargs_num))){
                    case SUCCESS:
                        optargs_num++;
                        continue;
                    case POSSIBLE_ERROR:
                        phase = 4;
                        break;
                    case UNEXPECTED_ERROR:
                        errdesc = "optarg";
                    default:
                        goto errexit;
                }
            case 4:
                assert(phase == 4);
                data->first_args = argv;
                data->first_args_num = argc;
                phase = 5;
            case 5:
                if (strchr(*argv, '=')){
                    errdesc = "first arg";
                    goto errexit;
                }
        }
    } while (--argc);

    for (optarg_info *p_info = info; optargs_num--; p_info++)
        if (! append_optarg(data, p_info, nothing))
            goto errexit;

    return true;

errexit:
    if (errdesc)
        xperror_invalid_arg('C', 1, errdesc, *argv);

    return false;
}




/**
 * @brief parse the target string as short options.
 *
 * @param[in]  target  target string
 * @return int  0 (success), 1 (skip this parsing) or -1 (invalid optstring)
 *
 * @note 'ascii_table' is used as a boolean array representing whether each short option is not duplicated.
 */
static int parse_short_opts(const char *target){
    assert(target);

    int exit_status = POSSIBLE_ERROR;

    if (! strchr(target, '=')){
        unsigned int i;
        int colons = 2;
        char ascii_table[UCHAR_MAX + 1] = {0};

        exit_status = SUCCESS;

        while ((i = *(target++))){
            assert(i <= UCHAR_MAX);

            if (i == ':'){
                if (colons < 2){
                    colons++;
                    continue;
                }
            }
            else if ((i != '?') && (! ascii_table[i])){
                colons = 0;
                ascii_table[i] = true;
                continue;
            }

            exit_status = UNEXPECTED_ERROR;
            break;
        }
    }

    return exit_status;
}




/**
 * @brief parse the target string as long options.
 *
 * @param[in]  target  target string
 * @param[out] data  variable to store the results of parsing the additional settings
 * @return int  0 (success), 1 (skip this parsing), -1 (invalid optstring) or -2 (unexpected error)
 *
 * @note in order to call 'append_long_opt' last, checks the end of the target string after each iteration.
 */
static int parse_long_opts(const char *target, additional_settings *data){
    assert(target);
    assert(data);

    int exit_status = POSSIBLE_ERROR;

    if (! strchr(target, '=')){
        const char *name;
        unsigned int i = true;
        int colons = 2;
        size_t count = 0;

        exit_status = SUCCESS;
        name = target;

        for (; i; target++){
            if ((i = *target) == ':'){
                if (colons < 2){
                    colons++;
                    continue;
                }
            }
            else if (i != '?'){
                if (i && (! (colons && count))){
                    colons = 0;
                    count++;
                    continue;
                }
                switch (append_long_opt(data, name, count, colons)){
                    case SUCCESS:
                        name = target;
                        colons = 0;
                        count = 1;
                        continue;
                    case UNEXPECTED_ERROR:
                        exit_status = UNEXPECTED_ERROR;
                }
            }
            assert((! exit_status) || (exit_status == -1));
            exit_status--;
            break;
        }
    }

    return exit_status;
}


/**
 * @brief append long options as the additional settings.
 *
 * @param[out] data  variable to store the results of parsing the additional settings
 * @param[in]  name  option name
 * @param[in]  name_len  the length of option name
 * @param[in]  colons  the number of colons meaning whether the long option has an argument
 * @return int  0 (success), 1 (duplicated long option) or -1 (unexpected error)
 *
 * @note can append virtually unlimited number of long options.
 * @note detects duplication of long options as an error to avoid unexpected errors in 'getopt_long'.
 */
static int append_long_opt(additional_settings *data, const char *name, size_t name_len, int colons){
    assert(data);
    assert(name);
    assert((colons >= 0) && (colons < 3));

    if (name_len){
        long_opt_info *p_long_opt;
        size_t tmp;

        for (p_long_opt = data->long_opts, tmp = data->long_opts_num; tmp--; p_long_opt++)
            if ((name_len == p_long_opt->name_len) && (! strncmp(name, p_long_opt->name, name_len)))
                return POSSIBLE_ERROR;

        if (data->long_opts_num == data->long_opts_max){
            size_t size;
            void *ptr;

            tmp = data->long_opts_max;
            size = tmp ? (tmp * 2) : IG_INITIAL_LONG_OPTS_MAX;

            if ((tmp < size) && (ptr = realloc(data->long_opts, (sizeof(long_opt_info) * size)))){
                data->long_opts = (long_opt_info *) ptr;
                data->long_opts_max = size;
            }
            else
                return UNEXPECTED_ERROR;
        }

        assert(data->long_opts);

        p_long_opt = data->long_opts + data->long_opts_num++;
        p_long_opt->name = name;
        p_long_opt->name_len = name_len;
        p_long_opt->colons = colons;
    }

    return SUCCESS;
}




/**
 * @brief parse the target string as an optarg.
 *
 * @param[in]  target  target string
 * @param[out] data  variable to store the results of parsing the additional settings
 * @param[out] p_info  variable to store the results of this parsing
 * @return int  0 (success), 1 (skip this parsing), -1 (invalid optarg) or -2 (unexpected error)
 *
 * @note in order to analyze which options are equivalent first, construct JSON data in 2 steps.
 * @note this function analyzes which options are equivalent and extracts the data for the next step.
 */
static int parse_optarg(const char *target, additional_settings *data, optarg_info *p_info){
    assert(target);
    assert(data);
    assert(data->optargs);
    assert(data->optargs->root);
    assert(p_info);

    int exit_status = POSSIBLE_ERROR;

    if (strchr(target, '=')){
        size_t count = 0;
        const char *name = NULL;
        yyjson_mut_val *mval, *mkey;

        p_info->name = NULL;
        exit_status = UNEXPECTED_ERROR;

        for (; *target; target++){
            if (*target == '='){
                if (count){
                    assert(name);

                    if (! p_info->name){
                        p_info->name = name;
                        p_info->name_len = count;
                    }
                    else {
                        while ((mval = yyjson_mut_obj_getn(data->optargs->root, name, count))){
                            count = yyjson_mut_get_len(mval);
                            name = yyjson_mut_get_str(mval);
                            assert(name);
                        }
                        if ((count != p_info->name_len) || strncmp(name, p_info->name, count)){
                            mkey = yyjson_mut_strncpy(data->optargs, name, count);
                            mval = yyjson_mut_strn(data->optargs, p_info->name, p_info->name_len);

                            if (! yyjson_mut_obj_add(data->optargs->root, mkey, mval)){
                                exit_status--;
                                goto exit;
                            }
                        }
                    }
                    count = 0;
                }
                else
                    goto exit;
            }
            else if (! count++)
                name = target;
        }

        p_info->arg = count ? name : target;
        p_info->arg_len = count;
        exit_status = SUCCESS;
    }

exit:
    return exit_status;
}


/**
 * @brief append optarg as the additional settings.
 *
 * @param[out] data  variable to store the results of parsing the additional settings
 * @param[in]  p_info  variable containing the results of parsing in the previous step
 * @param[in]  nothing  string meaning no arguments in the additional settings
 * @return bool  successful or not
 *
 * @note in order to reduce waste, the duplication between optargs are removed.
 */
static bool append_optarg(additional_settings *data, const optarg_info *p_info, const char *nothing){
    assert(data);
    assert(data->optargs);
    assert(data->optargs->root);
    assert(p_info);
    assert(p_info->name);
    assert(p_info->name_len > 0);
    assert(p_info->arg);
    assert(nothing);

    const char *name;
    size_t size, idx;
    yyjson_mut_val *mval, *marr;
    bool no_arg;

    name = p_info->name;
    size = p_info->name_len;

    do {
        if ((mval = yyjson_mut_obj_getn(data->optargs->root, name, size))){
            if ((name = yyjson_mut_get_str(mval))){
                size = yyjson_mut_get_len(mval);
                continue;
            }
            marr = mval;
            assert(yyjson_mut_is_arr(marr));
        }
        else {
            mval = yyjson_mut_strncpy(data->optargs, name, size);
            marr = yyjson_mut_arr(data->optargs);

            if (! yyjson_mut_obj_add(data->optargs->root, mval, marr))
                return false;
        }
        break;
    } while (true);

    no_arg = (! xstrcmp_upper_case(p_info->arg, nothing));

    yyjson_mut_arr_foreach(marr, idx, size, mval){
        if (yyjson_mut_is_null(mval)){
            if (no_arg)
                return true;
        }
        else if (! no_arg){
            name = yyjson_mut_get_str(mval);
            assert(name);

            if (! strncmp(name, p_info->arg, p_info->arg_len))
                return true;
        }
    }

    return no_arg ?
        yyjson_mut_arr_add_null(data->optargs, marr) :
        yyjson_mut_arr_add_strn(data->optargs, marr, p_info->arg, p_info->arg_len);
}




/******************************************************************************
    * Individual Functions that handle set of ignored commands
******************************************************************************/


/**
 * @brief display the contents of the ignore-file on screen.
 *
 * @param[in]  idoc  immutable JSON data that is the contents of the ignore-file
 * @param[in]  argc  the number of non-optional arguments
 * @param[out] argv  array of strings that are non-optional arguments
 *
 * @note in order to use 'receive_expected_string', sorts the argument vector in alphabetical order.
 */
static void display_ignore_set(const yyjson_doc *idoc, int argc, char **argv, yyjson_write_err *write_err){
    assert(idoc);
    assert(argv);
    assert(write_err);

    if (argc > 0)
        qsort(argv, argc, sizeof(char *), qstrcmp);

    size_t idx, max;
    yyjson_val *ikey, *ival;
    const char *cmd_name;
    char *skey, *sval;

    yyjson_obj_foreach(idoc->root, idx, max, ikey, ival){
        cmd_name = yyjson_get_str(ikey);
        assert(cmd_name);

        if ((argc > 0) && (receive_expected_string(cmd_name, ((void *) argv), argc, 0) < 0))
            continue;

        if ((skey = yyjson_val_write_opts(ikey, YYJSON_WRITE_NOFLAG, NULL, NULL, write_err))){
            if ((sval = yyjson_val_write_opts(ival, YYJSON_WRITE_PRETTY, NULL, NULL, write_err))){
                fprintf(stdout, "%s: %s\n", skey, sval);
                free(sval);
            }
            free(skey);
        }

        if (! (skey && sval))
            break;
    }
}




/**
 * @brief edit set of commands in the ignore-file.
 *
 * @param[out] mdoc  variable to store JSON data that is the result of editing
 * @param[in]  argc  the number of non-optional arguments
 * @param[in]  argv  array of strings that are non-optional arguments
 * @param[in]  unset_flag  whether to only remove the contents of the ignore-file
 * @return bool  successful or not
 *
 * @note make sure new commands are appended to the end of the ignore-file.
 */
static bool edit_ignore_set(yyjson_mut_doc *mdoc, int argc, char **argv, bool unset_flag){
    assert(mdoc);
    assert(argc > 0);
    assert(argv);

    const char *key;

    do {
        key = *(argv++);
        yyjson_mut_obj_remove_key(mdoc->root, key);

        if (! (unset_flag || yyjson_mut_obj_add_null(mdoc, mdoc->root, key)))
            return false;
    } while (--argc);

    return true;
}




/**
 * @brief append an ignored command with detailed conditions to set of commands in the ignore-file.
 *
 * @param[out] mdoc  variable to store JSON data that is the result of editing
 * @param[in]  data  variable to store the results of parsing the additional settings
 * @param[in]  nothing  string meaning no arguments in the additional settings
 * @return bool  successful or not
 *
 * @note make sure new commands are appended to the end of the ignore-file.
 * @note in order to reduce waste, the duplication between first args are removed.
 */
static bool append_ignore_set(yyjson_mut_doc *mdoc, const additional_settings *data, const char *nothing){
    assert(mdoc);
    assert(data);
    assert(data->cmd_name);
    assert(nothing);

    yyjson_mut_val *mobj, *mkey, *mval;
    size_t size;

    mobj = mdoc->root;
    mkey = yyjson_mut_str(mdoc, data->cmd_name);
    mval = yyjson_mut_obj(mdoc);

    if (! yyjson_mut_obj_put(mobj, mkey, mval))
        return false;

    mobj = mval;

    assert(data->short_opts || (! data->long_opts));
    if (data->short_opts){
        mkey = yyjson_mut_strn(mdoc, additional_settings_keys[0], 10);  // short_opts
        mval = yyjson_mut_str(mdoc, data->short_opts);

        if (! yyjson_mut_obj_add(mobj, mkey, mval))
            return false;

        if (data->long_opts){
            mkey = yyjson_mut_strn(mdoc, additional_settings_keys[1], 9);  // long_opts
            mval = yyjson_mut_arr(mdoc);

            if (! yyjson_mut_obj_add(mobj, mkey, mval))
                return false;

            mkey = mval;

            long_opt_info *p_long_opt;
            for (p_long_opt = data->long_opts, size = data->long_opts_num; size--; p_long_opt++){
                if (! (mval = yyjson_mut_arr_add_arr(mdoc, mkey)))
                    return false;

                assert(p_long_opt->name);
                assert(p_long_opt->name_len > 0);
                if (! yyjson_mut_arr_add_strn(mdoc, mval, p_long_opt->name, p_long_opt->name_len))
                    return false;

                assert((p_long_opt->colons >= 0) && (p_long_opt->colons < 3));
                if (! yyjson_mut_arr_add_uint(mdoc, mval, p_long_opt->colons))
                    return false;
            }
        }
    }

    if (data->optargs){
        assert(data->optargs->root);
        mkey = yyjson_mut_strn(mdoc, additional_settings_keys[2], 7);  // optargs
        mval = yyjson_mut_val_mut_copy(mdoc, data->optargs->root);

        if (! yyjson_mut_obj_add(mobj, mkey, mval))
            return false;
    }

    if (data->first_args){
        mkey = yyjson_mut_strn(mdoc, additional_settings_keys[3], 10);  // first_args
        mval = yyjson_mut_arr(mdoc);

        if (! yyjson_mut_obj_add(mobj, mkey, mval))
            return false;

        size = data->first_args_num;
        assert(size > 0);

        char no_args[size];
        memset(no_args, false, size);

        size_t i, j;
        bool success;

        for (i = 0; i < size; i++){
            assert(data->first_args[i]);
            no_args[i] = (! xstrcmp_upper_case(data->first_args[i], nothing));

            for (j = 0; j < i; j++){
                if (no_args[i]){
                    if (no_args[j])
                        break;
                }
                else if (! (no_args[j] || strcmp(data->first_args[i], data->first_args[j])))
                    break;
            }

            if (i == j){
                success = no_args[i] ?
                    yyjson_mut_arr_add_null(mdoc, mval) :
                    yyjson_mut_arr_add_str(mdoc, mval, data->first_args[i]);

                if (! success)
                    return false;
            }
        }
    }

    return true;
}




/**
 * @brief check if the execution of the specified command should not be ignored.
 *
 * @param[in]  argc the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @param[in]  target_c  character representing the target ignore-file ('d' or 'h')
 * @return int  offset to start of the non-optional arguments if should be ignored, 0 or -1 otherwise
 *
 * @note returns a negative integer only if JSON data could not be successfully read from the ignore-file.
 * @note the contents of the ignore-file are used as much as possible, except for invalid data.
 */
int check_if_ignored(int argc, char **argv, int target_c){
    assert(argc > 0);
    assert(argv);
    assert((target_c == 'd') || (target_c == 'h'));

    int offset = 1;
    yyjson_doc *idoc;

    if (target_c == 'h')
        offset = 0;

    if ((idoc = yyjson_read_file(ignore_files[0][offset], 0, NULL, NULL))){
        yyjson_val *ictn;
        offset = 0;

        if ((ictn = yyjson_obj_get(idoc->root, *argv))){
            yyjson_obj_iter obj_iter;

            if (yyjson_obj_iter_init(ictn, &obj_iter)){
                const char * const *p_key;
                yyjson_val *ival;
                const char *short_opts;
                bool no_short_opts = false;

                p_key = additional_settings_keys;
                ival = yyjson_obj_iter_getn(&obj_iter, *(p_key++), 10);  // short_opts
                short_opts = yyjson_get_str(ival);

                if ((! short_opts) || parse_short_opts(short_opts)){
                    short_opts = "";
                    no_short_opts = true;
                }

                size_t size, tmp, long_opts_num = 0;
                yyjson_arr_iter arr_iter;
                const char *name;
                uint64_t colons;
                const int has_arg_table[3] = { no_argument, required_argument, optional_argument };

                ictn = yyjson_obj_iter_getn(&obj_iter, *(p_key++), 9);  // long_opts
                size = yyjson_arr_size(ictn);

                if (size >= INT_MAX)
                    size = INT_MAX - 1;
                struct option long_opts[size + 1], *p_long_opt;

                for (ictn = yyjson_arr_get_first(ictn); size--; ictn = unsafe_yyjson_get_next(ictn)){
                    yyjson_arr_iter_init(ictn, &arr_iter);

                    for (tmp = 0; (ival = yyjson_arr_iter_next(&arr_iter)); tmp++){
                        switch (tmp){
                            case 0:
                                if ((! (name = yyjson_get_str(ival))) || strpbrk(name, ":=?"))
                                    break;
                                continue;
                            case 1:
                                if ((! yyjson_is_uint(ival)) || ((colons = yyjson_get_uint(ival)) >= 3))
                                    break;
                            default:
                                continue;
                        }
                        break;
                    }
                    if (tmp != 2)
                        continue;

                    for (
                        tmp = long_opts_num, p_long_opt = long_opts;
                            tmp && strcmp(name, (p_long_opt++)->name);
                        tmp--
                    );
                    if (tmp)
                        continue;

                    long_opts_num++;
                    p_long_opt->name = name;
                    p_long_opt->has_arg = has_arg_table[colons];
                    p_long_opt->flag = NULL;
                    p_long_opt->val = 0;
                }

                p_long_opt = long_opts + long_opts_num;
                p_long_opt->name = NULL;
                p_long_opt->has_arg = 0;
                p_long_opt->flag = NULL;
                p_long_opt->val = 0;

                ictn = yyjson_obj_iter_getn(&obj_iter, *(p_key++), 7);  // optargs

                optind = 0;
                opterr = 0;

                int c, i;
                while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) >= 0){
                    switch (c){
                        case '?':
                            if (no_short_opts && (! long_opts_num))
                                continue;
                            goto exit;
                        case 0:
                            if (long_opts[i].has_arg == no_argument)
                                continue;
                            name = long_opts[i].name;
                            size = strlen(name);
                            break;
                        default:
                            assert(short_opts && *short_opts);
                            assert(c && (c != ':') && (c != '='));
                            name = strchr(short_opts, c);
                            assert(name && *name);
                            if (name[1] != ':')
                                continue;
                            size = 1;
                    }

                    while ((ival = yyjson_obj_getn(ictn, name, size)) && (name = yyjson_get_str(ival)))
                        size = yyjson_get_len(ival);

                    if (! check_if_contained(optarg, ival, &arr_iter))
                        goto exit;
                }

                ictn = yyjson_obj_iter_getn(&obj_iter, *p_key, 10);  // first_args

                name = NULL;
                if (argc > optind)
                    name = argv[optind];

                if (! check_if_contained(name, ictn, &arr_iter))
                    goto exit;
            }

            offset = optind;
            assert(offset);
        }

        yyjson_doc_free(idoc);
    }
    else
        offset = -1;

exit:
    optind = 0;
    opterr = 1;
    return offset;
}


/**
 * @brief check if the passed string is contained within the JSON array of expected strings.
 *
 * @param[in]  target  target string
 * @param[in]  iarr  JSON array of expected strings
 * @param[out] p_arr_iter  pointer to a JSON array iterator
 * @return bool  the resulting boolean
 */
static bool check_if_contained(const char *target, yyjson_val *ival, yyjson_arr_iter *p_arr_iter){
    assert(p_arr_iter);

    if (yyjson_arr_iter_init(ival, p_arr_iter)){
        const char *name;
        size_t name_len;

        do {
            if (! (ival = yyjson_arr_iter_next(p_arr_iter)))
                return false;

            if (yyjson_is_null(ival)){
                if (! target)
                    break;
            }
            else if (target && (name = yyjson_get_str(ival))){
                name_len = yyjson_get_len(ival);

                if (! strncmp(target, name, name_len))
                    break;
            }
        } while (true);
    }

    return true;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/


void ignore_test(void){
    fputs("ignore test\n", stdout);
}


#endif // NDEBUG