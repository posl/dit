#include "main.h"



int label(int argc, char **argv){
    fputs("label\n", stdout);
    return 0;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void label_test(void){
    fputs("label test\n", stdout);
}


#endif // NDEBUG