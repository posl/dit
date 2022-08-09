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


static int (* const __get_subcmd(const char *target))(int, char **);


const char * const subcmds[CMDS_NUM] = {
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
    if (--argc > 0){
        int (* subcmd)(int, char **);
        if ((subcmd = __get_subcmd(*(++argv))))
            return subcmd(argc, argv);

        fprintf(stderr, "dit: '%s' is not a dit command.\n", *argv);
    }
    else
        fputs("dit: requires one subcommand.\n", stderr);

    fputs("Try 'dit help' for more information.\n", stderr);
    return 1;
}


/**
 * @brief extract the corresponding subcommand.
 *
 * @param[in]  target  string that is the first argument passed on the command line
 * @return int(* const)(int, char**)  function of the desired subcommand or NULL
 */
static int (* const __get_subcmd(const char *target))(int, char **){
    int (* const cmd_funcs[CMDS_NUM])(int, char **) = {
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

    int i;
    return ((i = bsearch_subcmds(target, strcmp)) >= 0) ? cmd_funcs[i] : NULL;
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
 *
 * @note considering the possibility that size_t type is not unsigned.
 */
char *xstrndup(const char *src, size_t n){
    if (n < 0){
        fputs("xstrndup: the length of string must not be a negative integer", stderr);
        exit(1);
    }

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
 * @brief search subcommands for target string.
 *
 * @param[in]  target  target string
 * @param[in]  comp  comparison function
 * @return int  index of the corresponding subcommand or -1
 *
 * @note using binary search.
 */
int bsearch_subcmds(const char *target, int (* const comp)(const char *, const char *)){
    int min = 0, max = CMDS_NUM - 1, mid, tmp;
    while (min < max){
        mid = (min + max) / 2;
        tmp = comp(target, subcmds[mid]);
        if (tmp){
            if (tmp > 0)
                min = mid + 1;
            else
                max = mid - 1;
        }
        else
            return mid;
    }

    return ((min == max) && (! comp(target, subcmds[min]))) ? min : -1;
}


/**
 * @brief detect whether the first string forwardly matches the second string.
 *
 * @param[in]  target  target string
 * @param[in]  expected  expected string
 * @return int  comparison result
 *
 * @attention both of the arguments must not be NULL.
 */
int strcmp_forward_match(const char *target, const char *expected){
    signed char c;
    do
        if (! *target)
            return 0;
        else if (! *expected)
            return 1;
    while (! (c = *(target++) - *(expected++)));

    return (c < 0) ? -1 : 1;
}