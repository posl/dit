#include "main.h"


#define COMMAND_LINE_FILE "/dit/srv/last-command-line"

#define CONVERT_RESULT_FILE_D "/dit/srv/convert-result.dock"
#define CONVERT_RESULT_FILE_H "/dit/srv/convert-result.hist"




int convert(int argc, char **argv){
    int exit_status = FAILURE;

    if (! get_last_exit_status()){
        char *line;
        int errid = 0;
        bool first_line = true;

        while (xfgets_for_loop(COMMAND_LINE_FILE, &line, &errid)){
            if (! first_line)
                errid = -1;

            first_line = false;
        }

        if (line){
            int modes[2];

            if (! (errid || get_config(NULL, modes))){
                int offset = 0, mode;
                const char *file_name;
                FILE* fp;

                exit_status = SUCCESS;

                do {
                    if (! offset){
                        mode = modes[0];
                        file_name = CONVERT_RESULT_FILE_D;
                    }
                    else {
                        mode = modes[1];
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