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
#define XFGETS_INITIAL_MAX 1023  // 2^n - 1

#define XSTRCAT_INITIAL_MAX 1023  // 2^n - 1


/** Data type for storing the information for one loop for 'xfgets_for_loop' */
typedef struct {
    const void *key;    /** address of the area where the source file name is stored or NULL */
    FILE *fp;           /** handler for the source file */
    char *dest;         /** pointer to the beginning of dynamic memory for storing read lines as string */
    int curr_max;       /** the current maximum length of the string that can be preserved */
    int curr_len;       /** the total length of the preserved strings including null characters */
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
        memcpy(buf, "#NULL", (sizeof(char) * 6));


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
 * @param[in]  lineno  line number
 * @param[in]  msg  the error message
 *
 * @note line number starts from 1.
 */
void xperror_file_contents(const char *file_name, size_t lineno, const char *msg){
    assert(lineno);
    assert(msg);

    if (! file_name)
        file_name = "stdin";

    fprintf(stderr, "%s: %s: line %zu: %s\n", program_name, file_name, lineno, msg);
}




/******************************************************************************
    * Extensions of Standard Library Functions
******************************************************************************/


/**
 * @brief read the contents of the specified file exactly one line at a time.
 *
 * @param[in]  src_file  source file name or NULL to indicate that the source is standard input
 * @param[out] p_start  variable to store the beginning of a series of strings corresponding to read lines
 * @param[out] p_len  variable to store the total length of the preserved strings including null characters
 * @param[out] p_errid  variable to store the error number when an error occurs
 * @return char*  the resulting line or NULL
 *
 * @note read all lines of the file by using this function as a conditional expression in a loop statement.
 * @note this function can be nested up to a depth of 'XFGETS_NESTINGS_MAX' by passing different 'src_file'.
 * @note if 'p_start' is non-NULL, preserves read lines into the dynamic memory pointed to by its contents.
 * @note the contents of 'p_start' points to a valid address only when the file has one or more lines.
 * @note all lines of the file whose size is larger than 'INT_MAX' cannot be preserved.
 * @note the contents of 'p_len' acts as an offset for concatenating newly read line into preserved lines.
 * @note line concatenation by specifying a valid 'p_len' will occur even between separate series of calls.
 * @note if you want to prevent the above behavior, you should initialize the contents of 'p_len' to 0.
 * @note if aborting due to an error, the contents of 'p_errid' will be the positive error number.
 * @note if the contents of 'p_errid' is changed to a non-zero value inside the loop, it does end processing.
 * @note the trailing newline character of the line that is the return value is stripped.
 *
 * @attention this function must be called until NULL is returned, since it uses dynamic memory internally.
 * @attention if you specify a non-NULL for the argument, its contents should be properly initialized.
 * @attention normally, the address pointed to by 'p_start' should remain constant during a series of calls.
 * @attention if preserving read lines, they should be released by the caller after a series of calls.
 * @attention the contents of 'p_len' should not be updated except to discard the previously read lines.
 * @attention the contents of 'p_errid' makes no sense until at least one line is read in a series of calls.
 * @attention except for trivial cases, you should check for errors and abort when an error has occurred.
 * @attention the return value must not used outside the loop because 'realloc' function may invalidate it.
 */
char *xfgets_for_loop(const char *src_file, char **p_start, size_t *p_len, int *p_errid){
    assert(XFGETS_NESTINGS_MAX > 0);

    static int info_idx = -1;
    static xfgets_info info_list[XFGETS_NESTINGS_MAX] = {0};

    xfgets_info info;
    char *start;
    int used_len = 0, errid = 0;
    unsigned int len;

    if ((info_idx < 0) || (src_file != info_list[info_idx].key)){
        if (info_idx >= (XFGETS_NESTINGS_MAX - 1))
            return NULL;

        info.key = src_file;
        info.fp = stdin;

        if (src_file && (! (info.fp = fopen(src_file, "r")))){
            if (p_errid)
                *p_errid = errno;
            return NULL;
        }

        info_idx++;
        assert(info_idx >= 0);

        if (! (p_len && *p_len && (*p_len <= info_list[info_idx].curr_len))){
            info.dest = NULL;
            info.curr_max = XFGETS_INITIAL_MAX;
            info.curr_len = 0;
            goto allocate;
        }

        info.dest = info_list[info_idx].dest;
        info.curr_max = info_list[info_idx].curr_max;
        info.curr_len = *p_len;
    }
    else {
        memcpy(&info, (info_list + info_idx), sizeof(xfgets_info));

        if (p_len && (*p_len <= info.curr_len))
            info.curr_len = *p_len;
    }

    assert(info.dest);
    assert(info.curr_max > 0);
    assert(info.curr_len < info.curr_max);

    if (! (p_errid && *p_errid)){
        start = info.dest;
        used_len = info.curr_len;
        goto reading;
    }

    goto finish;


resize:
    len = info.curr_max + 1;
    assert(! (len & (len - 1)));

    if (! (len <<= 1)){
        errid = EFBIG;
        goto finish;
    }
    info.curr_max = len - 1;

allocate:
    assert(info.curr_max > 0);

    if (! (start = (char *) realloc(info.dest, (sizeof(char) * info.curr_max)))){
        errid = errno;
        goto finish;
    }
    info.dest = start;


reading:
    assert(start == info.dest);
    assert((used_len >= 0) && (used_len < info.curr_max));

    errno = 0;
    start += used_len;

    if (fgets(start, (info.curr_max - used_len), info.fp)){
        len = strlen(start);
        used_len += len;
        assert(used_len >= len);

        if ((! len) || (start[--len] != '\n'))
            goto resize;

        start[len] = '\0';
    }
    else if ((ferror(info.fp) && (errid = errno)) || (used_len++ == info.curr_len))
        goto finish;

    start = info.dest;

    if (p_start){
        *p_start = start;
        start += info.curr_len;
        info.curr_len = used_len;
    }
    if (p_len){
        assert(info.curr_len < INT_MAX);
        *p_len = info.curr_len;
    }

    assert(info.dest);
    assert(info.curr_max > 0);

    memcpy((info_list + info_idx), &info, sizeof(xfgets_info));
    return start;


finish:
    info_list[info_idx--].curr_len = info.curr_len;

    if (p_errid && errid)
        *p_errid = errid;

    assert(src_file == info.key);
    assert(info.fp);

    if (src_file)
        fclose(info.fp);
    else
        clearerr(info.fp);

    if (p_start){
        if ((! errid) && (info.curr_len > 0)){
            assert(info.dest);
            assert(info.curr_max > 0);
            assert(info_list[info_idx + 1].dest == info.dest);
            assert(info_list[info_idx + 1].curr_max == info.curr_max);

            *p_start = info.dest;
            return NULL;
        }
        *p_start = NULL;
    }

    if (info.dest)
        free(info.dest);

    memset((info_list + info_idx + 1), 0, sizeof(xfgets_info));
    return NULL;
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

    size_t curr_max;
    bool allocate_flag;
    char *start;

    curr_max = base->max;
    allocate_flag = (! curr_max);

    if (allocate_flag)
        curr_max = XSTRCAT_INITIAL_MAX;

    while ((curr_max - base_len) < suf_len){
        curr_max++;
        assert(! (curr_max & (curr_max - 1)));

        if (! (curr_max <<= 1))
            return false;
        curr_max--;
        allocate_flag = true;
    }

    if (allocate_flag){
        if (! (start = (char *) realloc(base->ptr, (sizeof(char) * curr_max))))
            return false;
        base->ptr = start;
        base->max = curr_max;
    }

    assert(base->ptr);
    assert(base->max);

    start = base->ptr + base_len;
    memcpy(start, suf, (sizeof(char) * suf_len));

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
 * @param[in]  mode  some flags (bit 1: how to handle stdout, bit 2: refrain from printing extra messages)
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

    struct sigaction new_act = {0}, sigint_act, sigquit_act;
    sigset_t old_mask;

    pid_t pid, err = -1;
    int tmp = 0, exit_status = -1;

    if (! (mode & 0b10)){
        fputc('+', stderr);

        for (char * const *p_arg = argv; *p_arg; p_arg++)
            print_sanitized_string(*p_arg);

        fputc('\n', stderr);
    }

    new_act.sa_handler = SIG_IGN;
    sigemptyset(&(new_act.sa_mask));
    sigaction(SIGINT, &new_act, &sigint_act);
    sigaction(SIGQUIT, &new_act, &sigquit_act);

    sigaddset(&(new_act.sa_mask), SIGCHLD);
    sigprocmask(SIG_BLOCK, &(new_act.sa_mask), &old_mask);

    switch ((pid = fork())){
        case 0:   // child
            tmp = (mode & 0b01) ? open("/dev/null", O_WRONLY) : STDERR_FILENO;
            exit_status = 126;

            if (tmp >= 0){
                if (dup2(tmp, STDOUT_FILENO) != -1){
                    err = 0;
                    exit_status++;
                }
                if (tmp != STDERR_FILENO)
                    close(tmp);
            }
            break;
        case -1:  // error
            break;
        default:  // parent
            while (((err = waitpid(pid, &tmp, 0)) == -1) && (errno == EINTR));
    }

    sigaction(SIGINT, &sigint_act, NULL);
    sigaction(SIGQUIT, &sigquit_act, NULL);
    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    // child
    if (! pid){
        if (! err){
            execv(cmd_file, argv);
            assert(exit_status == 127);
        }
        _exit(exit_status);
    }

    // parent
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
 * @brief the function that recursively scans the specified file and all files below it
 *
 * @param[in]  pwdfd  file descriptor that serves as the current working directory
 * @param[in]  name  name of the file we are currently looking at
 * @param[in]  type  file type (true (is directory), false (is not directory) or -1 (not yet known))
 * @param[in]  callback  a callback function like "*at()" family of syscalls
 * @return int  successful or not
 *
 * @note the arguments received by the callback function are the almost identical to those of this function.
 * @note the third argument of the callback function indicates whether the file of interest is a directory.
 * @note the callback function must return 0 on success and non-zero on failure.
 */
bool walkat(int pwdfd, const char *name, int type, int (* callback)(int, const char *, bool)){
    assert((pwdfd >= 0) || (pwdfd == AT_FDCWD));
    assert(name && *name);
    assert((type >= -1) && (type <= 1));
    assert(callback);

    bool call_ok;
    int new_fd;
    DIR *dir;

    call_ok = (! type);

    if (type){
        if ((new_fd = openat(pwdfd, name, (O_RDONLY | O_DIRECTORY))) != -1){
            if ((dir = fdopendir(new_fd))){
                struct dirent *entry;
                const char *child;
                bool isdir;
                struct stat file_stat;

                while ((entry = readdir(dir))){
                    child = entry->d_name;
                    assert(child && *child);

                    if (check_if_valid_dirent(child)){
#ifdef _DIRENT_HAVE_D_TYPE
                        if (entry->d_type != DT_UNKNOWN)
                            isdir = (entry->d_type == DT_DIR);
                        else
#endif
                        if (! fstatat(new_fd, child, &file_stat, AT_SYMLINK_NOFOLLOW))
                            isdir = S_ISDIR(file_stat.st_mode);
                        else
                            break;

                        if (! walkat(new_fd, child, isdir, callback))
                            break;
                    }
                }

                closedir(dir);

                type = true;
                call_ok = (! entry);
            }
            else
                close(new_fd);
        }
        else if ((type == -1) && (errno == ENOTDIR)){
            type = false;
            call_ok = true;
        }
    }

    assert((! call_ok) || (type == ((bool) type)));
    return (bool) (call_ok && (! callback(pwdfd, name, type)));
}




/**
 * @brief the callback function to be passed as 'callback' in above 'walkat' function
 *
 * @param[in]  pwdfd  file descriptor that serves as the current working directory
 * @param[in]  name  name of the file we are currently looking at
 * @param[in]  isdir  whether it is a directory
 * @return int  0 (success) or -1 (unexpected error)
 */
int removeat(int pwdfd, const char *name, bool isdir){
    assert((pwdfd >= 0) || (pwdfd == AT_FDCWD));
    assert(name && *name);

    return unlinkat(pwdfd, name, (isdir ? AT_REMOVEDIR : 0));
}


/**
 * @brief the callback function to be passed as 'filter' in glibc 'scandir' function
 *
 * @param[in]  entry  a directory entry
 * @return int  whether it is a valid directory entry
 */
int filter_dirent(const struct dirent *entry){
    assert(entry);
    assert(entry->d_name && *(entry->d_name));

    return check_if_valid_dirent(entry->d_name);
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
    int result = -1;

    if (target){
        int c, i;

        result = 0;

        do {
            c = (unsigned char) *target;
            i = c - '0';

            if ((i >= 0) && (i < 10)){
                if (result <= (INT_MAX / 10)){
                    result *= 10;
                    result += i;

                    if (result >= i)
                        continue;
                }
            }
            else if (p_left && (c == '-')){
                *p_left = result;

                result = 0;
                p_left = NULL;
                continue;
            }

            result = -1;
            break;

        } while (*(++target));
    }

    return result;
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
 * @note array of expected strings can contain duplicate strings and empty strings.
 * @note in normal mode, it uses 'strcmp' function for speed efficiency, albeit verbosely.
 *
 * @attention array of expected strings must be pre-sorted.
 */
int receive_expected_string(const char *target, const char * const *reprs, size_t size, unsigned int mode){
    assert(reprs);
    assert(size && (size < INT_MAX));
    assert(check_if_presorted(reprs, size));
    assert(mode < 4);

    if (target && size){
        const char *expecteds[size];
        memcpy(expecteds, reprs, (sizeof(const char *) * size));

        bool upper_case, forward_match;
        int min, max, tmp, c, mid;

        upper_case = mode & 0b01;
        forward_match = mode & 0b10;

        min = 0;
        max = size - 1;
        tmp = -max;

    bsearch:
        assert(tmp == (min - max));
        assert(tmp <= 0);

        // while statement (next character)
        if ((c = (unsigned char) *(target++))){
            if (upper_case)
                c = toupper(c);

            while (tmp){
                mid = (min + max) / 2;
                tmp = c - ((unsigned char) *(expecteds[mid]++));

                if (! tmp){
                    for (tmp = mid; (--tmp >= min) && (c == ((unsigned char) *(expecteds[tmp]++))););
                    min = tmp + 1;
                    assert(min <= mid);

                    for (tmp = mid; (++tmp <= max) && (c == ((unsigned char) *(expecteds[tmp]++))););
                    max = tmp - 1;
                    assert(max >= mid);

                    tmp = min - max;
                    goto bsearch;
                }
                if (tmp > 0)
                    min = mid + 1;
                else
                    max = mid - 1;

                tmp = min - max;
                assert(tmp || (min != mid));

                if (tmp > 0)
                    return -2;
            }

            assert(min == max);

            if (c == ((unsigned char) *(expecteds[min]++))){
                if (mode)
                    goto bsearch;
                if (! strcmp(target, expecteds[min]))
                    return min;
            }
            return -2;
        }

        if (! *(expecteds[min]))
            return min;
        if (tmp)
            return -1;
        if (forward_match)
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

    size_t len = 0;
    char instr[12] = {0};
    bool invalid;

    do {
        while (isspace((unsigned char) *line))
            line++;

        if (! len){
            if (*line && (*line != '#')){
                do
                    if (! (line[++len] && (len < sizeof(instr))))
                        return NULL;
                while (! isspace((unsigned char) line[len]));

                memcpy(instr, line, (sizeof(char) * len));

                if (*p_id < 0){
                    *p_id = receive_expected_string(instr, docker_instr_reprs, DOCKER_INSTRS_NUM, 1);
                    invalid = (*p_id < 0);
                }
                else {
                    assert(*p_id < DOCKER_INSTRS_NUM);
                    invalid = xstrcmp_upper_case(instr, docker_instr_reprs[*p_id]);
                }

                if (! invalid){
                    line += len + 1;
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
 * @note on success, resize the resulting string to the minimum necessary size if possible.
 *
 * @attention internally, it uses 'xfgets_for_loop' with a depth of 1.
 * @attention if the return value is non-NULL, it should be released by the caller.
 */
char *get_one_liner(const char *file_name){
    char *line = NULL;
    size_t len = 0;
    int errid = 0;
    bool first_line = true;
    void *ptr;

    while (xfgets_for_loop(file_name, &line, &len, &errid)){
        if (! first_line){
            len = 0;
            errid = -1;
        }
        first_line = false;
    }

    if (line){
        assert(len);
        assert(! errid);

        if ((ptr = realloc(line, (sizeof(char) * len))))
            line = (char *) ptr;
    }

    return line;
}


/**
 * @brief get the response to the specified inquiry.
 *
 * @param[in]  inquiry  inquiry body
 * @param[in]  format  the format of the input you want to receive
 * @param[out] ...  variables to receive the input
 *
 * @attention the expected input must fit on one line.
 */
void get_response(const char *inquiry, const char *format, ...){
    assert(inquiry);
    assert(format && strstr(format, "[^\n]"));

    va_list sp;
    int count;

    fputs(inquiry, stderr);
    fflush(stderr);

    va_start(sp, format);
    count = vscanf(format, sp);
    va_end(sp);

    if ((count == EOF) || (scanf("%*[^\n]") == EOF) || (getchar() == EOF))
        clearerr(stdin);
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
    int exit_status = -1;

    if ((line = get_one_liner(EXIT_STATUS_FILE))){
        if ((exit_status = receive_positive_integer(line, NULL)) >= 256)
            exit_status = -1;
        free(line);
    }
    return exit_status;
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

        if (! (i / 128)){
            assert(i < sizeof(escape_char_table));

            switch ((c = escape_char_table[i])){
                case '?':
                    memcpy(buf, "\\x", (sizeof(char) * 2));
                    buf += 2;
                    memcpy(buf, ((i / 32) ? "7F" : &(escape_hex_table[i * 2])), (sizeof(char) * 2));
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
        "You can try changing the value of the macro 'XFGETS_INITIAL_MAX' before testing.",
        "",
            NULL
    };

    const char * const *p_line;
    FILE *fp;
    char *line;
    int errid = 0;

    for (p_line = lines_without_newline; *p_line; p_line++){
        fprintf(stderr, "  Reading '%s' ...\n", *p_line);

        assert((fp = fopen(TMP_FILE1, "w")));
        assert(fputs(*p_line, fp) != EOF);
        assert(! fclose(fp));

        if (**p_line){
            assert((line = xfgets_for_loop(TMP_FILE1, NULL, NULL, &errid)));
            assert(! strcmp(line, *p_line));
            assert(! errid);
        }

        assert(! (line = xfgets_for_loop(TMP_FILE1, NULL, NULL, &errid)));
        assert(! errid);
    }


    // when specifying the file where multiple lines are stored and interrupting the reading in the middle

    // changeable part for updating test cases
    const char * const lines_with_trailing_newline[] = {
        "                                   .... (:((!\"`'?7OC+--...                      ",
        "                                       ...?7~           _~_~~_.<~<!`.-?9~..     ",
        "                                              ..?!                ``````` ``    ",
        "  `_!__<~.                                            ..^      .`               ",
        "                    `_<.   ,^~?-                               .?`       .(~    ",
        "    .              _.             `_<>.    1.                           .?      ",
        "   (``      .(!              `` <.               <-    <                        ",
        " .?         .>``     .J~.`                 `__                1.   1            ",
        "           J`         .~`     ..!_~ `                  `(                 (-. ._",
        "                     >`  `      `z     .c~   .`    ._               <           ",
        "     . 1.`1                    :` .(.     ._     `.:`   -    . ~..       .`    .",
        "`  .l            ``<..                   >   _>.    .C`     .:`  .<.    .( ``   ",
        "    .-     :`  j_              (_-(.               J.   (>`   .Z_     .C   .v.  ",
        "   q  .        (.      \\` (:              `<!  (             .!    (z.   d:     ",
        " v~  .C_      j  .        <.` .....  (>               .1  /             <     (l",
        "_  /f.     (:   v~      ..  .    ....>.JJWWV1Jn.(l:               _+(_          ",
        "   ~     _1l  O>     :V   J:      .-C> .-<((((! v      (0f.(I& >             ~(2",
        "             .      ~+'`ld      (<  .<      `-  I (OAwOwn  }_ -+WVY=..(Z   :~   ",
        "        _([             (.      (l`lR      y~ .C       ;   O _yZAmdphgX77=~_..  ",
        "-dr   <:~_          :O.            <        ? dD     ( _ (~   .7<_(   O  (AWWY\"!",
        ".(_.`      _Hk    _~~_..(~.    :(~            (.        ?S5     j . 0:     ((v, ",
        "     v>      -`       (Wk  .wxuuXVT4X_    ~(;            ._~       Jtr     U _.I",
        ":    J>  %~~(<   -      (`      _JTWkX\"\"  :  (0Y:    :~[            `1_~     (  ",
        "r    ( :_JI  ~(v~       h~~<n-      >      ,v)IWAQ._v+US_~     _~~[             ",
        " -(~    J  $    z :_Kv:(Jnwg       (<{  I<     .-     >),7wWW9rX<\\        ~:_\\  ",
        "            (!    _0'  w   k ~(HWqHHMHHHHHH@H   &-$j       . >~h\\(\"\"  (?k    d  ",
        "     ~~._              (_   ~( '` :   w :?0Y::(MQHMHHHHMMHHMHU~l  l   }>)   (f.-",
        "(   ?Xkzw      (c~(~              !.   -Z~_~k~   O ~(`````WMM#=,``P:~?THb._   ( ",
        "        -dHHMHHHHWky      (6 $              w`    f``.      t ~(``  .MNd, .(, ``",
        "__~~ `:  ?_    .MHMWH@MHr<<7THHH  (< I  (             zt    (!``~      t ~(_` .J",
        "MHWNB=?b  ``      -!?<m`J ..#=,M| ````(0=(  :, k  %             J ~    (_``_~j  ",
        "  t _({`  J|  \"= .F  `          ``  JMB, .?b   ``` (   U<   SJ`           .  ~. ",
        "   L ` ~d    t  ($`  .h.77!.d!                 .MMTH=J]    ``.l V:~  tk`        ",
        "     R y _     n.``(    ld-?d_`  -?9.J\"                   J|  & .F    .zc&JZl  $",
        "               .+ r`     d  a,,    1   <-                            .h?7!uF    ",
        "` ? H\\  O  (S             v  }`     O _       ( b.   ~_(<_~                     ",
        "  9.7^    `  (R 'I (Z   9           J!  !`     l   tw .  . d_    ````           ",
        "        ._     _>)~     X0'`x  (<  l          ,(! .``     v  0  [ ,    {        ",
        "                          ````     W '`k   ?  ._        . I  ( `     I d `  $  _",
        " 4u                      _              (v?Td `-'|     <_ 1        C.) `( `   . ",
        " _0 `' C    : OU-.                 / ``\"        _-?4. ``?2~.       <  .      . (",
        "!   -`   ~( _I `'` j-  z:    yU+..(?77=i.                 ````j_?+.``z+     |   ",
        " >`      3._   t``_ ~d_(Z `'`  ;. _z_4x7=!...-77<. `?.               .>? d6*~_=.",
        "CC     ({   (_             7. _(Zt(7~~_Id(X_ .?- l._~(v````.(/   42TM#NNmgmO&+jV",
        "\"`   <>?v~_(d     .l    |               _7_d(i,     I;j-..O   (J>     `,L  `.Ye ",
        "   79% w  ,N,, 1j    ~(W     ((   `~                            jI1+(   +z!     ",
        ". ,H, .< ,h..J!      .#Nz.?(    _(v    _C.    `                             (<~`",
        "  >+!     .  ` .\"YSa+JMMSaJ....JY-X3..`.. -~Z JJ__J`,   J                       ",
        "      J~.``  j!`.   .C     .._-z   ?M   @ +Hg>?1z-. .~(!_~999  77!`      ",
            NULL
    };

    size_t size, remain, len = 0;
    char *start_for_file = NULL;

    size = numof(lines_with_trailing_newline);
    assert(size);

#ifdef XFGETS_TEST_COMPLETE
    size -= rand() % size;
    assert(size);
#endif
    size = rand() % size;
    remain = size;


    assert((fp = fopen(TMP_FILE1, "w")));

    for (p_line = lines_with_trailing_newline; *p_line; p_line++){
        assert(fputs(*p_line, fp) != EOF);
        assert(fputc('\n', fp) != EOF);
    }
    assert(! fclose(fp));


    for (p_line = lines_with_trailing_newline; size; p_line++, size--){
        fprintf(stderr, "  Reading '%s' ...\n", *p_line);

        assert((line = xfgets_for_loop(TMP_FILE1, &start_for_file, &len, &errid)));
        assert(! strcmp(line, *p_line));
        assert(len-- && (len == strlen(start_for_file)));
        assert(! errid);
    }

#ifdef XFGETS_TEST_COMPLETE
    if (start_for_file){
        for (size = 0; (size += 91) < len;)
            start_for_file[size - 1] = '\n';

        fprintf(stderr, "\n%s\n", start_for_file);

        for (line = start_for_file; (line = strchr(line, '\n')); *(line++) = ' ');
    }
#endif


    // when accepting the input from standard input while reading other file

    char *start_for_stdin = NULL;

    fputs("\nChecking if it works the same as 'cat - -' ...\n", stderr);

    for (len = 0, size = 2; size; size--)
        while ((line = xfgets_for_loop(NULL, &start_for_stdin, &len, NULL)))
            assert(puts(line) != EOF);

    do {
        assert(check_if_visually_no_problem());

        if (len){
            fputs("Checking if the output matches the input ...\n", stderr);

            assert(start_for_stdin);
            line = start_for_stdin;

            do {
                assert(puts(line) != EOF);

                size = strlen(line) + 1;
                line += size;
                len -= size;
            } while (len);

            free(start_for_stdin);
            start_for_stdin = NULL;
        }
        else {
            assert(! start_for_stdin);
            break;
        }
    } while (true);


    // when performing terminate processing for the interrupted file reading and confirming its correctness

    if (start_for_file){
        assert(remain);

        errid = -1;
        assert(! xfgets_for_loop(TMP_FILE1, &start_for_file, NULL, &errid));
        assert(errid == -1);

        line = start_for_file;
        p_line = lines_with_trailing_newline;

        do {
            len = strlen(*p_line);
            assert(! memcmp(line, *(p_line++), (sizeof(char) * len)));
            line += len;
        } while (--remain);

        free(start_for_file);
    }

    assert(! remain);


    // when specifying a non-existing file

    assert(! unlink(TMP_FILE1));
    assert(! xfgets_for_loop(TMP_FILE1, NULL, &len, &errid));
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
    inf_str start = {0};

    for (i = 0; table[i].path; i++){
        assert(xstrcat_inf_len(&start, table[i].inherit, table[i].path, (strlen(table[i].path) + 1)));
        assert(start.ptr);
        assert(start.max == XSTRCAT_INITIAL_MAX);
        assert(! strcmp(start.ptr, table[i].result));

        print_progress_test_loop('\0', -1, i);
        fprintf(stderr, "%s\n", table[i].result);
    }

    assert(i > 0);


    // changeable part for updating test cases
    const char *repeat = " abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

    size_t size, len = 0;
    int iter = 0;

    size = strlen(repeat);
    assert(size);

    while ((size * (++iter) + 1) <= XSTRCAT_INITIAL_MAX);

    for (i = 0; i < iter; i++){
        assert(start.ptr);
        assert(start.max == XSTRCAT_INITIAL_MAX);
        assert(xstrcat_inf_len(&start, len, repeat, (size + 1)));

        len += size;
        print_progress_test_loop('\0', -1, i);
        fprintf(stderr, "total length:  %4zu\n", len);
    }

    assert(i > 0);
    assert(start.ptr);
    assert(start.max > XSTRCAT_INITIAL_MAX);

    free(start.ptr);
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

        assert(check_if_visually_no_problem());
    }
}




static inf_str walked_start = {0};
static size_t walked_len = 0;


static int walk_test_stub(int pwdfd, const char *name, bool isdir){
    assert((pwdfd >= 0) || (pwdfd == AT_FDCWD));
    assert(name && *name);

    struct stat file_stat;
    size_t size;

    assert(! fstatat(pwdfd, name, &file_stat, AT_SYMLINK_NOFOLLOW));
    assert(isdir == S_ISDIR(file_stat.st_mode));

    size = strlen(name) + 1;
    assert(xstrcat_inf_len(&walked_start, walked_len, name, size));

    walked_len += size;
    return SUCCESS;
}


static void walk_test(void){
    // changeable part for updating test cases
    const char * const startpoint[] = {
        TMP_FILE1,
        "/tmp",
        "/dit/tmp/..",
        "/etc/.//.",
            NULL
    };

    int i, size, fd;
    char cmdline[128];
    void *addr;

    const char *found, *walked, *tmp;
    size_t name_len;

    for (i = 0; startpoint[i]; i++){
        assert(*(startpoint[i]));
        fprintf(stderr, "  Walking '%s' ...\n", startpoint[i]);

        size = snprintf(cmdline, sizeof(cmdline), "find %s -depth -print0 > "TMP_FILE1, startpoint[i]);
        assert((size >= 0) && (size < sizeof(cmdline)));
        assert(system(NULL) && (! system(cmdline)));

        assert(! walked_len);
        assert(walk(startpoint[i], walk_test_stub));

        assert((size = get_file_size(TMP_FILE1)) >= 0);
        assert((fd = open(TMP_FILE1, O_RDONLY)) != -1);
        assert((addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0)) != MAP_FAILED);

        found = (const char *) addr;
        walked = walked_start.ptr;

        while (walked_len){
            if ((! strchr(walked, '/')) && (tmp = strrchr(found, '/')))
                found = tmp + 1;
            name_len = strlen(found) + 1;

            fprintf(stderr, "    memcmp(W, F):  '%s'  '%s'\n", walked, found);
            assert(! memcmp(walked, found, (sizeof(char) * name_len)));

            found += name_len;
            walked += name_len;
            walked_len -= name_len;
        }

        assert(! munmap(addr, size));
        assert(! close(fd));
    }

    assert(i > 0);
    assert(walked_start.ptr);

    free(walked_start.ptr);
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
    // changeable part for updating test cases
    const char * const candidates[] = {
        "",
        " ",
        "#TEST",
        "-",
        "--help",
        "--long",
        "--long-help",
        "// TEST END",
        "2023/04/01",
        "2023/04/10",
        "@=_AT_",
        "AAA",
        "ACK",
        "SAT",
        "SYN",
        "_AT_=@",
        "~"
    };


    const struct {
        const char * const target;
        const int mode;
        const int result;
    }
    // changeable part for updating test cases
    table[] = {
        { "",            0,  0 },
        { "--long",      0,  5 },
        { "2023/04/10",  0,  9 },
        { "SAT",         0, 13 },
        { " ",           1,  1 },
        { "// Test End", 1,  7 },
        { "aAa",         1, 11 },
        { "_at_=@",      1, 15 },
        { "-",           2,  3 },
        { "--h",         2,  4 },
        { "2023/04/0",   2,  8 },
        { "_",           2, 15 },
        { "#T",          3,  2 },
        { "@=_",         3, 10 },
        { "Sy",          3, 14 },
        { "~",           3, 16 },
        { "\t",          0, -2 },
        { "--",          0, -1 },
        { "2022",        0, -2 },
        { "FIN",         0, -2 },
        { "# test",      1, -2 },
        { "--long-help", 1, -2 },
        { "aa",          1, -2 },
        { "_at_",        1, -2 },
        { "=",           2, -2 },
        { "/* TEST */",  2, -2 },
        { "2023/04",     2, -1 },
        { "syn",         2, -2 },
        { "a",           3, -1 },
        { "main()",      3, -2 },
        { ";",           3, -2 },
        { "HealthCheck", 3, -2 },
        {  0,            0,  0 }
    };


    int i, id;
    const char *selected;

    for (i = 0; table[i].target; i++){
        id = receive_expected_string(table[i].target, candidates, numof(candidates), table[i].mode);
        assert(id == table[i].result);

        if (id >= 0){
            selected = candidates[id];
            id = SUCCESS;
        }
        else {
            selected = "?";
            id = FAILURE;
        }

        print_progress_test_loop('S', id, i);
        fprintf(stderr, "%-11s  '%s'\n", table[i].target, selected);
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


    int i, id;
    char *args;

    for (i = 0; table[i].line; i++){
        id = table[i].expected_id;
        args = receive_dockerfile_instr(table[i].line, &id);

        if (table[i].offset >= 0){
            assert(args == (table[i].line + table[i].offset));
            assert(id == table[i].actual_id);
            id = SUCCESS;
        }
        else {
            assert(! args);
            id = FAILURE;
        }

        print_progress_test_loop('S', id, i);
        fprintf(stderr, "%-38s  %2d  %2d\n", table[i].line, table[i].expected_id, table[i].actual_id);
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
    // changeable part for updating test cases
    const int digit = 6;
    assert((digit >= 0) && (digit <= 10));


    // when specifying a valid file

    int i, divisor, fd;
    size_t size;

    for (i = 0, divisor = 1; i < digit; i++, divisor *= 10) {
        size = rand() % divisor;
        assert(size < INT_MAX);

        assert((fd = open(TMP_FILE1, (O_WRONLY | O_CREAT))) != -1);
        assert(! ftruncate(fd, size));
        assert(! close(fd));

        assert(get_file_size(TMP_FILE1) == size);

        fprintf(stderr, "  size:  %*zu\n", digit, size);
    };


    // when specifying a file that is too large

    assert(! truncate(TMP_FILE1, INT_MAX));
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
    size_t len;

    for (i = 0; table[i].target; i++){
        len = get_sanitized_string(buf, table[i].target, table[i].quoted);
        assert(len < 64);
        assert(len == strlen(table[i].result));
        assert(! memcmp(buf, table[i].result, (sizeof(char) * (len + 1))));

        print_progress_test_loop('\0', -1, i);
        fprintf(stderr, "'%s'\n", table[i].result);
    }
}




#endif // NDEBUG