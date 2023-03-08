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


#include "debug.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "srcglob.h"


#define SUCCESS 0
#define FAILURE 1

#define ERR_STANDARD  1
#define ERR_OTHERWISE 2

#define MOUNT_DIR_LEN 8
#define MOUNT_DIR_PATH "/dit/mnt"


/** Data type for storing the error information */
typedef struct {
    const char *msg;     /** error message */
    const char *info;    /** additional information, if any */
} srcglob_err;


/** Data type that is applied to the smallest element that makes up the results of this command */
typedef struct {
    char *path;    /** real path */
    bool isdir;    /** whether this is a directory */
    size_t len;    /** the size of real path */
} srcglob_data;


static int do_srcglob(int argc, char **argv);

static int resolve_src_paths(srcglob_data *src_array, srcglob_info *result);
static void report_src_paths(srcglob_data *src_array, const srcglob_info *result);

static void xperror_message(int type, srcglob_err *err);


/** string representing an invoked command name */
static const char *program_name;

/** descriptor for the temporary file to receive the results of my golang library being called */
static int fd_buf = -1;

/** handler for the data file to store the results of this command */
static FILE *fp_dat = NULL;




/******************************************************************************
    * Global Main Interface
******************************************************************************/


/**
 * @brief the extra command 'srcglob'
 *
 * @param[in]  argc  the number of command line arguments
 * @param[in]  argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note before executing 'chroot', it is necessary to call functions that need to resolve the file path.
 * @note opened files are closed at the end because it is a problem if 'errno' is overwritten.
 */
int main(int argc, char **argv){
    int exit_status = ERR_STANDARD;
    srcglob_err err = {0};

    if ((argc <= 0) || (! (argv && (program_name = *(argv++)))))
        return -1;

    if (! --argc){
        exit_status = ERR_OTHERWISE;
        err.msg = "requires one or more arguments";
        goto exit;
    }
    if ((fd_buf = open(SRCGLOB_FILE_B, (O_RDWR | O_CREAT | O_TRUNC), (S_IRUSR | S_IWUSR))) == -1){
        err.info = SRCGLOB_FILE_B;
        goto exit;
    }
    if (! (fp_dat = fopen(SRCGLOB_FILE_D, "wb"))){
        err.info = SRCGLOB_FILE_D;
        goto exit;
    }

    if ((exit_status = do_srcglob(argc, argv)) == ERR_OTHERWISE){
        exit_status = ERR_STANDARD;
        err.info = MOUNT_DIR_PATH;
    }

exit:
    switch (exit_status){
        case SUCCESS:
            break;
        case ERR_STANDARD:
        case ERR_OTHERWISE:
            xperror_message(exit_status, &err);
        default:
            exit_status = FAILURE;
    }

    if (fd_buf != -1)
        close(fd_buf);
    if (fp_dat)
        fclose(fp_dat);

    return exit_status;
}




/**
 * @brief perform source path expansion.
 *
 * @param[in]  argc  the length of the argument vector below
 * @param[in ] argv  array of strings that are wildcard patterns of COPY/ADD instruction
 * @return int  -1 (failure), 0 (success), 1 (standard error) or 2 (unexpected error)
 *
 * @note if the return value is 2, you should print a standard error message with additional information.
 */
static int do_srcglob(int argc, char **argv){
    assert(argc > 0);
    assert(argv);

    if (chdir(MOUNT_DIR_PATH))
        return ERR_OTHERWISE;

    if (chroot("."))
        return ERR_STANDARD;

    srcglob_info result = {0};
    int exit_status = -1;

    /*
    if (! (result.total_num = MyGlob(program_name, argc, argv, fd_buf)))
        return exit_status;
    */
//
    result.total_num = 3;
    write(fd_buf, "/Dockerfile", 12);
    write(fd_buf, "../tmp/", 8);
    write(fd_buf, "./.dockerignore", 16);
//

    srcglob_data src_array[result.total_num];
    memset(src_array, 0, (sizeof(srcglob_data) * result.total_num));

    switch ((exit_status = resolve_src_paths(src_array, &result))){
        case SUCCESS:
            report_src_paths(src_array, &result);
            break;
        case ERR_OTHERWISE:
            exit_status = -1;
            break;
        case -1:
            for (srcglob_data *p_src = src_array; result.total_num; p_src++, result.total_num--)
                if (p_src->path)
                    free(p_src->path);
    }

    assert(exit_status != ERR_OTHERWISE);
    return exit_status;
}




/******************************************************************************
    * Utilities
******************************************************************************/


/**
 * @brief construct the data that are the results of source path expansion.
 *
 * @param[out] src_array  array for storing the results of source path expansion
 * @param[out] result  variable to store the information about the results of source path expansion
 * @return int  -1 (failure), 0 (success), 1 (standard error) or 2 (unexpected error)
 *
 * @note after calling my golang library, the file position of the buffer is set at the end of the file.
 * @note if the return value is 1, you should convey the fact that an error has occurred through a message.
 *
 * @attention each element of 'src_array' and 'result' must be initialized properly before the call.
 * @attention the absolute paths stored in 'src_array' should be freed by the caller when no longer needed.
 */
static int resolve_src_paths(srcglob_data *src_array, srcglob_info *result){
    assert(src_array);
    assert(result);
    assert(result->total_num);

    off_t file_size;
    void *addr;

    if ((file_size = lseek(fd_buf, 0, SEEK_CUR)) == -1)
        return ERR_STANDARD;

    if ((file_size <= 1) || lseek(fd_buf, 0, SEEK_SET))
        return ERR_OTHERWISE;

    if ((addr = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd_buf, 0)) == MAP_FAILED)
        return ERR_STANDARD;

    srcglob_data *p_src;
    size_t srcs_num;
    char *path;
    struct stat file_stat;
    int exit_status = SUCCESS;
    srcglob_err err;

    p_src = src_array;
    srcs_num = result->total_num;
    path = (char *) addr;

    do {
        if ((p_src->path = realpath(path, NULL))){
            errno = 0;

            if (! lstat(p_src->path, &file_stat))
                switch ((file_stat.st_mode & S_IFMT)){
                    case S_IFDIR:
                        p_src->isdir = true;
                        result->dirs_num++;
                    case S_IFREG:
                        p_src->len = strlen(p_src->path) + 1;
                        result->written_size += p_src->len + MOUNT_DIR_LEN;
                        goto next;
                }
        }

        exit_status = ERR_OTHERWISE;
        err.info = path;

        switch (errno){
            case 0:
                err.msg = "is a special file";
                break;
            case ENOENT:
                err.msg = "not in build context";
                break;
            default:
                exit_status = ERR_STANDARD;
        }
        xperror_message(exit_status, &err);

    next:
        p_src++;
        path += strlen(path) + 1;
    } while (--srcs_num);

    if (exit_status)
        exit_status = -1;

    munmap(addr, file_size);

    assert((! exit_status) || (exit_status == -1));
    return exit_status;
}




/**
 * @brief report the results of source path expansion.
 *
 * @param[out] src_array  array for storing the results of source path expansion
 * @param[in]  result  variable to store the information about the results of source path expansion
 *
 * @note the results are reported in the order of regular files and directories.
 * @note the absolute paths stored in 'src_array' are freed in this function.
 */
static void report_src_paths(srcglob_data *src_array, const srcglob_info *result){
    assert(src_array);
    assert(result);
    assert(result->total_num);
    assert(result->written_size);

    char *path, addr[result->written_size];
    size_t total_num, dirs_num;
    const char *attr = "regular";
    int offset = 1;
    srcglob_data *p_src, *p_dest;

    path = addr;
    total_num = result->total_num;
    dirs_num = result->dirs_num;

    if (total_num == dirs_num)
        total_num = 0;

    do {
        if (! total_num){
            if (! dirs_num)
                break;
            total_num = dirs_num;
            dirs_num = 0;
            attr = "directory";
        }

        assert((! offset) || (offset == 1));
        fprintf(stdout, ("\n < %s >\n" + offset), attr);
        offset = 0;

        for (p_src = (p_dest = src_array); total_num; p_src++, total_num--){
            if (dirs_num && p_src->isdir){
                if (p_src != p_dest)
                    memcpy(p_dest, p_src, sizeof(srcglob_data));
                p_dest++;
                continue;
            }
            memcpy(path, MOUNT_DIR_PATH, (sizeof(char) * MOUNT_DIR_LEN));
            memcpy((path + MOUNT_DIR_LEN), p_src->path, (sizeof(char) * p_src->len));
            free(p_src->path);

            fputs(path, stdout);
            fputc('\n', stdout);
            path += p_src->len + MOUNT_DIR_LEN;
        }
    } while (true);

    fwrite(result, sizeof(srcglob_info), 1, fp_dat);
    fwrite(addr, sizeof(char), result->written_size, fp_dat);
}




/**
 * @brief print the specified error message to stderr.
 *
 * @param[in]  type  1 (standard error) or 2 (original error)
 * @param[out] err  the error information
 */
static void xperror_message(int type, srcglob_err *err){
    assert(err);

    int offset = 0;

    if (type == ERR_STANDARD)
        err->msg = strerror(errno);

    if (! err->info){
        offset = 4;
        err->info = err->msg;
    }

    assert(program_name);
    fprintf(stderr, ("%s: %s: %s\n" + offset), program_name, err->info, err->msg);
}