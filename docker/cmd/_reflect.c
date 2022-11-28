/**
 * @file _reflect.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'reflect', that reflects some lines in Dockerfile or history-file.
 * @author Tsukasa Inada
 * @date 2022/09/21
 *
 * @note In the provisional report file, two integers are stored as unsigned short.
 * @note In the conclusive report file, the text to show on prompt the number of reflected lines is stored.
 */

#include "main.h"

#define DOCKER_FILE_BASE "/dit/etc/Dockerfile.base"

#define REFLECT_FILE_P "/dit/tmp/reflect-report.prov"
#define REFLECT_FILE_R "/dit/tmp/reflect-report.real"

#define CC(target)  "\\[\\e[1;%dm\\]" target "\\[\\e[m\\]"

#define update_provisional_report(curr_reflecteds)  manage_provisional_report(curr_reflecteds, "r+w\0")
#define reset_provisional_report(prov_reflecteds)  manage_provisional_report(prov_reflecteds, "r\0w\0")


/** Data type for storing the results of option parse */
typedef struct {
    int target_c;    /** character representing destination file ('d', 'h' or 'b') */
    int blank_c;     /** how to handle the empty lines ('p', 's' or 't') */
    bool verbose;    /** whether to display reflected lines on screen */
} refl_opts;


static int parse_opts(int argc, char **argv, refl_opts *opt);
static int do_reflect(int argc, char **argv, refl_opts *opt);

static int reflect_file(const char *src_file, int target_c, const refl_opts *opt, int *p_errid);
static int reflect_line(char *line, int target_c, int next_target_c, bool verbose, bool onbuild_flag);

static int init_dockerfile(FILE *fp);
static int check_dockerfile_instruction(char *line, bool onbuild_flag);

static int record_reflected_lines(void);
static int manage_provisional_report(unsigned short reflecteds[2], const char *mode);


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
    const char *short_opts = "dhpsv";

    int flag;
    const struct option long_opts[] = {
        { "verbose", no_argument,        NULL, 'v'   },
        { "help",    no_argument,        NULL,  1    },
        { "blank",   required_argument, &flag, true  },
        { "target",  required_argument, &flag, false },
        {  0,         0,                  0,    0    }
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
                if (flag){
                    ptr = &(opt->blank_c);
                    valid_args = blank_args;
                }
                else {
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
 * @brief parse the arguments to determine the target files and reflect their contents one by one.
 *
 * @param[in]  argc  the number of non-optional arguments
 * @param[in]  argv  array of strings that are non-optional arguments
 * @param[out] opt  variable to store the results of option parse
 * @return int  0 (success), 1 (possible error) or negative integer (unexpected error)
 *
 * @note if no non-optional arguments, uses the convert-file prepared by this tool and reset it after use.
 * @note 'opt->target_c' keeps updating in the loop to represent the next 'target_c'.
 */
static int do_reflect(int argc, char **argv, refl_opts *opt){
    int target_c, errid, tmp, exit_status = SUCCESS;
    const char *src_file;
    FILE *fp;

    do {
        target_c = opt->target_c;

        if (argc-- > 0){
            if (! argc)
                opt->target_c = '\0';

            src_file = *(argv++);
            if (check_if_stdin(src_file))
                src_file = NULL;
        }
        else {
            opt->target_c = '\0';

            switch (target_c){
                case 'b':
                    target_c = 'd';
                    opt->target_c = 'h';
                case 'd':
                    src_file = CONVERT_RESULT_FILE_D;
                    break;
                default:
                    src_file = CONVERT_RESULT_FILE_H;
            }
        }

        errid = 0;
        if ((tmp = reflect_file(src_file, target_c, opt, &errid)) && (exit_status >= 0))
            exit_status = tmp;

        if (argc < 0){
            if ((fp = fopen(src_file, "w")))
                fclose(fp);
            else
                exit_status = UNEXPECTED_ERROR;
        }
        else if (errid){
            if (errid < 0)
                break;
            if (! exit_status)
                exit_status = POSSIBLE_ERROR;
            xperror_standards(errid, src_file);
        }
    } while (opt->target_c);

    return exit_status;
}




/******************************************************************************
    * Reflect Part
******************************************************************************/


/**
 * @brief reflect the contents of the specified files in Dockerfile or history-file.
 *
 * @param[in]  src_file  source file name
 * @param[in]  target_c  character representing destination file ('d' or 'h')
 * @param[in]  opt  variable to store the results of option parse
 * @param[out] p_errid  variable for recording error that occur, if necessary
 * @return int  0 (success), 1 (possible error) or negative integer (unexpected error)
 *
 * @note squeezing repeated empty lines is done across multiple source files.
 * @note whether there is an error when opening the source file is not reflected in the return value.
 * @note if the content of 'p_errid' is -1 after this call, processing used the same source should be aborted.
 */
static int reflect_file(const char *src_file, int target_c, const refl_opts *opt, int *p_errid){
    static bool first_blank = true;

    char *line;
    int tmp, exit_status = SUCCESS;

    if (! p_errid){
        int errid = 0;
        p_errid = &errid;
    }

    do {
        if ((line = xfgets_for_loop(src_file, NULL, p_errid))){
            if (! *line){
                switch (opt->blank_c){
                    case 's':
                        if (first_blank){
                            first_blank = false;
                            break;
                        }
                    case 't':
                        continue;
                }
            }
            else
                first_blank = true;
        }
        if ((tmp = reflect_line(line, target_c, opt->target_c, opt->verbose, false))){
            if (exit_status >= 0)
                exit_status = tmp;
            if (tmp != UNEXPECTED_ERROR)
                *p_errid = -1;
        }
    } while (line);

    return exit_status;
}




/**
 * @brief reflect the specified line in Dockerfile or history-file, and record the number of reflected lines.
 *
 * @param[in]  line  target line
 * @param[in]  target_c  character representing destination file ('d' or 'h')
 * @param[in]  next_target_c  the next 'target_c' if there are files waiting to be processed or '\0'
 * @param[in]  verbose  whether to display reflected lines on screen
 * @param[in]  onbuild_flag  whether to convert normal Dockerfile instructions to ONBUILD instructions
 * @return int  0 (success), 1 (possible error), -1 (unexpected error) -2 (unexpected error & error exit)
 *
 * @note if the destination file is too large to be represented by int type, exits with the error.
 * @note if both files are destination, it will be indicated when reflecting the first line in each.
 * @note record the number of reflected lines after there are no more lines to be reflected.
 * @note if the return value is -1, an internal file error has occurred, but processing can be continued.
 *
 * @attention this function must be called until NULL is passed to 'line' to close the destination file.
 */
static int reflect_line(char *line, int target_c, int next_target_c, bool verbose, bool onbuild_flag){
    static FILE *fp = NULL;
    static unsigned short curr_reflecteds[2] = {0};
    static int prev_target_c = '\0';

    int exit_status = SUCCESS;

    if (line){
        const char *dest_file, *format = "ONBUILD %s\n";
        unsigned int target_id;
        int size;

        do {
            if (target_c == 'd'){
                if ((exit_status = check_dockerfile_instruction(line, onbuild_flag)) &&
                    (exit_status != UNEXPECTED_ERROR))
                        break;
                dest_file = DOCKER_FILE_DRAFT;
                target_id = 0;
            }
            else {
                dest_file = HISTORY_FILE;
                target_id = 1;
            }

            if (((size = get_file_size(dest_file)) == -2) || (! (fp || (fp = fopen(dest_file, "a"))))){
                exit_status = exit_status ? (UNEXPECTED_ERROR + ERROR_EXIT) : POSSIBLE_ERROR;
                xperror_standards(errno, dest_file);
                break;
            }
            if ((size <= 0) && (! target_id) && ((curr_reflecteds[0] += init_dockerfile(fp)) != 3)){
                exit_status = UNEXPECTED_ERROR + ERROR_EXIT;
                break;
            }

            if (! onbuild_flag)
                format += 8;

            curr_reflecteds[target_id]++;
            fprintf(fp, format, line);

            if (verbose){
                if (prev_target_c != target_c){
                    if ((target_c != next_target_c) && (next_target_c != prev_target_c))
                        fprintf(stdout, ("\n < %s >\n" + (target_id ^ 1)), target_args[target_id + 1]);

                    prev_target_c = target_c;
                }
                fprintf(stdout, format, line);
            }
        } while (false);
    }
    else if (target_c != next_target_c){
        if (fp){
            fclose(fp);
            fp = NULL;
        }
        if (! next_target_c){
            exit_status = update_provisional_report(curr_reflecteds);
            curr_reflecteds[0] = 0;
            curr_reflecteds[1] = 0;
        }
    }

    return exit_status;
}


/**
 * @brief reflect the specified line in Dockerfile, and record the number of reflected lines.
 *
 * @param[in]  line  target line
 * @param[in]  verbose  whether to display reflected lines on screen
 * @param[in]  onbuild_flag  whether to convert normal Dockerfile instructions to ONBUILD instructions
 * @return int  0 (success), 1 (possible error), -1 (unexpected error) -2 (unexpected error & error exit)
 *
 * @note if the return value is -1, an internal file error has occurred, but processing can be continued.
 *
 * @attention this function must be called until NULL is passed to 'line' to close the destination file.
 * @attention internally, it may use 'xfgets_for_loop' with a depth of 1.
 */
int reflect_to_Dockerfile(char *line, bool verbose, bool onbuild_flag){
    return reflect_line(line, 'd', '\0', verbose, onbuild_flag);
}




/******************************************************************************
    * Functions related to Dockerfile
******************************************************************************/


/**
 * @brief reflect the Dockerfile base instructions in an empty Dockerfile.
 *
 * @param[out] fp  handler for destination file
 * @return int  the number of reflected lines
 *
 * @note reflect FROM, SHELL and WORKDIR instructions exactly one by one in this order, otherwise an error.
 */
static int init_dockerfile(FILE *fp){
    char *line;
    int errid = 0, reflected = 0;

    int instr_ids[3] = {
        ID_FROM,
        ID_SHELL,
        ID_WORKDIR
    };

    while ((line = xfgets_for_loop(DOCKER_FILE_BASE, NULL, &errid))){
        if ((reflected < 3) && (receive_dockerfile_instruction(line, (instr_ids + reflected)))){
            reflected++;
            fprintf(fp, "%s\n", line);
        }
        else
            errid = -1;
    }

    return reflected;
}


/**
 * @brief check the validity of the target line as Dockerfile instruction.
 *
 * @param[in]  line  target line
 * @param[in]  onbuild_flag  whether to convert normal Dockerfile instructions to ONBUILD instructions
 * @return int  0 (success), 1 (possible error), -1 (unexpected error) -2 (unexpected error & error exit)
 *
 * @note satisfy the restriction that each of CMD and ENTRYPOINT instructions is one or less in Dockerfile.
 * @note if the return value is -1, an internal file error has occurred, but processing can be continued.
 *
 * TODO: error checking function by parsing
 */
static int check_dockerfile_instruction(char *line, bool onbuild_flag){
    static bool cmd_ent_duplicates[2] = {true, true};

    const char *errdesc;
    int instr_id = -1, exit_status = SUCCESS;

    if (*line){
        errdesc =
            (! (receive_dockerfile_instruction(line, &instr_id))) ?
                "Invalid Instruction" :
            ((instr_id == ID_FROM) || (instr_id == ID_MAINTAINER)) ?
                "Instruction not Allowed" :
            (onbuild_flag && (instr_id == ID_ONBUILD)) ?
                "Instruction Format Error" :
            NULL;

        if (! errdesc){
            const int cmd_ent_ids[2] = {ID_CMD, ID_ENTRYPOINT};

            char tmp[] = "^CMD^ENTRYPOINT";
            char *cmd_ent_patterns[2] = {tmp, (tmp + 4)};

            int i = 1;
            do
                if (instr_id == cmd_ent_ids[i]){
                    if (cmd_ent_duplicates[i]){
                        // exit_status = delete_from_dockerfile(cmd_ent_patterns, 2, false, 'Y');
                        // TODO: waiting the implementation of cmd commnad

                        cmd_ent_duplicates[0] = false;
                        cmd_ent_duplicates[1] = false;
                        break;
                    }
                    else
                        cmd_ent_duplicates[i] = true;
                }
            while (i--);
        }
        else {
            exit_status = POSSIBLE_ERROR;
            xperror_message(line, errdesc);
        }
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
    int exit_status, tmp;

    unsigned short reflecteds[2] = {0};
    exit_status = reset_provisional_report(reflecteds);

    tmp = update_erase_logs(reflecteds);
    if (! exit_status)
        exit_status = tmp;

    if (exit_status && (! get_file_size(VERSION_FILE)))
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
static int manage_provisional_report(unsigned short reflecteds[2], const char *mode){
    static FILE *fp = NULL;

    int mode_c, keep_c, exit_status = SUCCESS;
    char fm[] = "rb+";
    unsigned short array_for_read[2], *array_for_write;

    array_for_write = reflecteds;

    do {
        mode_c = (unsigned char) *(mode++);
        keep_c = (unsigned char) *(mode++);

        if (! fp){
            fm[0] = mode_c;
            fm[2] = keep_c;
            fp = fopen(REFLECT_FILE_P, fm);
        }

        if (fp){
            if (mode_c == 'r'){
                if (fread(array_for_read, sizeof(unsigned short), 2, fp) == 2){
                    reflecteds[0] += array_for_read[0];
                    reflecteds[1] += array_for_read[1];
                }
                else
                    exit_status = UNEXPECTED_ERROR;
            }
            else
                fwrite(array_for_write, sizeof(unsigned short), 2, fp);

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
 * @param[out] prov_reflecteds  array of length 2 for storing the provisional number of reflected lines
 * @return int  0 (success) or -1 (unexpected error)
 *
 * @attention each element of 'prov_reflecteds' must be initialized with 0 before calling this function.
 * @attention the file handler cannot be released without calling 'write_provisional_report' after this call.
 */
int read_provisional_report(unsigned short prov_reflecteds[2]){
    return manage_provisional_report(prov_reflecteds, "r+");
}


/**
 * @brief write the provisional number of reflected lines to the file that records them.
 *
 * @param[in]  prov_reflecteds  array of length 2 for storing the provisional number of reflected lines
 * @return int  0 (success) or -1 (unexpected error)
 *
 * @note basically read the current values by 'read_provisional_report' before calling this function.
 */
int write_provisional_report(unsigned short prov_reflecteds[2]){
    return manage_provisional_report(prov_reflecteds, "w\0");
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void reflect_test(void){
    fputs("reflect test\n", stdout);
}


#endif // NDEBUG