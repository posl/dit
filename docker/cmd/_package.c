/**
 * @file _package.c
 *
 * Copyright (c) 2023 Tsukasa Inada
 *
 * @brief Described the dit command 'package', that 
 * @author Tsukasa Inada
 * @date 
 */

#include "main.h"




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief 
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note treated like a normal main function.
 */
int package(int argc, char **argv){
    char *line;

    if ((line = get_one_liner("/dit/etc/package_manager"))){
        if (! strcmp(line, "apk")){
            char * const cmd[] = { "apk", "add", "--no-cache", "bash", NULL };

            if (! execute("/sbin/apk", cmd, 1)){
                char *instr = "RUN apk add --no-cache bash";

                if (! get_file_size(DIT_PROFILE)){
                    fputs(instr, stdout);
                    fputc('\n', stdout);
                }
                else
                    reflect_to_dockerfile(1, instr, true, '\0');
            }
        }
        free(line);
    }
    return SUCCESS;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void package_test(void){
    fputs("package test\n", stdout);
}


#endif // NDEBUG