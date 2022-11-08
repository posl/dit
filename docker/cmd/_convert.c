#include "main.h"



int convert(int argc, char **argv){
    const char * const files[2] = {
        "/dit/tmp/convert-result.dock",
        "/dit/tmp/convert-result.hist"
    };
    const char * const targets[2] = {
        "Dockerfile",
        "history-file"
    };
    const char * const outputs[2] = {
        "ENV abc=123",
        "export abc=123"
    };

    int i = 0;
    FILE* fp;

    while (1){
        if ((fp = fopen(files[i], "w"))){
            fprintf(stdout, " < %s >\n%s\n", targets[i], outputs[i]);
            fprintf(fp, "%s\n", outputs[i]);
            fclose(fp);
        }
        else {
            fprintf(stderr, "convert: unexpected error in working with '%s'\n", files[i]);
            return 1;
        }

        if (++i < 2)
            fputc('\n', stdout);
        else
            break;
    }
    return 0;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void convert_test(void){
    fputs("convert test\n", stdout);
}


#endif // NDEBUG