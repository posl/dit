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


static int (* const __get_dit_cmd(const char *target))(int, char **);

static int __strcmp_forward_match(const char *target, const char *expected, int upper_flag);


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

/** array of strings representing each argument for '--display' in alphabetical order */
const char * const display_contents_reprs[DISPLAYS_NUM] = {
    "BOTH",
    "IN",
    "OUT"
};

/** array of strings representing each response to the Y/n question in alphabetical order */
const char * const response_reprs[RESPONSES_NUM] = {
    "NO",
    "YES"
};

/** array of strings representing each argument for '--target' in alphabetical order */
const char * const target_files_reprs[TARGETS_NUM] = {
    "both",
    "dockerfile",
    "history-file"
};


/** string representing a dit command invoked */
const char *program_name;




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
    if (--argc > 0){
        program_name = *(++argv);

        int (* cmd)(int, char **);
        if ((cmd = __get_dit_cmd(program_name)))
            return cmd(argc, argv);

        fprintf(stderr, " dit: '%s' is not a dit command\n", program_name);
    }
    else
        fputs(" dit: requires a dit command\n", stderr);

    xperror_suggestion(false);
    return 1;
}


/**
 * @brief extract the corresponding dit command.
 *
 * @param[in]  target  string that is the first argument passed on the command line
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
 * @param[in]  type  error type (0 (unrecognized), -1 (ambiguous) or others (invalid))
 * @param[in]  code  error code (0 (for optarg), positive (for number) or negative (for cmdarg))
 *
 * @note the return value of 'receive_expected_string' can be used as it is for the error code.
 */
void xperror_invalid_arg(int type, int code, ...){
    const char *adj;
    adj = (type ? ((type == -1) ? "ambiguous" : "invalid") : "unrecognized");
    fprintf(stderr, " %s: %s ", program_name, adj);

    const char *format;
    if (code){
        if (code > 0)
            fputs("number of ", stderr);
        format = "%s: '%s'";
    }
    else
        format = "argument '%s' for '--%s'";

    va_list sp;
    va_start(sp, code);
    vfprintf(stderr, format, sp);
    va_end(sp);

    fputc('\n', stderr);
}


/**
 * @brief print the valid arguments to stderr.
 *
 * @param[in]  expected  array of expected string
 * @param[in]  size  array size
 */
void xperror_valid_args(const char * const expected[], int size){
    fputs(" Valid arguments are:\n", stderr);
    for (int i = 0; i < size; i++)
        fprintf(stderr, "   - '%s'\n", expected[i]);
}




/**
 * @brief print an error message to stderr that some arguments are missing.
 *
 * @param[in]  desc  a description for the arguments
 * @param[in]  before_arg  the string previously specified as an argument, if any
 */
void xperror_missing_args(const char *desc, const char *before_arg){
    if (! desc)
        desc = "'-d', '-h' or '--target' option";
    fprintf(stderr, " %s: missing %s", program_name, desc);

    if (before_arg)
        fprintf(stderr, " after '%s'", before_arg);
    fputc('\n', stderr);
}


/**
 * @brief print an error message to stderr that the number of arguments is too many.
 *
 * @param[in]  limit  the maximum number of arguments
 *
 * @attention if limit is 2 or more, it does not work as expected.
 */
void xperror_too_many_args(unsigned int limit){
    const char *adj;
    adj = limit ? "two or more" : "any";
    fprintf(stderr, " %s: doesn't allow %s arguments\n", program_name, adj);
}




/**
 * @brief print the error message corresponding to errno to stderr, along with the program name.
 *
 */
void xperror_standards(){
    fputc(' ', stderr);
    perror(program_name);
}


/**
 * @brief print a suggenstion message to stderr.
 *
 * @param[in]  individual_flag  whether to suggest displaying the manual of each dit command
 */
void xperror_suggestion(bool individual_flag){
    fputs(" Try 'dit ", stderr);
    if (individual_flag)
        fprintf(stderr, "%s --", program_name);
    fputs("help' for more information.\n", stderr);
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
 *
 * @note considering the possibility that 'size_t' is not unsigned type.
 */
char *xstrndup(const char *src, size_t n){
    char *dest = NULL;
    if ((n >= 0) && (dest = (char *) malloc(sizeof(char) * (n + 1)))){
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
int strcmp_upper_case(const char *target, const char *expected){
    int c;
    while (! (c = toupper(*target) - *(expected++)))
        if (! *(target++))
            return 0;

    return c;
}


/**
 * @brief determine if the first string forwardly matches the second string.
 *
 * @param[in]  target  target string
 * @param[in]  expected  expected string
 * @param[in]  upper_flag  whether to capitalize all of the characters in target string
 * @return int  comparison result
 */
static int __strcmp_forward_match(const char *target, const char *expected, int upper_flag){
    int c;
    do {
        if (! *target)
            return 0;
        if (! *expected)
            return 1;

        c = *(target++);
        if (upper_flag)
            c = toupper(c);
    } while (! (c -= *(expected++)));

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
 * @param[in]  mode  mode of compare (bit 1: perform uppercase conversion, bit 2: allow forward match)
 * @return int  index number of the corresponding string, -1 (ambiguous) or others (invalid)
 *
 * @note make efficient by applying binary search sequentially from the first character of target string.
 * @attention array of expected string must be pre-sorted alphabetically.
 */
int receive_expected_string(const char *target, const char * const expected[], int size, int mode){
    if (target){
        const char *A[size];
        memcpy(A, expected, size * sizeof(const char *));

        int c, min, max, mid, tmp;
        min = 0;
        max = size - 1;

        if ((c = *target)){
            if (mode & 1)
                c = toupper(c);

            while (min < max){
                mid = (min + max) / 2;

                if ((tmp = *(A[mid]++))){
                    tmp -= c;
                    if (tmp){
                        if (tmp < 0)
                            min = mid + 1;
                        else
                            max = mid - 1;
                    }
                    else {
                        tmp = mid;
                        while ((--tmp >= min) && (c == *(A[tmp]++)));
                        min = tmp + 1;

                        tmp = mid;
                        while ((++tmp <= max) && (c == *(A[tmp]++)));
                        max = tmp - 1;

                        if (! (c = *(++target)))
                            break;
                        if (mode & 1)
                            c = toupper(c);
                    }
                }
                else
                    min = mid + 1;
            }
        }

        if (min == max){
            tmp =
                (mode & 2) ? __strcmp_forward_match(target, A[min], (mode & 1)) :
                (mode & 1) ? strcmp_upper_case(target, A[min]) :
                strcmp(target, A[min]);

            if (! tmp)
                return min;
        }
        else if (min < max)
            return -1;

        return -2;
    }
    return -3;
}