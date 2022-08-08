/**
 * @file main.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the main function and the functions commonly used in other files.
 * @author Tsukasa Inada
 * @date 2022/07/18
 */

#include "main.h"

#define CMDS_NUM 10


static int (* const __get_subcmd(const char *target))(int, char **);




/******************************************************************************
    * Global Main Interface
******************************************************************************/


/**
 * @brief user interface for all subcommand
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 */
int main(int argc, char **argv){
    if (--argc <= 0){
        fputs("dit: requires one subcommand. See 'dit help'.\n", stderr);
        return 1;
    }

    int (* subcmd)(int, char **);
    if (! (subcmd = __get_subcmd(*(++argv)))){
        fprintf(stderr, "dit: '%s' is not a dit command. See 'dit help'.\n", *argv);
        return 1;
    }

    return subcmd(argc, argv);
}


/**
 * @brief extract the corresponding subcommand.
 *
 * @param[in]  target  string of the first argument passed on the command line
 * @return int(* const)(int, char**)  function of the desired subcommand or NULL
 *
 * @note using binary search.
 */
static int (* const __get_subcmd(const char *target))(int, char **){
    const char *cmds_str[CMDS_NUM] = {
        "commit",
        "config",
        "convert",
        "erase",
        "healthcheck",
        "help",
        "ignore",
        "inspect",
        "label",
        "optimize"
    };
    int (* const cmds_func[CMDS_NUM])(int, char **) = {
        commit,
        config,
        convert,
        erase,
        healthcheck,
        help,
        ignore,
        inspect,
        label,
        optimize
    };

    int min = 0, max = CMDS_NUM - 1, mid, tmp;
    while (min < max){
        mid = (min + max) / 2;
        tmp = strcmp(target, cmds_str[mid]);
        if (tmp)
            if (tmp > 0)
                min = mid + 1;
            else
                max = mid - 1;
        else
            return cmds_func[mid];
    }

    if ((min == max) && (! strcmp(target, cmds_str[min])))
        return cmds_func[min];
    return NULL;
}




/******************************************************************************
    * Extensions of Standard Library Functions
******************************************************************************/


/**
 * @brief extension of malloc function
 * @details added processing to exit abnormally when an error occurs.
 *
 * @param[in]  size  the number of bytes you want to reserve in the heap area
 * @return void*  pointer to the beginning of the reserved area
 */
void *xmalloc(size_t size){
    void *p;
    if (! (p = malloc(size))){
        perror("malloc");
        exit(1);
    }
    return p;
}


/**
 * @brief extension of realloc function
 * @details added processing to exit abnormally when an error occurs.
 *
 * @param[out] ptr  pointer that has already reserved a heap area
 * @param[in]  size  the number of bytes you want to again-reserve in the heap area
 * @return void*  pointer to the beginning of the again-reserved area
 */
void *xrealloc(void *ptr, size_t size){
    if (! (ptr = realloc(ptr, size))){
        perror("realloc");
        exit(1);
    }
    return ptr;
}




/******************************************************************************
    * Extra Implementation of Library Functions
******************************************************************************/


/**
 * @brief extra implementation of strndup function
 *
 * @param[in]  src  string you want to make a copy of in the heap area
 * @param[in]  n  the length of string
 * @return char*  pointer to string copied in the heap area
 */
char *xstrndup(const char *src, size_t n){
    char *dest, *tmp;
    dest = (tmp = (char *) xmalloc(sizeof(char) * (n + 1)));
    while (n--)
        *(tmp++) = *(src++);
    *tmp = '\0';
    return dest;
}




/******************************************************************************
    * Utilitys
******************************************************************************/


/**
 * @brief  detect whether the first string forwardly matches the second string.
 *
 * @param[in]  target  target string to see if it matches prefix with expected string
 * @param[in]  expected  expected string
 * @return int  comparison result
 *
 * @attention both of the arguments must not be NULL.
 */
int strcmp_forward_match(const char *target, const char *expected){
    char c;
    do
        if (! *target)
            return 0;
        else if (! *expected)
            return 1;
    while (! (c = *(target++) - *(expected++)));

    return (c < 0) ? -1 : 1;
}