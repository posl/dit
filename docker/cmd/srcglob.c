/**
 * @file srcglob.c
 *
 * Copyright (c) 2023 Tsukasa Inada
 *
 * @brief Described the extra command 'srcglob', that performs source path expansion of COPY/ADD instruction.
 * @author Tsukasa Inada
 * @date 2023/02/14
 *
 * @note source file paths must be resolved only within the build context.
 * @note implemented this command because 'chroot' syscall and some async-signal-unsafe functions are needed.
 */


// if you want to enable the assertion, comment out the line immediately after
#ifndef NDEBUG
#define NDEBUG
#endif


#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
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

#ifndef GLOB_PERIOD
#define GLOB_PERIOD 0
#endif

#define DIT_MOUNT_DIR "/dit/mnt"

#define OUTPUT_FORMAT_REG  (DIT_MOUNT_DIR "%s\n")
#define OUTPUT_FORMAT_DIR  (DIT_MOUNT_DIR "%s/\n")


/** Data type that is applied to the smallest element that makes up the results of this command */
typedef struct {
    char *path;    /** real path */
    bool isdir;    /** whether this is a directory */
    size_t len;    /** the size of real path */
} src_data;


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
    int errtype = '\0', code = -1, exit_status = FAILURE;
    const char *errinfo = NULL;

    bool glob_done = false;
    glob_t pglob;

    if ((argc <= 0) || (! (argv && (program_name = *argv))))
        goto exit;

    if (! --argc){
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
        code = SRCGLOB_NO_PRIVILEGE;
        goto report;
    }
    if (chdir(DIT_MOUNT_DIR)){
        errtype = 'S';
        code = errno;
        errinfo = DIT_MOUNT_DIR;
        goto report;
    }
    if (chroot(".")){
        errtype = 'S';
        code = errno;
        goto report;
    }


    // inconsistent with COPY instruction if GNU extension to 'glob' function is not available
    code = GLOB_PERIOD;
    assert(code);

    do {
        if (! *(++argv))
            goto free;

        glob_done = true;

        switch (glob(*argv, code, report_standard_err, &pglob)){
            case 0:
                break;
            case GLOB_NOMATCH:
                errtype = 'O';
                code = SRCGLOB_NOTHING_MATCHED;
                errinfo = *argv;
            default:
                goto report;
        }
        code = GLOB_PERIOD | GLOB_APPEND;
    } while (--argc);

    assert(pglob.gl_pathc > 0);
    assert(pglob.gl_pathv && *(pglob.gl_pathv));


    src_data *src_array = NULL, *p_src, *p_dest;
    char * const *p_path;
    srcglob_info result = {0};
    struct stat file_stat;
    const char *format = OUTPUT_FORMAT_REG;

    if ((src_array = (src_data *) malloc(sizeof(src_data) * pglob.gl_pathc))){
        for (p_src = src_array, p_path = pglob.gl_pathv; *p_path; p_src++, p_path++){
            if ((p_src->path = realpath(*p_path, NULL))){
                errno = 0;
                result.total_num++;

                if (! lstat(p_src->path, &file_stat)){
                    if ((p_src->isdir = S_ISDIR(file_stat.st_mode)))
                        result.dirs_num++;

                    if (p_src->isdir || S_ISREG(file_stat.st_mode)){
                        p_src->len = strlen(p_src->path) + 1;
                        result.written_size += p_src->len + 8;  // add the length of "/dit/mnt"
                        continue;
                    }
                }
            }

            errtype = 'O';
            code = errno;
            errinfo = *p_path;

            switch (code){
                case ENOENT:
                    code = SRCGLOB_NOT_IN_CONTEXT;
                    break;
                case 0:
                    code = SRCGLOB_IS_SPECIAL_FILE;
                    break;
                default:
                    errtype = 'S';
            }
            break;
        }

        if (! errtype){
            exit_status = SUCCESS;
            assert(result.total_num == pglob.gl_pathc);
            fwrite(&result, sizeof(result), 1, srcglob_fp);
        }

        assert(result.total_num || (! result.dirs_num));

        do {
            if (! result.total_num){
                if (! result.dirs_num)
                    break;
                result.total_num = result.dirs_num;
                result.dirs_num = 0;
                format = OUTPUT_FORMAT_DIR;
            }
            for (p_src = (p_dest = src_array); result.total_num; p_src++, result.total_num--){
                if (! errtype){
                    if (result.dirs_num && p_src->isdir){
                        if (p_src != p_dest)
                            memcpy(p_dest, p_src, sizeof(src_data));
                        p_dest++;
                        continue;
                    }
                    fputs(DIT_MOUNT_DIR, srcglob_fp);
                    fwrite(p_src->path, sizeof(char), p_src->len, srcglob_fp);
                    fprintf(stdout, format, p_src->path);
                }
                free(p_src->path);
            }
        } while (! errtype);

        free(src_array);
    }


report:
    if (errtype){
        report_srcglob_err(errtype, code, errinfo);
        errtype = '\0';
    }

free:
    if (glob_done)
        globfree(&pglob);

    assert(srcglob_fp);
    fclose(srcglob_fp);

exit:
    if (errtype){
        assert(errtype == 'M');
        xperror_message(errinfo, NULL);
    }

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

    assert(srcglob_fp);
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