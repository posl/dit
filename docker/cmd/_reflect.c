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

#define update_provisional_report(curr_reflecteds)  __manage_provisional_report(curr_reflecteds, "r+w\0")
#define reset_provisional_report(prov_reflecteds)  __manage_provisional_report(prov_reflecteds, "r\0w\0")


/** Data type for storing the results of option parse */
typedef struct {
    int target_c;    /** character representing destination file ('d', 'h' or 'b') */
    int blank_c;     /** how to handle the blank lines ('p', 's' or 't') */
} refl_opts;


static int __parse_opts(int argc, char **argv, refl_opts *opt);

static int __do_reflect(int argc, char **argv, refl_opts *opt);
static int __reflect_file(const char *src_file, int target_c, refl_opts *opt, int *p_errid);
static int __reflect_line(char *line, int target_c, int next_target_c, bool verbose, bool onbuild_flag);
static int __init_dockerfile(FILE *fp);

static const char *__check_dockerfile_instruction(char *line, bool onbuild_flag);

static int __record_reflected_lines();
static int __manage_provisional_report(unsigned short reflecteds[2], const char *mode);


extern const char * const blank_args[BLANKS_NUM];
extern const char * const target_args[TARGETS_NUM];




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
    int exit_status = 1;

    if ((argc > 1) || check_file_size(REFLECT_FILE_R)){
        int i;
        refl_opts opt;

        if (! (i = __parse_opts(argc, argv, &opt))){
            if (((argc -= optind) <= 0) || (opt.target_c != 'b')){
                argv += optind;
                exit_status = __do_reflect(argc, argv, &opt);
            }
            else
                xperror_too_many_args(-1);
        }
        else if (i > 0)
            exit_status = 0;
    }
    else
        exit_status = __record_reflected_lines();

    if (exit_status){
        if (exit_status < 0){
            exit_status = 1;
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
static int __parse_opts(int argc, char **argv, refl_opts *opt){
    const char *short_opts = "dhps";

    int flag;
    const struct option long_opts[] = {
        { "help",   no_argument,        NULL,  1    },
        { "blank",  required_argument, &flag, true  },
        { "target", required_argument, &flag, false },
        {  0,        0,                  0,    0    }
    };

    opt->target_c = '\0';
    opt->blank_c = 't';

    int c, i, *ptr;
    const char * const *valid_args = NULL;
    size_t size;

    while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) >= 0){
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
            case 1:
                reflect_manual();
                return 1;
            case 0:
                if (flag){
                    ptr = &(opt->blank_c);
                    valid_args = blank_args;
                    size = BLANKS_NUM;
                }
                else {
                    ptr = &(opt->target_c);
                    valid_args = target_args;
                    size = TARGETS_NUM;
                }
                if ((c = receive_expected_string(optarg, valid_args, size, 2)) >= 0){
                    *ptr = *(valid_args[c]);
                    break;
                }
                xperror_invalid_arg('O', c, long_opts[i].name, optarg);
                xperror_valid_args(valid_args, size);
            default:
                return -1;
        }
    }

    if (opt->target_c)
        return 0;

    xperror_target_files();
    return -1;
}




/**
 * @brief parse the arguments to determine the target files and reflect their contents one by one.
 *
 * @param[in]  argc  the number of non-optional arguments
 * @param[in]  argv  array of strings that are non-optional arguments
 * @param[out] opt  variable to store the results of option parse
 * @return int  0 (success), 1 (error with an obvious reason) or -1 (unexpected error)
 *
 * @note if no non-optional arguments, use the convert-file prepared by this tool and reset it after use.
 * @note 'opt->target_c' keeps updating in the loop to represent the next 'target_c'.
 */
static int __do_reflect(int argc, char **argv, refl_opts *opt){
    int target_c, errid, break_flag, exit_status = 0;
    const char *src_file;
    FILE *fp;

    do {
        target_c = opt->target_c;

        if (argc-- > 0){
            if (! argc)
                opt->target_c = '\0';

            src_file = *(argv++);
            if (check_string_isstdin(src_file))
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
                case 'h':
                    src_file = CONVERT_RESULT_FILE_H;
            }
        }

        errid = 0;
        if ((break_flag = __reflect_file(src_file, target_c, opt, &errid))){
            if (exit_status >= 0)
                exit_status = break_flag;
        }

        if (argc < 0){
            if ((fp = fopen(src_file, "w")))
                fclose(fp);
            else
                exit_status = -1;
        }
        else if (errid){
            if (errid > 0)
                xperror_standards(errid, src_file);
            if (break_flag)
                break;
            if (! exit_status)
                exit_status = 1;
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
 * @param[out] p_errid  variable for recording errors that occur, if necessary
 * @return int  0 (success), 1 (error with an obvious reason) or -1 (unexpected error)
 *
 * @note squeezing blank lines is done across multiple source files.
 * @note file open errors are not reflected in the return value.
 * @note the return value is non-zero only when processing should be aborted.
 */
static int __reflect_file(const char *src_file, int target_c, refl_opts *opt, int *p_errid){
    static bool first_blank = true;

    char *line;
    int tmp, exit_status = 0;

    if (! p_errid){
        int errid = 0;
        p_errid = &errid;
    }

    do {
        if ((line = xfgets_for_loop(src_file, false, p_errid))){
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
        if ((tmp = __reflect_line(line, target_c, opt->target_c, false, false))){
            if (! *p_errid)
                *p_errid = -1;
            if (exit_status >= 0)
                exit_status = tmp;
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
 * @return int  0 (success), 1 (error with an obvious reason) or -1 (unexpected error)
 *
 * @note if the destination file is too large to be represented by int type, exit with the error.
 * @note record the number of reflected lines after there are no more lines to be reflected.
 *
 * @attention this function must be called until NULL is passed to 'line' to close the destination file.
 */
static int __reflect_line(char *line, int target_c, int next_target_c, bool verbose, bool onbuild_flag){
    static FILE *fp = NULL;
    static unsigned short curr_reflecteds[2] = {0};

    int exit_status = 0;

    if (line){
        do {
            const char *err_desc, *dest_file;
            int size, target_id;

            if (target_c == 'd'){
                if ((err_desc = __check_dockerfile_instruction(line, onbuild_flag))){
                    exit_status = 1;
                    xperror_message(line, err_desc);
                    break;
                }
                dest_file = DOCKER_FILE_DRAFT;
                target_id = 0;
            }
            else {
                dest_file = HISTORY_FILE;
                target_id = 1;
            }

            if (((size = check_file_size(dest_file)) == -2) || (! (fp || (fp = fopen(dest_file, "a"))))){
                exit_status = 1;
                xperror_standards(errno, dest_file);
                break;
            }
            if ((size <= 0) && (! target_id) && ((curr_reflecteds[0] += __init_dockerfile(fp)) != 3)){
                exit_status = -1;
                break;
            }

            const char *format = "ONBUILD %s\n";
            if (! onbuild_flag)
                format += 8;

            curr_reflecteds[target_id]++;
            fprintf(fp, format, line);

            if (verbose)
                fprintf(stdout, format, line);
        } while (0);
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
 * @return int  0 (success), 1 (error with an obvious reason) or -1 (unexpected error)
 *
 * @attention this function must be called until NULL is passed to 'line' to close the destination file.
 * @attention internally, it may use 'xfgets_for_loop' with a depth of 1.
 */
int reflect_to_Dockerfile(char *line, bool verbose, bool onbuild_flag){
    return __reflect_line(line, 'd', '\0', verbose, onbuild_flag);
}




/**
 * @brief reflect the Dockerfile base instructions in an empty Dockerfile.
 *
 * @param[out] fp  handler for destination file
 * @return int  the number of reflected lines
 *
 * @note reflect FROM, SHELL and WORKDIR instructions exactly one by one in this order, otherwise an error.
 */
static int __init_dockerfile(FILE *fp){
    char *line;
    int errid = 0, reflected = 0;

    int instr_ids[3] = {
        ID_FROM,
        ID_SHELL,
        ID_WORKDIR
    };

    while ((line = xfgets_for_loop(DOCKER_FILE_BASE, false, &errid))){
        if ((reflected < 3) && (receive_dockerfile_instruction(line, instr_ids + reflected))){
            reflected++;
            fprintf(fp, "%s\n", line);
        }
        else
            errid = -1;
    }

    return reflected;
}




/******************************************************************************
    * Check Part
******************************************************************************/


/**
 * @brief check the validity of the target line as Dockerfile instruction.
 *
 * @param[in]  line  target line
 * @param[in]  onbuild_flag  whether to convert normal Dockerfile instructions to ONBUILD instructions
 * @return int  abnormal or not
 *
 * @note satisfy the restriction that each of CMD and ENTRYPOINT instructions is one or less in Dockerfile.
 *
 * TODO: error checking function by parsing
 */
static const char *__check_dockerfile_instruction(char *line, bool onbuild_flag){
    static bool cmd_ent_duplicates[2] = {true, true};

    const char *err_desc;
    int instr_id = -1;

    err_desc =
        (! (receive_dockerfile_instruction(line, &instr_id))) ? "Invalid Instruction" :
        ((instr_id == ID_FROM) || (instr_id == ID_MAINTAINER)) ? "Instruction not Allowed" :
        (onbuild_flag && (instr_id == ID_ONBUILD)) ? "Instruction Format Error" :
            NULL;

    if (! err_desc){
        const int instr_ids[2] = {
            ID_CMD,
            ID_ENTRYPOINT
        };

        for (int i = 0; i < 2; i++){
            if (instr_id == instr_ids[i]){
                if (cmd_ent_duplicates[i]){
                    // TODO: waiting the implementation of erase commnad
                    // TODO: waiting the implementation of setcmd commnad

                    cmd_ent_duplicates[0] = false;
                    cmd_ent_duplicates[1] = false;
                    break;
                }
                else
                    cmd_ent_duplicates[i] = true;
            }
        }
    }

    return err_desc;
}




/******************************************************************************
    * Record Part
******************************************************************************/


/**
 * @brief record the number of reflected lines in various files.
 *
 * @return int  0 (success), 1 (error with an obvious reason) or -1 (unexpected error)
 *
 * @note some functions detect errors when initializing each file, but they are ignored.
 */
static int __record_reflected_lines(){
    int tmp, exit_status = 0;
    unsigned short prov_reflecteds[2] = {0};

    tmp = reset_provisional_report(prov_reflecteds);

    // TODO: waiting the implementation of erase commnad
    // tmp |= reflect_erase_file()

    FILE *fp;
    if ((fp = fopen(REFLECT_FILE_R, "w"))){
        fprintf(fp, "d:+%hu h:+%hu\n", prov_reflecteds[0], prov_reflecteds[1]);
        fclose(fp);
    }
    else
        exit_status = -1;

    if (tmp && check_file_size(VERSION_FILE)){
        exit_status |= tmp;
    }
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
static int __manage_provisional_report(unsigned short reflecteds[2], const char *mode){
    static FILE *fp = NULL;

    char mode_c, keep_c;
    char fm[] = "rb+";
    unsigned short array_for_read[2], *array_for_write;
    int exit_status = 0;

    array_for_write = reflecteds;

    do {
        mode_c = *(mode++);
        keep_c = *(mode++);

        if (! fp){
            fm[0] = mode_c;
            fm[2] = keep_c;

            if (! (fp = fopen(REFLECT_FILE_P, fm))){
                if (! (*mode))
                    return 1;

                exit_status = -1;
                continue;
            }
        }

        if (mode_c == 'r'){
            if (fread(array_for_read, sizeof(unsigned short), 2, fp) == 2){
                reflecteds[0] += array_for_read[0];
                reflecteds[1] += array_for_read[1];
            }
            else
                exit_status = -1;
        }
        else
            fwrite(array_for_write, sizeof(unsigned short), 2, fp);

        if (! keep_c){
            fclose(fp);
            fp = NULL;
        }
        else
            fseek(fp, 0, SEEK_SET);

        if (*mode){
            if (! keep_c){
                array_for_read[0] = 0;
                array_for_read[1] = 0;
                array_for_write = array_for_read;
            }
        }
        else
            return exit_status;
    } while (1);
}


/**
 * @brief read the provisional number of reflected lines from the file that recorded them.
 *
 * @param[out] prov_reflecteds  array of length 2 for storing the provisional number of reflected lines
 * @return int  0 (success) or -1 (unexpected error)
 *
 * @attention 'prov_reflecteds' must be initialized with 0 before the first call.
 * @attention the file handler cannot be released without calling 'write_provisional_report' after this call.
 */
int read_provisional_report(unsigned short prov_reflecteds[2]){
    return __manage_provisional_report(prov_reflecteds, "r+");
}


/**
 * @brief write the provisional number of reflected lines to the file that records them.
 *
 * @param[in]  prov_reflecteds  array of length 2 for storing the provisional number of reflected lines
 * @return int  0 (success) or -1 (unexpected error)
 *
 * @note basically read the current values by 'read_provisional_report', add some processing and call this.
 */
int write_provisional_report(unsigned short prov_reflecteds[2]){
    return __manage_provisional_report(prov_reflecteds, "w\0");
}