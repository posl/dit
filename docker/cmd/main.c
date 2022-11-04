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

#define EXIT_STATUS_FILE "/dit/tmp/last-exit-status"

#define XFGETS_NESTINGS_MAX 2


/** Data type for storing the information for one loop for 'xfgets_for_loop' */
typedef struct {
    const char *src_file;    /** source file name or NULL */
    FILE *fp;                /** handler for the source file */
    char *dest;              /** pointer to the beginning of the dynamic memory allocated */
    size_t curr_size;        /** the size of dynamic memory currently in use */
    size_t curr_length;      /** the total length of the preserved strings including null characters */
} xfgets_info;


static int (* const get_dit_cmd(const char *target))(int, char **);


/** array of strings in alphabetical order representing each dit command */
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


/** array of strings in alphabetical order representing each response to the Y/n question */
const char * const assume_args[ARGS_NUM] = {
    "NO",
    "QUIT",
    "YES"
};

/** array of strings in alphabetical order representing how to handle the blank lines */
const char * const blank_args[ARGS_NUM] = {
    "preserve",
    "squeeze",
    "truncate"
};

/** array of strings in alphabetical order representing what contents should be displayed */
const char * const display_args[ARGS_NUM] = {
    "BOTH",
    "IN",
    "OUT"
};

/** array of strings in alphabetical order representing whether the target is Dockerfile or history-file */
const char * const target_args[ARGS_NUM] = {
    "both",
    "dockerfile",
    "history-file"
};


/** array of strings in alphabetical order representing each Dockerfile instruction */
const char * const docker_instr_reprs[DOCKER_INSTRS_NUM] = {
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
    "MAINTAINER",
    "ONBUILD",
    "RUN",
    "SHELL",
    "STOPSIGNAL",
    "USER",
    "VOLUME",
    "WORKDIR"
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
        if ((cmd = get_dit_cmd(program_name)))
            return cmd(argc, argv);

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
    return FAILURE;
}


/**
 * @brief extract the corresponding dit command.
 *
 * @param[in]  target  target string
 * @return int(* const)(int, char**)  function of the desired dit command or NULL
 */
static int (* const get_dit_cmd(const char *target))(int, char **){
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
    int (* cmd)(int, char **) = NULL;

    if ((i = receive_expected_string(target, cmd_reprs, CMDS_NUM, 0)) >= 0){
        cmd = cmd_funcs[i];

        FILE *fp;
        int mode;

        i = 1;
        do {
            if (i){
                fp = stdout;
                mode = (cmd == inspect) ? _IOFBF : _IOLBF;
            }
            else {
                fp = stderr;
                mode = _IONBF;
            }

            setvbuf(fp, NULL, mode, 0);
        } while (i--);
    }

    return cmd;
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
 * @param[in]  reprs  array of expected strings
 * @param[in]  size  array size
 */
void xperror_valid_args(const char * const reprs[], size_t size){
    fputs("Valid arguments are:\n", stderr);

    for (int i = 0; i < size; i++)
        fprintf(stderr, "  - '%s'\n", reprs[i]);
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
 * @note if 'limit' is negative integer, print an error message about specifying both files as targets.
 * @attention if 'limit' is 2 or more, it does not work as expected.
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
 * @param[in]  msg  the error message or NULL
 * @param[in]  addition  additional information, if any
 *
 * @note if 'msg' is NULL, print an error message about manipulating an internal file.
 */
void xperror_message(const char * restrict msg, const char * restrict addition){
    int offset = 0;

    if (! msg)
        msg = "unexpected error while manipulating an internal file";
    if (! addition){
        offset = 4;
        addition = msg;
    }

    fprintf(stderr, ("%s: %s: %s\n" + offset), program_name, addition, msg);
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
 * @note this function updates the contents of 'p_errid' only when an error occurs while opening the file.
 * @note if the contents of 'p_errid' was set to something other than 0, perform finish processing.
 * @note the trailing newline character of the line that is the return value is stripped.
 *
 * @attention the string returned at the first call should be released at the end if you preserve read lines.
 * @attention this function must be called until NULL is returned, since it uses dynamic memory internally.
 */
char *xfgets_for_loop(const char *src_file, bool preserve_flag, int *p_errid){
    static int info_idx = -1;
    static xfgets_info info_list[XFGETS_NESTINGS_MAX];

    xfgets_info *p_info;
    bool allocate_flag = false;

    p_info = info_list + info_idx;

    if ((info_idx < 0) || (p_info->src_file != src_file)){
        FILE *fp;
        errno = 0;

        if ((info_idx < (XFGETS_NESTINGS_MAX - 1)) && (fp = src_file ? fopen(src_file, "r") : stdin)){
            info_idx++;
            p_info++;

            p_info->src_file = src_file;
            p_info->fp = fp;
            p_info->dest = NULL;
            p_info->curr_size = 1023;
            p_info->curr_length = 0;

            allocate_flag = true;
        }
        else {
            if (p_errid)
                *p_errid = errno;
            return NULL;
        }
    }

    size_t length;
    char *start;
    unsigned int tmp;
    bool reset_flag = true;

    length = p_info->curr_length;

    do {
        if (allocate_flag){
            if ((start = (char *) realloc(p_info->dest, (sizeof(char) * p_info->curr_size)))){
                p_info->dest = start;
                allocate_flag = false;
                continue;
            }
        }
        else if ((! (p_errid && *p_errid)) && ((tmp = p_info->curr_size - length) > 1)){
            start = p_info->dest + length;

            if (fgets(start, tmp, p_info->fp)){
                length += strlen(start);

                if (p_info->dest[length - 1] != '\n'){
                    tmp = p_info->curr_size + 1;
                    if ((tmp <<= 1)){
                        p_info->curr_size = tmp - 1;
                        allocate_flag = true;
                        continue;
                    }
                }
                else {
                    p_info->dest[--length] = '\0';
                    reset_flag = false;
                }
            }
            else if (length != p_info->curr_length)
                reset_flag = false;
        }

        if (reset_flag){
            if (src_file)
                fclose(p_info->fp);
            else
                clearerr(p_info->fp);

            if ((! preserve_flag) && p_info->dest)
                free(p_info->dest);

            start = NULL;
            info_idx--;
        }
        else if (preserve_flag){
            start = p_info->dest + p_info->curr_length;
            p_info->curr_length = length + 1;
        }
        else
            start = p_info->dest;

        return start;
    } while (true);
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
    int c, d;

    do {
        c = (unsigned char) *(target++);
        d = (unsigned char) *(expected++);
    } while ((! (d = toupper(c) - d)) && c);

    return d;
}




/******************************************************************************
    * String Recognizers
******************************************************************************/


/**
 * @brief receive the passed string as a positive integer or a set of two positive integer.
 *
 * @param[in]  target  target string
 * @param[out] p_left  if enable a range specification by using a hyphen, variable to store the left integer
 * @return int  the resulting (right) integer or -1
 *
 * @note receive an integer that can be expressed as "/^[0-9]+$/" in a regular expression.
 * @note when specifying a range by using a hyphen and omitting an integer, it is interpreted as specifying 0.
 */
int receive_positive_integer(const char *target, int *p_left){
    int curr = -1;

    if (target){
        int c, prev;

        curr = 0;
        c = (unsigned char) *target;

        do {
            if (isdigit(c)){
                if (curr <= (INT_MAX / 10)){
                    prev = curr * 10;
                    curr = prev + (c - '0');

                    if (curr >= prev)
                        continue;
                }
            }
            else if (p_left && (c == '-')){
                *p_left = curr;
                p_left = NULL;
                curr = 0;
                continue;
            }
            curr = -1;
            break;
        } while ((c = (unsigned char) *(++target)));
    }

    return curr;
}


/**
 * @brief find the passed string in array of expected strings.
 *
 * @param[in]  target  target string
 * @param[in]  reprs  array of expected strings
 * @param[in]  size  array size
 * @param[in]  mode  mode of comparison (bit 1: perform uppercase conversion, bit 2: accept forward match)
 * @return int  index number of the corresponding string, -1 (ambiguous) or -2 (invalid)
 *
 * @note make efficient by applying binary search sequentially from the first character of target string.
 * @attention array of expected strings must be pre-sorted alphabetically.
 */
int receive_expected_string(const char *target, const char * const reprs[], size_t size, unsigned int mode){
    if (target && (size > 0)){
        const char *expecteds[size];
        memcpy(expecteds, reprs, (sizeof(const char *) * size));

        bool upper_case, forward_match, break_flag = false;
        int min, max, tmp, c, mid;

        upper_case = mode & 0b01;
        forward_match = mode & 0b10;

        min = 0;
        max = size - 1;
        tmp = -max;

        while ((c = (unsigned char) *(target++))){
            if (upper_case)
                c = toupper(c);

            while (tmp){
                if (tmp < 0){
                    mid = (min + max) / 2;

                    tmp = c - ((unsigned char) *(expecteds[mid]++));
                    if (tmp){
                        if (tmp > 0)
                            min = mid + 1;
                        else
                            max = mid - 1;
                    }
                    else {
                        tmp = mid;
                        while ((--tmp >= min) && (c == ((unsigned char) *(expecteds[tmp]++))));
                        min = ++tmp;

                        tmp = mid;
                        while ((++tmp <= max) && (c == ((unsigned char) *(expecteds[tmp]++))));
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
            else if (c != ((unsigned char) *(expecteds[min]++)))
                return -2;
        }

        if (! tmp)
            return (forward_match || (! *(expecteds[min]))) ? min : -2;
    }
    return -1;
}


/**
 * @brief analyze which instruction in Dockerfile the specified line corresponds to.
 *
 * @param[in]  line  target line with only one trailing newline character
 * @param[out] p_id  variable to store index number of the instruction to be compared
 * @return char*  substring that is the argument for the instruction in the target line or NULL
 *
 * @note when there is a corresponding instruction, its index number is stored in 'p_id'.
 * @attention the instruction must not contain unnecessary leading white spaces.
 */
char *receive_dockerfile_instruction(char *line, int *p_id){
    char *tmp;
    size_t instr_len = 0;

    tmp = line;
    while (! isspace((unsigned char) *(tmp++)))
        instr_len++;

    char instr[instr_len + 1];
    memcpy(instr, line, (sizeof(char) * instr_len));
    instr[instr_len] = '\0';

    if ((*p_id >= 0) && (*p_id < DOCKER_INSTRS_NUM)){
        if (xstrcmp_upper_case(instr, docker_instr_reprs[*p_id]))
            tmp = NULL;
    }
    else if ((*p_id = receive_expected_string(instr, docker_instr_reprs, DOCKER_INSTRS_NUM, 1)) < 0)
        tmp = NULL;

    if (tmp)
        while (isspace((unsigned char) *tmp))
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
 *
 * @note if the file is too large, set an error number indicating that.
 */
int check_file_size(const char *file_name){
    struct stat file_stat;
    int i = -1;

    if (! stat(file_name, &file_stat)){
        if ((file_stat.st_size >= 0) && (file_stat.st_size <= INT_MAX))
            i = (int) file_stat.st_size;
        else {
            i = -2;
            errno = EFBIG;
        }
    }

    return i;
}


/**
 * @brief check the exit status of last executed command line.
 *
 * @return int  the resulting integer or -1
 *
 * @attention internally, it uses 'xfgets_for_loop' with a depth of 1.
 */
int check_last_exit_status(void){
    const char *line;
    int errid = 0, i = -1;

    while ((line = xfgets_for_loop(EXIT_STATUS_FILE, false, &errid))){
        errid = -1;

        if (((i = receive_positive_integer(line, NULL)) >= 0) && (i >= 256))
            i = -1;
    }

    return i;
}