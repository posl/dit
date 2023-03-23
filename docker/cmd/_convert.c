#include "main.h"


#define COMMAND_LINE_FILE "/dit/srv/last-command-line"


extern const char * const convert_results[2];




int convert(int argc, char **argv){
    int exit_status = FAILURE;

    if (! get_last_exit_status()){
        char *line;
        int offset = 0;
        bool first_line = true;

        while (xfgets_for_loop(COMMAND_LINE_FILE, &line, &offset)){
            if (! first_line)
                offset = -1;

            first_line = false;
        }

        if (line){
            int modes[2];

            if (! (offset || get_config(NULL, modes))){
                FILE* fp;

                exit_status = SUCCESS;
                offset = 2;

                do
                    if (modes[--offset] && (fp = fopen(convert_results[offset], "w"))){
                        if (offset)
                            fputs("RUN ", fp);

                        fputs(line, fp);
                        fputc('\n', fp);

                        fclose(fp);
                    }
                while (offset);
            }

            free(line);
        }
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