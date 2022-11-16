/**
 * @file _erase.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'erase', that delete some lines from Dockerfile or history-file.
 * @author Tsukasa Inada
 * @date 2022/09/09
 *
 * @note In the log-file, array size and the array are stored as size_t and unsigned char* respectively.
 */

#include "main.h"

#define ERASE_FILE_D "/dit/var/erase.log.dock"
#define ERASE_FILE_H "/dit/var/erase.log.hist"

#define ERASE_OPTID_NUMBERS 1
#define ERASE_OPTID_UNDOES 2
#define ERASE_OPTID_MAX_COUNT 4

#define if_necessary_assign_exit_status(tmp, exit_status) \
    if ((tmp) && (exit_status != (UNEXPECTED_ERROR + ERROR_EXIT))) \
        exit_status = ((tmp) + exit_status) ? (tmp) : (UNEXPECTED_ERROR + ERROR_EXIT)

#define getsize_check_list(i)  ((((i) - 1) >> 5) + 1)
#define getidx_check_list(i)  ((i) >> 5)
#define getmask_check_list(i)  (1 << ((i) & 0b11111))

#define setbit_check_list(list, i)  (list[getidx_check_list(i)] |= getmask_check_list(i))
#define getbit_check_list(list, i)  (list[getidx_check_list(i)] & getmask_check_list(i))
#define invbit_check_list(list, i)  (list[getidx_check_list(i)] ^= getmask_check_list(i))


/** Data type for storing the results of option parse */
typedef struct {
    bool has_delopt;     /** whether to have the options for deletion */
    int undoes;          /** how many times to undo the editing of the target files */
    int target_c;        /** character representing the files to be edited ('d', 'h' or 'b') */
    bool ignore_case;    /** whether regular expression pattern string is case sensitive */
    int max_count;       /** the maximum number of lines to delete, counting from the most recently added */
    bool reset_flag;     /** whether to reset the log-file (specified by optional arguments) */
    int blank_c;         /** how to handle the empty lines ('p', 's' or 't') */
    bool verbose;        /** whether to display deleted lines on screen */
    int assume_c;        /** the response to the delete confirmation ('Y', 'N', 'Q' or '\0') */
} erase_opts;


/** Data type for storing some log-data recorded in the log-file */
typedef struct {
    int total;                    /** the total number of reflected lines */
    size_t size;                  /** the size of array containing the log-data */
    unsigned char *array;         /** array of the number of previously reflected lines */
    unsigned short *p_provlog;    /** variable to store the provisional number of reflected lines */
    bool reset_flag;              /** whether to reset the log-file */
} erase_logs;


/** Data type for storing some data commonly used in this command */
typedef struct {
    size_t lines_num;            /** the total number of lines in the target file */
    char *lines;                 /** sequence of all lines read from the target file */
    size_t list_size;            /** the size of array representing the lines to be deleted */
    unsigned int *check_list;    /** array of bits representing whether to delete the corresponding lines */
    bool first_mark;             /** whether to mark lines for deletion for the first time */
    erase_logs *logs;            /** variable to store the log-data recorded in the log-file */
} erase_data;


/** Data type for storing the function that determines which lines to delete */
typedef int (* delopt_func)(int, char **, erase_opts *, erase_data *);


static int parse_opts(int argc, char **argv, erase_opts *opt, erase_data *data);
static void display_prev(int target_c);

static int do_erase(int argc, char **argv, erase_opts *opt, delopt_func marklines_func);

static int construct_erase_data(erase_data *data, int target_c, unsigned short provlogs[2], bool no_delete);

static int marklines_in_dockerfile(int size, char **patterns, erase_opts *opt, erase_data *data);
static int marklines_containing_pattern(erase_data *data, const char *pattern, bool ignore_case);

static int marklines_with_numbers(erase_data *data, const char *range);
static void marklines_to_undo(erase_data *data, int undoes);

static int delete_marked_lines(erase_data *data, const erase_opts *opt, int both_flag);
static bool comfirm_deleted_lines(erase_data *data, const erase_opts *opt, bool both_flag);

static bool receive_range_specification(char *range, int stop, unsigned int *check_list);
static int popcount_check_list(unsigned int *check_list, size_t size);

static int manage_erase_logs(const char *file_name, int mode_c, erase_logs *logs, bool concat_flag);


extern const char * const assume_args[ARGS_NUM];
extern const char * const blank_args[ARGS_NUM];
extern const char * const target_args[ARGS_NUM];




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief delete some lines from Dockerfile or history-file.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note treated like a normal main function.
 */
int erase(int argc, char **argv){
    int i, exit_status = FAILURE;
    erase_opts opt;

    if (! (i = parse_opts(argc, argv, &opt, NULL))){
        if (argc <= optind)
            exit_status = do_erase(argc, argv, &opt, parse_opts);
        else
            xperror_too_many_args(0);
    }
    else if (i > 0)
        exit_status = SUCCESS;

    if (exit_status){
        if (exit_status < 0){
            exit_status = FAILURE;
            xperror_internal_file();
        }
        xperror_suggestion(true);
    }
    return exit_status;
}




/**
 * @brief parse optional arguments.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @param[out] opt  variable to store the results of option parse
 * @param[out] data  variable to store the data commonly used in this command or NULL
 * @return int  0 (parse success), 1 (normally exit) or -1 (error exit)
 *
 * @note the arguments are expected to be passed as-is from main function.
 * @note if 'data' is NULL, parse the options for behavior for use in subsequent processing.
 *
 * @attention options are separated for behavior and for deletion, and must be parsed in that order.
 */
static int parse_opts(int argc, char **argv, erase_opts *opt, erase_data *data){
    assert(opt);

    const char *short_opts = "E:N:Z::dhim:rstvy";

    int flag;
    const struct option long_opts[] = {
        { "extended-regexp", required_argument,  NULL, 'E' },
        { "numbers",         required_argument,  NULL, 'N' },  // ERASE_OPTID_NUMBERS = 1
        { "undoes",          optional_argument,  NULL, 'Z' },  // ERASE_OPTID_UNDOES = 2
        { "ignore-case",     no_argument,        NULL, 'i' },
        { "max-count",       required_argument,  NULL, 'm' },  // ERASE_OPTID_MAX_COUNT = 4
        { "reset",           no_argument,        NULL, 'r' },
        { "verbose",         no_argument,        NULL, 'v' },
        { "help",            no_argument,        NULL,  1  },
        { "assume",          required_argument, &flag, 'A' },
        { "blank",           required_argument, &flag, 'B' },
        { "target",          required_argument, &flag, 'T' },
        {  0,                 0,                  0,    0  }
    };

    assert(! strcmp(long_opts[ERASE_OPTID_NUMBERS].name, "numbers"));
    assert(! strcmp(long_opts[ERASE_OPTID_UNDOES].name, "undoes"));
    assert(! strcmp(long_opts[ERASE_OPTID_MAX_COUNT].name, "max-count"));

    int c, i = -1, errcode = '\0';
    const char * const *valid_args = NULL;

    if (! data){
        opt->has_delopt = false;
        opt->undoes = 0;
        opt->target_c = '\0';
        opt->ignore_case = false;
        opt->max_count = -1;
        opt->reset_flag = false;
        opt->blank_c = 'p';
        opt->verbose = false;
        opt->assume_c = '\0';

        int *ptr = NULL, mode;
        while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) >= 0){
            assert(! ptr);

            switch (c){
                case 'E':
                case 'N':
                    opt->has_delopt = true;
                    break;
                case 'Z':
                    ptr = &(opt->undoes);
                    i = ERASE_OPTID_UNDOES;
                case 'm':
                    if (! ptr){
                        ptr = &(opt->max_count);
                        i = ERASE_OPTID_MAX_COUNT;
                    }
                    if (optarg){
                        if ((c = receive_positive_integer(optarg, NULL)) < 0){
                            errcode = 'N';
                            c = 1;
                            goto errexit;
                        }
                        *ptr = c;
                    }
                    else
                        *ptr = 1;
                    ptr = NULL;
                    break;
                case 'd':
                    assign_both_or_either(opt->target_c, 'h', 'b', 'd');
                    break;
                case 'h':
                    assign_both_or_either(opt->target_c, 'd', 'b', 'h');
                    break;
                case 'i':
                    opt->ignore_case = true;
                    break;
                case 'r':
                    opt->reset_flag = true;
                    break;
                case 's':
                case 't':
                    opt->blank_c = c;
                    break;
                case 'v':
                    opt->verbose = true;
                    break;
                case 'y':
                    opt->assume_c = 'Y';
                    break;
                case 1:
                    erase_manual();
                    return NORMALLY_EXIT;
                case 0:
                    switch (flag){
                        case 'A':
                            ptr = &(opt->assume_c);
                            valid_args = assume_args;
                            mode = 3;
                            break;
                        case 'B':
                            ptr = &(opt->blank_c);
                            valid_args = blank_args;
                            mode = 2;
                            break;
                        default:
                            assert(flag == 'T');
                            ptr = &(opt->target_c);
                            valid_args = target_args;
                            mode = 2;
                    }
                    if ((c = receive_expected_string(optarg, valid_args, ARGS_NUM, mode)) >= 0){
                        *ptr = *(valid_args[c]);
                        ptr = NULL;
                        break;
                    }
                    errcode = 'O';
                default:
                    goto errexit;
            }
        }

        if (! opt->target_c){
            errcode = 'T';
            goto errexit;
        }

        if (! (opt->has_delopt || opt->undoes)){
            if (opt->verbose){
                display_prev(opt->target_c);
                return NORMALLY_EXIT;
            }
            else
                opt->undoes = 1;
        }
    }
    else {
        assert(strlen(short_opts) >= 4);
        assert((sizeof(long_opts) / sizeof(*long_opts)) > 2);

        char short_delopts[5];
        memcpy(short_delopts, short_opts, (sizeof(char) * 4));
        short_delopts[4] = '\0';

        struct option long_delopts[3] = {0};
        memcpy(long_delopts, long_opts, (sizeof(struct option) * 2));

        optind = 1;
        opterr = 0;

        while ((c = getopt_long(argc, argv, short_delopts, long_delopts, NULL)) >= 0)
            switch (c){
                case 'E':
                    if (marklines_containing_pattern(data, optarg, opt->ignore_case))
                        goto errexit;
                    break;
                case 'N':
                    if ((c = marklines_with_numbers(data, optarg))){
                        if (c == POSSIBLE_ERROR){
                            errcode = 'O';
                            c = 0;
                            i = ERASE_OPTID_NUMBERS;
                        }
                        goto errexit;
                    }
                    break;
            }
    }

    return SUCCESS;

errexit:
    if (errcode){
        if (errcode != 'T'){
            assert((i >= 0) && (i < (sizeof(long_opts) / sizeof(*long_opts) - 1)));
            xperror_invalid_arg(errcode, c, long_opts[i].name, optarg);

            if ((errcode == 'O') && (c < 0))
                xperror_valid_args(valid_args, ARGS_NUM);
        }
        else
            xperror_target_files();
    }
    return ERROR_EXIT;
}




/**
 * @brief display the previous deleted lines on screen.
 *
 * @param[in]  target_c  character representing the target files ('d', 'h' or 'b')
 */
static void display_prev(int target_c){
    int next_target_c, offset = 0;
    const char *src_file, *line;

    do {
        next_target_c = '\0';

        switch (target_c){
            case 'b':
                next_target_c = 'h';
                offset = 2;
            case 'd':
                src_file = ERASE_RESULT_FILE_D;
                break;
            default:
                assert(target_c == 'h');
                src_file = ERASE_RESULT_FILE_H;
        }

        if (offset){
            offset--;
            fprintf(stdout, ("\n < %s >\n" + offset), target_args[2 - offset]);
        }

        while ((line = xfgets_for_loop(src_file, false, NULL)))
            fprintf(stdout, "%s\n", line);
    } while ((target_c = next_target_c));

    assert(! offset);
}




/******************************************************************************
    * Series of Delete Processing
******************************************************************************/


/**
 * @brief perform the delete processing in order for each target file.
 *
 * @param[in]  argc  the length of the argument vector below
 * @param[out] argv  array of the arguments used to determine which lines to delete
 * @param[out] opt  variable to store the results of option parse
 * @param[in]  command_line  whether to use this function directly from the command line
 * @return int  0 (success), 1 (possible error), -1 (unexpected error) -2 (unexpected error & error exit)
 *
 * @note if the return value is -1, an internal file error has occurred, but the deletion was successful.
 */
static int do_erase(int argc, char **argv, erase_opts *opt, delopt_func marklines_func){
    assert(argc >= 0);
    assert(opt);
    assert(marklines_func);

    int both_flag = false, tmp, exit_status = SUCCESS;

    if (opt->target_c == 'b'){
        opt->target_c = 'd';
        both_flag = true;
    }

    erase_logs logs;
    erase_data data = { .logs = &logs };

    unsigned short prov_reflecteds[2] = {0};
    read_provisional_report(prov_reflecteds);

    do {
        assert((opt->target_c == 'd') || (opt->target_c == 'h'));

        logs.reset_flag = opt->reset_flag;
        tmp = construct_erase_data(&data, opt->target_c, prov_reflecteds, false);

        if (data.lines){
            if (data.check_list){
                data.first_mark = true;
                marklines_to_undo(&data, opt->undoes);

                if (opt->has_delopt && marklines_func(argc, argv, opt, &data))
                    both_flag = -1;

                tmp = delete_marked_lines(&data, opt, both_flag);
                if_necessary_assign_exit_status(tmp, exit_status);

                free(data.check_list);
            }
            else
                exit_status = UNEXPECTED_ERROR + ERROR_EXIT;

            free(data.lines);
        }
        else
            if_necessary_assign_exit_status(tmp, exit_status);

        if ((both_flag > 0) && (opt->target_c != 'h'))
            opt->target_c = 'h';
        else
            break;
    } while (true);

    if (write_provisional_report(prov_reflecteds) && (exit_status >= 0))
        exit_status = UNEXPECTED_ERROR - exit_status;

    return exit_status;
}


/**
 * @brief delete the specified lines from Dockerfile, and record this in the log-file.
 *
 * @param[in]  patterns  array of pattern strings to determine which lines to delete
 * @param[in]  size  array size
 * @param[in]  verbose  whether to display deleted lines on screen
 * @param[in]  assume_c  the response to delete confirmation ('Y' or '\0')
 * @return int  0 (success), 1 (possible error), -1 (unexpected error) -2 (unexpected error & error exit)
 *
 * @note if the return value is -1, an internal file error has occurred, but the deletion was successful.
 * @attention internally, it uses 'xfgets_for_loop' with a depth of 1.
 */
int delete_from_dockerfile(char **patterns, size_t size, bool verbose, int assume_c){
    assert(size <= INT_MAX);
    assert((assume_c == 'Y') || (! assume_c));

    erase_opts opt = {
        .has_delopt = true,
        .undoes = 0,
        .target_c = 'd',
        .ignore_case = true,
        .max_count = -1,
        .reset_flag = false,
        .blank_c = 'p',
        .verbose = verbose,
        .assume_c = assume_c
    };

    if (! patterns){
        opt.has_delopt = false;
        opt.undoes = 1;
    }

    return do_erase(size, patterns, &opt, marklines_in_dockerfile);
}




/******************************************************************************
    * Construct Part
******************************************************************************/


/**
 * @brief check the consiestency of the target log-file and construct data as needed.
 *
 * @param[out] data  variable to store the data commonly used in this command
 * @param[in]  target_c  character representing the target file ('d' or 'h')
 * @param[in]  provlogs  array of length 2 for storing the provisional number of reflected lines
 * @param[in]  no_delete  whether there is no subsequent delete processing
 * @return int  0 (success), 1 (possible error) or -1 (unexpected error)
 *
 * @note each element of 'data' is a non-null value if subsequent deletion operations can be performed.
 * @note only in the above case, terminate normally without calling 'manage_erase_logs'.
 * @note if no changes to the log-file are necessary, just release the log-data at 'manage_erase_logs'.
 */
static int construct_erase_data(erase_data *data, int target_c, unsigned short provlogs[2], bool no_delete){
    assert(data);
    assert(data->logs);
    assert(provlogs);

    const char *target_file, *log_file;

    data->lines_num = 0;
    data->lines = NULL;
    data->check_list = NULL;

    if (target_c == 'd'){
        data->logs->p_provlog = provlogs;
        target_file = DOCKER_FILE_DRAFT;
        log_file = ERASE_FILE_D;
    }
    else {
        data->logs->p_provlog = provlogs + 1;
        target_file = HISTORY_FILE;
        log_file = ERASE_FILE_H;
    }

    char *line;
    bool preserve_flag;
    int exit_status = SUCCESS;

    preserve_flag = (! no_delete);

    while ((line = xfgets_for_loop(target_file, preserve_flag, &exit_status))){
        data->lines_num++;

        if (preserve_flag && (! data->lines))
            data->lines = line;
    }

    if (! exit_status){
        assert(data->lines_num < INT_MAX);
        assert(data->logs->p_provlog);

        int mode_c = 'w';

        if ((data->logs->total = data->lines_num - *(data->logs->p_provlog)) < 0){
            data->logs->total = data->lines_num;
            *(data->logs->p_provlog) = 0;
        }
        if (! (data->logs->reset_flag || manage_erase_logs(log_file, 'r', data->logs, false))){
            if (! data->logs->reset_flag){
                if (! (no_delete && *(data->logs->p_provlog)))
                    mode_c = '\0';
            }
            else
                xperror_individually("the internal log-file is reset.");
        }

        if (data->lines){
            assert(! no_delete);
            assert(data->lines_num > 0);

            data->list_size = getsize_check_list(data->lines_num);
            data->check_list = (unsigned int *) calloc(data->list_size, sizeof(unsigned int));
        }
        if (! data->check_list)
            exit_status = manage_erase_logs(log_file, mode_c, data->logs, no_delete);
    }
    else {
        xperror_standards(exit_status, target_file);
        exit_status = POSSIBLE_ERROR;
    }

    return exit_status;
}


/**
 * @brief reflect the provisional number of reflected lines in log-file for the dit command 'erase'.
 *
 * @param[out] prov_reflecteds  array of length 2 for storing the provisional number of reflected lines
 * @return int  0 (success), 1 (possible error) or -1 (unexpected error)
 *
 * @attention if the provisional number of reflected line is invalid, that part of array may be set to 0.
 */
int update_erase_logs(unsigned short prov_reflecteds[2]){
    assert(prov_reflecteds);

    const char *targets = "dh";
    int tmp, exit_status = SUCCESS;

    erase_logs logs;
    erase_data data = { .logs = &logs };

    do {
        logs.reset_flag = false;

        if ((tmp = construct_erase_data(&data, *targets, prov_reflecteds, true)) && (exit_status >= 0))
            exit_status = tmp;
    } while (*(++targets));

    return exit_status;
}




/******************************************************************************
    * Determine by Regular Expression Matching
******************************************************************************/


/**
 * @brief mark for deletion the lines in Dockerfile containing any of the specified pattern strings.
 *
 * @param[in]  size  array size
 * @param[in]  patterns  array of pattern strings to determine which lines to delete
 * @param[in]  opt  variable to store the results of option parse
 * @param[out] data  variable to store the data commonly used in this command
 * @return int  0 (success) or -1 (error exit)
 *
 * @note the argument types match those of 'parse_opts' in order to treat this function in the same way.
 * @note the actual types are 'size_t', 'const char * const *', 'const erase_opts *' and 'erase_data *'.
 * @note set 'data->first_mark' to true during the loop to combine the conditions with a logical OR.
 */
static int marklines_in_dockerfile(int size, char **patterns, erase_opts *opt, erase_data *data){
    assert(size >= 0);
    assert(patterns);
    assert(opt);
    assert(data);

    int exit_status = SUCCESS;

    while (size--){
        data->first_mark = true;

        if (marklines_containing_pattern(data, *(patterns++), opt->ignore_case)){
            exit_status = ERROR_EXIT;
            break;
        }
    }

    return exit_status;
}


/**
 * @brief mark for deletion the lines containing extended regular expression pattern string.
 *
 * @param[out] data  variable to store the data commonly used in this command
 * @param[in]  pattern  pattern string to determine which lines to delete
 * @param[in]  ignore_case  whether regular expression pattern string is case sensitive
 * @return int  0 (success), 1 (argument recognition error) or -1 (unexpected error)
 *
 * @note combine the conditions with a logical OR within this function.
 * @note combine the conditions with a logical AND between multiple calls to the 'marklines' functions.
 *
 * @attention 'data' must be reliably constructed before calling this function.
 * @attention must not call this function if the target file does not contain any lines that can be deleted.
 */
static int marklines_containing_pattern(erase_data *data, const char *pattern, bool ignore_case){
    assert(data);
    assert(data->lines_num > 0);
    assert(data->lines);
    assert(data->check_list);

    int exit_status = POSSIBLE_ERROR;

    if (pattern){
        int cflags, errcode;
        regex_t preg;

        cflags = REG_EXTENDED | REG_NOSUB | (ignore_case ? REG_ICASE : 0);

        if (! (errcode = regcomp(&preg, pattern, cflags))){
            const char *line = NULL;
            unsigned int i = 0, idx, mask;

            exit_status = SUCCESS;

            do {
                if (! line)
                    line = data->lines;
                else
                    while (*(line++));

                idx = getidx_check_list(i);
                mask = getmask_check_list(i);

                if (! (data->first_mark || (data->check_list[idx] & mask)))
                    continue;

                if (! (errcode = regexec(&preg, line, 0, NULL, 0))){
                    if (data->first_mark)
                        data->check_list[idx] |= mask;
                }
                else if (errcode == REG_NOMATCH){
                    if (! data->first_mark){
                        assert(data->check_list[idx] & mask);
                        data->check_list[idx] ^= mask;
                    }
                }
                else {
                    exit_status = UNEXPECTED_ERROR;
                    break;
                }

            } while (++i < data->lines_num);

            regfree(&preg);
        }
        else {
            size_t errbuf_size = 0;
            char *errbuf = NULL;

            do {
                errbuf_size = regerror(errcode, &preg, errbuf, errbuf_size);

                if (errbuf){
                    assert(errbuf_size == (strlen(errbuf) + 1));
                    xperror_individually(errbuf);
                    free(errbuf);
                }
                else if ((errbuf = (char *) malloc(sizeof(char) * errbuf_size)))
                    continue;

                break;
            } while (true);
        }

        data->first_mark = false;
    }

    return exit_status;
}




/******************************************************************************
    * Determine by Range Specification
******************************************************************************/


/**
 * @brief mark for deletion the lines with the specified numbers.
 *
 * @param[out] data  variable to store the data commonly used in this command
 * @param[in]  range  string specifying a range of line numbers to delete
 * @return int  0 (success), 1 (argument recognition error) or -1 (unexpected error)
 *
 * @note combine the conditions with a logical OR within a single sequence of a range specification.
 * @note combine the conditions with a logical AND between multiple calls to the 'marklines' functions.
 *
 * @attention 'data' must be reliably constructed before calling this function.
 * @attention must not call this function if the target file does not contain any lines that can be deleted.
 */
static int marklines_with_numbers(erase_data *data, const char *range){
    assert(data);
    assert(data->lines_num > 0);
    assert(data->list_size > 0);
    assert(data->check_list);

    int exit_status = POSSIBLE_ERROR;

    if (range){
        void *ptr = NULL;

        if (data->first_mark || (ptr = calloc(data->list_size, sizeof(unsigned int)))){
            size_t size;
            size = strlen(range) + 1;

            char range_copy[size];
            memcpy(range_copy, range, (sizeof(char) * size));

            unsigned int *tmp;
            tmp = data->first_mark ? data->check_list : ((unsigned int *) ptr);

            if (receive_range_specification(range_copy, data->lines_num, tmp)){
                exit_status = SUCCESS;

                if (! data->first_mark){
                    unsigned int *dest;

                    size = data->list_size;
                    dest = data->check_list;

                    do
                        *(dest++) &= *(tmp++);
                    while (--size);
                }
            }

            if (ptr)
                free(ptr);

            data->first_mark = false;
        }
        else
            exit_status = UNEXPECTED_ERROR;
    }

    return exit_status;
}


/**
 * @brief mark for deletion the lines added within the last NUM times.
 *
 * @param[out] data  variable to store the data commonly used in this command
 * @param[in]  undoes  how many times to undo the editing of the target file
 *
 * @note array of log-data may be empty because they have not yet been recorded.
 * @attention 'data' must be reliably constructed before calling this function.
 */
static void marklines_to_undo(erase_data *data, int undoes){
    assert(data);
    assert(data->check_list);
    assert(data->logs);

    if (undoes > 0){
        if ((! data->logs->reset_flag) && (data->logs->size > 0)){
            assert((data->logs->total > 0) && (data->logs->total < INT_MAX));
            assert(data->logs->size < INT_MAX);
            assert(data->logs->array);

            unsigned int i;
            unsigned char *tmp;

            i = data->logs->total;
            tmp = data->logs->array + data->logs->size;

            if (undoes > data->logs->size)
                undoes = data->logs->size;

            assert(undoes > 0);

            do {
                i -= *(--tmp);
            } while (--undoes);

            for (; i < data->logs->total; i++)
                setbit_check_list(data->check_list, i);
        }

        data->first_mark = false;
    }
}




/******************************************************************************
    * Delete Part
******************************************************************************/


/**
 * @brief delete the lines recorded on checklist from the target file, and record this in the log-file.
 *
 * @param[out] data  variable to store the data commonly used in this command
 * @param[in]  opt  variable to store the results of option parse
 * @param[in]  flag  whether to edit both Dockerfile and history-file or -1
 * @return int  0 (success), 1 (possible error), -1 (unexpected error) -2 (unexpected error & error exit)
 *
 * @note modify the contents of the log-data as lines are deleted to properly update the log-file.
 * @note if no changes to the log-file are necessary, just release the log-data at 'manage_erase_logs'.
 * @note if the return value is -1, an internal file error has occurred, but the deletion was successful.
 *
 * @attention 'data' must be reliably constructed before calling this function.
 */
static int delete_marked_lines(erase_data *data, const erase_opts *opt, int both_flag){
    const char *result_file, *target_file, *log_file;

    if (opt->target_c == 'd'){
        result_file = ERASE_RESULT_FILE_D;
        target_file = DOCKER_FILE_DRAFT;
        log_file = ERASE_FILE_D;
    }
    else {
        result_file = ERASE_RESULT_FILE_H;
        target_file = HISTORY_FILE;
        log_file = ERASE_FILE_H;
    }

    int exit_status = POSSIBLE_ERROR, mode_c = '\0';

    if (both_flag >= 0){
        if (comfirm_deleted_lines(data, opt, both_flag)){
            FILE *result_fp, *target_fp;

            exit_status = UNEXPECTED_ERROR + ERROR_EXIT;

            if ((result_fp = fopen(result_file, "w"))){
                if ((target_fp = fopen(target_file, "w"))){
                    const char *line;
                    int total, accum = 0, logs_idx = -1;
                    unsigned int i = 0;
                    const char *format = "%s\n";

                    exit_status = SUCCESS;
                    mode_c = 'w';
                    line = data->lines;
                    total = data->logs->total;

                    do {
                        if (getbit_check_list(data->check_list, i)){
                            if (i < total){
                                data->logs->total--;

                                if (! data->logs->reset_flag){
                                    while (i >= accum)
                                        accum += data->logs->array[++logs_idx];
                                    data->logs->array[logs_idx]--;
                                }
                            }
                            else
                                (*(data->logs->p_provlog))--;

                            if (*line){
                                fprintf(result_fp, format, line);

                                if (opt->verbose)
                                    fprintf(stdout, format, line);
                            }
                        }
                        else
                            fprintf(target_fp, format, line);

                        while (*(line++));
                    } while (++i < data->lines_num);

                    if (! data->logs->reset_flag){
                        logs_idx = 0;

                        for (i = 0; i < data->logs->size; i++){
                            if (! data->logs->array[i])
                                logs_idx++;
                            else if (logs_idx)
                                data->logs->array[i - logs_idx] = data->logs->array[i];
                        }
                        data->logs->size -= logs_idx;
                    }

                    fclose(target_fp);
                }
                fclose(result_fp);
            }
        }
        else
            exit_status = SUCCESS;
    }

    if (data->logs->reset_flag)
        mode_c = 'w';
    if (manage_erase_logs(log_file, mode_c, data->logs, false) && (exit_status >= 0))
        exit_status = UNEXPECTED_ERROR - exit_status;

    return exit_status;
}




/**
 * @brief comfirm the lines to be deleted while respecting the user's intention.
 *
 * @param[out] data  variable to store the data commonly used in this command
 * @param[in]  opt  variable to store the results of option parse
 * @param[in]  both_flag  whether to edit both Dockerfile and history-file
 * @return bool  whether the target file should be updated
 *
 * @note in the first half, empty lines are handled and lines to be deleted are extracted.
 * @note in the second half, confirmation before deleting the extracted lines is performed.
 * @note whether or not to retain empty lines is only affected by 'opt->blank_c'.
 * @note index numbers pointing to deletion candidates wrap around at values less than their maximum number.
 *
 * @attention 'data' must be reliably constructed before calling this function.
 * @attention must not call this function if the target file does not contain any lines that can be deleted.
 */
static bool comfirm_deleted_lines(erase_data *data, const erase_opts *opt, bool both_flag){
    int offset = 0;

    if (both_flag){
        if (opt->target_c == 'd')
            offset = 1;

        if (opt->verbose){
            both_flag = false;
            fprintf(stdout, ("\n < %s >\n" + offset), target_args[2 - offset]);
        }
    }


    int marked_num, max_count, truncates_num = 0, deletes_num = 0;
    const char *line = NULL;
    unsigned int i = 0, idx, mask;
    bool first_blank = true;

    marked_num = popcount_check_list(data->check_list, data->list_size);
    max_count = ((opt->max_count >= 0) && (opt->max_count < marked_num)) ? opt->max_count : marked_num;

    int candidate_idx = 0;
    const char *candidate_lines[max_count];
    unsigned int candidate_idxs[max_count];

    do {
        if (! line)
            line = data->lines;
        else
            while (*(line++));

        idx = getidx_check_list(i);
        mask = getmask_check_list(i);

        if (! *line){
            data->check_list[idx] &= ~mask;

            switch (opt->blank_c){
                case 's':
                    if (first_blank){
                        first_blank = false;
                        continue;
                    }
                case 't':
                    truncates_num++;
                    data->check_list[idx] |= mask;
                    continue;
                default:
                    assert(opt->blank_c == 'p');
                    continue;
            }
        }
        first_blank = true;

        if (data->check_list[idx] & mask){
            if (max_count){
                if ((candidate_idx = deletes_num++) >= max_count){
                    candidate_idx %= max_count;
                    idx = candidate_idxs[candidate_idx];
                    invbit_check_list(data->check_list, idx);
                }
                candidate_lines[candidate_idx] = line;
                candidate_idxs[candidate_idx] = i;
            }
            else
                data->check_list[idx] ^= mask;
        }
    } while (++i < data->lines_num);


    if (deletes_num > max_count){
        deletes_num = max_count;
        candidate_idx++;
    }
    else
        candidate_idx = 0;

    if (opt->assume_c != 'Y'){
        int assume_c, select_idx = -1, selects_num = 0;
        unsigned int select_list[1], start = 0, end = 0;
        char answer[5], range[64];

        i = 0;
        marked_num = deletes_num;
        assume_c = opt->assume_c;

        do {
            if (select_idx >= 0){
                do {
                    if (select_idx < selects_num){
                        mask = 1 << (select_idx++);

                        if (*select_list & mask){
                            i++;
                            continue;
                        }
                    }
                    else {
                        select_idx = -1;
                        selects_num = 0;
                    }
                    break;
                } while (true);
            }

            if (i < marked_num){
                idx = (i++) + candidate_idx;
                idx %= max_count;

                if ((assume_c != 'Q') && (select_idx < 0)){
                    if (! selects_num){
                        if (both_flag){
                            both_flag = false;
                            fprintf(stderr, ("\n < %s >\n" + offset), target_args[2 - offset]);
                        }

                        start = i - 1;
                        end = start + 8;

                        if (end > marked_num)
                            end = marked_num;
                        fprintf(stderr, "\nLines to be Deleted (%u/%d):\n", end, marked_num);
                    }

                    selects_num++;
                    fprintf(stderr, "%3d  %s\n", selects_num, candidate_lines[idx]);

                    if (i == end){
                        fputc('\n', stderr);

                        if (! opt->assume_c){
                            *answer = '\0';

                            do {
                                fputs("Do you want to delete all of them? [Y/n]  ", stderr);
                                fscanf(stdin, "%4[^\n]%*[^\n]", answer);
                                fgetc(stdin);

                                assume_c = receive_expected_string(answer, assume_args, ARGS_NUM, 3);
                            } while (assume_c < 0);

                            assume_c = *(assume_args[assume_c]);
                        }
                        switch (assume_c){
                            case 'N':
                                fputs(
                                    "Select the lines to delete with numbers.\n"
                                    " (separated by commas, length within 63)  "
                                , stderr);

                                *range = '\0';
                                fscanf(stdin, "%63[^\n]%*[^\n]", range);
                                fgetc(stdin);

                                select_idx = 0;
                                *select_list = 0;
                                receive_range_specification(range, selects_num, select_list);
                            case 'Q':
                                i = start;
                                break;
                            default:
                                assert(assume_c == 'Y');
                                selects_num = 0;
                        }

                        if (i == marked_num)
                            fputc('\n', stderr);
                    }
                    continue;
                }

                deletes_num--;
                idx = candidate_idxs[idx];
                invbit_check_list(data->check_list, idx);
            }
            else
                break;
        } while (true);
    }

    return (bool) (truncates_num + deletes_num);
}




/******************************************************************************
    * Utilities
******************************************************************************/


/**
 * @brief parse a string representing a range specification.
 *
 * @param[in]  range  string specifying a range of line numbers to delete
 * @param[in]  stop  stop number in the range specification
 * @param[out] check_list  array of bits representing whether to delete the corresponding lines
 * @return bool  successful or not
 *
 * @attention negative integers are not allowed in the range specification.
 */
static bool receive_range_specification(char *range, int stop, unsigned int *check_list){
    assert(range);
    assert(stop > 0);
    assert(check_list);

    const char *token;
    int left, right, next = 0;
    unsigned int i;

    while ((token = strtok(range, ","))){
        range = NULL;
        left = -1;

        if ((right = receive_positive_integer(token, &left)) >= 0){
            if (! right){
                if (left < 0)
                    continue;
                right = stop;
            }

            assert(right > 0);

            if (left){
                if (left < 0)
                    left = right;
                else if (left > right){
                    next = right;
                    right = stop;
                }
                left--;
            }

            assert(left >= 0);

            do {
                if (right > stop)
                    right = stop;

                for (i = left; i < right; i++){
                    assert(i < stop);
                    setbit_check_list(check_list, i);
                }

                if (next){
                    left = 0;
                    right = next;
                    next = 0;
                }
                else
                    break;
            } while (true);

            assert(! next);
        }
        else
            return false;
    }

    return true;
}


/**
 * @brief calculate the number of lines to be deleted.
 *
 * @param[in]  check_list  array of bits representing whether to delete the corresponding line
 * @param[in]  size  array size
 * @return int  the resulting number
 *
 * @note it is helpful to note the number of bits required to represent the number of bits set to 1.
 * @attention must not call this function if the target file does not contain any lines that can be deleted.
 */
static int popcount_check_list(unsigned int *check_list, size_t size){
    assert(check_list);
    assert(size > 0);

    unsigned int i;
    int marked_num = 0;

    do {
        i = *(check_list++);

        i -= (i >> 1) & 0x55555555;
        i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
        i = (i + (i >> 4)) & 0x0f0f0f0f;
        i = i + (i >> 8);
        i = i + (i >> 16);

        marked_num += i & 0x3f;
    } while (--size);

    return marked_num;
}




/******************************************************************************
    * Record Part
******************************************************************************/


/**
 * @brief manage the log-data recorded in the log-file for the dit command 'erase'.
 *
 * @param[in]  file_name  name of the target log-file
 * @param[in]  mode_c  whether to read, write or do nothing ('r', 'w' or '\0')
 * @param[out] logs  variable to store the log-data recorded in the log-file
 * @param[in]  concat_flag  whether to reflect the provisional number of reflected lines in the log-file
 * @return int  0 (success) or -1 (unexpected error)
 *
 * @note when the return value is set to 0, reading and writing of the log-file are completed successfully.
 * @note when 'logs->reset_flag' is set to false, array of the log-data and its size are correctly defined.
 * @note except when reading the log-data, release the dynamic memory that is no longer needed.
 *
 * @attention must not exit after only reading the log-data, as dynamic memory cannot be released.
 */
static int manage_erase_logs(const char *file_name, int mode_c, erase_logs *logs, bool concat_flag){
    char fm[] = "rb";
    FILE *fp = NULL;
    int exit_status = SUCCESS;

    if (mode_c){
        fm[0] = mode_c;
        fp = fopen(file_name, fm);
        exit_status = UNEXPECTED_ERROR;
    }

    size_t size, total = 0;
    void *ptr;
    div_t tmp;

    switch (mode_c){
        case 'r':
            logs->size = 0;
            logs->array = NULL;
            logs->reset_flag = true;

            if (fp && (fread(&size, sizeof(size), 1, fp) == 1)){
                if (size){
                    if ((size < INT_MAX) && (ptr = malloc(sizeof(unsigned char) * size))){
                        logs->size = size;
                        logs->array = (unsigned char *) ptr;

                        if (fread(logs->array, sizeof(unsigned char), size, fp) == size){
                            exit_status = SUCCESS;

                            while (size--)
                                total += logs->array[size];
                        }
                    }
                }
                else
                    exit_status = SUCCESS;

                if ((! exit_status) && (total < INT_MAX) && (total == logs->total))
                    logs->reset_flag = false;
            }
            break;
        case 'w':
            if (fp){
                if (logs->reset_flag){
                    total = logs->total;
                    if (concat_flag)
                        total += *(logs->p_provlog);

                    logs->size = 0;
                }
                else if (concat_flag)
                    total = *(logs->p_provlog);

                exit_status = SUCCESS;

                if (total){
                    tmp = div((total - 1), 255);
                    size = tmp.quot + 1;

                    if ((ptr = realloc(logs->array, (sizeof(unsigned char) * (logs->size + size))))){
                        logs->array = (unsigned char *) ptr;
                        memset((logs->array + logs->size), 255, (size - 1));

                        logs->size += size;
                        logs->array[logs->size - 1] = tmp.rem + 1;
                    }
                    else
                        exit_status = UNEXPECTED_ERROR;
                }

                if (! exit_status){
                    fwrite(&(logs->size), sizeof(logs->size), 1, fp);

                    if (logs->size)
                        fwrite(logs->array, sizeof(unsigned char), logs->size, fp);
                }
            }
        default:
            if (logs->array)
                free(logs->array);
    }

    if (fp)
        fclose(fp);

    return exit_status;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/


static void assign_exit_status_macro_test(void);
static void getsize_check_list_macro_test(void);

static void marklines_containing_pattern_test(void);
static void marklines_with_numbers_test(void);
static void marklines_to_undo_test(void);

static void receive_range_specification_test(void);
static void popcount_check_list_test(void);

static void manage_erase_logs_test(void);




void erase_test(void){
    assert(sizeof(unsigned int) >= 4);

    do_test(assign_exit_status_macro_test);
    do_test(getsize_check_list_macro_test);

    do_test(marklines_containing_pattern_test);
    do_test(marklines_with_numbers_test);
    do_test(marklines_to_undo_test);

    do_test(receive_range_specification_test);
    do_test(popcount_check_list_test);

    do_test(manage_erase_logs_test);
}




static void assign_exit_status_macro_test(void){
    const int set_of_status[4] = { 0, -1, 1, -2 };

    const int results[16] = {
        0, -1,  1, -2,
        -1, -1, -2, -2,
        1, -2,  1, -2,
        -2, -2, -2, -2
    };

    int i, j, tmp, exit_status, count = 0;

    for (i = 0; i < 4; i++){
        tmp = set_of_status[i];

        for (j = 0; j < 4; j++){
            exit_status = set_of_status[j];
            if_necessary_assign_exit_status(tmp, exit_status);
            assert(exit_status == results[count]);

            print_progress_test_loop('\0', -1, count++);
            fprintf(stderr, "% d  % d\n", tmp, exit_status);
        }
    }
}




static void getsize_check_list_macro_test(void){
    const struct {
        const size_t lines_num;
        const size_t list_size;
    }
    table[] = {
        {   1,  1 },
        {   2,  1 },
        {  32,  1 },
        {  33,  2 },
        {  64,  2 },
        {  65,  3 },
        { 376, 12 },
        {   0,  0 }
    };

    for (int i = 0; table[i].lines_num; i++){
        assert(getsize_check_list(table[i].lines_num) == table[i].list_size);

        print_progress_test_loop('\0', -1, i);
        fprintf(stderr, "%3d  %2d\n", ((int) table[i].lines_num), ((int) table[i].list_size));
    }
}




static void marklines_containing_pattern_test(void){
    char lines_sequence[1024];
    unsigned int check_list[1] = {0};
    erase_data data = { .lines_num = 0, .lines = lines_sequence, .check_list = check_list };

    const char * const lines_array[] = {
        "ARG BASE_NAME",
        "ARG BASE_VERSION",
        "FROM \"${BASE_NAME}:${BASE_VERSION}\"",
        "# install essential packages",
        "WORKDIR /dit/src",
        "COPY ./etc/dit_install.sh /dit/etc/",
        "RUN sh /dit/etc/dit_install.sh curl gcc make",
        "RUN grep -Fq \'yum\' /dit/etc/package_manager || sh /dit/etc/dit_install.sh libc-dev",
        "# build the command to switch users",
        "ARG COMMIT_ID=\'212b75144bbc06722fbd7661f651390dc47a43d1\'",
        "RUN curl -L \"https://github.com/ncopa/su-exec/archive/${COMMIT_ID}.tar.gz\" | tar -xvz",
        "RUN cd \"su-exec-${COMMIT_ID}\" && make CC=\'gcc\' && mv su-exec /usr/local/bin/",
        "RUN rm -rf \"su-exec-${COMMIT_ID}\"",
        "# generate the dit command",
        "COPY ./cmd ./",
        "RUN make && make clean",
            NULL
    };

    const char * const *tmp;
    char *dest;
    size_t total_size = 0, size;

    for (tmp = lines_array; *tmp; tmp++){
        dest = lines_sequence + total_size;

        size = strlen(*tmp) + 1;
        assert((total_size += size) <= 1024);

        data.lines_num++;
        memcpy(dest, *tmp, size);
    }


    // successful cases

    
}




static void marklines_with_numbers_test(void){
    const struct {
        const char *range;
        const int flag;
        const unsigned int results[2];
    }
    table[] = {
        { "45-5,16-26,9,34-43",        true, { 0x03ff811f, 0x000077fe } },
        { "0-40",                     false, { 0x03ff811f, 0x000000fe } },
        { "10-",                      false, { 0x03ff8000, 0x000000fe } },
        { ",,12-21,,37-0,,34,,",      false, { 0x001f8000, 0x000000f2 } },
        { "-",                         true, { 0xffffffff, 0x00007fff } },
        { "19-24,35-15,,1000,,29,26", false, { 0x12fc7fff, 0x00007ffc } },
        { "-",                        false, { 0x12fc7fff, 0x00007ffc } },
        { "0",                        false, { 0x00000000, 0x00000000 } },
        { "-2-7",                       -1,  { 0x00000000, 0x00000000 } },
        { "12:38-41:55",                -1,  { 0x00000000, 0x00000000 } },
        { "60-20,40, ...",              -1,  { 0x00000000, 0x00000000 } },
        { "[:digit:]",                  -1,  { 0x00000000, 0x00000000 } },
        {  0,                            0,      0                      }
    };

    int i, type;
    unsigned int check_list[2] = {0};
    erase_data data = { .lines_num = 47, .list_size = 2, .check_list = check_list };


    for (i = 0; table[i].range; i++){
        type = SUCCESS;
        data.first_mark = table[i].flag;

        if (table[i].flag < 0)
            type = POSSIBLE_ERROR;

        assert(marklines_with_numbers(&data, table[i].range) == type);
        assert(! data.first_mark);

        if (type == SUCCESS){
            assert(check_list[0] == table[i].results[0]);
            assert(check_list[1] == table[i].results[1]);
        }

        print_progress_test_loop('S', type, i);
        fprintf(stderr, "%s\n", table[i].range);
    }
}




static void marklines_to_undo_test(void){
    unsigned char array[] = {15, 3, 4, 1, 5, 2, 2, 4, 1, 3};
    unsigned int check_list[2] = {0};

    erase_logs logs = { .array = array };
    erase_data data = { .check_list = check_list, .logs = &logs };


    int i, total = 0;
    const size_t size = sizeof(array) / sizeof(*array);

    for (i = 0; i < size; i++)
        total += array[i];

    logs.total = total;


    // when returning without marking the lines to be deleted

    data.first_mark = true;

    marklines_to_undo(&data, 0);
    assert(data.first_mark);
    assert(! check_list[0]);
    assert(! check_list[1]);


    logs.size = size;
    logs.reset_flag = false;

    if (rand() % 2)
        logs.size = 0;
    else
        logs.reset_flag = true;

    marklines_to_undo(&data, 1);
    assert(! data.first_mark);
    assert(! check_list[0]);
    assert(! check_list[1]);


    // when recording that a few lines will be deleted from the end

    unsigned int j, idx, mask;

    logs.size = size;
    logs.reset_flag = false;

    for (i = 0; i++ < size;){
        check_list[0] = 0;
        check_list[1] = 0;
        data.first_mark = true;
        total -= array[size - i];

        marklines_to_undo(&data, i);
        assert(! data.first_mark);

        for (j = 0; j < logs.total; j++){
            idx = getidx_check_list(j);
            mask = getmask_check_list(j);
            assert((check_list[idx] & mask) == ((j < total) ? 0 : mask));
        }

        fprintf(stderr, "  undoes:  %2d\n", i);
    }
}




static void receive_range_specification_test(void){
    const struct {
        const char *range;
        const int stop;
        const unsigned int results[2];
    }
    table[] = {
        { "",                            8, { 0x00000000, 0x00000000 } },
        { "0",                           8, { 0x00000000, 0x00000000 } },
        { "3",                           8, { 0x00000004, 0x00000000 } },
        { "8",                           8, { 0x00000080, 0x00000000 } },
        { "9",                           8, { 0x00000000, 0x00000000 } },
        { "1-1",                         8, { 0x00000001, 0x00000000 } },
        { "4-7",                         8, { 0x00000078, 0x00000000 } },
        { "6-2",                         8, { 0x000000e3, 0x00000000 } },
        { "-5",                          8, { 0x0000001f, 0x00000000 } },
        { "10-0",                        8, { 0x000000ff, 0x00000000 } },
        { "-",                           8, { 0x000000ff, 0x00000000 } },
        { "5-8,13,0,21-",               27, { 0x07f010f0, 0x00000000 } },
        { "31-7,100,,24-24",            32, { 0xc080007f, 0x00000000 } },
        { "123,,60-3,,9,,18-36,,45",    64, { 0xfffe0107, 0xf800100f } },
        { "zero",                       -8, { 0x00000000, 0x00000000 } },
        { "2 5",                        -8, { 0x00000000, 0x00000000 } },
        { "1-7-8",                      -8, { 0x00000000, 0x00000000 } },
        { "--",                         -8, { 0x00000000, 0x00000000 } },
        { "3.5,6",                      -8, { 0x00000000, 0x00000000 } },
        { "13,3-4,1o",                 -16, { 0x0000100c, 0x00000000 } },
        { "47-4,,8-11,,38,,29-33,.22", -52, { 0xf000078f, 0x000fc021 } },
        {  0,                            0,     0                      }
    };

    int i, type, stop;
    unsigned int check_list[2];
    char range[32];


    for (int i = 0; table[i].range; i++){
        type = SUCCESS;
        check_list[0] = 0;
        check_list[1] = 0;

        if ((stop = table[i].stop) < 0){
            type = FAILURE;
            stop = -stop;
        }

        memcpy(range, table[i].range, (strlen(table[i].range) + 1));
        assert(receive_range_specification(range, stop, check_list) == (! type));

        assert(check_list[0] == table[i].results[0]);
        assert(check_list[1] == table[i].results[1]);

        print_progress_test_loop('S', type, i);
        fprintf(stderr, "%s\n", table[i].range);
    }
}




static void popcount_check_list_test(void){
    const struct {
        const unsigned int bits;
        const int pops;
    }
    table[] = {
        { 0x8c000000,  3 },
        { 0x00000000,  0 },
        { 0x44083001,  6 },
        { 0x00001000,  1 },
        { 0x100a8a02,  7 },
        { 0x6011150c,  9 },
        { 0x9000d150,  8 },
        { 0x04087079, 10 },
        { 0x03004080,  4 },
        { 0x6ba3c1de, 18 },
        { 0xfa1d6806, 15 },
        { 0xc1904e11, 11 },
        { 0x8a8533be, 16 },
        { 0xef7afbff, 27 },
        { 0xf89b96ef, 21 },
        { 0xbf2e7eed, 23 },
        {   0,        -1 }
    };

    int i, j, count = 0;
    unsigned int check_list[4] = {0};


    for (i = 0; table[i].pops >= 0; i++){
        if ((j = i - 4) >= 0)
            count -= table[j].pops;

        check_list[i % 4] = table[i].bits;
        count += table[i].pops;
        assert(popcount_check_list(check_list, 4) == count);

        print_progress_test_loop('\0', -1, i);
        fputs("0b", stderr);

        for (j = 32; j;){
            if (! (j-- % 4))
                fputc('_', stderr);
            fprintf(stderr, "%u", ((table[i].bits >> j) & 1));
        }

        fprintf(stderr, "  %2d\n", table[i].pops);
    }
}




static void manage_erase_logs_test(void){
}


#endif // NDEBUG