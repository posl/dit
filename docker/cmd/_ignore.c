/**
 * @file _ignore.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'ignore', that edits the ignore-file used by the dit command 'convert'.
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

#define yyjson_mut_arr_add_arg(no_arg, mdoc, marr, mval) \
    ((no_arg) ? yyjson_mut_arr_add_null(mdoc, marr) : yyjson_mut_arr_add_str(mdoc, marr, mval))

#define IG_CONDITIONS_NUM   7

#define IG_SHORT_OPTS       0
#define IG_LONG_OPTS        1
#define IG_OPTARGS          2
#define IG_FIRST_ARGS       3
#define IG_MAX_ARGC         4
#define IG_DETECT_ANYMATCH  5
#define IG_INVERT_FLAG      6


/** Data type for storing the results of option parse */
typedef struct {
    int target_c;                /** character representing the target ignore-file ('d', 'h' or 'b') */
    bool invert_flag;            /** whether to describe about the command that should be reflected */
    bool unset_flag;             /** whether to only remove the contents of the ignore-file */
    bool print_flag;             /** whether to print the contents of the ignore-file */
    int reset_flag;              /** whether to reset the ignore-file */
    bool additional_settings;    /** whether to accept the additional settings in the arguments */
    bool detect_anymatch;        /** whether to change how to use detailed conditions */
    const char *eq_name;         /** the name of the command with equivalent detailed conditions */
    int max_argc;                /** the maximum number of non-optional arguments as detailed conditions */
    char *nothing;               /** string meaning no arguments in the additional settings */
} ig_opts;


/** Data type that is applied to the elements of the array of long options */
typedef struct {
    const char *name;    /** long option name (may not be null-terminated) */
    size_t len;          /** the length of long option name */
    int colons;          /** the number of colons meaning whether the long option has an argument */
} long_opt_info;


/** Data type that is applied to the elements of the array of optargs */
typedef struct {
    const char *name;    /** option name (may not be null-terminated) */
    size_t len;          /** the length of option name */
    const char *arg;     /** the argument to be specified for the option */
} optarg_info;


/** Data type for storing the results of parsing the additional settings */
typedef struct {
    const char *cmd_name;        /** command name */
    const char *short_opts;      /** short options */

    long_opt_info *long_opts;    /** array of long options */
    size_t long_opts_num;        /** the current number of long options */
    size_t long_opts_max;        /** the current maximum length of the array */

    yyjson_mut_doc *optargs;     /** JSON data representing the arguments to be specified for some options */

    char **first_args;           /** array of the first non-optional arguments */
    size_t first_args_num;       /** the current number of the first non-optional arguments */
} additional_settings;


static int parse_opts(int argc, char **argv, ig_opts *opt);
static int ignore_contents(int argc, char **argv, ig_opts *opt);

static bool parse_additional_settings(int argc, char **argv, additional_settings *data, const char *nothing);
static int parse_short_opts(const char *target);
static int parse_long_opts(const char *target, additional_settings *data);
static int append_long_opt(additional_settings *data, const char *name, size_t len, int colons);
static int parse_optarg(const char *target, additional_settings *data, optarg_info *p_info);
static bool append_optarg(additional_settings *data, const optarg_info *p_info, const char *nothing);
static yyjson_mut_val *append_opt(yyjson_mut_doc *mdoc, const char *name, size_t len, yyjson_mut_val *mval);

static void display_ignore_set(const yyjson_doc *idoc, int argc, char **argv, yyjson_write_err *write_err);
static bool edit_ignore_set(yyjson_mut_doc *mdoc, int argc, char **argv, const ig_opts *opt);
static bool append_ignore_set(yyjson_mut_doc *mdoc, const additional_settings *data, const ig_opts *opt);

static yyjson_val *get_setting_entity(yyjson_val *iobj, const char *name, size_t len);
static bool check_if_contained(const char *target, yyjson_val *ival);


extern const char * const target_args[ARGS_NUM];


/** 2D array of ignore-file name (axis 1: whether it is a base-file, axis 2: whether to target 'd' or 'h') */
const char * const ignore_files[2][2] = {
    { IGNORE_FILE_H,      IGNORE_FILE_D,     },
    { IGNORE_FILE_BASE_H, IGNORE_FILE_BASE_D }
};

/** array of keys pointing to each detailed condition */
static const char * const conds_keys[IG_CONDITIONS_NUM] = {
    "short_opts",
    "long_opts",
    "optargs",
    "first_args",
    "max_argc",
    "detect_anymatch",
    "invert_flag"
};




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief edit the ignore-file used by the dit command 'convert'.
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

    const char *short_opts = "dhinprAX";

    int flag;
    const struct option long_opts[] = {
        { "invert",              no_argument,        NULL, 'i' },
        { "unset",               no_argument,        NULL, 'n' },
        { "print",               no_argument,        NULL, 'p' },
        { "reset",               no_argument,        NULL, 'r' },
        { "additional-settings", no_argument,        NULL, 'A' },
        { "detect-anymatch",     no_argument,        NULL, 'X' },
        { "help",                no_argument,        NULL,  1  },
        { "equivalent-to",       required_argument, &flag, 'E' },
        { "max-argc",            required_argument, &flag, 'M' },
        { "same-as-nothing",     required_argument, &flag, 'S' },
        { "target",              required_argument, &flag, 'T' },
        {  0,                     0,                  0,    0  }
    };

    opt->target_c = '\0';
    opt->invert_flag = false;
    opt->unset_flag = false;
    opt->print_flag = false;
    opt->reset_flag = false;
    opt->additional_settings = false;
    opt->detect_anymatch = false;
    opt->eq_name = NULL;
    opt->max_argc = -1;
    opt->nothing = NULL;

    int c, i, errcode = 'O';

    while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) >= 0)
        switch (c){
            case 'd':
                assign_both_or_either(opt->target_c, 'h', 'b', 'd');
                break;
            case 'h':
                assign_both_or_either(opt->target_c, 'd', 'b', 'h');
                break;
            case 'i':
                opt->invert_flag = true;
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
            case 'X':
                opt->detect_anymatch = true;
                break;
            case 1:
                ignore_manual();
                return POSSIBLE_ERROR;
            case 0:
                switch (flag){
                    case 'E':
                        opt->eq_name = optarg;
                        continue;
                    case 'M':
                        if ((c = receive_positive_integer(optarg, NULL)) >= 0){
                            opt->max_argc = c;
                            continue;
                        }
                        errcode = 'N';
                        c = 1;
                        break;
                    case 'S':
                        if (optarg && (! strchr(optarg, '='))){
                            opt->nothing = optarg;
                            continue;
                        }
                        c = 1;
                        break;
                    default:
                        assert(flag == 'T');
                        if ((c = receive_expected_string(optarg, target_args, ARGS_NUM, 2)) >= 0){
                            opt->target_c = *(target_args[c]);
                            continue;
                        }
                }
                xperror_invalid_arg(errcode, c, long_opts[i].name, optarg);
                if (c < 0)
                    xperror_valid_args(target_args, ARGS_NUM);
            default:
                return ERROR_EXIT;
        }

    if (opt->invert_flag && (! opt->additional_settings))
        opt->unset_flag = true;

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
 * @param[in]  argv  array of strings that are non-optional arguments
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
            if (opt->unset_flag || opt->print_flag || opt->eq_name){
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
                                edit_ignore_set(mdoc, argc, argv, opt) :
                                append_ignore_set(mdoc, &data, opt);

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
    assert(argc > 0);
    assert(argv);
    assert(data);
    assert(nothing);

    const char *errdesc = NULL;

    if (! (data->cmd_name = *argv))
        goto errexit;

    if (--argc){
        int phase = 0;
        yyjson_mut_doc *mdoc;
        yyjson_mut_val *mobj;
        optarg_info info[argc];
        size_t optargs_num = 0;

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
                                assert(data->long_opts_num < INT_MAX);
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
    }

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
                        if (data->long_opts_num < INT_MAX){
                            name = target;
                            colons = 0;
                            count = 1;
                            continue;
                        }
                    case POSSIBLE_ERROR:
                        break;
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
 * @param[in]  len  the length of option name
 * @param[in]  colons  the number of colons meaning whether the long option has an argument
 * @return int  0 (success), 1 (duplicated long option) or -1 (unexpected error)
 *
 * @note can append virtually unlimited number of long options.
 * @note detects duplication of long options as an error to avoid unexpected errors in 'getopt_long'.
 */
static int append_long_opt(additional_settings *data, const char *name, size_t len, int colons){
    assert(data);
    assert(name);
    assert((colons >= 0) && (colons < 3));

    if (len){
        long_opt_info *p_long_opt;
        size_t tmp;

        for (p_long_opt = data->long_opts, tmp = data->long_opts_num; tmp--; p_long_opt++)
            if ((len == p_long_opt->len) && (! strncmp(name, p_long_opt->name, len)))
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
        p_long_opt->len = len;
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
        yyjson_mut_val *mval = NULL;

        p_info->name = NULL;
        exit_status = UNEXPECTED_ERROR;

        for (; *target; target++){
            if (*target == '='){
                if (count){
                    assert(name);

                    if (! p_info->name){
                        p_info->name = name;
                        p_info->len = count;
                        mval = yyjson_mut_strn(data->optargs, name, count);
                    }
                    else {
                        assert(mval);
                        mval = append_opt(data->optargs, name, count, mval);
                    }

                    if (! mval){
                        exit_status--;
                        goto exit;
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
    assert(p_info->len > 0);
    assert(p_info->arg);
    assert(nothing);

    yyjson_mut_val *marr, *mval;
    bool no_arg;
    size_t idx, max;
    const char *name;

    marr = append_opt(data->optargs, p_info->name, p_info->len, NULL);
    assert(yyjson_mut_is_arr(marr));

    no_arg = (! xstrcmp_upper_case(p_info->arg, nothing));

    yyjson_mut_arr_foreach(marr, idx, max, mval)
        if (yyjson_mut_is_null(mval)){
            if (no_arg)
                return true;
        }
        else if (! no_arg){
            name = yyjson_mut_get_str(mval);
            assert(name);

            if (! strcmp(name, p_info->arg))
                return true;
        }

    return yyjson_mut_arr_add_arg(no_arg, data->optargs, marr, p_info->arg);
}


/**
 * @brief append a JSON pair keyed by the option name to represent its arguments.
 *
 * @param[out] mdoc  JSON data representing the arguments to be specified for some options
 * @param[in]  name  target option name (may not be null-terminated)
 * @param[in]  len  the length of target option name
 * @param[in]  mval  JSON value corresponding to the key or NULL
 * @return yyjson_mut_val*  added JSON value or NULL
 *
 * @note if 'mval' is NULL, it uses a json array for storing the arguments to be specified for some options
 */
static yyjson_mut_val *append_opt(yyjson_mut_doc *mdoc, const char *name, size_t len, yyjson_mut_val *mval){
    assert(mdoc);
    assert(name);
    assert(len > 0);
    assert((! mval) || yyjson_mut_is_str(mval));

    yyjson_mut_val *mkey;

    do {
        if ((mkey = yyjson_mut_obj_getn(mdoc->root, name, len))){
            if ((name = yyjson_mut_get_str(mkey))){
                len = yyjson_mut_get_len(mkey);
                continue;
            }
            assert(! mval);
            mval = mkey;
        }
        else {
            if (! mval)
                mval = yyjson_mut_arr(mdoc);
            else if (! yyjson_mut_equals_strn(mval, name, len))
                mval = yyjson_mut_val_mut_copy(mdoc, mval);
            else
                break;

            mkey = yyjson_mut_strn(mdoc, name, len);

            if (! yyjson_mut_obj_add(mdoc->root, mkey, mval))
                return NULL;
        }
        break;
    } while (true);

    return mval;
}




/******************************************************************************
    * Individual Functions of the dit command 'ignore'
******************************************************************************/


/**
 * @brief display the contents of the ignore-file on screen.
 *
 * @param[in]  idoc  immutable JSON data that is the contents of the ignore-file
 * @param[in]  argc  the number of non-optional arguments
 * @param[in]  argv  array of strings that are non-optional arguments
 * @param[out] err  variable to store the error information for JSON writer
 *
 * @note the order of command names specified in non-optional arguments make sense.
 */
static void display_ignore_set(const yyjson_doc *idoc, int argc, char **argv, yyjson_write_err *err){
    assert(idoc);
    assert(argv);
    assert(err);

    size_t size;

    if ((size = yyjson_obj_size(idoc->root))){
        yyjson_val *ikey;
        const char *cmd_name = NULL, *key;
        int i;
        char *jsons[2];
        bool success;

        if (argc < 0)
            argc = 0;

        do {
            if (argc && (! (cmd_name = *(argv++))))
                continue;

            for (ikey = idoc->root + 1; size--; ikey = unsafe_yyjson_get_next(ikey + 1)){
                key = yyjson_get_str(ikey);
                assert(key);

                if (cmd_name && strcmp(cmd_name, key))
                    continue;

                for (i = 0; i < 2; i++)
                    if (! (jsons[i] = yyjson_val_write_opts(ikey + i, YYJSON_WRITE_PRETTY, NULL, NULL, err)))
                        break;

                if ((success = (i == 2)))
                    fprintf(stdout, "%s: %s\n", jsons[0], jsons[1]);

                while (i--)
                    free(jsons[i]);

                if (! success)
                    return;
                if (cmd_name)
                    break;
            }
        } while (--argc > 0);
    }
}




/**
 * @brief edit set of commands in the ignore-file.
 *
 * @param[out] mdoc  variable to store JSON data that is the result of editing
 * @param[in]  argc  the number of non-optional arguments
 * @param[in]  argv  array of strings that are non-optional arguments
 * @param[in]  opt  variable to store the results of option parse
 * @return bool  successful or not
 *
 * @note make sure new commands are appended to the end of the ignore-file.
 */
static bool edit_ignore_set(yyjson_mut_doc *mdoc, int argc, char **argv, const ig_opts *opt){
    assert(mdoc);
    assert(argc > 0);
    assert(argv);
    assert(opt);

    const char *key;
    yyjson_mut_val *mkey, *mval;

    do {
        key = *(argv++);
        yyjson_mut_obj_remove_key(mdoc->root, key);

        if (! opt->unset_flag){
            mkey = yyjson_mut_str(mdoc, key);
            mval = (! opt->eq_name) ? yyjson_mut_null(mdoc) : yyjson_mut_str(mdoc, opt->eq_name);

            if (! yyjson_mut_obj_add(mdoc->root, mkey, mval))
                return false;
        }
    } while (--argc);

    return true;
}




/**
 * @brief append an ignored command with detailed conditions to set of commands in the ignore-file.
 *
 * @param[out] mdoc  variable to store JSON data that is the result of editing
 * @param[in]  data  variable to store the results of parsing the additional settings
 * @param[in]  opt  variable to store the results of option parse
 * @return bool  successful or not
 *
 * @note make sure new commands are appended to the end of the ignore-file.
 * @note in order to reduce waste, the duplication between first args are removed.
 */
static bool append_ignore_set(yyjson_mut_doc *mdoc, const additional_settings *data, const ig_opts *opt){
    assert(mdoc);
    assert(data);
    assert(data->cmd_name);
    assert(opt);

    yyjson_mut_val *conds[IG_CONDITIONS_NUM], *mctn, *mkey, *mval;
    int i, conds_ids[IG_CONDITIONS_NUM], conds_count = 0;
    size_t size;

    i = opt->max_argc;

    assert(data->short_opts || (! data->long_opts));

    if (data->short_opts){
        conds[conds_count] = yyjson_mut_str(mdoc, data->short_opts);
        conds_ids[conds_count++] = IG_SHORT_OPTS;

        if (data->long_opts){
            conds[conds_count] = (mctn = yyjson_mut_obj(mdoc));
            conds_ids[conds_count++] = IG_LONG_OPTS;

            long_opt_info *p_long_opt;
            for (p_long_opt = data->long_opts, size = data->long_opts_num; size--; p_long_opt++){
                assert(p_long_opt->name);
                assert(p_long_opt->len > 0);
                mkey = yyjson_mut_strn(mdoc, p_long_opt->name, p_long_opt->len);

                assert((p_long_opt->colons >= 0) && (p_long_opt->colons < 3));
                mval = yyjson_mut_uint(mdoc, p_long_opt->colons);

                if (! yyjson_mut_obj_add(mctn, mkey, mval))
                    return false;
            }
        }
    }

    if (data->optargs){
        assert(data->optargs->root);
        conds[conds_count] = yyjson_mut_val_mut_copy(mdoc, data->optargs->root);
        conds_ids[conds_count++] = IG_OPTARGS;
    }

    if (data->first_args){
        size = data->first_args_num;
        assert(size > 0);

        size_t s, t, args_count = 0;
        const char *args_array[size];
        bool first_null = true;

        for (s = 0; s < size; s++){
            assert(data->first_args[s]);

            if (xstrcmp_upper_case(data->first_args[s], opt->nothing)){
                for (t = 0; t < s; t++)
                    if (! strcmp(data->first_args[s], data->first_args[t]))
                        break;

                if (s == t)
                    args_array[args_count++] = data->first_args[s];
            }
            else if (first_null){
                args_array[args_count++] = NULL;
                first_null = false;
            }
        }

        assert((args_count > 0) && (args_count <= size));

        if (opt->detect_anymatch || (args_count > 1) || *args_array){
            conds[conds_count] = (mctn = yyjson_mut_arr(mdoc));
            conds_ids[conds_count++] = IG_FIRST_ARGS;

            for (const char * const *p_arg = args_array; args_count--; p_arg++)
                if (! yyjson_mut_arr_add_arg((! *p_arg), mdoc, mctn, *p_arg))
                    return false;
        }
        else
            i = 0;
    }

    if (i >= 0){
        conds[conds_count] = yyjson_mut_uint(mdoc, i);
        conds_ids[conds_count++] = IG_MAX_ARGC;
    }

    assert(IG_DETECT_ANYMATCH == (IG_MAX_ARGC + 1));
    assert(IG_INVERT_FLAG == (IG_MAX_ARGC + 2));

    for (i = 0; i < 2;)
        if ((! i++) ? opt->detect_anymatch : opt->invert_flag){
            conds[conds_count] = yyjson_mut_true(mdoc);
            conds_ids[conds_count++] = i + IG_MAX_ARGC;
        }

    mctn = yyjson_mut_obj(mdoc);

    for (i = 0; i < conds_count; i++)
        if (! yyjson_mut_obj_add_val(mdoc, mctn, conds_keys[conds_ids[i]], conds[i]))
            return false;

    mkey = yyjson_mut_str(mdoc, data->cmd_name);

    if (! yyjson_mut_obj_put(mdoc->root, mkey, mctn))
        return false;

    return true;
}




/******************************************************************************
    * Function used in separate files
******************************************************************************/


/**
 * @brief check if the execution of the specified command should be ignored.
 *
 * @param[in]  idoc  immutable JSON data that is the contents of the ignore-file
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return bool  the resulting boolean
 *
 * @note the contents of the ignore-file are used as much as possible while excluding invalid data.
 */
bool check_if_ignored(const yyjson_doc *idoc, int argc, char **argv){
    assert(idoc);
    assert(argc > 0);
    assert(argv);

    char *key;
    yyjson_val *ival;
    bool result = false;
    yyjson_obj_iter iter;

    for (key = *argv;; key = get_suffix(key, '/', false))
        if ((ival = get_setting_entity(idoc->root, key, strlen(key))))
            break;
        else if (! *key)
            goto exit;

    if (yyjson_obj_iter_init(ival, &iter)){
        int i, c;
        yyjson_val *conds[IG_CONDITIONS_NUM];
        const char *name;
        size_t size = 1, remain, long_opts_num = 0;
        bool no_short_opts = true;
        uint64_t colons;
        const int has_arg_table[3] = { no_argument, required_argument, optional_argument };
        unsigned int detect_anymatch, invert_flag, matched = false;

        for (i = 0; i < IG_CONDITIONS_NUM; i++)
            conds[i] = yyjson_obj_iter_get(&iter, conds_keys[i]);

        ival = conds[IG_SHORT_OPTS];

        if ((name = yyjson_get_str(ival)) && (! parse_short_opts(name))){
            size += yyjson_get_len(ival);
            no_short_opts = false;
        }
        else
            name = "";

        char short_opts[size + 1];
        *short_opts = ':';
        memcpy((short_opts + 1), name, size);

        ival = conds[IG_LONG_OPTS];
        size = yyjson_obj_size(ival);

        if (size >= INT_MAX)
            size = INT_MAX - 1;

        struct option long_opts[size + 1];
        memset(long_opts, 0, (sizeof(struct option) * (size + 1)));

        for (ival++; size--; ival = unsafe_yyjson_get_next(ival)){
            if ((! (name = yyjson_get_str(ival++))) || (! *name) || strpbrk(name, ":=?") ||
                (! yyjson_is_uint(ival)) || ((colons = yyjson_get_uint(ival)) >= 3))
                    continue;

            for (remain = 0; (remain < long_opts_num) && strcmp(name, long_opts[remain].name); remain++);

            if (remain == long_opts_num){
                long_opts[long_opts_num].name = name;
                long_opts[long_opts_num++].has_arg = has_arg_table[colons];
            }
        }

        optind = 0;
        ival = conds[IG_OPTARGS];

        detect_anymatch = yyjson_get_bool(conds[IG_DETECT_ANYMATCH]);
        invert_flag = yyjson_get_bool(conds[IG_INVERT_FLAG]);
        result = detect_anymatch ^ invert_flag;

        while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) >= 0){
            switch (c){
                case ':':
                case '?':
                    if ((no_short_opts && (! long_opts_num)) || detect_anymatch)
                        continue;
                    assert(detect_anymatch == matched);
                    goto exit;
                case 0:
                    name = long_opts[i].name;
                    size = strlen(name);
                    matched = (long_opts[i].has_arg == no_argument);
                    break;
                default:
                    name = strchr(short_opts, c);
                    size = 1;
                    matched = (name[1] != ':');
            }

            assert(name && *name);
            assert(size > 0);

            if (! matched)
                matched = check_if_contained(optarg, get_setting_entity(ival, name, size));

            if (detect_anymatch == matched)
                goto exit;
        }

        argc -= optind;
        argv += optind;
        ival = conds[IG_FIRST_ARGS];

        if (yyjson_is_arr(ival)){
            matched = check_if_contained(((argc > 0) ? *argv : NULL), ival);

            if (detect_anymatch == matched)
                goto exit;
        }

        ival = conds[IG_MAX_ARGC];

        if (yyjson_is_uint(ival)){
            matched = (argc <= yyjson_get_uint(ival));

            if (detect_anymatch == matched)
                goto exit;
        }
    }

    result = (! result);
exit:
    return result;
}




/**
 * @brief get the entity of the setting by following the link via the key of the JSON object.
 *
 * @param[in]  iobj  immutable JSON object
 * @param[in]  name  the first key (may not be null-terminated)
 * @param[in]  len  the length of the first key
 * @return yyjson_val*  the resulting immutable JSON value or NULL
 */
static yyjson_val *get_setting_entity(yyjson_val *iobj, const char *name, size_t len){
    assert(iobj);
    assert(name);

    yyjson_val *ival;

    while ((ival = yyjson_obj_getn(iobj, name, len)) && (name = yyjson_get_str(ival)))
        len = yyjson_get_len(ival);

    assert(! yyjson_is_str(ival));

    return ival;
}


/**
 * @brief check if the passed string is contained within the JSON array of expected strings.
 *
 * @param[in]  target  target string
 * @param[in]  ival  JSON array of expected strings
 * @return bool  the resulting boolean
 */
static bool check_if_contained(const char *target, yyjson_val *ival){
    yyjson_arr_iter iter;

    if (yyjson_arr_iter_init(ival, &iter)){
        const char *name;

        do {
            if (! (ival = yyjson_arr_iter_next(&iter)))
                return false;

            if (yyjson_is_null(ival)){
                if (! target)
                    break;
            }
            else if (target && (name = yyjson_get_str(ival)) && (! strcmp(target, name)))
                break;
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