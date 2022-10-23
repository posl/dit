/**
 * @file main.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the main function and the utilitys commonly used in files for each dit command.
 * @author Tsukasa Inada
 * @date 2022/07/18
 */

#include "main.h"

#define XFGETS_NESTINGS_MAX 2

#define DOCKER_INSTRS_NUM 17

#define EXIT_STATUS_FILE "/dit/tmp/last-exit-status"


/** Data type for storing the information for one loop for 'xfgets_for_loop' */
typedef struct {
    const char *src_file;    /** source file name or NULL */
    FILE *fp;                /** handler for the source file */
    char *dest;              /** pointer to the beginning of the dynamic memory allocated */
    size_t curr_size;        /** the size of dynamic memory currently in use */
    size_t curr_length;      /** the total length of the preserved strings including null characters */
} xfgets_info;


static int (* const __get_dit_cmd(const char *target))(int, char **);


/** array of strings representing each dit command in alphabetical order */
const char * const cmd_reprs[CMDS_NUM] = {
    "config",
    "convert",
    "cp",
    "erase",
    "healthcheck",
    "help",
    "ignore",
    "inspect",
    "label",
    "onbuild",
    "optimize",
    "reflect",
    "setcmd"
};


/** array of strings representing each response to the Y/n question in alphabetical order */
const char * const assume_args[ASSUMES_NUM] = {
    "NO",
    "YES"
};

/** array of strings representing each argument for '--display' in alphabetical order */
const char * const display_args[DISPLAYS_NUM] = {
    "BOTH",
    "IN",
    "OUT"
};

/** array of strings representing each argument for '--target' in alphabetical order */
const char * const target_args[TARGETS_NUM] = {
    "both",
    "dockerfile",
    "history-file"
};


/** string representing a dit command invoked */
static const char *program_name;




/******************************************************************************
    * Global Main Interface
******************************************************************************/


/**
 * @brief user interface for all dit commands
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 */
int main(int argc, char **argv){
    const char *desc = "command";

    if (--argc > 0){
        program_name = *(++argv);

        int (* cmd)(int, char **);
        if ((cmd = __get_dit_cmd(program_name))){
            setvbuf(stdout, NULL, _IOFBF, 0);
            setvbuf(stderr, NULL, _IOLBF, 0);
            return cmd(argc, argv);
        }

        const char *arg;
        arg = program_name;
        program_name = *(--argv);
        xperror_invalid_arg('C', 1, desc, arg);
    }
    else {
        program_name = *argv;
        xperror_missing_args(desc, NULL);
    }

    xperror_suggestion(false);
    return 1;
}


/**
 * @brief extract the corresponding dit command.
 *
 * @param[in]  target  target string
 * @return int(* const)(int, char**)  function of the desired dit command or NULL
 */
static int (* const __get_dit_cmd(const char *target))(int, char **){
    int (* const cmd_funcs[CMDS_NUM])(int, char **) = {
        config,
        convert,
        cp,
        erase,
        healthcheck,
        help,
        ignore,
        inspect,
        label,
        onbuild,
        optimize,
        reflect,
        setcmd
    };

    int i;
    return ((i = receive_expected_string(target, cmd_reprs, CMDS_NUM, 0)) >= 0) ? cmd_funcs[i] : NULL;
}




/******************************************************************************
    * Error Message Functions
******************************************************************************/


/**
 * @brief print an error message about invalid argument to stderr.
 *
 * @param[in]  code  error code ('O' (for optarg), 'N' (for number) or 'C' (for cmdarg))
 * @param[in]  state  error state (0 (unrecognized), -1 (ambiguous) or others (invalid))
 * @param[in]  desc  the description for the argument
 * @param[in]  arg  actual argument passed on the command line
 *
 * @note the return value of 'receive_expected_string' can be used as it is for 'state'.
 */
void xperror_invalid_arg(int code_c, int state, const char * restrict desc, const char * restrict arg){
    const char *format = "", *addition = "", *adjective;

    switch (code_c){
        case 'O':
            format = "%s: %s argument '%s' for '--%s'\n";
            addition = arg;
            break;
        case 'N':
            addition = " number of";
        case 'C':
            format = "%s: %s%s %s: '%s'\n";
    }
    adjective = (state ? ((state == -1) ? "ambiguous" : "invalid") : "unrecognized");

    fprintf(stderr, format, program_name, adjective, addition, desc, arg);
}


/**
 * @brief print the valid arguments to stderr.
 *
 * @param[in]  expected  array of expected string
 * @param[in]  size  array size
 */
void xperror_valid_args(const char * const expected[], size_t size){
    fputs("Valid arguments are:\n", stderr);
    for (int i = 0; i < size; i++)
        fprintf(stderr, "  - '%s'\n", expected[i]);
}




/**
 * @brief print an error message to stderr that some arguments are missing.
 *
 * @param[in]  desc  the description for the arguments or NULL
 * @param[in]  before_arg  the immediately preceding argument, if any
 *
 * @note if 'desc' is NULL, print an error message about specifying the target file.
 */
void xperror_missing_args(const char * restrict desc, const char * restrict before_arg){
    char format[] = "%s: missing %s operand after '%s'\n";
    int offset = 0;

    if (! desc){
        desc = "'-d', '-h' or '--target' option";
        offset = 14;
    }
    else if (! before_arg)
        offset = 22;

    if (offset){
        format[offset++] = '\n';
        format[offset] = '\0';
    }
    fprintf(stderr, format, program_name, desc, before_arg);
}


/**
 * @brief print an error message to stderr that the number of arguments is too many.
 *
 * @param[in]  limit  the maximum number of arguments or negative integer
 *
 * @attention if limit is 2 or more, it does not work as expected.
 */
void xperror_too_many_args(int limit){
    char format[] = "%s: No %sarguments allowed when reflecting in both files\n";
    const char *adjective = "";

    if (limit >= 0){
        if (limit > 0)
            adjective = "more than two ";
        format[26] = '\n';
        format[27] = '\0';
    }
    fprintf(stderr, format, program_name, adjective);
}




/**
 * @brief print the specified error message to stderr.
 *
 * @param[in]  msg  the error message
 * @param[in]  addition  additional information, if any
 * @param[in]  omit_newline  whether to omit the trailing newline character
 */
void xperror_message(const char * restrict msg, const char * restrict addition, bool omit_newline){
    char format[] = "%s: %s: %s\n";
    int offset = 0;

    if (! addition){
        offset = 4;
        addition = msg;
    }
    if (omit_newline)
        format[10] = '\0';

    fprintf(stderr, (format + offset), program_name, addition, msg);
}


/**
 * @brief print a suggenstion message to stderr.
 *
 * @param[in]  cmd_flag  whether to suggest displaying the manual of each dit command
 */
void xperror_suggestion(bool cmd_flag){
    const char *tmp1, *tmp2;
    if (cmd_flag){
        tmp1 = program_name;
        tmp2 = " --";
    }
    else
        tmp1 = (tmp2 = "");

    fprintf(stderr, "Try 'dit %s%shelp' for more information.\n", tmp1, tmp2);
}




/******************************************************************************
    * Utilitys
******************************************************************************/


/**
 * @brief read the contents of the specified file exactly one line at a time.
 *
 * @param[in]  src_file  source file name or NULL
 * @param[in]  preserve_flag  whether to preserve or overwrite the previously read lines
 * @param[out] p_errid  variable to store the error ID when an error occurs
 * @return char*  the resulting line or NULL
 *
 * @note read to the end of the file by using it as a conditional expression in a loop statement.
 * @note this function can be nested by passing different 'src_file' to a depth of 'XFGETS_NESTINGS_MAX'.
 * @note all lines of the file whose size is too large to be represented by int type cannot be preserved.
 * @note this function only updates the contents of 'p_errid' when an error occurs while opening the file.
 * @note if the contents of 'p_errid' was set to something other than 0, perform finish processing.
 *
 * @attention the string returned at the first call should be released at the end if you preserve read lines.
 * @attention this function must be called until NULL is returned, since it uses dynamic memory internally.
 */
char *xfgets_for_loop(const char *src_file, bool preserve_flag, int *p_errid){
    static int info_idx = -1;
    static xfgets_info info_list[XFGETS_NESTINGS_MAX];

    xfgets_info *p_info;
    bool allocate_flag = false, reset_flag = true;
    char *tmp;
    size_t length;

    p_info = info_list + info_idx;

    if ((info_idx < 0) || (p_info->src_file != src_file)){
        if (info_idx >= (XFGETS_NESTINGS_MAX - 1))
            return NULL;

        FILE *fp;
        if (! (fp = src_file ? fopen(src_file, "r") : stdin)){
            if (p_errid)
                *p_errid = errno;
            return NULL;
        }

        info_idx++;
        p_info++;

        p_info->src_file = src_file;
        p_info->fp = fp;
        p_info->dest = NULL;
        p_info->curr_size = 1023;
        p_info->curr_length = 0;

        allocate_flag = true;
    }

    length = p_info->curr_length;

    do {
        if (allocate_flag){
            if ((tmp = (char *) realloc(p_info->dest, (sizeof(char) * p_info->curr_size)))){
                p_info->dest = tmp;
                allocate_flag = false;
                continue;
            }
        }
        else if (! (p_errid && *p_errid)){
            tmp = p_info->dest + length;
            if (fgets(tmp, (p_info->curr_size - length), p_info->fp)){
                length += strlen(tmp);

                if ((reset_flag = (p_info->dest[length - 1] != '\n'))){
                    p_info->curr_size++;
                    if ((p_info->curr_size <<= 1)){
                        p_info->curr_size--;
                        allocate_flag = true;
                        continue;
                    }
                }
            }
            else if (length != p_info->curr_length){
                p_info->dest[length++] = '\n';
                p_info->dest[length] = '\0';
                reset_flag = false;
            }
        }

        if (reset_flag){
            if (src_file)
                fclose(p_info->fp);
            else
                clearerr(p_info->fp);

            if ((! preserve_flag) && p_info->dest)
                free(p_info->dest);

            tmp = NULL;
            info_idx--;
        }
        else if (preserve_flag){
            tmp = p_info->dest + p_info->curr_length;
            p_info->curr_length = length + 1;
        }
        else
            tmp = p_info->dest;

        return tmp;
    } while (1);
}


/**
 * @brief determine if the first string after uppercase conversion matches the second string.
 *
 * @param[in]  target  target string
 * @param[in]  expected  expected string
 * @return int  comparison result
 *
 * @attention expected string must not contain lowercase letters.
 */
int xstrcmp_upper_case(const char * restrict target, const char * restrict expected){
    int c;
    while (! (c = toupper(*target) - *(expected++)))
        if (! *(target++))
            return 0;
    return c;
}




/******************************************************************************
    * String Recognizers
******************************************************************************/


/**
 * @brief receive the passed string as a positive integer.
 *
 * @param[in]  target  target string
 * @return int  the resulting integer or -1
 *
 * @note receive an integer that can be expressed as "/^[0-9]+$/" in a regular expression.
 */
int receive_positive_integer(const char *target){
    int i = -1;
    if (target){
        i = 0;
        do
            if (isdigit(*target)){
                i *= 10;
                i += (*target - '0');
            }
            else {
                i = -1;
                break;
            }
        while (*(++target));
    }
    return i;
}


/**
 * @brief find the passed string in array of expected strings.
 *
 * @param[in]  target  target string
 * @param[in]  expected  array of expected string
 * @param[in]  size  array size
 * @param[in]  mode  mode of comparison (bit 1: perform uppercase conversion, bit 2: accept forward match)
 * @return int  index number of the corresponding string, -1 (ambiguous) or others (invalid)
 *
 * @note make efficient by applying binary search sequentially from the first character of target string.
 * @attention array of expected string must be pre-sorted alphabetically.
 */
int receive_expected_string(const char *target, const char * const expected[], size_t size, int mode){
    if (target && (size > 0)){
        const char *A[size];
        memcpy(A, expected, (sizeof(const char *) * size));

        int c, tmp, mid, min, max;
        bool break_flag = false;

        min = 0;
        max = size - 1;
        tmp = -max;

        while ((c = *(target++))){
            if (mode & 1)
                c = toupper(c);

            while (tmp){
                if (tmp < 0){
                    mid = (min + max) / 2;

                    tmp = *(A[mid]++) - c;
                    if (tmp){
                        if (tmp < 0)
                            min = mid + 1;
                        else
                            max = mid - 1;
                    }
                    else {
                        tmp = mid;
                        while ((--tmp >= min) && (c == *(A[tmp]++)));
                        min = ++tmp;

                        tmp = mid;
                        while ((++tmp <= max) && (c == *(A[tmp]++)));
                        max = --tmp;

                        break_flag = true;
                    }

                    tmp = min - max;
                    if (break_flag)
                        break;
                }
                else
                    return -2;
            }

            if (break_flag)
                break_flag = false;
            else if (c != *(A[min]++))
                return -2;
        }

        return (! tmp) ? (((mode & 2) || (! *(A[min]))) ? min : -2) : -1;
    }
    return -3;
}


/**
 * @brief analyze which instruction in Dockerfile the specified line corresponds to.
 *
 * @param[in]  line  target line with only one trailing newline character
 * @param[out] p_id  variable to store index number of the instruction to be compared
 * @return char*  substring that is the argument for the instruction in the target line or NULL
 *
 * @note when there is a corresponding instruction, its index number is stored in 'p_id'.
 * @attention the instruction must not contain unnecessary leading white space and must be capitalized.
 */
char *receive_dockerfile_instruction(const char *line, int *p_id){
    const char * const instr_reprs[DOCKER_INSTRS_NUM] = {
        "ADD",
        "ARG",
        "CMD",
        "COPY",
        "ENTRYPOINT",
        "ENV",
        "EXPOSE",
        "FROM",
        "HEALTHCHECK",
        "LABEL",
        "ONBUILD",
        "RUN",
        "SHELL",
        "STOPSIGNAL",
        "USER",
        "VOLUME",
        "WORKDIR"
    };

    char *tmp;
    size_t instr_len = 0;

    tmp = line;
    while (! isspace(*(tmp++)))
        instr_len++;

    char instr[instr_len + 1];
    memcpy(instr, line, (sizeof(char) * instr_len));
    instr[instr_len] = '\0';

    if ((*p_id >= 0) && (*p_id < DOCKER_INSTRS_NUM)){
        if (strcmp(instr, instr_reprs[*p_id]))
            tmp = NULL;
    }
    else if ((*p_id = receive_expected_string(instr, instr_reprs, DOCKER_INSTRS_NUM, 0)) < 0)
        tmp = NULL;

    if (tmp)
        while (isspace(*tmp))
            tmp++;

    return tmp;
}




/******************************************************************************
    * Check Functions
******************************************************************************/


/**
 * @brief check if the size of the specified file is not too large to be represented by int type.
 *
 * @param[in]  file_name  target file name
 * @return int  the resulting file size, -1 (unexpected error) or -2 (too large)
 */
int check_file_size(const char *file_name){
    struct stat file_stat;
    int i = -1;

    if (! stat(file_name, &file_stat))
        i = ((file_stat.st_size >= 0) && (file_stat.st_size <= INT_MAX)) ? ((int) file_stat.st_size) : -2;
    return i;
}


/**
 * @brief check the exit status of last executed command line.
 *
 * @return int  the resulting integer or -1
 */
int check_last_exit_status(){
    FILE *fp;
    int i = -1;

    if ((fp = fopen(EXIT_STATUS_FILE, "r"))){
        unsigned int u;
        if ((fscanf(fp, "%u", &u) == 1) && (! (u >> 8)))
            i = u;
        fclose(fp);
    }
    return i;
}