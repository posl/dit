/**
 * @file _reflect.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'reflect', that reflects some lines in Dockerfile or history-file.
 * @author Tsukasa Inada
 * @date 2022/09/21
 *
 * @note In the provisional report file, two provisional numbers of reflected lines are stored.
 * @note In the conclusive report file, the text to show on prompt the number of reflected lines is stored.
 */

#include "main.h"

#define DOCKER_FILE_BASE "/dit/etc/Dockerfile.base"

#define REFLECT_FILE_P "/dit/srv/reflect-report.prov"
#define REFLECT_FILE_R "/dit/srv/reflect-report.real"

#define REFL_INITIAL_LINES_MAX 32

#define update_provisional_report(reflecteds)  manage_provisional_report(reflecteds, "r+w\0")
#define reset_provisional_report(reflecteds)  manage_provisional_report(reflecteds, "r\0w\0")

#define CC(target)  "\\[\\e[1;%dm\\]" target "\\[\\e[m\\]"


/** Data type for storing the results of option parse */
typedef struct {
    int target_c;    /** character representing the destination file ('d', 'h' or 'b') */
    int blank_c;     /** how to handle the empty lines ('p', 's' or 't') */
    bool verbose;    /** whether to display reflected lines on screen */
} refl_opts;


/** Data type for storing some data commonly used in this command */
typedef struct {
    size_t lines_num;     /** the number of lines to be reflected */
    char **lines;         /** array of lines to be reflected */
    size_t ptrs_num;      /** the number of dynamic memory allocated */
    char **ptrs;          /** array of pointers to the beginning of dynamic memory allocated */
    int reflecteds[2];    /** array of length 2 for storing the provisional number of reflected lines */
    bool cmd_flag;        /** whether to reflect CMD or ENTRYPOINT instruction in Dockerfile */
    bool onbuild_flag;    /** whether to convert normal Dockerfile instructions to ONBUILD instructions */
} refl_data;


static int parse_opts(int argc, char **argv, refl_opts *opt);
static int do_reflect(int argc, char **argv, refl_opts *opt);

static int construct_refl_data(refl_data *data, int argc, char **argv, const refl_opts *opt);
static int reflect_lines(refl_data *data, const refl_opts *opt, bool both_flag);

static int record_reflected_lines(void);
static int manage_provisional_report(int reflecteds[2], const char *mode);


extern const char * const blank_args[ARGS_NUM];
extern const char * const target_args[ARGS_NUM];

extern const char * const docker_instr_reprs[DOCKER_INSTRS_NUM];




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief reflect some lines in Dockerfile or history-file or record the number of reflected lines.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note treated like a normal main function.
 */
int reflect(int argc, char **argv){
    int exit_status = FAILURE;

    if ((argc > 1) || get_file_size(REFLECT_FILE_R)){
        int i;
        refl_opts opt;

        if (! (i = parse_opts(argc, argv, &opt))){
            if (((argc -= optind) <= 0) || (opt.target_c != 'b')){
                argv += optind;
                exit_status = do_reflect(argc, argv, &opt);
            }
            else
                xperror_too_many_args(-1);
        }
        else if (i > 0)
            exit_status = SUCCESS;
    }
    else
        exit_status = record_reflected_lines();

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
 * @return int  0 (parse success), 1 (normally exit) or -1 (error exit)
 *
 * @note the arguments are expected to be passed as-is from main function.
 */
static int parse_opts(int argc, char **argv, refl_opts *opt){
    assert(opt);

    const char *short_opts = "dhpsv";

    int flag;
    const struct option long_opts[] = {
        { "verbose", no_argument,        NULL, 'v' },
        { "help",    no_argument,        NULL,  1  },
        { "blank",   required_argument, &flag, 'B' },
        { "target",  required_argument, &flag, 'T' },
        {  0,         0,                  0,    0  }
    };

    opt->target_c = '\0';
    opt->blank_c = 't';
    opt->verbose = false;

    int c, i, *ptr;
    const char * const *valid_args = NULL;

    while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) >= 0)
        switch (c){
            case 'd':
                assign_both_or_either(opt->target_c, 'h', 'b', 'd');
                break;
            case 'h':
                assign_both_or_either(opt->target_c, 'd', 'b', 'h');
                break;
            case 'p':
            case 's':
                opt->blank_c = c;
                break;
            case 'v':
                opt->verbose = true;
                break;
            case 1:
                reflect_manual();
                return NORMALLY_EXIT;
            case 0:
                switch (flag){
                    case 'B':
                        ptr = &(opt->blank_c);
                        valid_args = blank_args;
                        break;
                    default:
                        assert(flag == 'T');
                        ptr = &(opt->target_c);
                        valid_args = target_args;
                }
                if ((c = receive_expected_string(optarg, valid_args, ARGS_NUM, 2)) >= 0){
                    *ptr = *(valid_args[c]);
                    break;
                }
                xperror_invalid_arg('O', c, long_opts[i].name, optarg);
                xperror_valid_args(valid_args, ARGS_NUM);
            default:
                return ERROR_EXIT;
        }

    if (opt->target_c)
        return SUCCESS;

    xperror_target_files();
    return ERROR_EXIT;
}




/**
 * @brief perform the reflect processing in order for each target file.
 *
 * @param[in]  argc  the number of non-optional arguments
 * @param[in]  argv  array of strings that are non-optional arguments
 * @param[out] opt  variable to store the results of option parse
 * @return int  0 (success), 1 (possible error) or negative integer (unexpected error)
 */
static int do_reflect(int argc, char **argv, refl_opts *opt){
    assert(opt);

    int tmp, exit_status = SUCCESS;
    bool both_flag = false;

    tmp = argc;

    if (argc <= 0){
        argc = 0;
        tmp = 1;
    }

    if (opt->target_c == 'b'){
        opt->target_c = 'd';
        both_flag = true;
    }

    char *ptrs[tmp], **p_ptr;
    refl_data data = { .reflecteds = {0}, .ptrs = ptrs };

    do {
        assert((opt->target_c == 'd') || (opt->target_c == 'h'));

        if (! construct_refl_data(&data, argc, argv, opt)){
            if ((tmp = reflect_lines(&data, opt, both_flag)) && (exit_status >= 0))
                exit_status = tmp;
        }
        else if (! exit_status)
            exit_status = POSSIBLE_ERROR;

        if (data.lines)
            free(data.lines);
        for (p_ptr = data.ptrs; data.ptrs_num--; p_ptr++)
            free(*p_ptr);

        if (both_flag && (opt->target_c != 'h'))
            opt->target_c = 'h';
        else
            break;
    } while (true);

    if (update_provisional_report(data.reflecteds))
        exit_status = UNEXPECTED_ERROR;

    return exit_status;
}


/**
 * @brief reflect the specified lines in Dockerfile, and record the number of reflected lines.
 *
 * @param[in]  lines  array of lines to be reflected
 * @param[in]  size  array size
 * @param[in]  cmd_flag  whether to reflect CMD or ENTRYPOINT instruction
 * @param[in]  flag_c  flag code ('C' (cmd_flag), 'O' (onbuild_flag) or '\0' (no flags))
 * @return int  0 (success), 1 (possible error), -1 (unexpected error) -2 (unexpected error & error exit)
 *
 * @note if the return value is -1, an internal file error has occurred, but the processing can be continued.
 * @attention internally, it may use 'xfgets_for_loop' with a depth of 1.
 */
int reflect_to_dockerfile(char **lines, size_t size, bool verbose, int flag_c){
    assert(lines);
    assert((flag_c == 'C') || (flag_c == 'O') || (! flag_c));

    int exit_status;

    refl_data data = {
        .lines_num = size,
        .lines = lines,
        .reflecteds = {0},
        .cmd_flag = (flag_c == 'C'),
        .onbuild_flag = (flag_c == 'O')
    };

    refl_opts opt = {
        .target_c = 'd',
        .verbose = verbose
    };

    exit_status = reflect_lines(&data, &opt, false);

    if (update_provisional_report(data.reflecteds) && (exit_status >= 0))
        exit_status = UNEXPECTED_ERROR - exit_status;

    return exit_status;
}




/******************************************************************************
    * Reflect Part
******************************************************************************/


/**
 * @brief construct the array of lines to be reflected in Dockerfile or history-file.
 *
 * @param[out] data  variable to store the data commonly used in this command
 * @param[in]  argc  the number of non-optional arguments
 * @param[in]  argv  array of strings that are non-optional arguments
 * @param[in]  opt  variable to store the results of option parse
 * @return int  0 (success) or 1 (possible error)
 *
 * @note if no non-optional arguments, uses the convert-file prepared by this tool and reset it after use.
 * @note squeezing repeated empty lines is done across multiple source files.
 * @note each CMD and ENTRYPOINT instruction must meet the limit of 1 or less.
 */
static int construct_refl_data(refl_data *data, int argc, char **argv, const refl_opts *opt){
    assert(data);
    assert(data->ptrs);
    assert(argc >= 0);
    assert(argv);
    assert(opt);

    char **p_ptr, *line;
    const char *src_file, *errdesc = NULL;
    int exit_status = SUCCESS, lineno, id;
    bool first_blank = true, first_cmd = true, first_entrypoint = true;
    size_t curr_size = 0, old_size = 0;
    void *ptr;

    data->lines_num = 0;
    data->lines = NULL;
    data->ptrs_num = 0;
    p_ptr = data->ptrs;

    do {
        if (argc){
            if (! ((src_file = *(argv++)) && *src_file))
                continue;
            if (check_if_stdin(src_file))
                src_file = NULL;
        }
        else if (opt->target_c == 'd')
            src_file = CONVERT_RESULT_FILE_D;
        else
            src_file = CONVERT_RESULT_FILE_H;

        if (src_file && (get_file_size(src_file) < 0)){
            exit_status = POSSIBLE_ERROR;
            xperror_standards(src_file, errno);
            break;
        }

        for (lineno = 0; (line = xfgets_for_loop(src_file, p_ptr, &exit_status)); lineno++){
            if (! *line){
                switch (opt->blank_c){
                    case 's':
                        if (first_blank){
                            first_blank = false;
                            break;
                        }
                    case 't':
                        continue;
                    default:
                        assert(opt->blank_c == 'p');
                }
            }
            else
                first_blank = true;

            if (argc && (opt->target_c == 'd')){
                id = -1;

                if (receive_dockerfile_instr(line, &id)){
                    assert((id >= -1) && (id < DOCKER_INSTRS_NUM));

                    switch (id){
                        case ID_CMD:
                            if (! first_cmd)
                                errdesc = "duplicated CMD instruction";
                            first_cmd = false;
                            break;
                        case ID_ENTRYPOINT:
                            if (! first_entrypoint)
                                errdesc = "duplicated ENTRYPOINT instruction";
                            first_entrypoint = false;
                            break;
                        case ID_FROM:
                        case ID_MAINTAINER:
                            errdesc = "instruction not allowed";
                    }
                }
                else
                    errdesc = "invalid instruction";

                if (errdesc){
                    exit_status = -1;
                    continue;
                }
            }

            if (data->lines_num == curr_size){
                curr_size = old_size ? (old_size * 2) : REFL_INITIAL_LINES_MAX;

                if ((old_size < curr_size) && (ptr = realloc(data->lines, (sizeof(char *) * curr_size)))){
                    data->lines = (char **) ptr;
                    old_size = curr_size;
                }
                else {
                    exit_status = -1;
                    continue;
                }
            }

            assert(data->lines);
            data->lines[data->lines_num++] = line;
        }

        if (*p_ptr){
            data->ptrs_num++;
            p_ptr++;
        }
        if (exit_status){
            exit_status = POSSIBLE_ERROR;

            if (errdesc)
                xperror_file_contents(src_file, lineno, errdesc);
            break;
        }
        if (! argc){
            assert(src_file);
            assert(! errdesc);

            if ((id = open(src_file, (O_RDWR | O_CREAT | O_TRUNC))) != -1)
                close(id);
        }
    } while (--argc > 0);

    data->cmd_flag = false;
    data->onbuild_flag = false;

    if (! (first_cmd && first_entrypoint))
        data->cmd_flag = true;

    return exit_status;
}




/**
 * @brief reflect the specified lines in Dockerfile or history-file.
 *
 * @param[out] data  variable to store the data commonly used in this command
 * @param[in]  opt  variable to store the results of option parse
 * @param[in]  both_flag  whether to edit both Dockerfile and history-file
 * @return int  0 (success), 1 (possible error), -1 (unexpected error) -2 (unexpected error & error exit)
 *
 * @note if the return value is -1, an internal file error has occurred, but the processing can be continued.
 * @note if the Dockerfile.draft is empty, it will be initialized based on its base file.
 */
static int reflect_lines(refl_data *data, const refl_opts *opt, bool both_flag){
    assert(data);
    assert(opt);

    unsigned int target_id;
    const char *dest_file;
    int file_size, exit_status = SUCCESS;

    if (opt->target_c == 'd'){
        target_id = 0;
        dest_file = DOCKER_FILE_DRAFT;
    }
    else {
        target_id = 1;
        dest_file = HISTORY_FILE;
    }

    assert(! data->reflecteds[target_id]);

    if ((file_size = get_file_size(dest_file)) != -2){
        if (data->cmd_flag && (file_size >= 0)){
            char seq[] = "^[[:space:]]*CMD[[:space:]]\0^[[:space:]]*ENTRYPOINT[[:space:]]";
            char *patterns[2] = { seq, (seq + 28) };

            exit_status = delete_from_dockerfile(patterns, 2, false, 'Y');
        }

        if ((exit_status == SUCCESS) || (exit_status == UNEXPECTED_ERROR)){
            FILE *fps[2];

            if ((fps[0] = fopen(dest_file, "a"))){
                int i, fpn = 1, phase = 1, id = 0, addition = 1;
                size_t remain, new_size;
                char **p_line, *line;
                const char *format = "%s\n";

                if (opt->verbose){
                    if (both_flag){
                        i = target_id ^ 1;
                        print_target_repr(i);
                    }
                    fps[fpn++] = stdout;
                }

                if (file_size <= 0){
                    file_size = 0;
                    if (opt->target_c == 'd')
                        phase = 0;
                }

                remain = data->lines_num;
                p_line = data->lines;

                new_size = file_size;

                do {
                    switch (phase){
                        case 0:
                            if ((line = xfgets_for_loop(DOCKER_FILE_BASE, NULL, &id))){
                                id = -1;
                                if (receive_dockerfile_instr(line, &id) && (file_size || (id == ID_FROM))){
                                    id = 0;
                                    break;
                                }
                                continue;
                            }
                            if (id){
                                exit_status = FATAL_ERROR;
                                break;
                            }
                        case 1:
                            if (data->onbuild_flag){
                                format = "ONBUILD %s\n";
                                addition += 8;
                            }
                            phase = 2;
                        default:
                            line = (remain--) ? *(p_line++) : NULL;
                    }
                    if (! line)
                        break;

                    assert(addition);
                    new_size += strlen(line) + addition;

                    if ((file_size < new_size) && (new_size < INT_MAX)){
                        file_size = new_size;
                        data->reflecteds[target_id]++;

                        for (i = fpn; i--;)
                            fprintf(fps[i], format, line);
                    }
                    else {
                        exit_status = (! exit_status) ? POSSIBLE_ERROR : FATAL_ERROR;
                        xperror_message("file size overflow detected, ending soon", dest_file);
                        break;
                    }
                } while (true);

                fclose(fps[0]);
            }
            else
                exit_status = (! exit_status) ? POSSIBLE_ERROR : FATAL_ERROR;
        }
    }
    else {
        exit_status = POSSIBLE_ERROR;
        xperror_standards(dest_file, errno);
    }

    return exit_status;
}




/******************************************************************************
    * Record Part
******************************************************************************/


/**
 * @brief record the number of reflected lines in various files.
 *
 * @return int  0 (success), 1 (possible error) or -1 (unexpected error)
 *
 * @note some functions detect errors when initializing each file, but they are ignored.
 */
static int record_reflected_lines(void){
    int exit_status, tmp, reflecteds[2] = {0};

    exit_status = reset_provisional_report(reflecteds);
    tmp = update_erase_logs(reflecteds);

    if (! exit_status)
        exit_status = tmp;

    if (exit_status && (! get_file_size(DIT_PROFILE)))
        exit_status = SUCCESS;

    FILE *fp;
    if ((fp = fopen(REFLECT_FILE_R, "w"))){
        tmp = 31;  // red
        if (! get_last_exit_status())
            tmp++;  // grean

        fprintf(
            fp, CC(" [") "d:+%hu h:+%hu" CC("] ") "\\u:\\w " CC("\\$ "),
            tmp, reflecteds[0], reflecteds[1], tmp, tmp
        );
        fclose(fp);
    }
    else
        exit_status = UNEXPECTED_ERROR;

    return exit_status;
}




/**
 * @brief manage the file in which the provisional number of reflected lines is recorded.
 *
 * @param[out] reflecteds  array of length 2 for storing the provisional number of reflected lines
 * @param[in]  mode  string representing the modes of length 2 or 4
 * @return int  0 (success) or -1 (unexpected error)
 *
 * @note the even numbered elements of 'mode' represent whether to read or write. ('r' or 'w')
 * @note the odd numbered elements of 'mode' represent whether to keep the used file handler. ('+' or '\0')
 *
 * @attention each element of 'reflecteds' must be defined before the first call.
 * @attention 'mode' of length 4 should be specified to read and write sequentially with one call.
 * @attention unless the end of 'mode' is '\0' at the last call, cannot release the file handler.
 */
static int manage_provisional_report(int reflecteds[2], const char *mode){
    assert(reflecteds);
    assert(reflecteds[0] >= 0);
    assert(reflecteds[1] >= 0);
    assert(mode);

    static FILE *fp = NULL;

    int *array_for_write, mode_c, keep_c, array_for_read[2], exit_status = SUCCESS;
    char fm[] = "rb+";

    array_for_write = reflecteds;

    do {
        mode_c = (unsigned char) *(mode++);
        keep_c = (unsigned char) *(mode++);

        assert((mode_c == 'r') || (mode_c == 'w'));
        assert((keep_c == '+') || (! keep_c));

        if (! fp){
            fm[0] = mode_c;
            fm[2] = keep_c;
            fp = fopen(REFLECT_FILE_P, fm);
        }

        if (fp){
            if (mode_c == 'r'){
                if (fread(array_for_read, sizeof(int), 2, fp) == 2){
                    reflecteds[0] += array_for_read[0];
                    reflecteds[1] += array_for_read[1];
                    assert(reflecteds[0] >= array_for_read[0]);
                    assert(reflecteds[1] >= array_for_read[1]);
                }
                else
                    exit_status = UNEXPECTED_ERROR;
            }
            else
                fwrite(array_for_write, sizeof(int), 2, fp);

            if (! keep_c){
                fclose(fp);
                fp = NULL;
            }
            else
                fseek(fp, 0, SEEK_SET);
        }
        else
            exit_status = UNEXPECTED_ERROR;

        if (*mode){
            if (! keep_c){
                array_for_read[0] = 0;
                array_for_read[1] = 0;
                array_for_write = array_for_read;
            }
        }
        else
            return exit_status;
    } while (true);
}


/**
 * @brief read the provisional number of reflected lines from the file that recorded them.
 *
 * @param[out] reflecteds  array of length 2 for storing the provisional number of reflected lines
 * @return int  0 (success) or -1 (unexpected error)
 *
 * @attention each element of 'reflecteds' must be initialized with 0 before calling this function.
 * @attention the file handler cannot be released without calling 'write_provisional_report' after this call.
 */
int read_provisional_report(int reflecteds[2]){
    return manage_provisional_report(reflecteds, "r+");
}


/**
 * @brief write the provisional number of reflected lines to the file that records them.
 *
 * @param[in]  reflecteds  array of length 2 for storing the provisional number of reflected lines
 * @return int  0 (success) or -1 (unexpected error)
 *
 * @note basically read the current values by 'read_provisional_report' before calling this function.
 */
int write_provisional_report(int reflecteds[2]){
    return manage_provisional_report(reflecteds, "w\0");
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/


void reflect_test(void){
    fputs("reflect test\n", stdout);
}


#endif // NDEBUG