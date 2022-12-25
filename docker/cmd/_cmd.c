#include "main.h"

#define CMD_FILE "/dit/var/cmd.log"



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