/**
 * @file _package.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'package', that 
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
int package(int argc, char **argv){
    fputs("package\n", stdout);
    return 0;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void package_test(void){
    fputs("package test\n", stdout);
}


#endif // NDEBUG