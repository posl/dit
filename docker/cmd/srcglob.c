/**
 * @file srcglob.c
 *
 * Copyright (c) 2023 Tsukasa Inada
 *
 * @brief Described the extra command 'srcglob', that performs source path expansion of COPY/ADD instruction.
 * @author Tsukasa Inada
 * @date 2023/02/14
 *
 * @note 
 */


// if you want to enable the assertion, comment out the line immediately after
#ifndef NDEBUG
#define NDEBUG
#endif


#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include <glob.h>
#include <sys/stat.h>
#include <unistd.h>

#include "srcglob.h"


#define SUCCESS 0
#define FAILURE 1


void xperror_message(const char *msg, const char *addition);
int xperror_standards(const char *entity, int errid);


/** string representing a invoked command name */
static const char *program_name;

/** a handler for the binary file that records the results of this command */
static FILE *srcglob_fp = NULL;




/******************************************************************************
    * Global Main Interface
******************************************************************************/


/**
 * @brief interface for the extra command 'srcglob'
 *
 * @param[in]  argc  the length of the argument vector below
 * @param[out] argv  array of strings that are wildcard patterns of COPY/ADD instruction
 * @return int  command's exit status
 */
int main(int argc, char **argv){
    const char *errmsg = NULL;
    int exit_status = FAILURE;

    if ((argc <= 0) || (! (program_name = *argv)))
        goto exit;

    if (argc == 1){
        errmsg = "requires one or more arguments";
        goto exit;
    }
    if (geteuid()){
        errmsg = "not a privileged user";
        goto exit;
    }
    if (! (srcglob_fp = fopen(SRCGLOB_FILE, "wb"))){
        errmsg = "cannot record the results";
        goto exit;
    }


    exit_status = SUCCESS;

exit:
    if (errmsg)
        xperror_message(errmsg, NULL);
    return exit_status;
}




/******************************************************************************
    * Utilities
******************************************************************************/


/**
 * @brief print the specified error message to stderr.
 *
 * @param[in]  msg  the error message
 * @param[in]  addition  additional information, if any
 */
void xperror_message(const char *msg, const char *addition){
    assert(msg);

    int offset = 0;

    if (! addition){
        offset = 4;
        addition = msg;
    }

    fprintf(stderr, ("%s: %s: %s\n" + offset), program_name, addition, msg);
}


/**
 * @brief print the standard error message represented by 'errno' to stderr.
 *
 * @param[in]  entity  the entity that caused the error or NULL
 * @param[in]  errid  error number
 * @return int  -1 (error exit)
 *
 * @note can be passed as 'errfunc' in glibc 'glob' function.
 */
int xperror_standards(const char *entity, int errid){
    assert(errid);

    srcglob_err errinfo = {
        .type = 'S',
        .code = errid,
        .written_size = 0
    };

    if (entity)
        errinfo.written_size = strlen(entity) + 1;

    fwrite(&errinfo, sizeof(errinfo), 1, srcglob_fp);

    if (errinfo.written_size){
        assert(entity);
        fwrite(entity, sizeof(const char), errinfo.written_size, srcglob_fp);
    }

    xperror_message(strerror(errid), entity);
    return -1;
}