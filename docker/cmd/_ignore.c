#include "main.h"



int ignore(int argc, char **argv){
    fputs("ignore\n", stdout);
    return 0;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void ignore_test(void){
    fputs("ignore test\n", stdout);
}


#endif // NDEBUG