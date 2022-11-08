#include "main.h"



int onbuild(int argc, char **argv){
    fputs("onbuild\n", stdout);
    return 0;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void onbuild_test(void){
    fputs("onbuild test\n", stdout);
}


#endif // NDEBUG