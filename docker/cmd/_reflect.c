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

#define update_provisional_report(reflecteds)  manage_provisional_report(reflecteds, "r+w\0")
#define reset_provisional_report(reflecteds)  manage_provisional_report(reflecteds, "r\0w\0")

#define PATTERN_CMD_OR_ENTRYPOINT "^[[:space:]]*(CMD|ENTRYPOINT)[[:space:]]"

#define CC(target)  "\\[\\e[1;%dm\\]" target "\\[\\e[m\\]"


/** Data type for storing the results of option parse */
typedef struct {
    int target_c;    /** character representing the destination file ('d', 'h' or 'b') */
    int blank_c;     /** how to handle the empty lines ('p', 's' or 't') */
    bool verbose;    /** whether to display reflected lines on screen */
} refl_opts;


/** Data type for storing some data commonly used in this command */
typedef struct {
    int target_id;        /** 1 (targets Dockerfile), 0 (targets history-file) */
    size_t lines_num;     /** the number of lines to be reflected */
    char *lines;          /** sequence of all lines to be reflected in the target file */
    int reflecteds[2];    /** array of length 2 for storing the provisional number of reflected lines */
    int instr_c;          /** flag set when reflecting specific Dockerfile instructions ('C', 'O' or '\0') */
} refl_data;


static int parse_opts(int argc, char **argv, refl_opts *opt);
static int do_reflect(int argc, char **argv, const refl_opts *opt);

static int construct_refl_data(refl_data *data, int argc, char **argv, int blank_c);
static int reflect_lines(refl_data *data, const refl_opts *opt);

static const char *check_dockerfile_instr(char *target);
static size_t read_dockerfile_base(char **p_start);

static int record_reflected_lines(void);
static int manage_provisional_report(int reflecteds[2], const char *mode);


extern const char * const target_files[2];
extern const char * const convert_results[2];

extern const char * const blank_args[ARGS_NUM];
extern const char * const target_args[ARGS_NUM];


/** boolean value to prevent display confusion in certain cases when some errors occur */
static bool no_suggestion = false;


/** whether this command found a CMD/ENTRYPOINT instruction in the lines to be reflected in Dockerfile */
static bool first_cmd = true;
static bool first_entrypoint = true;




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
        else if (no_suggestion)
            return FAILURE;
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

    if (opt->target_c){
        assert((opt->target_c == 'b') || (opt->target_c == 'd') || (opt->target_c == 'h'));
        return SUCCESS;
    }

    xperror_target_files();
    return ERROR_EXIT;
}




/**
 * @brief perform the reflect processing in order for each target file.
 *
 * @param[in]  argc  the number of non-optional arguments
 * @param[in]  argv  array of strings that are non-optional arguments
 * @param[in]  opt  variable to store the results of option parse
 * @return int  0 (success), 1 (possible error) or negative integer (unexpected error)
 */
static int do_reflect(int argc, char **argv, const refl_opts *opt){
    assert(opt);

    int offset = 2, tmp, exit_status = SUCCESS;
    refl_data data = { .reflecteds = {0} };

    if (argc < 0)
        argc = 0;

    do
        if (opt->target_c != "dh"[--offset]){
            data.target_id = offset;

            if (! construct_refl_data(&data, argc, argv, opt->blank_c)){
                if ((tmp = reflect_lines(&data, opt)) && (exit_status >= 0))
                    exit_status = tmp;
            }
            else if (! exit_status)
                exit_status = POSSIBLE_ERROR;

            if (data.lines)
                free(data.lines);
        }
    while (offset);

    if (update_provisional_report(data.reflecteds))
        exit_status = UNEXPECTED_ERROR;

    return exit_status;
}


/**
 * @brief reflect the specified lines in Dockerfile, and record the number of reflected lines.
 *
 * @param[in]  lines_num  the number of lines to be reflected
 * @param[in]  lines  sequence of all lines to be reflected in the target file
 * @param[in]  verbose  whether to display reflected lines on screen
 * @param[in]  instr_c  flag set when reflecting specific Dockerfile instructions ('C', 'O' or '\0')
 * @return int  0 (success), 1 (possible error), -1 (unexpected error) -2 (unexpected error & error exit)
 *
 * @note if the return value is -1, an internal file error has occurred, but the processing can be continued.
 * @attention internally, it may use 'xfgets_for_loop' with a depth of 1.
 */
int reflect_to_dockerfile(size_t lines_num, char *lines, bool verbose, int instr_c){
    assert(lines);
    assert((instr_c == 'C') || (instr_c == 'O') || (! instr_c));

    int exit_status;

    refl_data data = {
        .target_id = 1,
        .lines_num = lines_num,
        .lines = lines,
        .reflecteds = {0},
        .instr_c = instr_c
    };

    refl_opts opt = {
        .target_c = 'd',
        .verbose = verbose
    };

    exit_status = reflect_lines(&data, &opt);

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
 * @param[in]  blank_c  how to handle the empty lines ('p', 's' or 't')
 * @return int  0 (success) or 1 (possible error)
 *
 * @note if no non-optional arguments, uses the convert-file prepared by this tool and reset it after use.
 * @note squeezing repeated empty lines is done across multiple source files.
 * @note each CMD and ENTRYPOINT instruction must meet the limit of 1 or less.
 */
static int construct_refl_data(refl_data *data, int argc, char **argv, int blank_c){
    assert(data);
    assert(data->target_id == ((bool) data->target_id));
    assert(! data->reflecteds[data->target_id]);
    assert(argc >= 0);
    assert(argv);

    const char *src_file, *msg = NULL;
    size_t lineno, size = 0;
    char *line;
    int errid = 0, exit_status = SUCCESS;
    bool first_blank = true;

    data->lines_num = 0;
    data->lines = NULL;
    data->instr_c = '\0';

    do {
        if (argc){
            if (! (src_file = *(argv++)))
                continue;
            if (*src_file && check_if_stdin(src_file))
                src_file = NULL;
        }
        else
            src_file = convert_results[data->target_id];

        if ((! src_file) || (get_file_size(src_file) >= 0)){
            for (lineno = 1; (line = xfgets_for_loop(src_file, &(data->lines), &size, &errid));){
                if (! *line){
                    switch (blank_c){
                        case 's':
                            if (first_blank){
                                first_blank = false;
                                break;
                            }
                        case 't':
                            lineno++;
                            size--;
                            continue;
                        default:
                            assert(blank_c == 'p');
                    }
                }
                else {
                    if (argc && data->target_id && (msg = check_dockerfile_instr(line))){
                        errid = -1;
                        continue;
                    }
                    first_blank = true;
                }
                lineno++;
                data->lines_num++;
            }

            if (errid){
                if (errid == EFBIG)
                    msg = "read size overflow detected";
                if (msg)
                    xperror_file_contents(src_file, lineno, msg);
                exit_status = POSSIBLE_ERROR;
                break;
            }
        }
        else {
            exit_status = POSSIBLE_ERROR;
            xperror_standards(src_file, errno);
        }
    } while (--argc > 0);

    assert(data->lines_num < INT_MAX);

    if (! (first_cmd && first_entrypoint))
        data->instr_c = 'C';

    return exit_status;
}




/**
 * @brief reflect the specified lines in Dockerfile or history-file.
 *
 * @param[out] data  variable to store the data commonly used in this command
 * @param[in]  opt  variable to store the results of option parse
 * @return int  0 (success), 1 (possible error), -1 (unexpected error) -2 (unexpected error & error exit)
 *
 * @note if the return value is -1, an internal file error has occurred, but the processing can be continued.
 * @note if Dockerfile is empty, it will be initialized based on its base file.
 */
static int reflect_lines(refl_data *data, const refl_opts *opt){
    assert(data);
    assert(data->target_id == ((bool) data->target_id));
    assert(! data->reflecteds[data->target_id]);
    assert(opt);

    const char *dest_file;
    int file_size, exit_status = SUCCESS;
    char *seq = NULL;

    dest_file = target_files[data->target_id];

    if ((file_size = get_file_size(dest_file)) != -2){
        if (data->lines_num){
            size_t remain = 0;
            char *line;
            FILE *fps[2];

            if (data->target_id){
                if (file_size <= 0){
                    if (! (remain = read_dockerfile_base(&seq))){
                        exit_status = FATAL_ERROR;
                        goto exit;
                    }
                    assert(seq);
                    line = seq;
                }
                else if (data->instr_c == 'C'){
                    line = PATTERN_CMD_OR_ENTRYPOINT;
                    exit_status = delete_from_dockerfile(&line, 1, false, 'Y');

                    if (exit_status && (exit_status != UNEXPECTED_ERROR)){
                        assert((exit_status == POSSIBLE_ERROR) || (exit_status == FATAL_ERROR));
                        goto exit;
                    }
                }
            }

            if ((fps[0] = fopen(dest_file, "a"))){
                int fpn = 1, extra_size = 0, offset;
                const char *format = "%s\n";
                size_t size, new_size;

                if (file_size < 0)
                    file_size = 0;

                if (opt->verbose){
                    if (opt->target_c == 'b'){
                        no_suggestion = (! data->target_id);
                        print_target_repr(data->target_id);
                    }
                    fps[fpn++] = stdout;
                }

                do {
                    if (! remain){
                        remain = data->lines_num;
                        line = data->lines;
                        data->lines_num = 0;

                        if (data->instr_c == 'O'){
                            format = "ONBUILD %s\n";
                            extra_size = 8;
                        }
                    }
                    assert(remain);
                    assert(line);

                    do {
                        size = strlen(line) + 1;
                        new_size = size + extra_size;

                        if (size <= new_size){
                            assert(new_size);
                            new_size += file_size;

                            if ((file_size < new_size) && (new_size < INT_MAX)){
                                file_size = new_size;
                                data->reflecteds[data->target_id]++;

                                for (offset = fpn; offset--;)
                                    fprintf(fps[offset], format, line);

                                line += size;
                                continue;
                            }
                        }
                        xperror_message("write size overflow detected", dest_file);
                        assert(! data->lines_num);
                        break;
                    } while (--remain);

                } while (data->lines_num);

                fclose(fps[0]);
            }
            else
                exit_status = FATAL_ERROR;
        }
    }
    else {
        assert(errno == EFBIG);
        xperror_standards(dest_file, errno);
    }

exit:
    if (seq)
        free(seq);

    return exit_status;
}




/******************************************************************************
    * Utilities
******************************************************************************/


/**
 * @brief receive the target string as a Dockerfile instruction and determine if there is an error.
 *
 * @param[in]  target  target string
 * @return const char*  a message indicating an error, if any
 *
 * @note updates the global variables 'first_cmd' and 'first_entrypoint' appropriately..
 */
static const char *check_dockerfile_instr(char *target){
    assert(target);

    int instr_id = -1;
    const char *msg = "invalid instruction";

    if (receive_dockerfile_instr(target, &instr_id)){
        assert((instr_id >= -1) && (instr_id < DOCKER_INSTRS_NUM));
        msg = NULL;

        switch (instr_id){
            case ID_CMD:
                if (! first_cmd)
                    msg = "duplicated CMD instruction";
                first_cmd = false;
                break;
            case ID_ENTRYPOINT:
                if (! first_entrypoint)
                    msg = "duplicated ENTRYPOINT instruction";
                first_entrypoint = false;
                break;
            case ID_FROM:
            case ID_MAINTAINER:
                msg = "instruction not allowed";
        }
    }

    return msg;
}


/**
 * @brief load into memory the base file of Dockerfile developed by this tool.
 *
 * @param[out] p_start  pointer to the beginning of a series of strings
 * @return size_t  the number of lines loaded or 0
 *
 * @note the return value of 0 means an unexpected error occurred.
 * @attention if the contents of 'p_start' is non-NULL, it should be released by the caller.
 */
static size_t read_dockerfile_base(char **p_start){
    assert(p_start && (! *p_start));

    char *line;
    int errid = 0, instr_id;
    size_t lines_num = 0;

    while ((line = xfgets_for_loop(DOCKER_FILE_BASE, p_start, NULL, &errid))){
        instr_id = -1;

        if (receive_dockerfile_instr(line, &instr_id) && (lines_num || (instr_id == ID_FROM)))
            lines_num++;
        else
            errid = -1;
    }

    if (errid)
        lines_num = 0;

    return lines_num;
}




/******************************************************************************
    * Record Part
******************************************************************************/


/**
 * @brief record the number of reflected lines in various files.
 *
 * @return int  0 (success) or -1 (unexpected error)
 *
 * @note some functions detect errors when initializing each file, but they are ignored.
 */
static int record_reflected_lines(void){
    bool first_access;
    int exit_status, reflecteds[2] = {0};

    first_access = (! get_file_size(DIT_PROFILE));
    exit_status = reset_provisional_report(reflecteds);

    if ((reflecteds[1] || reflecteds[0] || first_access) && update_erase_logs(reflecteds))
        exit_status = UNEXPECTED_ERROR;

    if (first_access)
        exit_status = SUCCESS;

    FILE *fp;
    int code;

    if ((fp = fopen(REFLECT_FILE_R, "w"))){
        code = 31;  // red
        if (! get_last_exit_status())
            code++;  // grean

        fprintf(
            fp, CC(" [") "d:+%hu h:+%hu" CC("] ") "\\u:\\w " CC("\\$ "),
            code, reflecteds[1], reflecteds[0], code, code
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
    assert(reflecteds[1] >= 0);
    assert(reflecteds[0] >= 0);
    assert(mode);

    static FILE *fp = NULL;

    int *array_for_write, array_for_read[2];
    int mode_c, keep_c, i, j, exit_status = SUCCESS;
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
                    i = 1;

                    do {
                        j = reflecteds[i] + array_for_read[i];

                        if (reflecteds[i] <= j)
                            reflecteds[i] = j;
                        else
                            exit_status = UNEXPECTED_ERROR;
                    } while (i--);
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
                array_for_read[1] = 0;
                array_for_read[0] = 0;
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