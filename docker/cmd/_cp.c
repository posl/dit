#include "main.h"



int cp(int argc, char **argv){
    fputs("cp\n", stdout);
    return 0;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void cp_test(void){
    fputs("cp test\n", stdout);
}


#endif // NDEBUG