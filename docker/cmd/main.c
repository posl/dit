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


const char * const dit_cmds[CMDS_NUM] = {
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
    "setcmd"
};




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
        int (* cmd)(int, char **);
        if ((cmd = __get_dit_cmd(*(++argv))))
            return cmd(argc, argv);

        fprintf(stderr, "dit: '%s' is not a dit command\n", *argv);
    }
    else
        fputs("dit: requires a dit command\n", stderr);

    fputs("Try 'dit help' for more information.\n", stderr);
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
        setcmd
    };

    int i;
    return ((i = receive_expected_string(target, dit_cmds, CMDS_NUM, 0)) >= 0) ? cmd_funcs[i] : NULL;
}




/******************************************************************************
    * Utilitys
******************************************************************************/


/**
 * @brief another implementation of strndup function
 *
 * @param[in]  src  string you want to make a copy of in the heap area
 * @param[in]  n  the length of string
 * @return char*  pointer to string copied in the heap area or NULL
 *
 * @note considering the possibility that size_t type is not unsigned.
 */
char *xstrndup(const char *src, size_t n){
    char *dest;
    if ((n < 0) || (! (dest = (char *) malloc(sizeof(char) * (n + 1)))))
        return NULL;

    char *tmp;
    tmp = dest;
    while (n--)
        *(tmp++) = *(src++);
    *tmp = '\0';
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
    * Argument Parsers
******************************************************************************/


/**
 * @brief receive the passed string as positive integer.
 *
 * @param[in]  target  target string
 * @return int  the resulting integer or -1
 *
 * @note receive an integer that can be expressed as "/^[0-9]+$/" in a regular expression.
 */
int receive_positive_integer(const char *target){
    int i = 0;
    do
        if (isdigit(*target)){
            i *= 10;
            i += (*target - '0');
        }
        else
            return -1;
    while (*(++target));

    return i;
}


/**
 * @brief find the passed string in array of expected strings.
 *
 * @param[in]  target  target string
 * @param[in]  expected  array of expected string
 * @param[in]  size  array size
 * @param[in]  mode  mode of compare (bit 1: perform uppercase conversion, bit 2: allow forward match)
 * @return int  index number of the corresponding string, -1 (ambiguous) or -2 (invalid)
 *
 * @note make efficient by applying binary search sequentially from the first character of target string.
 * @attention array of expected string must be pre-sorted alphabetically.
 */
int receive_expected_string(const char *target, const char * const expected[], int size, int mode){
    const char *A[size];
    memcpy(A, expected, size * sizeof(const char *));

    int min, max, mid, tmp;
    min = 0;
    max = size - 1;

    char c;
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
                    else if (mode & 1)
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


/**
 * @brief receive a yes or no response to a query.
 *
 * @param[in]  target  target string
 * @return int  the resulting answer or zero
 */
int receive_yes_or_no(const char *target){
    const char * const expected[] = {
        "NO",
        "YES"
    };
    int i;
    return ((! (i = receive_expected_string(target, expected, 2, 3))) ? 'y' : ((i > 0) ? 'n' : '\0'));
}