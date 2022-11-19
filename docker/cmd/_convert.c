#include "main.h"


#define COMMAND_LINE_FILE "/dit/tmp/last-command-line"

#define CONVERT_RESULT_FILE_D "/dit/tmp/convert-result.dock"
#define CONVERT_RESULT_FILE_H "/dit/tmp/convert-result.hist"




int convert(int argc, char **argv){
    char *line;
    int errid = 0;
    bool first_line = true;

    while (xfgets_for_loop(COMMAND_LINE_FILE, &line, &errid)){
        if (! first_line)
            errid = -1;

        first_line = false;
    }

    if (line){
        int mode2d, mode2h;

        if (! (errid || get_config(NULL, &mode2d, &mode2h))){
            int offset = 0, mode;
            const char *file_name;
            FILE* fp;

            do {
                if (! offset){
                    mode = mode2d;
                    file_name = CONVERT_RESULT_FILE_D;
                }
                else {
                    mode = mode2h;
                    file_name = CONVERT_RESULT_FILE_H;
                }

                if (mode && (fp = fopen(file_name, "w"))){
                    fprintf(fp, ("RUN %s\n" + offset), line);
                    fclose(fp);
                }

                if (offset)
                    break;
                offset = 4;
            } while (true);
        }

        free(line);
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