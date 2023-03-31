/**
 * @file main.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the main function and the utilities commonly used in files for each dit command.
 * @author Tsukasa Inada
 * @date 2022/07/18
 */

#include "main.h"

#define EXIT_STATUS_FILE "/dit/srv/last-exit-status"

#define XFGETS_NESTINGS_MAX 2
#define XFGETS_INITIAL_SIZE 1023  // 2^n - 1

#define XSTRCAT_INITIAL_MAX 1023  // 2^n - 1

#define NULLDEV_FILENO  open("/dev/null", (O_WRONLY | O_CLOEXEC))

#define sigreset(sigdfl_int, sigdfl_quit, old_mask) \
    do { \
        sigaction(SIGINT, &sigdfl_int, NULL); \
        sigaction(SIGQUIT, &sigdfl_quit, NULL); \
        sigprocmask(SIG_SETMASK, &old_mask, NULL); \
    } while (false)


/** Data type for storing the information for one loop for 'xfgets_for_loop' */
typedef struct {
    const char *src_file;    /** source file name or NULL */
    FILE *fp;                /** handler for the source file */
    char **p_start;          /** pointer to the beginning of a series of strings or NULL */
    char *dest;              /** pointer to the beginning of dynamic memory allocated */
    size_t curr_size;        /** the size of dynamic memory currently in use */
    size_t curr_len;         /** the total length of the preserved strings including null characters */
} xfgets_info;


/** array of the names of the target files */
const char * const target_files[2] = {
    HISTORY_FILE,
    DOCKER_FILE_DRAFT
};

/** array of the names of files for storing the result of the previous dit command 'convert' */
const char * const convert_results[2] = {
    CONVERT_RESULT_FILE_H,
    CONVERT_RESULT_FILE_D
};

/** array of the names of files for storing the previous deleted lines from the target files */
const char * const erase_results[2] = {
    ERASE_RESULT_FILE_H,
    ERASE_RESULT_FILE_D
};


/** array of strings in alphabetical order representing each dit command */
const char * const cmd_reprs[CMDS_NUM] = {
    "cmd",
    "config",
    "convert",
    "copy",
    "erase",
    "healthcheck",
    "help",
    "ignore",
    "inspect",
    "label",
    "onbuild",
    "optimize",
    "package",
    "reflect"
};


/** array of strings in alphabetical order representing each response to the Y/n question */
const char * const assume_args[ARGS_NUM] = {
    "NO",
    "QUIT",
    "YES"
};

/** array of strings in alphabetical order representing how to handle the blank lines */
const char * const blank_args[ARGS_NUM] = {
    "preserve",
    "squeeze",
    "truncate"
};

/** array of strings in alphabetical order representing what contents should be displayed */
const char * const display_args[ARGS_NUM] = {
    "BOTH",
    "IN",
    "OUT"
};

/** array of strings in alphabetical order representing whether the target is Dockerfile or history-file */
const char * const target_args[ARGS_NUM] = {
    "both",
    "dockerfile",
    "history-file"
};


/** array of strings in alphabetical order representing each Dockerfile instruction */
const char * const docker_instr_reprs[DOCKER_INSTRS_NUM] = {
    "ADD",
    "ARG",
    "CMD",
    "COPY",
    "ENTRYPOINT",
    "ENV",
    "EXPOSE",
    "FROM",
    "HEALTHCHECK",
    "LABEL",
    "MAINTAINER",
    "ONBUILD",
    "RUN",
    "SHELL",
    "STOPSIGNAL",
    "USER",
    "VOLUME",
    "WORKDIR"
};


/** array of the function pointers corresponding to each dit command */
static int (* const cmd_funcs[CMDS_NUM])(int, char **) = {
    cmd,
    config,
    convert,
    copy,
    erase,
    healthcheck,
    help,
    ignore,
    inspect,
    label,
    onbuild,
    optimize,
    package,
    reflect
};


/** conversion table to use for string sanitization */
static const char escape_char_table[128] = {
    '?', '?', '?', '?', '?', '?', '?', 'a',
    'b', 't', 'n', 'v', 'f', 'r', '?', '?',
    '?', '?', '?', '?', '?', '?', '?', '?',
    '?', '?', '?', 'e', '?', '?', '?', '?',
    ' ', '_', '\"', '_', '_', '_', '_', '\'',
    '_', '_', '_', '_', '_', '_', '_', '_',
    '_', '_', '_', '_', '_', '_', '_', '_',
    '_', '_', '_', '_', '_', '_', '_', '_',
    '_', '_', '_', '_', '_', '_', '_', '_',
    '_', '_', '_', '_', '_', '_', '_', '_',
    '_', '_', '_', '_', '_', '_', '_', '_',
    '_', '_', '_', '_', '\\', '_', '_', '_',
    '_', '_', '_', '_', '_', '_', '_', '_',
    '_', '_', '_', '_', '_', '_', '_', '_',
    '_', '_', '_', '_', '_', '_', '_', '_',
    '_', '_', '_', '_', '_', '_', '_', '?'
};

/** table for converting control characters to hexadecimal numbers */
static const char escape_hex_table[32 * 2] = {
    '0','0', '0','1', '0','2', '0','3', '0','4', '0','5', '0','6', '0','7',
    '0','8', '0','9', '0','A', '0','B', '0','C', '0','D', '0','E', '0','F',
    '1','0', '1','1', '1','2', '1','3', '1','4', '1','5', '1','6', '1','7',
    '1','8', '1','9', '1','A', '1','B', '1','C', '1','D', '1','E', '1','F',
};


/** string representing a dit command invoked */
static const char *program_name = "dit";



/******************************************************************************
    * Global Main Interface
******************************************************************************/


/**
 * @brief user interface for all dit commands
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note switches the processing to each dit command in the same way as busybox.
 * @note if unit tests were performed in 'test', it will not return to this function.
 * @note leave the command name in a global variable for error message output.
 */
int main(int argc, char **argv){
    setvbuf(stderr, NULL, _IOLBF, 0);

    if ((argc > 0) && argv && *argv){
        char *tmp;
        int cmd_id;

        if ((tmp = strrchr(*argv, '/')))
            *argv = tmp + 1;

        // compare with "dit"
        if (strcmp(*argv, program_name) || (--argc && *(++argv))){
            cmd_id = receive_expected_string(*argv, cmd_reprs, CMDS_NUM, 0);

#ifndef NDEBUG
            srand((unsigned int) time(NULL));
            test(argc, argv, cmd_id);
#endif
            if (cmd_id >= 0){
                assert(cmd_id < CMDS_NUM);
                program_name = *argv;
                return cmd_funcs[cmd_id](argc, argv);
            }

            xperror_invalid_arg('C', 1, "command", *argv);
        }
        else
            xperror_missing_args("command");
    }

    xperror_suggestion(false);
    return FAILURE;
}




/******************************************************************************
    * Error Message Functions
******************************************************************************/


/**
 * @brief print an error message about invalid argument to stderr.
 *
 * @param[in]  code_c  error code ('O' (for optarg), 'N' (for number) or 'C' (for cmdarg))
 * @param[in]  state  error state (0 (unrecognized), -1 (ambiguous) or others (invalid))
 * @param[in]  desc  the description for the argument
 * @param[in]  arg  actual argument passed on the command line
 *
 * @note the return value of 'receive_expected_string' can be used as it is for 'state'.
 */
void xperror_invalid_arg(int code_c, int state, const char *desc, const char *arg){
    assert(desc);

    size_t size;
    size = arg ? (strlen(arg) * 4 + 1) : 6;

    char buf[size];

    if (arg)
        get_sanitized_string(buf, arg, true);
    else
        memcpy(buf, "#NULL", 6);

    const char *format, *addition = "", *adjective;

    switch (code_c){
        case 'N':
            addition = " number of";
        case 'C':
            format = "%s: %s%s %s: '%s'\n";
            break;
        default:
            assert(code_c == 'O');
            format = "%s: %s argument '%s' for '--%s'\n";
            addition = buf;
    }

    adjective = state ? ((state == -1) ? "ambiguous" : "invalid") : "unrecognized";

    fprintf(stderr, format, program_name, adjective, addition, desc, buf);
}


/**
 * @brief print the valid arguments to stderr.
 *
 * @param[in]  reprs  array of expected strings
 * @param[in]  size  array size
 */
void xperror_valid_args(const char * const *reprs, size_t size){
    assert(reprs);

    fputs("Valid arguments are:\n", stderr);

    for (const char * const *p_repr = reprs; size--; p_repr++){
        assert(*p_repr);
        fprintf(stderr, "  - '%s'\n", *p_repr);
    }
}




/**
 * @brief print an error message to stderr that some arguments are missing.
 *
 * @param[in]  desc  the description for the arguments or NULL
 *
 * @note if 'desc' is NULL, prints an error message about specifying the target file.
 */
void xperror_missing_args(const char *desc){
    char format[] = "%s: missing %s operand\n";

    if (! desc){
        desc = "'-d', '-h' or '--target' option";
        format[14] = '\n';
        format[15] = '\0';
    }

    fprintf(stderr, format, program_name, desc);
}


/**
 * @brief print an error message to stderr that the number of arguments is too many.
 *
 * @param[in]  limit  the maximum number of arguments or negative integer
 *
 * @note if 'limit' is negative integer, prints an error message about specifying both files as targets.
 * @attention if 'limit' is 2 or more, it does not work as expected.
 */
void xperror_too_many_args(int limit){
    assert(limit < 2);

    char format[] = "%s: no %sarguments allowed when reflecting in both files\n";
    const char *adjective = "";

    switch (limit){
        case 1:
            adjective = "more than two ";
        case 0:
            format[26] = '\n';
            format[27] = '\0';
    }

    fprintf(stderr, format, program_name, adjective);
}




/**
 * @brief print the specified error message to stderr.
 *
 * @param[in]  msg  the error message or NULL
 * @param[in]  addition  additional information, if any
 *
 * @note if 'msg' is NULL, prints an error message about manipulating an internal file.
 * @attention if 'msg' is NULL, 'addition' must also be NULL.
 */
void xperror_message(const char *msg, const char *addition){
    assert(msg || (! addition));

    int offset = 0;

    if (! addition){
        if (! msg)
            msg = "unexpected error while manipulating an internal file";
        offset = 4;
        addition = msg;
    }

    fprintf(stderr, ("%s: %s: %s\n" + offset), program_name, addition, msg);
}


/**
 * @brief print a suggenstion message to stderr.
 *
 * @param[in]  cmd_flag  whether to suggest displaying the manual of each dit command
 */
void xperror_suggestion(bool cmd_flag){
    const char *str1 = "", *str2 = "";

    if (cmd_flag){
        str1 = program_name;
        str2 = " --";
    }

    fprintf(stderr, "Try 'dit %s%shelp' for more information.\n", str1, str2);
}




/**
 * @brief print the standard error message represented by 'errno' to stderr.
 *
 * @param[in]  entity  the entity that caused the error or NULL
 * @param[in]  errid  error number
 */
void xperror_standards(const char *entity, int errid){
    assert(errid);

    xperror_message(strerror(errid), entity);
}


/**
 * @brief print the error message related to child process to stderr.
 *
 * @param[in]  cmd_name  command name
 * @param[in]  status  command's exit status, 0 or less integer
 *
 * @note reports an error caused by syscall, or that the command in child process ended with an error.
 */
void xperror_child_process(const char *cmd_name, int status){
    assert(cmd_name);
    assert(status < 256);

    size_t size;
    size = strlen(cmd_name);

    char entity[size + 9];
    memcpy(entity, cmd_name, (sizeof(char) * size));
    memcpy((entity + size), " (child)", (sizeof(char) * 9));

    if (status <= 0){
        assert(errno);
        xperror_standards(entity, errno);
    }
    else
        fprintf(stderr, "%s: %s: exited with exit status %d\n", program_name, entity, status);
}


/**
 * @brief print the error message about the contents of the specified file to stderr.
 *
 * @param[in]  file_name  file name of NULL to indicate that the source is standard input
 * @param[in]  lineno  the line number
 * @param[in]  msg  the error message
 *
 * @note line number starts from 1.
 */
void xperror_file_contents(const char *file_name, int lineno, const char *msg){
    assert(lineno > 0);
    assert(msg);

    if (! file_name)
        file_name = "stdin";

    fprintf(stderr, "%s: %s: line %d: %s\n", program_name, file_name, lineno, msg);
}




/******************************************************************************
    * Extensions of Standard Library Functions
******************************************************************************/


/**
 * @brief read the contents of the specified file exactly one line at a time.
 *
 * @param[in]  src_file  source file name or NULL to indicate that the source is standard input
 * @param[out] p_start  pointer to the beginning of a series of strings corresponding to each line or NULL
 * @param[out] p_errid  variable to store the error ID when an error occurs
 * @return char*  the resulting line or NULL
 *
 * @note read to the end of the file by using it as a conditional expression in a loop statement.
 * @note this function can be nested up to a depth of 'XFGETS_NESTINGS_MAX' by passing different 'src_file'.
 * @note if 'p_start' is non-NULL, preserves read line into the dynamic memory pointed to by its contents.
 * @note all lines of the file whose size is larger than 'INT_MAX' cannot be preserved.
 * @note this function updates the contents of 'p_errid' only when an error occurs while opening the file.
 * @note if the contents of 'p_errid' was set to something other than 0, performs finish processing.
 * @note the trailing newline character of the line that is the return value is stripped.
 *
 * @attention the value of 'p_start' only makes sense for the first one in a series of calls.
 * @attention if 'p_start' and its contents are non-NULL, its contents should be released by the caller.
 * @attention this function must be called until NULL is returned, since it uses dynamic memory internally.
 */
char *xfgets_for_loop(const char *src_file, char **p_start, int *p_errid){
    static int info_idx = -1;
    static xfgets_info info_list[XFGETS_NESTINGS_MAX];

    xfgets_info *p_info;
    bool allocate_flag = false;

    p_info = info_list + info_idx;

    if ((info_idx < 0) || (p_info->src_file != src_file)){
        FILE *fp = stdin;
        errno = 0;

        if (p_start)
            *p_start = NULL;

        if ((info_idx < (XFGETS_NESTINGS_MAX - 1)) && ((! src_file) || (fp = fopen(src_file, "r")))){
            info_idx++;
            p_info++;

            assert((info_idx >= 0) && (info_idx < XFGETS_NESTINGS_MAX));
            assert(p_info == (info_list + info_idx));

            p_info->src_file = src_file;
            p_info->fp = fp;
            p_info->p_start = p_start;

            p_info->dest = NULL;
            p_info->curr_size = XFGETS_INITIAL_SIZE;
            p_info->curr_len = 0;

            allocate_flag = true;
        }
        else {
            if (p_errid)
                *p_errid = errno;
            return NULL;
        }
    }

    char *start;
    size_t len;
    unsigned int tmp;
    bool reset_flag = true;

    start = p_info->dest;
    len = p_info->curr_len;

    do {
        if (allocate_flag){
            assert(p_info->curr_size <= INT_MAX);

            if ((start = (char *) realloc(p_info->dest, (sizeof(char) * p_info->curr_size)))){
                if (p_info->p_start)
                    *(p_info->p_start) = start;

                p_info->dest = start;
                allocate_flag = false;
                continue;
            }
        }
        else if (! (p_errid && *p_errid)){
            assert(start == p_info->dest);
            start += len;

            tmp = p_info->curr_size - len;
            assert((tmp > 0) && (tmp <= INT_MAX));

            if (fgets(start, tmp, p_info->fp)){
                tmp = strlen(start);
                len += tmp;

                if (! (tmp && (start[--tmp] == '\n'))){
                    tmp = p_info->curr_size + 1;
                    assert(! (p_info->curr_size & tmp));

                    if ((tmp <<= 1)){
                        p_info->curr_size = tmp - 1;
                        allocate_flag = true;
                        continue;
                    }
                }
                else {
                    len--;
                    start[tmp] = '\0';
                    reset_flag = false;
                }
            }
            else if (len != p_info->curr_len)
                reset_flag = false;
        }

        start = p_info->dest;

        if (reset_flag){
            assert(src_file == p_info->src_file);

            if (src_file)
                fclose(p_info->fp);
            else
                clearerr(p_info->fp);

            if ((! (p_info->p_start && p_info->curr_len)) && start){
                free(start);

                if (p_info->p_start)
                    *(p_info->p_start) = NULL;
            }

            info_idx--;
            start = NULL;
        }
        else if (p_info->p_start){
            start += p_info->curr_len;
            p_info->curr_len = len + 1;
        }

        return start;
    } while (true);
}




/**
 * @brief determine if the first string after uppercase conversion matches the second string.
 *
 * @param[in]  target  target string
 * @param[in]  expected  expected string
 * @return int  comparison result
 *
 * @attention expected string must not contain lowercase letters.
 */
int xstrcmp_upper_case(const char * restrict target, const char * restrict expected){
    assert(target);
    assert(expected);

    int c, d;
    do {
        c = (unsigned char) *(target++);
        d = (unsigned char) *(expected++);
    } while ((! (d = toupper(c) - d)) && c);

    return d;
}


/**
 * @brief comparison function between strings used when qsort
 *
 * @param[in]  a  pointer to string1
 * @param[in]  b  pointer to string2
 * @return int  comparison result
 */
int qstrcmp(const void *a, const void *b){
    assert(a);
    assert(b);

    const char *str1, *str2;
    str1 = *((const char **) a);
    str2 = *((const char **) b);

    assert(str1 && str2);
    return strcmp(str1, str2);
}




/**
 * @brief concatenate any string to the end of base string.
 *
 * @param[out] base  base string
 * @param[in]  base_len  the length of base string
 * @param[in]  suf  string to concatenate to the end of base string
 * @param[in]  suf_len  the length of the string including the terminating null character
 * @return bool  successful or not
 *
 * @note this function allows for strings of virtually infinite length.
 *
 * @attention each element of 'base' must be initialized with 0 before the first call.
 * @attention if 'base->ptr' is non-NULL, it should be released by the caller.
 */
bool xstrcat_inf_len(inf_str *base, size_t base_len, const char *suf, size_t suf_len){
    assert(base);
    assert(base->max >= base_len);
    assert(suf);
    assert(suf_len == (strlen(suf) + 1));

    size_t curr;
    bool allocate_flag = false;
    void *ptr;
    char *dest;

    if (! (curr = base->max)){
        curr = XSTRCAT_INITIAL_MAX;
        allocate_flag = true;
    }

    do {
        if ((curr - base_len) < suf_len){
            curr++;
            assert(! (curr & (curr - 1)));

            if ((curr <<= 1)){
                curr--;
                allocate_flag = true;
                continue;
            }
        }
        else if (! allocate_flag)
            break;
        else if ((ptr = realloc(base->ptr, (sizeof(char) * curr)))){
            base->ptr = (char *) ptr;
            base->max = curr;
            break;
        }

        return false;
    } while (true);

    assert(base->ptr);

    dest = base->ptr + base_len;
    memcpy(dest, suf, (sizeof(char) * suf_len));

    return true;
}




/******************************************************************************
    * Extensions of System Calls in Unix and C
******************************************************************************/


/**
 * @brief execute the specified command in a child process.
 *
 * @param[in]  cmd_file  command path
 * @param[in]  argv  NULL-terminated array of strings that are command line arguments
 * @param[in]  mode  some flags (bit 1: how to handle stdout, bit 2: be quiet)
 * @return int  0 (success), -1 (syscall error) or positive integer (command error)
 *
 * @note this function is to avoid the inefficiency and the inconvenience when using 'system' function.
 * @note signal handling conforms to the specifications of 'system' function.
 * @note 'pthread_sigmask' function is not used because it is not any of async-signal-safe functions.
 * @note discards stdout if the LSB of 'mode' is set, otherwise groups stdout with stderr.
 * @note the exit status that can be returned as a return value is based on the shell's.
 *
 * @attention the subsequent processing should not be continued if this function returns a non-zero value.
 * @attention calling this function in a multithreaded process is not recommended.
 */
int execute(const char *cmd_file, char * const argv[], unsigned int mode){
    assert(cmd_file);
    assert(argv && argv[0]);
    assert(mode < 4);

    struct sigaction new_act = {0}, sigdfl_int, sigdfl_quit;
    sigset_t old_mask;
    pid_t pid, err = 0;
    int tmp = 0, exit_status = -1;

    if (! (mode & 0b10)){
        fputc('+', stderr);

        for (char * const *p_arg = argv; *p_arg; p_arg++)
            print_sanitized_string(*p_arg);

        fputc('\n', stderr);
    }

    new_act.sa_handler = SIG_IGN;
    sigemptyset(&new_act.sa_mask);
    sigaction(SIGINT, &new_act, &sigdfl_int);
    sigaction(SIGQUIT, &new_act, &sigdfl_quit);

    sigaddset(&new_act.sa_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &new_act.sa_mask, &old_mask);

    switch ((pid = fork())){
        case -1:  // error
            err = -1;
            break;
        case 0:   // child
            tmp = STDERR_FILENO;
            exit_status = 126;

            if (! (((mode & 0b01) && ((tmp = NULLDEV_FILENO) == -1)) || (dup2(tmp, STDOUT_FILENO) == -1))){
                sigreset(sigdfl_int, sigdfl_quit, old_mask);
                execv(cmd_file, argv);
                exit_status++;
            }
            _exit(exit_status);
        default:  // parent
            while (((err = waitpid(pid, &tmp, 0)) == -1) && (errno == EINTR));
            break;
    }

    sigreset(sigdfl_int, sigdfl_quit, old_mask);

    if (err != -1){
        if (WIFEXITED(tmp))
            exit_status = WEXITSTATUS(tmp);
        else {
            exit_status = 128;
            if (WIFSIGNALED(tmp))
                exit_status += WTERMSIG(tmp);
        }
    }
    if ((mode & 0b10) ? (exit_status < 0) : exit_status)
        xperror_child_process(argv[0], exit_status);

    return exit_status;
}




/**
 * @brief the function that recursively scans the specified directory
 *
 * @param[in]  pwdfd  file descriptor that serves as the current working directory
 * @param[in]  name  name of the directory we are currently looking at
 * @param[in]  callback  a callback function like "*at()" family of syscalls
 * @return int  0 (success) or -1 (unexpected error)
 *
 * @attention the callback function is not called for the root directory specified at the first call.
 */
int walkat(int pwdfd, const char *name, int (* callback)(int, const char *, bool)){
    assert((pwdfd >= 0) || (pwdfd == AT_FDCWD));
    assert(name && *name);
    assert(callback);

    int new_fd, exit_status = UNEXPECTED_ERROR;

    if ((new_fd = openat(pwdfd, name, (O_RDONLY | O_DIRECTORY))) != -1){
        DIR *dir;

        if ((dir = fdopendir(new_fd))){
            struct dirent *entry;
            struct stat file_stat;
            bool isdir;

            while ((entry = readdir(dir))){
                name = entry->d_name;

                if (check_if_valid_entry(name)){
#ifdef _DIRENT_HAVE_D_TYPE
                    if (entry->d_type != DT_UNKNOWN)
                        isdir = (entry->d_type == DT_DIR);
                    else
#endif
                    if (! fstatat(new_fd, name, &file_stat, AT_SYMLINK_NOFOLLOW))
                        isdir = S_ISDIR(file_stat.st_mode);
                    else
                        goto exit;

                    if ((isdir && walkat(new_fd, name, callback)) || callback(new_fd, name, isdir))
                        goto exit;
                }
            }

            exit_status = SUCCESS;
        exit:
            closedir(dir);
        }
        else
            close(new_fd);
    }

    return exit_status;
}




/**
 * @brief wrapper function for 'unlinkat' syscall
 *
 * @param[in]  pwdfd  file descriptor that serves as the current working directory
 * @param[in]  name  file or directory name
 * @param[in]  isdir  whether it is a directory
 * @return int  0 (success) or -1 (unexpected error)
 */
int removeat(int pwdfd, const char *name, bool isdir){
    assert((pwdfd >= 0) || (pwdfd == AT_FDCWD));
    assert(name && *name);

    return unlinkat(pwdfd, name, (isdir ? AT_REMOVEDIR : 0));
}


/**
 * @brief remove all files under the specified file.
 *
 * @param[in]  name  file or directory name
 * @param[in]  isdir  whether it is a directory, if known
 * @return bool  successful or not
 */
bool remove_all(const char *name, bool isdir){
    assert(name && *name);

    bool success = true;

    if ((isdir || (unlink(name) && (success = (errno == EISDIR)))) && (walk(name, removeat) || rmdir(name)))
        success = false;

    return success;
}




/**
 * @brief the callback function to be passed as 'filter' in glibc 'scandir' function.
 *
 * @param[in]  entry  a directory entry
 * @return int  whether it is a valid directory entry
 */
int filter_dirent(const struct dirent *entry){
    assert(entry);

    const char *name;

    name = entry->d_name;
    return check_if_valid_entry(name);
}




/******************************************************************************
    * String Recognizers
******************************************************************************/


/**
 * @brief receive the passed string as a positive integer or a set of two positive integer.
 *
 * @param[in]  target  target string
 * @param[out] p_left  if enable a range specification by using a hyphen, variable to store the left integer
 * @return int  the resulting (right) integer or -1
 *
 * @note receive an integer that can be expressed as "/^[0-9]+$/" in a regular expression.
 * @note when omitting an integer in the range specification, it is interpreted as specifying 0.
 */
int receive_positive_integer(const char *target, int *p_left){
    int curr = -1;

    if (target){
        int c, prev;

        curr = 0;
        c = (unsigned char) *target;

        do {
            if (isdigit(c)){
                if (curr <= (INT_MAX / 10)){
                    prev = curr * 10;
                    curr = prev + (c - '0');

                    if (curr >= prev)
                        continue;
                }
            }
            else if (p_left && (c == '-')){
                *p_left = curr;
                p_left = NULL;
                curr = 0;
                continue;
            }
            curr = -1;
            break;
        } while ((c = (unsigned char) *(++target)));
    }

    return curr;
}


/**
 * @brief find the passed string in array of expected strings.
 *
 * @param[in]  target  target string
 * @param[in]  reprs  array of expected strings
 * @param[in]  size  array size
 * @param[in]  mode  mode of comparison (bit 1: perform uppercase conversion, bit 2: accept forward match)
 * @return int  index number of the corresponding string, -1 (ambiguous) or -2 (invalid)
 *
 * @note make efficient by applying binary search sequentially from the first character of target string.
 */
int receive_expected_string(const char *target, const char * const *reprs, size_t size, unsigned int mode){
    assert(reprs);
    assert(size && ((size - 1) <= INT_MAX));
    assert(check_if_alphabetical_order(reprs, size));
    assert(mode < 4);

    if (target && (size > 0)){
        const char *expecteds[size];
        memcpy(expecteds, reprs, (sizeof(const char *) * size));

        bool upper_case, forward_match, break_flag;
        int min, max, tmp, c, mid;

        upper_case = mode & 0b01;
        forward_match = mode & 0b10;

        min = 0;
        max = size - 1;
        tmp = -max;

        while ((c = (unsigned char) *(target++))){
            if (upper_case)
                c = toupper(c);

            break_flag = false;

            assert(tmp == (min - max));
            while (tmp){
                if (tmp < 0){
                    mid = (min + max) / 2;

                    tmp = c - ((unsigned char) *(expecteds[mid]++));
                    if (tmp){
                        if (tmp > 0)
                            min = mid + 1;
                        else
                            max = mid - 1;
                    }
                    else {
                        for (tmp = mid; (--tmp >= min) && (c == ((unsigned char) *(expecteds[tmp]++))););
                        min = tmp + 1;

                        for (tmp = mid; (++tmp <= max) && (c == ((unsigned char) *(expecteds[tmp]++))););
                        max = tmp - 1;

                        break_flag = true;
                    }

                    tmp = min - max;
                    if (break_flag)
                        break;
                }
                else
                    return -2;
            }

            if (! (break_flag || (c == ((unsigned char) *(expecteds[min]++)))))
                return -2;
        }

        if (tmp)
            return -1;
        if (forward_match || (! *(expecteds[min])))
            return min;
    }
    return -2;
}


/**
 * @brief analyze which instruction in Dockerfile the specified line corresponds to.
 *
 * @param[in]  line  target line
 * @param[out] p_id  variable to store index number of the instruction to be compared
 * @return char*  substring that is the argument for the instruction in the target line or NULL
 *
 * @note if the content of 'p_id' is a valid index number, compares only with the corresponding instruction.
 * @note when there is a corresponding instruction, its index number is stored in 'p_id'.
 * @note if any instructions can be accepted, blank lines or comments are also accepted as valid lines.
 */
char *receive_dockerfile_instr(char *line, int *p_id){
    assert(line);
    assert(p_id);

    char *tmp = NULL, instr[12];
    size_t instr_len = 0;
    bool invalid;

    do {
        while (isspace((unsigned char) *line))
            line++;

        if (! tmp){
            if (*line && (*line != '#')){
                tmp = line;
                assert(tmp);

                do
                    if (! (*(++tmp) && (++instr_len < sizeof(instr))))
                        return NULL;
                while (! isspace((unsigned char) *tmp));

                memcpy(instr, line, (sizeof(char) * instr_len));
                instr[instr_len] = '\0';

                if (*p_id < 0){
                    *p_id = receive_expected_string(instr, docker_instr_reprs, DOCKER_INSTRS_NUM, 1);
                    invalid = (*p_id < 0);
                }
                else {
                    assert(*p_id < DOCKER_INSTRS_NUM);
                    invalid = xstrcmp_upper_case(instr, docker_instr_reprs[*p_id]);
                }

                if (! invalid){
                    line = tmp + 1;
                    continue;
                }
            }
            else if (*p_id < 0)
                break;
        }
        else if (*line)
            break;

        return NULL;
    } while (true);

    return line;
}




/******************************************************************************
    * Get Methods
******************************************************************************/


/**
 * @brief get the first line of target file.
 *
 * @param[in]  file_name  target file name
 * @return char*  the resulting line or NULL
 *
 * @note if the content of the target file spans multiple lines, it is regarded as an error.
 *
 * @attention internally, it uses 'xfgets_for_loop' with a depth of 1.
 * @attention if the return value is non-NULL, it should be released by the caller.
 */
char *get_one_liner(const char *file_name){
    char *line;
    int errid = 0;
    bool first_line = true;

    while (xfgets_for_loop(file_name, &line, &errid)){
        if (! first_line){
            assert(line);
            free(line);
            line = NULL;
            errid = -1;
        }
        first_line = false;
    }
    return line;
}




/**
 * @brief get the size of target file.
 *
 * @param[in]  file_name  target file name
 * @return int  the resulting file size, -1 (unexpected error) or -2 (too large)
 *
 * @note if the file is too large to pass to 'xfgets_for_loop', sets an error number indicating that.
 */
int get_file_size(const char *file_name){
    assert(file_name);

    struct stat file_stat;
    int i = -1;

    if (! stat(file_name, &file_stat)){
        if ((file_stat.st_size >= 0) && (file_stat.st_size < INT_MAX))
            i = file_stat.st_size;
        else {
            i = -2;
            errno = EFBIG;
        }
    }
    return i;
}


/**
 * @brief get the exit status of last executed command line.
 *
 * @return int  the resulting integer or -1
 *
 * @attention internally, it uses 'xfgets_for_loop' with a depth of 1.
 */
int get_last_exit_status(void){
    char *line;
    int i = -1;

    if ((line = get_one_liner(EXIT_STATUS_FILE))){
        if ((i = receive_positive_integer(line, NULL)) >= 256)
            i = -1;
        free(line);
    }
    return i;
}




/**
 * @brief get the sanitized string for display.
 *
 * @param[out] dest  where to store the sanitized string
 * @param[in]  target  target string
 * @param[in]  quoted  whether to use quotation
 * @return size_t  the length of the stored string
 *
 * @attention the size of 'dest' must be greater than four times the length of the string before conversion.
 */
size_t get_sanitized_string(char *dest, const char *target, bool quoted){
    assert(dest);
    assert(target);

    char *buf;
    unsigned int i;
    int c;

    buf = dest;

    while ((i = (unsigned char) *(target++))){
        c = '?';

        if (! (i & 0x80)){
            assert(i < sizeof(escape_char_table));

            switch ((c = escape_char_table[i])){
                case '?':
                    assert(iscntrl(i));
                    memcpy(buf, "\\x", 2);
                    buf += 2;
                    memcpy(buf, ((i & 0x60) ? "7F" : &escape_hex_table[i * 2]), 2);
                    buf += 2;
                    continue;
                case ' ':
                    assert(i == ' ');
                    if (! quoted)
                        break;
                case '_':
                    assert(isprint(i));
                    *(buf++) = i;
                    continue;
            }
        }

        assert(strchr("abefnrtv \"\'\\?", c));
        *(buf++) = '\\';
        *(buf++) = c;
    }

    *buf = '\0';

    assert(buf >= dest);
    return (size_t) (buf - dest);
}


/**
 * @brief print the sanitized string to stderr.
 *
 * @param[in]  target  target string
 *
 * @note add a space before the target string and display it.
 */
void print_sanitized_string(const char *target){
    assert(target);

    size_t size;
    size = (strlen(target) * 4 + 1) + 1;

    char buf[size];
    get_sanitized_string((buf + 1), target, false);

    *buf = ' ';
    fputs(buf, stderr);
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/


// if you want to a normal test for 'fgets_for_loop', comment out the line immediately after
#ifndef XFGETS_TEST_COMPLETE
// #define XFGETS_TEST_COMPLETE
#endif


static void xfgets_for_loop_test(void);
static void xstrcmp_upper_case_test(void);
static void xstrcat_inf_len_test(void);

static void execute_test(void);
static void walk_test(void);

static void receive_positive_integer_test(void);
static void receive_expected_string_test(void);
static void receive_dockerfile_instr_test(void);

static void get_one_liner_test(void);
static void get_file_size_test(void);
static void get_last_exit_status_test(void);
static void get_sanitized_string_test(void);




void dit_test(void){
    do_test(xfgets_for_loop_test);
    do_test(xstrcmp_upper_case_test);
    do_test(xstrcat_inf_len_test);

    do_test(receive_positive_integer_test);
    do_test(receive_expected_string_test);
    do_test(receive_dockerfile_instr_test);

    do_test(get_one_liner_test);
    do_test(get_file_size_test);
    do_test(get_last_exit_status_test);
    do_test(get_sanitized_string_test);

    do_test(execute_test);
    do_test(walk_test);
}




static void xfgets_for_loop_test(void){
    // when specifying the file whose contents do not end with a newline character

    // changeable part for updating test cases
    const char * const lines_without_newline[] = {
        "The parts where the test case can be updated by changing the code are commented as shown above.",
        "You can try changing the value of the macro 'XFGETS_INITIAL_SIZE' before testing.",
        "",
        NULL
    };

    const char * const *p_line;
    FILE *fp;
    bool isempty;
    char *line;
    int errid = 0;

    for (p_line = lines_without_newline; *p_line; p_line++){
        fprintf(stderr, "  Reading '%s' ...\n", *p_line);

        assert((fp = fopen(TMP_FILE1, "w")));
        assert(fputs(*p_line, fp) != EOF);
        assert(! fclose(fp));

        isempty = (! **p_line);

        do {
            line = xfgets_for_loop(TMP_FILE1, NULL, &errid);
            assert(! errid);

            if (isempty){
                assert(! line);
                break;
            }
            else {
                assert(! strcmp(line, *p_line));
                isempty = true;
            }
        } while (true);
    }


    // when specifying the file where multiple lines are stored and interrupting the reading in the middle

    // changeable part for updating test cases
    const char * const lines_with_trailing_newline[] = {
        "                                   .... (:((!\"`'?7OC+--...                      ",
        "          8                            ...?7~           _~_~~_.<~<!`.-?9~..     ",
        "                     8                        ..?!                ``````` ``    ",
        "  `_!__<~.                      8                     ..^      .`               ",
        "                    `_<.   ,^~?-           8                   .?`       .(~    ",
        "    .              _.             `_<>.    1.         8                 .?      ",
        "   (``      .(!              `` <.               <-    <         8              ",
        " .?         .>``     .J~.`                 `__                1.   1        8   ",
        "           J`         .~`     ..!_~ `                  `(                 (-. ._",
        "       8             >`  `      `z     .c~   .`    ._               <           ",
        "     . 1.`1       8            :` .(.     ._     `.:`   -    . ~..       .`    .",
        "`  .l            ``<..       8           >   _>.    .C`     .:`  .<.    .( ``   ",
        "    .-     :`  j_              (_-(.    8          J.   (>`   .Z_     .C   .v.  ",
        "   q  .        (.      \\` (:              `<!  (   8         .!    (z.   d:     ",
        " v~  .C_      j  .        <.` .....  (>               .1  /   8         <     (l",
        "_  /f.     (:   v~      ..  .    ....>.JJWWV1Jn.(l:               _+(_   8      ",
        "   ~     _1l  O>     :V   J:      .-C> .-<((((! v      (0f.(I& >             ~(2",
        "    8        .      ~+'`ld      (<  .<      `-  I (OAwOwn  }_ -+WVY=..(Z   :~   ",
        "        _([    8        (.      (l`lR      y~ .C       ;   O _yZAmdphgX77=~_..  ",
        "-dr   <:~_          :O.   8        <        ? dD     ( _ (~   .7<_(   O  (AWWY\"!",
        ".(_.`      _Hk    _~~_..(~.    :(~   8        (.        ?S5     j . 0:     ((v, ",
        "     v>      -`       (Wk  .wxuuXVT4X_    ~(;   8        ._~       Jtr     U _.I",
        ":    J>  %~~(<   -      (`      _JTWkX\"\"  :  (0Y:    :~[   8        `1_~     (  ",
        "r    ( :_JI  ~(v~       h~~<n-      >      ,v)IWAQ._v+US_~     _~~[   8         ",
        " -(~    J  $    z :_Kv:(Jnwg       (<{  I<     .-     >),7wWW9rX<\\        ~:_\\  ",
        " 8          (!    _0'  w   k ~(HWqHHMHHHHHH@H   &-$j       . >~h\\(\"\"  (?k    d  ",
        "     ~~._   8          (_   ~( '` :   w :?0Y::(MQHMHHHHMMHHMHU~l  l   }>)   (f.-",
        "(   ?Xkzw      (c~(~   8          !.   -Z~_~k~   O ~(`````WMM#=,``P:~?THb._   ( ",
        "        -dHHMHHHHWky      (6 $    8         w`    f``.      t ~(``  .MNd, .(, ``",
        "__~~ `:  ?_    .MHMWH@MHr<<7THHH  (< I  (    8        zt    (!``~      t ~(_` .J",
        "MHWNB=?b  ``      -!?<m`J ..#=,M| ````(0=(  :, k  %     8       J ~    (_``_~j  ",
        "  t _({`  J|  \"= .F  `          ``  JMB, .?b   ``` (   U<   SJ`    8      .  ~. ",
        "   L ` ~d    t  ($`  .h.77!.d!                 .MMTH=J]    ``.l V:~  tk`      8 ",
        "     R y _     n.``(    ld-?d_`  -?9.J\"                   J|  & .F    .zc&JZl  $",
        "         8     .+ r`     d  a,,    1   <-                            .h?7!uF    ",
        "` ? H\\  O  (S       8     v  }`     O _       ( b.   ~_(<_~                     ",
        "  9.7^    `  (R 'I (Z   9      8    J!  !`     l   tw .  . d_    ````           ",
        "        ._     _>)~     X0'`x  (<  l      8   ,(! .``     v  0  [ ,    {        ",
        "                          ````     W '`k   ?  ._     8  . I  ( `     I d `  $  _",
        " 4u                      _              (v?Td `-'|     <_ 1     8  C.) `( `   . ",
        " _0 `' C    : OU-.                 / ``\"        _-?4. ``?2~.       <  .    8 . (",
        "!   -`   ~( _I `'` j-  z:    yU+..(?77=i.                 ````j_?+.``z+     |   ",
        " >`   8  3._   t``_ ~d_(Z `'`  ;. _z_4x7=!...-77<. `?.               .>? d6*~_=.",
        "CC     ({   (_   8         7. _(Zt(7~~_Id(X_ .?- l._~(v````.(/   42TM#NNmgmO&+jV",
        "\"`   <>?v~_(d     .l    |   8           _7_d(i,     I;j-..O   (J>     `,L  `.Ye ",
        "   79% w  ,N,, 1j    ~(W     ((   `~   8                        jI1+(   +z!     ",
        ". ,H, .< ,h..J!      .#Nz.?(    _(v    _C.    `   8                         (<~`",
        "  >+!     .  ` .\"YSa+JMMSaJ....JY-X3..`.. -~Z JJ__J`,   J    8                  ",
        "      J~.``  j!`.   .C     .._-z   ?M   @ +Hg>?1z-. .~(!_~999  77!`     8",
        NULL
    };


    size_t count = 1, remain;
    char *start_for_file;

    assert((fp = fopen(TMP_FILE1, "w")));
    for (p_line = lines_with_trailing_newline; *p_line; p_line++){
        count++;
        assert(fputs(*p_line, fp) != EOF);
        assert(fputc('\n', fp) != EOF);
    }
    assert(! fclose(fp));

#ifdef XFGETS_TEST_COMPLETE
    assert(count);
    count -= rand() % count;
#endif
    assert(count);
    remain = rand() % count;
    count = remain;


    for (p_line = lines_with_trailing_newline; remain--; p_line++){
#ifndef XFGETS_TEST_COMPLETE
        fprintf(stderr, "  Reading '%s' ...\n", *p_line);
#endif
        assert((line = xfgets_for_loop(TMP_FILE1, &start_for_file, &errid)));
        assert(! strcmp(line, *p_line));
        assert(! errid);

#ifdef XFGETS_TEST_COMPLETE
        for (; *line; line++)
            fputc(((*line != '8') ? *line : '\n'), stderr);
#endif
    }


    // when accepting the input from standard input while reading other file

    char *start_for_stdin;

    fputs("\nChecking if it works the same as 'cat -' ...\n", stderr);

    for (remain = 0; (line = xfgets_for_loop(NULL, &start_for_stdin, NULL)); remain++)
        assert(puts(line) != EOF);

    do {
        if (! check_if_visually_no_problem())
            assert(false);

        if (start_for_stdin){
            assert(remain);

            fputs("Checking if the output matches the input ...\n", stderr);

            line = start_for_stdin;

            do {
                assert(puts(line) != EOF);
                line += strlen(line) + 1;
            } while (--remain);

            free(start_for_stdin);
            start_for_stdin = NULL;
        }
        else {
            assert(! remain);
            break;
        }
    } while (true);


    // when performing terminate processing for the interrupted file reading and confirming its correctness

    errid = -1;
    assert(! xfgets_for_loop(TMP_FILE1, &start_for_file, &errid));
    assert(errid == -1);

    if (start_for_file){
        assert(count);

        line = start_for_file;
        p_line = lines_with_trailing_newline;

        do {
            assert(! strcmp(line, *(p_line++)));
            line += strlen(line) + 1;
        } while (--count);

        free(start_for_file);
    }

    assert(! count);


    // when specifying a non-existing file

    assert(! unlink(TMP_FILE1));
    assert(! xfgets_for_loop(TMP_FILE1, NULL, &errid));
    assert(errid == ENOENT);
}




static void xstrcmp_upper_case_test(void){
    // changeable part for updating test cases
    comptest_table table[] = {
        { { .name = "none"                   }, { .name = "NONE"                      }, equal   },
        { { .name = "hoGe-PIyO"              }, { .name = "HOGE-PIYO"                 }, equal   },
        { { .name = "f.lwhaeopyr;pfqwnel.FG" }, { .name = "F.LWHAEOPYR;PFQWNEL.FG"    }, equal   },
        { { .name = "Quit"                   }, { .name = "YES"                       }, lesser  },
        { { .name = "niPPon"                 }, { .name = "NIPPORI"                   }, lesser  },
        { { .name = "fasldhfoNLASOL>NAZHO"   }, { .name = "FASLFN/L?EF=ONLAS|OLX{ZHO" }, lesser  },
        { { .name = "SIGKILL"                }, { .name = "SIGINT"                    }, greater },
        { { .name = "Super Sento"            }, { .name = "SUPER MARKET"              }, greater },
        { { .name = "On your marks, Set, Go" }, { .name = "ON YOUR MARK!"             }, greater },
        { { .name =  0                       }, { .name =  0                          }, end     }
    };

    int i;
    const char *target, *expected;

    for (i = 0; table[i].type != end; i++){
        target = table[i].elem1.name;
        expected = table[i].elem2.name;
        assert(check_if_correct_cmp_result(table[i].type, xstrcmp_upper_case(target, expected)));

        print_progress_test_loop('C', table[i].type, i);
        fprintf(stderr, "%-22s  %s\n", target, expected);
    }
}




static void xstrcat_inf_len_test(void){
    const struct {
        const char * const path;
        const size_t inherit;
        const char * const result;
    }
    // changeable part for updating test cases
    table[] = {
        { "dit/",                 0, "dit/"                        },
        { "tmp/",                 4, "dit/tmp/"                    },
        { "last-history-number",  8, "dit/tmp/last-history-number" },
        { "reflect-report.prov",  8, "dit/tmp/reflect-report.prov" },
        { "etc/config.stat",      4, "dit/etc/config.stat"         },
        { "./",                   0, "./"                          },
        { "../etc/passwd",        2, "./../etc/passwd"             },
        { "../etc/passwd",        5, "./../../etc/passwd"          },
        { "../etc/passwd",        8, "./../../../etc/passwd"       },
        { "malware.sh",          15, "./../../../etc/malware.sh"   },
        {  0,                     0,  0                            }
    };

    int i;
    inf_str istr = {0};

    for (i = 0; table[i].path; i++){
        assert(xstrcat_inf_len(&istr, table[i].inherit, table[i].path, (strlen(table[i].path) + 1)));
        assert(istr.ptr);
        assert(istr.max == XSTRCAT_INITIAL_MAX);
        assert(! strcmp(istr.ptr, table[i].result));

        print_progress_test_loop('\0', -1, i);
        fprintf(stderr, "%s\n", table[i].result);
    }


    // changeable part for updating test cases
    const char *repeat = "'a string of arbitrary length'";

    size_t size, istr_len = 0;
    int iter = 0;

    size = strlen(repeat);
    assert(size);

    while ((size * (++iter) + 1) <= XSTRCAT_INITIAL_MAX);

    for (i = 0; i < iter;){
        assert(istr.max == XSTRCAT_INITIAL_MAX);
        assert(xstrcat_inf_len(&istr, istr_len, repeat, (size + 1)));
        assert(istr.ptr);

        istr_len += size;
        print_progress_test_loop('\0', -1, i);
        fprintf(stderr, "%s * %d\n", repeat, ++i);
    }

    assert(istr.max > XSTRCAT_INITIAL_MAX);

    free(istr.ptr);
}




static void execute_test(void){
    char * const argv[] = { "cat", "-", NULL };

    int i, c, exit_status;
    const char *addition;

    for (i = 0; i < 2; i++){
        if (! i){
            c = 'D';
            exit_status = 0;
            addition = "1>&2";
        }
        else {
            c = 'C';
            exit_status = 128 + SIGINT;
            addition = "> /dev/null";
        }

        fprintf(stderr, "\nChecking if child process can be exited by 'Ctrl + %c' ...\n", c);

        assert(execute("/bin/cat", argv, i) == exit_status);

        fprintf(stderr, "- cat %s\n", addition);

        if (! check_if_visually_no_problem())
            assert(false);
    }
}




static inf_str walked_istr = {0};
static size_t walked_len = 0;


static int walk_test_stub(int pwdfd, const char *name, bool isdir){
    assert(pwdfd >= 0);
    assert(name && *name);

    size_t size;
    int exit_status = SUCCESS;

    if (! isdir){
        size = strlen(name) + 1;

        if (xstrcat_inf_len(&walked_istr, walked_len, name, size))
            walked_len += size;
        else
            exit_status = FAILURE;
    }

    return exit_status;
}


static void walk_test(void){
    // changeable part for updating test cases
    const char * const root_dirs[] = {
        "/dit",
        "/etc/./",
        "/usr/local/..",
        NULL
    };

    int i, c;
    char cmdline[128];
    struct stat file_stat;
    void *addr;
    const char *found, *walked, *tmp;
    size_t name_len;

    for (i = 0; root_dirs[i]; i++){
        assert(*(root_dirs[i]));
        fprintf(stderr, "  Walking '%s' ...\n", root_dirs[i]);

        c = snprintf(cmdline, sizeof(cmdline), "find %s -depth ! -type d -print0 > "TMP_FILE1, root_dirs[i]);
        assert((c >= 0) && (c < sizeof(cmdline)));
        assert(system(NULL) && (! system(cmdline)));

        assert(! walked_len);
        assert(! walk(root_dirs[i], walk_test_stub));

        assert((c = open(TMP_FILE1, O_RDONLY)) != -1);
        assert(! fstat(c, &file_stat));
        assert(file_stat.st_size >= 0);
        assert((addr = mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, c, 0)) != MAP_FAILED);

        found = (const char *) addr;
        walked = walked_istr.ptr;

        while (walked_len){
            if ((tmp = strrchr(found, '/')))
                found = tmp + 1;
            name_len = strlen(found) + 1;

            fprintf(stderr, "    strcmp(W, F):  '%s'  '%s'\n", walked, found);
            assert(! memcmp(walked, found, name_len));

            found += name_len;
            walked += name_len;
            walked_len -= name_len;
        }

        assert(! munmap(addr, file_stat.st_size));
        assert(! close(c));
    }

    assert(walked_istr.ptr);
    free(walked_istr.ptr);
}




static void receive_positive_integer_test(void){
    const struct {
        const char * const target;
        const int left;
        const int right;
    }
    // changeable part for updating test cases
    table[] = {
        { "0",            -2,    0 },
        { "23",           -2,   23 },
        { "0601",         -2,  601 },
        { "456",          -1,  456 },
        { "4-17",          4,   17 },
        { "89-12",        89,   12 },
        { "-2022",         0, 2022 },
        { "03629-",     3629,    0 },
        { "-",             0,    0 },
        { "2o1",          -2,   -1 },
        { "int",          -2,   -1 },
        { "-29",          -2,   -1 },
        { "4294967295",   -2,   -1 },
        { "_73",          -1,   -1 },
        { "3278o-3y28",   -1,   -1 },
        { "zwei-vier",    -1,   -1 },
        { "7-ten",         7,   -1 },
        { "--",            0,   -1 },
        {  0,              0,    0 }
    };

    int i, left;
    bool range_flag;


    for (i = 0; table[i].target; i++){
        range_flag = (table[i].left != -2);
        left = -1;

        assert(receive_positive_integer(table[i].target, (range_flag ? &left : NULL)) == table[i].right);
        if (range_flag)
            assert(left == table[i].left);

        print_progress_test_loop('S', ((table[i].right != -1) ? SUCCESS : FAILURE), i);
        fprintf(stderr, "%-10s  % 5d % 5d\n", table[i].target, table[i].left, table[i].right);
    }
}




static void receive_expected_string_test(void){
    const struct {
        const char * const target;
        const unsigned int mode;
        const int result;
    }
    // changeable part for updating test cases
    table[] = {
        { "COPY",      0, ID_COPY        },
        { "WORKDIR",   0, ID_WORKDIR     },
        { "SHELL",     0, ID_SHELL       },
        { "LABEL",     0, ID_LABEL       },
        { "Volume",    1, ID_VOLUME      },
        { "from",      1, ID_FROM        },
        { "add",       1, ID_ADD         },
        { "ExPose",    1, ID_EXPOSE      },
        { "HEA",       2, ID_HEALTHCHECK },
        { "R",         2, ID_RUN         },
        { "ENT",       2, ID_ENTRYPOINT  },
        { "AR",        2, ID_ARG         },
        { "env",       3, ID_ENV         },
        { "main",      3, ID_MAINTAINER  },
        { "sTo",       3, ID_STOPSIGNAL  },
        { "Cmd",       3, ID_CMD         },
        { "copy",      0,   -2           },
        { "WORK DIR",  0,   -2           },
        { "S",         0,   -1           },
        { "QWERT",     0,   -2           },
        { "vol"   ,    1,   -2           },
        { "Form",      1,   -2           },
        { "a",         1,   -1           },
        { "Lap-Top",   1,   -2           },
        { "health",    2,   -2           },
        { "Run",       2,   -2           },
        { "EN",        2,   -1           },
        { "STRAW",     2,   -2           },
        { "environ",   3,   -2           },
        { "Main Tain", 3,   -2           },
        { "",          3,   -1           },
        { "2.3],fm';", 3,   -2           },
        {  0,          0,    0           }
    };

    int i, id;
    const char *instr_repr;


    for (i = 0; table[i].target; i++){
        id = receive_expected_string(table[i].target, docker_instr_reprs, DOCKER_INSTRS_NUM, table[i].mode);
        assert(id == table[i].result);

        if (id >= 0){
            instr_repr = docker_instr_reprs[id];
            id = SUCCESS;
        }
        else {
            instr_repr = " ?";
            id = FAILURE;
        }

        print_progress_test_loop('S', id, i);
        fprintf(stderr, "%-9s  %s\n", table[i].target, instr_repr);
    }
}




static void receive_dockerfile_instr_test(void){
    const struct {
        char * const line;
        const int expected_id;
        const int actual_id;
        const int offset;
    }
    // changeable part for updating test cases
    table[] = {
        { "ADD abc.tar.gz ./",                      ID_ADD,         ID_ADD,          4 },
        { " USER root",                             ID_USER,        ID_USER,         6 },
        { "HealthCheck  Cmd sh /bin/check-running", ID_HEALTHCHECK, ID_HEALTHCHECK, 13 },
        { "EXPOSE 80/tcp 80/udp",                     -1,           ID_EXPOSE,       7 },
        { "RUN make && make clean",                   -1,           ID_RUN,          4 },
        { "    OnBuild  WorkDir /",                   -1,           ID_ONBUILD,     13 },
        { "",                                         -1,             -1,            0 },
        { " # the last success case",                 -1,             -1,            1 },
        { "COPY ./etc/dit_install.sh /dit/etc/",    ID_ADD,         ID_COPY,        -1 },
        { "MainTainer inada",                       ID_LABEL,       ID_MAINTAINER,  -1 },
        { "form alpine:latest",                     ID_FROM,          -1,           -1 },
        { "  Volume",                                 -1,             -1,           -1 },
        { "En dit inspect",                           -1,             -1,           -1 },
        { "setcmd  [ \"/bin/bash\", \"--login\" ]",   -1,             -1,           -1 },
        { "    ",                                   ID_STOPSIGNAL,    -1,           -1 },
        { "# escape=`",                             ID_ENV,           -1,           -1 },
        {  0,                                          0,              0,            0 }
    };

    int i, id, remain;
    char *line, *args;


    for (i = 0; (line = table[i].line); i++){
        id = table[i].expected_id;
        args = receive_dockerfile_instr(line, &id);

        if (table[i].offset >= 0){
            assert(args == (line + table[i].offset));
            assert(id == table[i].actual_id);
            id = SUCCESS;
        }
        else {
            assert(! args);
            id = FAILURE;
        }

        print_progress_test_loop('S', id, i);
        fprintf(stderr, "%-38s", line);

        for (remain = 2; remain--;){
            id = remain ? table[i].expected_id : table[i].actual_id;
            fprintf(stderr, "  %-11s", ((id >= 0) ? docker_instr_reprs[id] : " ?"));
        }

        assert(remain == -1);
        fputc('\n', stderr);
    }
}




static void get_one_liner_test(void){
    const struct {
        const char * const input;
        const char * const result;
    }
    // changeable part for updating test cases
    table[] = {
        { "\n",           ""       },
        { "abc de\n",     "abc de" },
        { "\t-1",         "\t-1"   },
        { "%%~%%",        "%%~%%"  },
        { "",              NULL    },
        { "apple\npen\n",  NULL    },
        { "\n\n",          NULL    },
        { "\n\n\n",        NULL    },
        {  0,                0     }
    };


    int i;
    FILE *fp;
    char *line;

    for (i = 0; table[i].input; i++){
        assert((fp = fopen(TMP_FILE1, "w")));
        assert(fputs(table[i].input, fp) >= 0);
        assert(! fclose(fp));

        line = get_one_liner(TMP_FILE1);

        if (table[i].result){
            assert(line);
            assert(! strcmp(line, table[i].result));
            free(line);
            line = NULL;
        }
        assert(! line);

        print_progress_test_loop('\0', -1, i);
        fprintf(stderr, "'%s'\n", table[i].result ? table[i].result : "#NULL");
    }

    assert(! unlink(TMP_FILE1));
    assert(! get_one_liner(TMP_FILE1));
    assert(errno == ENOENT);
}




static void get_file_size_test(void){
    assert(sizeof(char) == 1);

    // when specifying a valid file

    // changeable part for updating test cases
    const unsigned int digit = 6;
    assert(digit <= 10);

    int i, divisor = 1, fd;
    size_t size;
    char *tmp;

    for (i = -1; ++i < digit; divisor *= 10) {
        size = rand() % divisor;

        assert((fd = open(TMP_FILE1, (O_WRONLY | O_CREAT | O_TRUNC))) != -1);
        if (size){
            assert((tmp = calloc(size, sizeof(char))));
            assert(write(fd, tmp, size) == size);
            free(tmp);
        }
        assert(! close(fd));

        assert(get_file_size(TMP_FILE1) == size);

        fprintf(stderr, "  size:  %*zu\n", digit, size);
    };


    // when specifying a file that is too large

    assert(system(NULL) && (! system("dd if=/dev/zero of="TMP_FILE1" bs=1M count=2K 2> /dev/null")));
    assert(get_file_size(TMP_FILE1) == -2);
    assert(errno == EFBIG);


    // when specifying a non-existing file

    assert(! unlink(TMP_FILE1));
    assert(get_file_size(TMP_FILE1) == -1);
    assert(errno == ENOENT);
}




static void get_last_exit_status_test(void){
    int curr;

    curr = get_last_exit_status();
    assert((curr >= 0) && (curr < 256));


    const struct {
        const char * const format;
        const int input;
        const int result;
    }
    // changeable part for updating test cases
    table[] = {
        { "%d\n",                     0,    0 },
        { "127\n",                   -1,  127 },
        { "%d",                     255,  255 },
        { "1",                       -1,    1 },
        { "%d\n",                   256,   -1 },
        { "\n",                      -1,   -1 },
        { "%d\nsuperfluous\n",        2,   -1 },
        { "",                        -1,   -1 },
        { "%d\n",                  curr, curr },
        {  0,                         0,    0 }
    };


    int i;
    FILE *fp;

    for (i = 0; table[i].format; i++){
        assert((fp = fopen(EXIT_STATUS_FILE, "w")));
        assert(fprintf(fp, table[i].format, table[i].input) >= 0);
        assert(! fclose(fp));

        assert(get_last_exit_status() == table[i].result);

        print_progress_test_loop('\0', -1, i);
        fprintf(stderr, "% 6d  % 4d\n", table[i].input, table[i].result);
    }
}




static void get_sanitized_string_test(void){
    const struct {
        const char *target;
        const bool quoted;
        const char *result;
    }
    // changeable part for updating test cases
    table[] = {
        { "",                     true, ""                         },
        { "*.txt",                true, "*.txt"                    },
        { "([\'\"])@you\\1",      true, "([\\\'\\\"])@you\\\\1"    },
        { " \t",                  true, " \\t"                     },
        { "extension",           false, "extension"                },
        { "\002-\020",           false, "\\x02-\\x10"              },
        { "\a\b \r\n \v\f",      false, "\\a\\b\\ \\r\\n\\ \\v\\f" },
        { "\033[??;??m \033[0m", false, "\\e[??;??m\\ \\e[0m"      },
        {  0,                      0,    0                         }
    };

    int i;
    char buf[64];
    size_t size;

    for (i = 0; table[i].target; i++){
        size = get_sanitized_string(buf, table[i].target, table[i].quoted);
        assert(size == strlen(table[i].result));
        assert(! strcmp(buf, table[i].result));

        print_progress_test_loop('\0', -1, i);
        fprintf(stderr, "'%s'\n", table[i].result);
    }
}




#endif // NDEBUG