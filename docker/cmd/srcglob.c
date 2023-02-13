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


static void xperror_message(const char *msg, const char *addition);

static void report_srcglob_err(int type, int code, const char *entity);
static int report_standard_err(const char *entity, int errid);


/** string representing a invoked command name */
static const char *program_name;

/** a handler for the binary file that records the results of this command */
static FILE *srcglob_fp = NULL;




/******************************************************************************
    * Global Main Interface
******************************************************************************/


/**
 * @brief the extra command 'srcglob'
 *
 * @param[in]  argc  the length of the argument vector below
 * @param[out] argv  array of strings that are wildcard patterns of COPY/ADD instruction
 * @return int  command's exit status
 */
int main(int argc, char **argv){
    int errtype = '\0', errcode = -1, exit_status = FAILURE;
    const char *errinfo = NULL;

    if ((argc <= 0) || (! (argv && (program_name = *argv))))
        goto exit;

    if (argc == 1){
        errtype = 'M';
        errinfo = "requires one or more arguments";
        goto exit;
    }
    if (! (srcglob_fp = fopen(SRCGLOB_FILE, "wb"))){
        errtype = 'M';
        errinfo = "cannot record the results";
        goto exit;
    }

    if (geteuid()){
        errtype = 'O';
        errcode = SRCGLOB_ERRCODE_NO_PRIVILEGE;
        goto exit;
    }
    if (chdir(DIT_MOUNT_DIR) || chroot(DIT_MOUNT_DIR)){
        errtype = 'S';
        errcode = errno;
        errinfo = DIT_MOUNT_DIR;
        goto exit;
    }

    

    exit_status = SUCCESS;

exit:
    if (errtype){
        if (errcode < 0){
            assert(errinfo);
            xperror_message(errinfo, NULL);
        }
        else
            report_srcglob_err(errtype, errcode, errinfo);
    }
    if (srcglob_fp)
        fclose(srcglob_fp);

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
static void xperror_message(const char *msg, const char *addition){
    assert(msg);

    int offset = 0;

    if (! addition){
        offset = 4;
        addition = msg;
    }

    assert(program_name);
    fprintf(stderr, ("%s: %s: %s\n" + offset), program_name, addition, msg);
}




/**
 * @brief report any errors that occur while running this command.
 *
 * @param[in]  type  error type
 * @param[in]  code  error code
 * @param[in]  entity  the entity that caused the error or NULL
 *
 * @note see the data type 'srcglob_err' for details on the arguments of this function.
 */
static void report_srcglob_err(int type, int code, const char *entity){
    assert((type == 'S') || (type == 'O'));
    assert(code >= 0);

    srcglob_err errinfo = {
        .type = type,
        .code = code,
        .written_size = 0
    };

    const char *errmsg;

    if (entity)
        errinfo.written_size = strlen(entity) + 1;

    fwrite(&errinfo, sizeof(errinfo), 1, srcglob_fp);

    if (errinfo.written_size){
        assert(entity);
        fwrite(entity, sizeof(const char), errinfo.written_size, srcglob_fp);
    }

    errmsg = (type == 'S') ? strerror(code) : srcglob_errmsgs[code];
    xperror_message(errmsg, entity);
}


/**
 * @brief report the standard error represented by 'errno'.
 *
 * @param[in]  entity  the entity that caused the error or NULL
 * @param[in]  errid  error number
 * @return int  -1 (error exit)
 *
 * @note can be passed as 'errfunc' in glibc 'glob' function.
 */
static int report_standard_err(const char *entity, int errid){
    assert(errid);

    report_srcglob_err('S', errid, entity);
    return -1;
}