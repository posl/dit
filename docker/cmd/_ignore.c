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

#define yyjson_mut_arr_add_arg(no_arg, mdoc, marr, arg) \
    ((no_arg) ? yyjson_mut_arr_add_null(mdoc, marr) : yyjson_mut_arr_add_str(mdoc, marr, arg))

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


/** Data type for storing the results of parsing the additional settings */
typedef struct {
    const char *cmd_name;          /** command name */
    const char *short_opts;        /** short options */
    yyjson_mut_doc *pool;          /** memory pool for below JSON values */
    yyjson_mut_val *long_opts;     /** long options (JSON object) */
    yyjson_mut_val *optargs;       /** optargs (JSON object) */
    yyjson_mut_val *first_args;    /** first args (JSON array) */
} ig_conds;


/** Data type that is applied to the elements of the array of optargs */
typedef struct {
    const char *name;    /** option name (may not be null-terminated) */
    size_t len;          /** the length of option name */
    const char *arg;     /** the argument to be specified for the option */
} optarg_info;


static int parse_opts(int argc, char **argv, ig_opts *opt);
static int ignore_contents(int argc, char **argv, ig_opts *opt);

static bool parse_additional_settings(ig_conds *data, int argc, char **argv, ig_opts *opt);
static int parse_short_opts(const char *target);
static int parse_long_opts(const char *target, ig_conds *data);
static int append_long_opt(ig_conds *data, const char *name, size_t len, int colons);
static int parse_optargs(const char *target, ig_conds *data, optarg_info *p_info);
static bool append_optarg(ig_conds *data, const optarg_info *p_info, const char *nothing);
static bool append_first_args(ig_conds *data, int argc, char **argv, ig_opts *opt);

static void display_ignore_set(int argc, char **argv, yyjson_write_err *err);
static bool edit_ignore_set(yyjson_mut_doc *mdoc, int argc, char **argv, const ig_opts *opt);
static bool append_ignore_set(yyjson_mut_doc *mdoc, const ig_conds *data, const ig_opts *opt);

static yyjson_val *get_setting_entity(yyjson_val *iobj, const char *name, size_t len);
static bool check_if_contained(const char *target, yyjson_val *ival);


extern const char * const target_args[ARGS_NUM];


/** 2D array of ignore-file name (axis 1: whether it is a base-file, axis 2: whether to target 'd' or 'h') */
static const char * const ignore_files[2][2] = {
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

/** array of integers representing whether a long option takes an argument */
static const int has_arg_table[3] = { no_argument, required_argument, optional_argument };


/** immutable JSON data that is the contents of the ignore-file (to use 'check_if_ignored' as callback) */
static yyjson_doc *idoc = NULL;




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

    assert(opt->reset_flag == ((bool) opt->reset_flag));

    if (opt->target_c){
        if (opt->invert_flag && (! opt->additional_settings))
            opt->unset_flag = true;

        if (opt->nothing){
            for (char *tmp = opt->nothing; *tmp; tmp++)
                *tmp = toupper(*tmp);
        }
        else
            opt->nothing = "NONE";

        return SUCCESS;
    }

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

    ig_conds data = {0};
    int offset = 1, exit_status = FAILURE;
    const char *file_name, *errmsg;
    bool success;

    yyjson_read_err read_err;
    yyjson_mut_doc *mdoc;
    yyjson_write_err write_err;

    if (argc > 0){
        if (opt->additional_settings){
            if (opt->unset_flag || opt->print_flag || opt->eq_name){
                argc = 1;
                opt->additional_settings = false;
            }
            else if (! parse_additional_settings(&data, argc, argv, opt))
                goto exit;
        }
    }
    else if (! opt->reset_flag)
        opt->print_flag = true;

    if (opt->target_c == 'h')
        offset = 0;
    exit_status = SUCCESS;

    do {
        file_name = ignore_files[opt->reset_flag][offset];

        if ((idoc = yyjson_read_file(file_name, 0, NULL, &read_err))){
            success = true;
            mdoc = NULL;
            write_err.msg = NULL;

            if (opt->print_flag){
                if (opt->target_c == 'b')
                    print_target_repr(offset);
                display_ignore_set(argc, argv, &write_err);
            }
            else {
                file_name = ignore_files[0][offset];

                if (argc <= 0){
                    assert(opt->reset_flag);
                    yyjson_write_file(file_name, idoc, IG_WRITER_FLAG, NULL, &write_err);
                }
                else {
                    success = false;
                    mdoc = yyjson_doc_mut_copy(idoc, NULL);
                }
            }

            yyjson_doc_free(idoc);
            idoc = NULL;

            if (mdoc){
                success = (! opt->additional_settings) ?
                    edit_ignore_set(mdoc, argc, argv, opt) : append_ignore_set(mdoc, &data, opt);

                if (success)
                    yyjson_mut_write_file(file_name, mdoc, IG_WRITER_FLAG, NULL, &write_err);
                yyjson_mut_doc_free(mdoc);
            }

            if (! success)
                exit_status = FAILURE;
            errmsg = write_err.msg;
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

exit:
    yyjson_mut_doc_free(data.pool);

    return exit_status;
}




/******************************************************************************
    * Argument Parser
******************************************************************************/


/**
 * @brief parse non-optional arguments as the additional settings.
 *
 * @param[out] data  variable to store the results of parsing the additional settings
 * @param[in]  argc  the number of non-optional arguments
 * @param[in]  argv  array of strings that are non-optional arguments
 * @param[out] opt  variable to store the results of option parse
 * @return int  successful or not
 */
static bool parse_additional_settings(ig_conds *data, int argc, char **argv, ig_opts *opt){
    assert(data);
    assert(argc > 0);
    assert(argv);
    assert(opt);
    assert(opt->nothing);

    bool success;

    if ((success = (data->cmd_name = *argv)) && --argc){
        int phase = 0, optargs_num = 0, first_args_num = 0;
        const char *errdesc = NULL;

        optarg_info info[argc];
        char *array[argc];

        do
            if (*(++argv)){
                if (! phase){
                    phase = 1;

                    switch (parse_short_opts(*argv)){
                        case SUCCESS:
                            data->short_opts = *argv;
                            continue;
                        case POSSIBLE_ERROR:
                            phase = 2;
                            break;
                        case UNEXPECTED_ERROR:
                            errdesc = "short opts";
                            goto next;
                    }
                }
                if (! (data->pool || (data->pool = yyjson_mut_doc_new(NULL))))
                    return false;

                switch (phase){
                    case 1:
                        switch (parse_long_opts(*argv, data)){
                            case SUCCESS:
                                assert(data->long_opts);
                                continue;
                            case POSSIBLE_ERROR:
                                phase = 2;
                                break;
                            case UNEXPECTED_ERROR:
                                errdesc = "long opts";
                                goto next;
                            default:
                                return false;
                        }
                    case 2:
                        assert(*argv && **argv);
                        if (strchrcmp(*argv, '=')){
                            phase = 4;
                            continue;
                        }
                        if (! (data->optargs = yyjson_mut_obj(data->pool)))
                            return false;
                        phase = 3;
                    case 3:
                        switch (parse_optargs(*argv, data, (info + optargs_num))){
                            case SUCCESS:
                                optargs_num++;
                                continue;
                            case POSSIBLE_ERROR:
                                phase = 4;
                                break;
                            case UNEXPECTED_ERROR:
                                errdesc = "optarg";
                                goto next;
                            default:
                                return false;
                        }
                    case 4:
                        if (! strchr(*argv, '=')){
                            array[first_args_num++] = *argv;
                            continue;
                        }
                        errdesc = "first arg";
                }

            next:
                assert(errdesc);
                xperror_invalid_arg('C', 1, errdesc, *argv);

                success = false;
                errdesc = NULL;
            }
        while (--argc);

        if (success){
            for (optarg_info *p_info = info; optargs_num--; p_info++)
                if (! append_optarg(data, p_info, opt->nothing))
                    return false;

            if ((first_args_num > 0) && (! append_first_args(data, first_args_num, array, opt)))
                success = false;
        }
    }

    return success;
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
static int parse_long_opts(const char *target, ig_conds *data){
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
                if ((! count) || (i && (! colons))){
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
 * @brief append a long option as the additional settings.
 *
 * @param[out] data  variable to store the results of parsing the additional settings
 * @param[in]  name  option name
 * @param[in]  len  the length of option name
 * @param[in]  colons  the number of colons meaning whether the long option has an argument
 * @return int  0 (success), 1 (duplicated long option) or -1 (unexpected error)
 *
 * @note detects duplication of long options as an error to avoid errors in glibc 'getopt_long' function.
 */
static int append_long_opt(ig_conds *data, const char *name, size_t len, int colons){
    assert(data);
    assert(data->pool);
    assert(name);
    assert(len);
    assert((colons >= 0) && (colons < 3));

    yyjson_mut_val *mkey, *mval;

    if (data->long_opts){
        size_t idx, max;
        const char *key;

        yyjson_mut_obj_foreach(data->long_opts, idx, max, mkey, mval){
            key = yyjson_mut_get_str(mkey);
            assert(key);

            if ((len == yyjson_mut_get_len(mkey)) && (! strncmp(name, key, len)))
                return POSSIBLE_ERROR;
        }

        if (max > INT_MAX)
            return POSSIBLE_ERROR;
    }
    else
        data->long_opts = yyjson_mut_obj(data->pool);

    mkey = yyjson_mut_strn(data->pool, name, len);
    mval = yyjson_mut_uint(data->pool, colons);

    if (! yyjson_mut_obj_add(data->long_opts, mkey, mval))
        return UNEXPECTED_ERROR;

    return SUCCESS;
}




/**
 * @brief parse the target string as optargs.
 *
 * @param[in]  target  target string
 * @param[out] data  variable to store the results of parsing the additional settings
 * @param[out] p_info  variable to store the results of this parsing
 * @return int  0 (success), 1 (skip this parsing), -1 (invalid optarg) or -2 (unexpected error)
 *
 * @note in order to analyze which options are equivalent first, construct JSON data in 2 steps.
 * @note this function analyzes which options are equivalent and extracts the data for the next step.
 */
static int parse_optargs(const char *target, ig_conds *data, optarg_info *p_info){
    assert(target);
    assert(data);
    assert(data->pool);
    assert(data->optargs);
    assert(p_info);

    int exit_status = POSSIBLE_ERROR;

    if (strchr(target, '=')){
        size_t count = 0;
        const char *name = NULL;
        yyjson_mut_val *mkey, *mval;

        p_info->name = NULL;
        exit_status = UNEXPECTED_ERROR;

        for (; *target; target++){
            if (*target == '='){
                if (count){
                    assert(name);

                    while ((mkey = yyjson_mut_obj_getn(data->optargs, name, count))){
                        name = yyjson_mut_get_str(mkey);
                        count = yyjson_mut_get_len(mkey);
                        assert(name);
                        assert(count);
                    }

                    if (! p_info->name){
                        p_info->name = name;
                        p_info->len = count;
                    }
                    else if ((count != p_info->len) || strncmp(name, p_info->name, count)){
                        mkey = yyjson_mut_strn(data->pool, name, count);
                        mval = yyjson_mut_strn(data->pool, p_info->name, p_info->len);

                        if (! yyjson_mut_obj_add(data->optargs, mkey, mval))
                            return exit_status - 1;
                    }

                    count = 0;
                    continue;
                }
                return exit_status;
            }
            else if (! count++)
                name = target;
        }

        p_info->arg = count ? name : target;
        exit_status = SUCCESS;
    }

    return exit_status;
}


/**
 * @brief append an optarg as the additional settings.
 *
 * @param[out] data  variable to store the results of parsing the additional settings
 * @param[in]  p_info  variable containing the results of parsing in the previous step
 * @param[in]  nothing  string meaning no arguments in the additional settings
 * @return bool  successful or not
 *
 * @note in order to reduce waste, the duplication between optargs are removed.
 */
static bool append_optarg(ig_conds *data, const optarg_info *p_info, const char *nothing){
    assert(data);
    assert(data->pool);
    assert(data->optargs);
    assert(p_info);
    assert(p_info->name);
    assert(p_info->len);
    assert(p_info->arg);
    assert(nothing);

    const char *name;
    size_t size;
    bool no_arg;
    yyjson_mut_val *mval, *marr;

    name = p_info->name;
    size = p_info->len;
    no_arg = (! xstrcmp_upper_case(p_info->arg, nothing));

    while ((mval = yyjson_mut_obj_getn(data->optargs, name, size))){
        name = yyjson_mut_get_str(mval);
        size = yyjson_mut_get_len(mval);

        if (! name)
            break;
    }

    if (mval){
        assert(! name);
        assert(size);

        marr = mval;
        assert(yyjson_mut_is_arr(marr));
        assert(size == yyjson_mut_arr_size(marr));

        for (mval = yyjson_mut_arr_get_first(marr); size; mval = mval->next, size--)
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
    }
    else {
        assert(name);
        assert(size);

        mval = yyjson_mut_strn(data->pool, name, size);
        marr = yyjson_mut_arr(data->pool);

        if (! yyjson_mut_obj_add(data->optargs, mval, marr))
            return false;
    }

    return yyjson_mut_arr_add_arg(no_arg, data->pool, marr, p_info->arg);
}




/**
 * @brief append first args as the additional settings.
 *
 * @param[out] data  variable to store the results of parsing the additional settings
 * @param[in]  argc  the length of the argument vector below
 * @param[out] argv  array of strings corresponding to first args
 * @param[out] opt  variable to store the results of option parse
 * @return bool  successful or not
 *
 * @note in order to reduce waste, the duplication between first args are removed.
 */
static bool append_first_args(ig_conds *data, int argc, char **argv, ig_opts *opt){
    assert(data);
    assert(data->pool);
    assert(! data->first_args);
    assert(argc > 0);
    assert(argv);
    assert(opt);
    assert(opt->nothing);

    int i, j, first_args_num = 0;
    bool first_null = true;

    for (i = 0; i < argc; i++){
        assert(argv[i]);

        if (xstrcmp_upper_case(argv[i], opt->nothing)){
            for (j = 0; j < i; j++)
                if (argv[j] && (! strcmp(argv[i], argv[j])))
                    break;

            if (i == j)
                argv[first_args_num++] = argv[i];
        }
        else if (first_null){
            first_null = false;
            argv[first_args_num++] = NULL;
        }
    }

    if (opt->detect_anymatch || (first_args_num > 1) || *argv){
        data->first_args = yyjson_mut_arr(data->pool);

        for (i = 0; i < first_args_num; i++)
            if (! yyjson_mut_arr_add_arg((! argv[i]), data->pool, data->first_args, argv[i]))
                return false;
    }
    else
        opt->max_argc = 0;

    return true;
}




/******************************************************************************
    * Individual Functions of the dit command 'ignore'
******************************************************************************/


/**
 * @brief display the contents of the ignore-file on screen.
 *
 * @param[in]  argc  the number of non-optional arguments
 * @param[in]  argv  array of strings that are non-optional arguments
 * @param[out] err  variable to store the error information for JSON writer
 *
 * @note the order of command names specified in non-optional arguments make sense.
 */
static void display_ignore_set(int argc, char **argv, yyjson_write_err *err){
    assert(idoc);
    assert(argv);
    assert(err);

    size_t size;

    if ((size = yyjson_obj_size(idoc->root))){
        const char *key;
        size_t remain;
        yyjson_val *ikey;
        int i;
        char *jsons[2];
        bool success;

        if (argc < 0)
            argc = 0;

        do {
            if (argc && (! (key = *(argv++))))
                continue;

            for (remain = size, ikey = idoc->root + 1; remain--; ikey = unsafe_yyjson_get_next(ikey + 1)){
                assert(yyjson_is_str(ikey));

                if (argc && strcmp(key, yyjson_get_str(ikey)))
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
                if (argc)
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

    do
        if ((key = *(argv++))){
            yyjson_mut_obj_remove_key(mdoc->root, key);

            if (! opt->unset_flag){
                mkey = yyjson_mut_str(mdoc, key);
                mval = (! opt->eq_name) ? yyjson_mut_null(mdoc) : yyjson_mut_str(mdoc, opt->eq_name);

                if (! yyjson_mut_obj_add(mdoc->root, mkey, mval))
                    return false;
            }
        }
    while (--argc);

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
 */
static bool append_ignore_set(yyjson_mut_doc *mdoc, const ig_conds *data, const ig_opts *opt){
    assert(mdoc);
    assert(data);
    assert(data->cmd_name);
    assert(opt);

    yyjson_mut_val *conds[IG_CONDITIONS_NUM], *mkey, *mval;
    int conds_count = 0, conds_ids[IG_CONDITIONS_NUM];

    assert(data->short_opts || (! data->long_opts));

    if (data->short_opts){
        conds[conds_count] = yyjson_mut_str(mdoc, data->short_opts);
        conds_ids[conds_count++] = IG_SHORT_OPTS;

        if (data->long_opts){
            conds[conds_count] = yyjson_mut_val_mut_copy(mdoc, data->long_opts);
            conds_ids[conds_count++] = IG_LONG_OPTS;
        }
    }

    if (data->optargs){
        conds[conds_count] = yyjson_mut_val_mut_copy(mdoc, data->optargs);
        conds_ids[conds_count++] = IG_OPTARGS;
    }

    if (data->first_args){
        conds[conds_count] = yyjson_mut_val_mut_copy(mdoc, data->first_args);
        conds_ids[conds_count++] = IG_FIRST_ARGS;
    }

    if (opt->max_argc >= 0){
        conds[conds_count] = yyjson_mut_uint(mdoc, opt->max_argc);
        conds_ids[conds_count++] = IG_MAX_ARGC;
    }

    if (opt->detect_anymatch){
        conds[conds_count] = yyjson_mut_true(mdoc);
        conds_ids[conds_count++] = IG_DETECT_ANYMATCH;
    }

    if (opt->invert_flag){
        conds[conds_count] = yyjson_mut_true(mdoc);
        conds_ids[conds_count++] = IG_INVERT_FLAG;
    }

    mkey = yyjson_mut_str(mdoc, data->cmd_name);

    if (conds_count){
        mval = yyjson_mut_obj(mdoc);

        for (int i = 0; i < conds_count; i++)
            if (! yyjson_mut_obj_add_val(mdoc, mval, conds_keys[conds_ids[i]], conds[i]))
                return false;
    }
    else
        mval = yyjson_mut_null(mdoc);

    if (! yyjson_mut_obj_put(mdoc->root, mkey, mval))
        return false;

    return true;
}




/******************************************************************************
    * Function used in separate files
******************************************************************************/


/**
 * @brief load the ignore-file into memory as a JSON data.
 *
 * @param[in]  offset  1 (for Dockerfile), 0 (for history-file)
 * @param[in]  original  whether to use the original ignore-file
 * @return bool  successful or not
 *
 * @attention the JSON data must be properly unloaded when finished using.
 */
bool load_ignore_file(int offset, int original){
    assert(! idoc);
    assert(offset == ((bool) offset));
    assert(original == ((bool) original));

    idoc = yyjson_read_file(ignore_files[original][offset], 0, NULL, NULL);
    return (bool) idoc;
}


/**
 * @brief unload immutable JSON data that is the contents of the ignore-file.
 *
 */
void unload_ignore_file(void){
    yyjson_doc_free(idoc);

    idoc = NULL;
}




/**
 * @brief check if the execution of the specified command should be ignored.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return bool  the resulting boolean
 *
 * @note the contents of the ignore-file are used as much as possible while excluding invalid data.
 */
bool check_if_ignored(int argc, char **argv){
    assert(argc > 0);
    assert(argv);

    bool result = false;

    if (idoc){
        char *key;
        yyjson_val *ival;
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
            size_t size = 1, idx, long_opts_num = 0;
            bool no_short_opts = true;
            uint64_t colons;
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

            if (size > INT_MAX)
                size = INT_MAX;

            struct option long_opts[size + 1];
            memset(long_opts, 0, (sizeof(struct option) * (size + 1)));

            for (ival++; size--; ival = unsafe_yyjson_get_next(ival)){
                if ((! (name = yyjson_get_str(ival++))) || (! *name) || strpbrk(name, ":=?") ||
                    (! yyjson_is_uint(ival)) || ((colons = yyjson_get_uint(ival)) >= 3))
                        continue;

                for (idx = 0; (idx < long_opts_num) && strcmp(name, long_opts[idx].name); idx++);

                if (idx == long_opts_num){
                    long_opts_num++;
                    long_opts[idx].name = name;
                    long_opts[idx].has_arg = has_arg_table[colons];
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
            name = (argc > 0) ? *argv : NULL;

            if (yyjson_is_arr(ival) && (detect_anymatch == check_if_contained(name, ival)))
                goto exit;

            ival = conds[IG_MAX_ARGC];

            if (yyjson_is_uint(ival) && (detect_anymatch == (argc <= yyjson_get_uint(ival))))
                goto exit;
        }

        result = (! result);
    }

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