#include "main.h"



int healthcheck(int argc, char **argv){
    fputs("healthcheck\n", stdout);
    return 0;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void healthcheck_test(void){
    fputs("healthcheck test\n", stdout);
}


#endif // NDEBUG