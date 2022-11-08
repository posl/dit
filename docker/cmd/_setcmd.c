/**
 * @file _setcmd.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'setcmd', 
 * @author Tsukasa Inada
 * @date 
 *
 * @note 
 */

#include "main.h"

#define SETCMD_FILE "/dit/var/setcmd.log"




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
int setcmd(int argc, char **argv){
    fputs("setcmd\n", stdout);
    return 0;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void setcmd_test(void){
    fputs("setcmd test\n", stdout);
}


#endif // NDEBUG