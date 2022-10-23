/**
 * @file _reflect.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'reflect', that reflects some lines in Dockerfile or history-file.
 * @author Tsukasa Inada
 * @date 2022/09/21
 *
 * @note In the provisional report file, two integers are stored as unsigned short type.
 * @note In the conclusive report file, the text to show on prompt the number of reflected lines is stored.
 */

#include "main.h"

#define BLANKS_NUM 3

#define DOCKER_FILE_BASE "/dit/etc/Dockerfile.base"

#define REFLECT_FILE_P "/dit/tmp/reflect-report.prov"
#define REFLECT_FILE_R "/dit/tmp/reflect-report.real"

#define update_provisional_report(curr_reflecteds)  __manage_provisional_report(curr_reflecteds, "r+w\0")
#define reset_provisional_report(prov_reflecteds)  __manage_provisional_report(prov_reflecteds, "r\0w\0")


/** Data type for storing the results of option parse */
typedef struct {
    int target_c;    /** character representing destination file */
    int blank_c;     /** how to handle blank lines */
} refl_opts;


static int __parse_opts(int argc, char **argv, refl_opts *opt);

static int __parse_and_reflect(int argc, char **argv, refl_opts *opt);
static int __reflect_file(const char *src_file, int target_c, refl_opts *opt, int *p_errid);
static int __reflect_line(const char *line, int target_c, int next_target_c);
static int __init_dockerfile(FILE *fp);

static int __check_dockerfile_instruction(const char *line);

static int __record_reflected_lines();
static int __manage_provisional_report(unsigned short reflecteds[2], const char *mode);


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
                exit_status = __parse_and_reflect(argc, argv, &opt);
            }
            else
                xperror_too_many_args(-1);
        }
        else if (i > 0)
            exit_status = 0;
    }
    else
        exit_status = - __record_reflected_lines();

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

    const char * const blank_args[BLANKS_NUM] = {
        "ignore",
        "preserve",
        "squeeze"
    };

    opt->target_c = '\0';
    opt->blank_c = 'i';

    int c, i, *ptr;
    const char * const *valid_args = NULL;

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

#if (BLANKS_NUM != TARGETS_NUM)
    #error "inconsistent with the definition of the macros"
#endif
            case 0:
                if (flag){
                    ptr = &(opt->blank_c);
                    valid_args = blank_args;
                }
                else {
                    ptr = &(opt->target_c);
                    valid_args = target_args;
                }
                if ((c = receive_expected_string(optarg, valid_args, BLANKS_NUM, 2)) >= 0){
                    *ptr = valid_args[c][0];
                    break;
                }
                xperror_invalid_arg('O', c, long_opts[i].name, optarg);
                xperror_valid_args(valid_args, TARGETS_NUM);
            default:
                return -1;
        }
    }

    if (opt->target_c)
        return 0;

    xperror_target_files();
    return -1;
}




/******************************************************************************
    * Reflect Part
******************************************************************************/


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
static int __parse_and_reflect(int argc, char **argv, refl_opts *opt){
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
                    src_file = CONVERT_FILE_D;
                    break;
                case 'h':
                    src_file = CONVERT_FILE_H;
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


/**
 * @brief reflect the contents of the specified files in Dockerfile or history-file.
 *
 * @param[in]  src_file  source file name
 * @param[in]  target_c  whether the destination file is Dockerfile or history-file ('d' or 'h')
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

    const char *line;
    int tmp, exit_status = 0;

    if (! p_errid){
        int errid = 0;
        p_errid = &errid;
    }

    do {
        if ((line = xfgets_for_loop(src_file, false, p_errid))){
            if (check_string_isnewline(line)){
                switch (opt->blank_c){
                    case 's':
                        if (first_blank){
                            first_blank = false;
                            break;
                        }
                    case 'i':
                        continue;
                }
            }
            else
                first_blank = true;
        }
        if ((tmp = __reflect_line(line, target_c, opt->target_c))){
            if (! *p_errid)
                *p_errid = -1;
            if (exit_status >= 0)
                exit_status = tmp;
        }
    } while (line);

    return exit_status;
}




/**
 * @brief reflect the specified line in Dockerfile or history-file and record the number of reflected lines.
 *
 * @param[in]  line  target line
 * @param[in]  target_c  whether the destination file is Dockerfile or history-file ('d' or 'h')
 * @param[in]  next_target_c  the next 'target_c' if there are files waiting to be processed or '\0'
 * @return int  0 (success), 1 (error with an obvious reason) or -1 (unexpected error)
 *
 * @note if the destination file is too large to be represented by int type, exit with the error.
 * @note record the number of reflected lines after there are no more lines to be reflected.
 *
 * @attention this function must be called until NULL is passed to 'line' to close the destination file.
 */
static int __reflect_line(const char *line, int target_c, int next_target_c){
    static FILE *fp = NULL;
    static unsigned short curr_reflecteds[2] = {0};

    int exit_status = 0;

    if (line){
        do {
            int target_id;
            if (target_c == 'd'){
                if (__check_dockerfile_instruction(line)){
                    exit_status = 1;
                    break;
                }
                target_id = 0;
            }
            else
                target_id = 1;

            if (! fp){
                const char *dest_file;
                int i;

                dest_file = (! target_id) ? DOCKER_FILE_DRAFT : HISTORY_FILE;
                if ((i = check_file_size(dest_file)) == -2){
                    xperror_standards(EFBIG, dest_file);
                    exit_status = 1;
                    break;
                }
                if ((! (fp = fopen(dest_file, "a"))) || ((i <= 0) && (! target_id) && __init_dockerfile(fp))){
                    exit_status = -1;
                    break;
                }
            }

            curr_reflecteds[target_id]++;
            fputs(line, fp);
        } while (0);
    }
    else if (target_c != next_target_c){
        if (fp){
            fclose(fp);
            fp = NULL;
        }
        if (! next_target_c){
            exit_status = - update_provisional_report(curr_reflecteds);
            curr_reflecteds[0] = 0;
            curr_reflecteds[1] = 0;
        }
    }

    return exit_status;
}


/**
 * @brief reflect the specified line in Dockerfile and record the number of reflected lines.
 *
 * @param[in]  line  target line
 * @return int  exit status like command's one
 */
int reflect_to_Dockerfile(const char *line){
    return __reflect_line(line, 'd', '\0');
}




/**
 * @brief reflect the Dockerfile base instructions in an empty Dockerfile.
 *
 * @param[out] fp  handler for destination file
 * @return int  exit status like command's one
 *
 * @note reflect FROM, SHELL, and WORKDIR instructions exactly one by one in this order, otherwise an error.
 */
static int __init_dockerfile(FILE *fp){
    const char *line;
    int errid = 0, idx = 0, exit_status = 0;

    int instr_ids[3] = {
        ID_FROM,
        ID_SHELL,
        ID_WORKDIR
    };

    while ((line = xfgets_for_loop(DOCKER_FILE_BASE, false, &errid))){
        if ((idx < 3) && (receive_dockerfile_instruction(line, instr_ids + idx))){
            idx++;
            fputs(line, fp);
        }
        else {
            errid = -1;
            exit_status = 1;
        }
    }

    return exit_status;
}




/******************************************************************************
    * Check Part
******************************************************************************/


/**
 * @brief check the validity of the target line as Dockerfile instruction.
 *
 * @param[in]  line  target line
 * @return int  exit status like command's one
 *
 * @note satisfy the restriction that each of CMD and ENTRYPOINT instructions is one or less in Dockerfile.
 */
static int __check_dockerfile_instruction(const char *line){
    static bool cmd_ent_duplicates[2] = {true, true};

    int instr_id = -1;
    if ((! (receive_dockerfile_instruction(line, &instr_id))) || (instr_id == ID_FROM)){
        xperror_message(line, "Invalid instruction", true);
        return 1;
    }

    // TODO: error checking function by parsing
    // 'instr_id' and the return value of the function above are useful

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
    return 0;
}




/******************************************************************************
    * Record Part
******************************************************************************/


/**
 * @brief record the number of reflected lines in various files.
 *
 * @return int  exit status like command's one
 *
 * @note some functions detect errors when initializing each file, but they are ignored.
 */
static int __record_reflected_lines(){
    int tmp, exit_status = 0;
    unsigned short prov_reflecteds[2] = {0};

    tmp = reset_provisional_report(prov_reflecteds);

    FILE *fp;
    if ((fp = fopen(REFLECT_FILE_R, "w"))){
        fprintf(fp, "d:+%hu h:+%hu\n", prov_reflecteds[0], prov_reflecteds[1]);
        fclose(fp);
    }
    else
        exit_status = 1;

    // TODO: waiting the implementation of erase commnad
    // tmp |= reflect_erase_file()

    if (tmp && check_file_size(VERSION_FILE))
        exit_status |= tmp;
    return exit_status;
}




/**
 * @brief manage the file in which the provisional numbers of reflected lines is recorded.
 *
 * @param[out] reflecteds  array of length 2 for storing the provisional numbers of reflected lines
 * @param[in]  mode  string representing the modes of length 2 or 4
 * @return int  exit status like command's one
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

                exit_status = 1;
                continue;
            }
        }

        if (mode_c == 'r'){
            if (fread(array_for_read, sizeof(unsigned short), 2, fp) == 2){
                reflecteds[0] += array_for_read[0];
                reflecteds[1] += array_for_read[1];
            }
            else
                exit_status = 1;
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
 * @brief read the provisional numbers of reflected lines from the file that recorded them.
 *
 * @param[out] prov_reflecteds  array of length 2 for storing the provisional numbers of reflected lines
 * @return int  exit status like command's one
 *
 * @attention 'prov_reflecteds' must be initialized with 0 before the first call.
 * @attention the file handler cannot be released without calling 'write_provisional_report' after this call.
 */
int read_provisional_report(unsigned short prov_reflecteds[2]){
    return __manage_provisional_report(prov_reflecteds, "r+");
}


/**
 * @brief write the provisional numbers of reflected lines to the file that records them.
 *
 * @param[in]  prov_reflecteds  array of length 2 for storing the provisional numbers of reflected lines
 * @return int  exit status like command's one
 *
 * @note basically read the current values by 'read_provisional_report', add some processing and call.
 */
int write_provisional_report(unsigned short prov_reflecteds[2]){
    return __manage_provisional_report(prov_reflecteds, "w\0");
}