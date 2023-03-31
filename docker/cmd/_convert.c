#include "main.h"


#define COMMAND_LINE_FILE "/dit/srv/last-command-line"


extern const char * const convert_results[2];




int convert(int argc, char **argv){
    int exit_status = FAILURE;

    if (! get_last_exit_status()){
        char *line;

        if ((line = get_one_liner(COMMAND_LINE_FILE))){
            unsigned char modes[2];

            if (! get_config(NULL, modes)){
                int offset = 2;
                FILE* fp;

                exit_status = SUCCESS;

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