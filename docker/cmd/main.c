/**
 * @file main.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the main function and the utilitys commonly used in other files.
 * @author Tsukasa Inada
 * @date 2022/07/18
 */

#include "main.h"

#define EXIT_STATUS_FILE "/dit/tmp/last-exit-status"


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
        if ((cmd = __get_dit_cmd(program_name)))
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
    * Error Handling Functions
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
void xperror_invalid_arg(int code, int state, const char * restrict desc, const char * restrict arg){
    const char *format = "", *addition = "", *adjective;

    switch (code){
        case 'O':
            format = "%s: %s argument '%s' for '--%s'\n";
            addition = arg;
            break;
        case 'N':
            addition = " number of";
        case 'C':
            format = "%s: %s%s %s: '%s'\n";
            break;
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
 * @param[in]  limit  the maximum number of arguments
 *
 * @attention if limit is 2 or more, it does not work as expected.
 */
void xperror_too_many_args(unsigned int limit){
    const char *adjective;
    adjective = limit ? "two or more" : "any";
    fprintf(stderr, "%s: doesn't allow %s arguments\n", program_name, adjective);
}




/**
 * @brief print the specified error message to stderr.
 *
 * @param[in]  msg  the error message
 * @param[in]  addition  additional information, if any
 */
void xperror_message(const char * restrict msg, const char * restrict addition){
    const char *format = "%s: %s: %s\n";
    if (! addition){
        format += 4;
        addition = msg;
    }
    fprintf(stderr, format, program_name, addition, msg);
}


/**
 * @brief print a suggenstion message to stderr.
 *
 * @param[in]  cmd_flag  whether to suggest displaying the manual of each dit command
 */
void xperror_suggestion(bool cmd_flag){
    const char *tmp1 = "", *tmp2 = "";
    if (cmd_flag){
        tmp1 = program_name;
        tmp2 = " --";
    }
    fprintf(stderr, "Try 'dit %s%shelp' for more information.\n", tmp1, tmp2);
}




/******************************************************************************
    * Utilitys
******************************************************************************/


/**
 * @brief another implementation of strndup function
 *
 * @param[in]  src  string you want to make a copy of in the heap area
 * @param[in]  n  the length of string
 * @return char*  string copied in the heap area or NULL
 */
char *xstrndup(const char *src, size_t n){
    char *dest;
    if ((dest = (char *) malloc(sizeof(char) * (n + 1)))){
        char *tmp;
        tmp = dest;

        while (n--)
            *(tmp++) = *(src++);
        *tmp = '\0';
    }
    return dest;
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
int strcmp_upper_case(const char * restrict target, const char * restrict expected){
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
        memcpy(A, expected, size * sizeof(const char *));

        bool bin_search = true, break_flag = false;
        int c, min, max, mid, tmp;

        min = 0;
        max = size - 1;
        tmp = -max;

        while ((c = *(target++))){
            if (mode & 1)
                c = toupper(c);

            while (bin_search){
                if (tmp){
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
                else
                    bin_search = false;
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




/******************************************************************************
    * Check Functions
******************************************************************************/


/**
 * @brief check if the specified file is not empty.
 *
 * @param[in]  file_name  target file name
 * @return int  0 (is empty), 1 (is not empty) or -1 (unexpected error)
 */
int check_file_size(const char *file_name){
    struct stat file_stat;
    return (! lstat(file_name, &file_stat)) ? (!! file_stat.st_size) : -1;
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