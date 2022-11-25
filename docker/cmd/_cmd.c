/**
 * @file _cmd.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'cmd', 
 * @author Tsukasa Inada
 * @date 
 *
 * @note 
 */

#include "main.h"

#define CMD_FILE "/dit/var/cmd.log"




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
int cmd(int argc, char **argv){
    fputs("cmd\n", stdout);
    return 0;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void cmd_test(void){
    fputs("cmd test\n", stdout);
}


#endif // NDEBUG