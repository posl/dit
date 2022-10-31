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

#define ERASE_REG_FLAGS  (REG_EXTENDED | REG_NOSUB)

#define getsize_check_list(lines_num)  (((lines_num - 1) >> 5) + 1)
#define bit_operation_check_list(list, i, oper)  (list[(i >> 5)] oper (1 << (i & 0b11111)))

#define if_necessary_assign_exit_status(tmp, exit_status) \
    if (tmp && (exit_status != (UNEXPECTED_ERROR + ERROR_EXIT))) \
        exit_status = (tmp + exit_status) ? tmp : (UNEXPECTED_ERROR + ERROR_EXIT)


/** Data type for storing the results of option parse */
typedef struct {
    bool has_delopt;    /** whether to have the options for deletion */
    int undoes;         /** how many times to undo */
    int target_c;       /** character representing the files to be edited ('d', 'h' or 'b') */
    int regex_flags;    /** flags that determine the format of compilation of regular expression */
    int max_count;      /** the maximum number of lines to delete, counting from the most recently added */
    bool reset_flag;    /** whether to reset the log-file (specified by optional arguments) */
    int blank_c;        /** how to handle the empty lines ('p', 's' or 't') */
    bool verbose;       /** whether to display deleted lines on screen */
    int assume_c;       /** the response to the delete confirmation ('Y', 'N', 'Q' or '\0') */
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
    char *lines;                 /** sequence of all lines read from the target file */
    unsigned int *check_list;    /** array of bits representing whether to delete the corresponding lines */
    bool first_mark;             /** whether to mark lines for deletion for the first time */
    erase_logs *logs;            /** variable to store the log-data recorded in the log-file */
} erase_data;


/** Data type for storing the function that determines which lines to delete */
typedef int (* delopt_func)(int, void *, erase_opts *, erase_data *);


static int __parse_opts(int argc, void *argv, erase_opts *opt, erase_data *data);
static void __display_prev(int target_c);

static int __do_erase(int argc, void *argv, erase_opts *opt, delopt_func marklines_func);
static int __construct_erase_data(erase_data *data, int target_c, unsigned short provlogs[2], bool no_delete);

static bool __marklines_containing_pattern(const char *pattern, erase_opts *opt, erase_data *data);
static bool __marklines_with_numbers(const char *range, erase_opts *opt, erase_data *data);
static void __marklines_to_undo(erase_opts *opt, erase_data *data);
static int __marklines_in_dockerfile(int size, void *patterns, erase_opts *opt, erase_data *data);

static int __delete_marked_lines(erase_opts *opt, erase_data *data, int flag);
static int __manage_erase_logs(const char *file_name, int mode_c, erase_logs *logs, bool concat_flag);


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

    if (! (i = __parse_opts(argc, argv, &opt, NULL))){
        if (argc <= optind)
            exit_status = __do_erase(argc, argv, &opt, __parse_opts);
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
static int __parse_opts(int argc, void *argv, erase_opts *opt, erase_data *data){
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

    int c, i, err_code = '\0';
    const char * const *valid_args;

    if (! data){
        opt->has_delopt = false;
        opt->undoes = 0;
        opt->target_c = '\0';
        opt->regex_flags = ERASE_REG_FLAGS;
        opt->max_count = -1;
        opt->reset_flag = false;
        opt->blank_c = 'p';
        opt->verbose = false;
        opt->assume_c = '\0';

        int *ptr, mode;
        while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) >= 0){
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
                            err_code = 'N';
                            c = 1;
                            goto err_exit;
                        }
                    }
                    else
                        c = 1;
                    *ptr = c;
                    ptr = NULL;
                    break;
                case 'd':
                    assign_both_or_either(opt->target_c, 'h', 'b', 'd');
                    break;
                case 'h':
                    assign_both_or_either(opt->target_c, 'd', 'b', 'h');
                    break;
                case 'i':
                    opt->regex_flags |= REG_ICASE;
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
                        case 'T':
                            ptr = &(opt->target_c);
                            valid_args = target_args;
                            mode = 2;
                    }
                    if ((c = receive_expected_string(optarg, valid_args, ARGS_NUM, mode)) >= 0){
                        *ptr = *(valid_args[c]);
                        ptr = NULL;
                        break;
                    }
                    err_code = 'O';
                default:
                    goto err_exit;
            }
        }

        if (! opt->target_c){
            err_code = 'T';
            goto err_exit;
        }

        if (! (opt->has_delopt || opt->undoes)){
            if (opt->verbose){
                __display_prev(opt->target_c);
                return NORMALLY_EXIT;
            }
            else
                opt->undoes = 1;
        }
    }
    else {
        char short_opts_copy[5];
        memcpy(short_opts_copy, short_opts, (sizeof(char) * 4));
        short_opts_copy[4] = '\0';

        struct option long_opts_copy[3] = {0};
        memcpy(long_opts_copy, long_opts, (sizeof(struct option) * 2));

        optind = 1;
        opterr = 0;

        while ((c = getopt_long(argc, argv, short_opts_copy, long_opts_copy, NULL)) >= 0){
            switch (c){
                case 'E':
                    if (__marklines_containing_pattern(optarg, opt, data))
                        break;
                    goto err_exit;
                case 'N':
                    if (__marklines_with_numbers(optarg, opt, data))
                        break;
                    err_code = 'O';
                    c = 0;
                    i = ERASE_OPTID_NUMBERS;
                    goto err_exit;
            }
        }
    }

    return SUCCESS;

err_exit:
    if (err_code){
        if (err_code != 'T'){
            xperror_invalid_arg(err_code, c, long_opts[i].name, optarg);

            if ((err_code == 'O') && (c < 0))
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
static void __display_prev(int target_c){
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
            case 'h':
                src_file = ERASE_RESULT_FILE_H;
        }

        if (offset)
            fprintf(stdout, ("\n < %s >\n" + --offset), target_args[2 - offset]);

        while ((line = xfgets_for_loop(src_file, false, NULL)))
            fprintf(stdout, "%s\n", line);

    } while ((target_c = next_target_c));
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
static int __do_erase(int argc, void *argv, erase_opts *opt, delopt_func marklines_func){
    int both_flag = false, tmp, exit_status = SUCCESS;

    erase_logs logs;
    erase_data data = { .logs = &logs };

    unsigned short prov_reflecteds[2] = {0};
    read_provisional_report(prov_reflecteds);

    do {
        if (opt->target_c == 'b'){
            opt->target_c = 'd';
            both_flag = true;
        }
        else if (both_flag)
            opt->target_c = 'h';

        logs.reset_flag = opt->reset_flag;
        tmp = __construct_erase_data(&data, opt->target_c, prov_reflecteds, false);

        if (data.lines){
            if (data.check_list){
                data.first_mark = true;

                if (opt->undoes && (! logs.reset_flag))
                    __marklines_to_undo(opt, &data);
                if (opt->has_delopt && marklines_func(argc, argv, opt, &data))
                    both_flag = -1;

                tmp = __delete_marked_lines(opt, &data, both_flag);
                if_necessary_assign_exit_status(tmp, exit_status);

                free(data.check_list);
            }
            else
                exit_status = UNEXPECTED_ERROR + ERROR_EXIT;

            free(data.lines);
        }
        else
            if_necessary_assign_exit_status(tmp, exit_status);
    } while (both_flag > 0);

    if (write_provisional_report(prov_reflecteds)){
        if (exit_status >= 0)
            exit_status = UNEXPECTED_ERROR - exit_status;
    }

    return exit_status;
}


/**
 * @brief delete the specified lines from Dockerfile, and record this in the log-file.
 *
 * @param[in]  patterns  array of strings at the beginning of the line to be deleted
 * @param[in]  size  array size
 * @param[in]  verbose  whether to display deleted lines on screen
 * @param[in]  assume_c  the response to delete confirmation ('Y' or '\0')
 * @return int  0 (success), 1 (possible error), -1 (unexpected error) -2 (unexpected error & error exit)
 *
 * @note if the return value is -1, an internal file error has occurred, but the deletion was successful.
 * @attention internally, it uses 'xfgets_for_loop' with a depth of 1.
 */
int delete_from_dockerfile(const char * const patterns[], size_t size, bool verbose, bool assume_c){
    int exit_status = UNEXPECTED_ERROR + ERROR_EXIT;

    erase_opts opt = {
        .has_delopt = true,
        .undoes = 0,
        .target_c = 'd',
        .regex_flags = (ERASE_REG_FLAGS | REG_ICASE),
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
    if (size <= INT_MAX)
        exit_status = __do_erase(size, patterns, &opt, __marklines_in_dockerfile);

    return exit_status;
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
 * @note only in the above case, terminate normally without calling '__manage_erase_logs'.
 * @note if no changes to the log-file are necessary, just release the log-data at '__manage_erase_logs'.
 */
static int __construct_erase_data(erase_data *data, int target_c, unsigned short provlogs[2], bool no_delete){
    data->lines = NULL;
    data->check_list = NULL;

    const char *target_file, *log_file;
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
    size_t lines_num = 0;

    preserve_flag = (! no_delete);
    while ((line = xfgets_for_loop(target_file, preserve_flag, &exit_status))){
        lines_num++;

        if (preserve_flag && (! data->lines))
            data->lines = line;
    }

    if (! exit_status){
        int mode_c = 'w';
        void *ptr;

        if ((data->logs->total = lines_num - *(data->logs->p_provlog)) < 0){
            data->logs->total = lines_num;
            *(data->logs->p_provlog) = 0;
        }
        if (! (data->logs->reset_flag || __manage_erase_logs(log_file, 'r', data->logs, false))){
            if (! data->logs->reset_flag){
                if (! (no_delete && *(data->logs->p_provlog)))
                    mode_c = '\0';
            }
            else
                xperror_individually("the internal log-file is reset");
        }

        if (data->lines && (ptr = calloc(getsize_check_list(lines_num), sizeof(unsigned int))))
            data->check_list = (unsigned int *) ptr;
        else
            exit_status = __manage_erase_logs(log_file, mode_c, data->logs, no_delete);
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
    const char *targets = "dh";
    int tmp, exit_status = SUCCESS;

    erase_logs logs;
    erase_data data = { .logs = &logs };

    while (*targets){
        logs.reset_flag = false;
        if ((tmp = __construct_erase_data(&data, *(targets++), prov_reflecteds, true)) && (exit_status >= 0))
            exit_status = tmp;
    }

    return exit_status;
}




/******************************************************************************
    * Mark Part
******************************************************************************/


/**
 * @brief mark for deletion the lines containing extended regular expression pattern string.
 *
 * @param[in]  pattern  pattern string to determine which lines to delete
 * @param[in]  opt  variable to store the results of option parse
 * @param[out] data  variable to store the data commonly used in this command
 * @return bool  successful or not
 */
static bool __marklines_containing_pattern(const char *pattern, erase_opts *opt, erase_data *data){

}


/**
 * @brief mark for deletion the lines with the specified numbers.
 *
 * @param[in]  range  string specifying a range of line numbers to delete
 * @param[in]  opt  variable to store the results of option parse
 * @param[out] data  variable to store the data commonly used in this command
 * @return bool  successful or not
 */
static bool __marklines_with_numbers(const char *range, erase_opts *opt, erase_data *data){

}


/**
 * @brief mark for deletion the lines added within the last NUM times.
 *
 * @param[in]  opt  variable to store the results of option parse
 * @param[out] data  variable to store the data commonly used in this command
 */
static void __marklines_to_undo(erase_opts *opt, erase_data *data){

}




/**
 * @brief mark for deletion the lines in Dockerfile containing any of the specified pattern strings.
 *
 * @param[in]  size  the length of below array
 * @param[in]  patterns  array of pattern strings to determine which lines to delete
 * @param[in]  opt  variable to store the results of option parse
 * @param[out] data  variable to store the data commonly used in this command
 * @return int  0 (parse success) or -1 (error exit)
 */
static int __marklines_in_dockerfile(int size, void *patterns, erase_opts *opt, erase_data *data){

}




/******************************************************************************
    * Delete Part
******************************************************************************/


/**
 * @brief delete the lines recorded on checklist from the target file, and record this in the log-file.
 *
 * @param[in]  opt  variable to store the results of option parse
 * @param[out] data  variable to store the data commonly used in this command
 * @param[in]  flag  whether to edit both Dockerfile and history-file or -1
 * @return int  0 (success), 1 (possible error), -1 (unexpected error) -2 (unexpected error & error exit)
 *
 * @note if the return value is -1, an internal file error has occurred, but the deletion was successful.
 */
static int __delete_marked_lines(erase_opts *opt, erase_data *data, int flag){
    const char *target_file, *dest_file, *log_file;
    if (opt->target_c == 'd'){
        target_file = DOCKER_FILE_DRAFT;
        dest_file = ERASE_RESULT_FILE_D;
        log_file = ERASE_FILE_D;
    }
    else {
        target_file = HISTORY_FILE;
        dest_file = ERASE_RESULT_FILE_H;
        log_file = ERASE_FILE_H;
    }

    int mode_c = 'w';

    if (flag >= 0){
        
    }

    __manage_erase_logs(log_file, mode_c, data->logs, false);
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
 * @note except when reading log-data, release the dynamic memory that is no longer needed.
 * @note when the reset flag is set to false, the array and its size are also set to the correct values.
 */
static int __manage_erase_logs(const char *file_name, int mode_c, erase_logs *logs, bool concat_flag){
    char fm[] = "rb";
    FILE *fp = NULL;
    int exit_status = SUCCESS;

    if (mode_c){
        fm[0] = mode_c;
        fp = fopen(file_name, fm);
        exit_status = UNEXPECTED_ERROR;
    }

    size_t size, total = 0;
    void *array;
    div_t tmp;

    switch (mode_c){
        case 'r':
            logs->size = 0;
            logs->array = NULL;
            logs->reset_flag = true;

            if (fp && (fread(&size, sizeof(size), 1, fp) == 1)){
                if (size){
                    if ((size < INT_MAX) && (array = malloc(sizeof(unsigned char) * size))){
                        if (fread(array, sizeof(unsigned char), size, fp) == size){
                            logs->size = size;
                            logs->array = (unsigned char *) array;

                            while (size--)
                                total += logs->array[size];

                            if ((total < INT_MAX) && (total == logs->total))
                                logs->reset_flag = false;

                            exit_status = SUCCESS;
                        }
                        else
                            free(array);
                    }
                }
                else
                    exit_status = SUCCESS;
            }
            break;
        case 'w':
            if (fp){
                if (logs->reset_flag){
                    total = logs->total;
                    if (concat_flag)
                        total += *(logs->p_provlog);

                    concat_flag = true;
                    logs->size = 0;
                }
                else if (concat_flag)
                    total = *(logs->p_provlog);

                exit_status = SUCCESS;

                if (concat_flag && total){
                    tmp = div(total - 1, 255);
                    size = tmp.quot + 1;

                    if ((array = realloc(logs->array, (sizeof(unsigned char) * (logs->size + size))))){
                        logs->array = (unsigned char *) array;
                        memset((logs->array + logs->size), 255, size - 1);

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