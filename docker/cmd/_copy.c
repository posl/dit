/**
 * @file _copy.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'copy', that 
 * @author Tsukasa Inada
 * @date 
 */

#include "main.h"




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief 
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note treated like a normal main function.
 */
int copy(int argc, char **argv){
    fputs("copy\n", stdout);
    return 0;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void copy_test(void){
    fputs("copy test\n", stdout);
}


#endif // NDEBUG