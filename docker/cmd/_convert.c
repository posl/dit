#include "main.h"


#define COMMAND_LINE_FILE "/dit/tmp/last-command-line"

#define CONVERT_RESULT_FILE_D "/dit/tmp/convert-result.dock"
#define CONVERT_RESULT_FILE_H "/dit/tmp/convert-result.hist"




int convert(int argc, char **argv){
    const char *tmp, *line = NULL;
    bool first_line = true;
    int errid = 0, exit_status = SUCCESS;

    while ((tmp = xfgets_for_loop(COMMAND_LINE_FILE, true, &errid))){
        if (first_line){
            line = tmp;
            first_line = false;
        }
        else
            errid = -1;
    }

    if (line && (! errid)){
        int mode2d, mode2h;

        if (! get_config(NULL, &mode2d, &mode2h)){
            FILE* fp;

            if (mode2d){
                if ((fp = fopen(CONVERT_RESULT_FILE_D, "w"))){
                    fprintf(fp, "RUN %s\n", line);
                    fclose(fp);
                }
                else
                    exit_status = FAILURE;
            }

            if (mode2h){
                if ((fp = fopen(CONVERT_RESULT_FILE_H, "w"))){
                    fprintf(fp, "%s\n", line);
                    fclose(fp);
                }
                else
                    exit_status = FAILURE;
            }
        }
        else
            exit_status = FAILURE;
    }

    return exit_status;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void convert_test(void){
    fputs("convert test\n", stdout);
}


#endif // NDEBUG