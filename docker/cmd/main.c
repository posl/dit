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
#define XFGETS_INITIAL_SIZE 1023


/** Data type for storing the information for one loop for 'xfgets_for_loop' */
typedef struct {
    const char *src_file;    /** source file name or NULL */
    FILE *fp;                /** handler for the source file */
    char **p_start;          /** pointer to the beginning of a series of strings or NULL */
    char *dest;              /** pointer to the beginning of dynamic memory allocated */
    size_t curr_size;        /** the size of dynamic memory currently in use */
    size_t curr_len;         /** the total length of the preserved strings including null characters */
} xfgets_info;


static int call_dit_command(int argc, char **argv, int cmd_id);


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
    if ((argc > 0) && argv && *argv){
        *argv = get_suffix(*argv, '/', true);

        // compare with "dit"
        if (strcmp(*argv, program_name) || (--argc && *(++argv))){
            int cmd_id;
            cmd_id = receive_expected_string(*argv, cmd_reprs, CMDS_NUM, 0);

#ifndef NDEBUG
            srand((unsigned int) time(NULL));
            test(argc, argv, cmd_id);
#endif
            if (cmd_id >= 0){
                program_name = *argv;
                return call_dit_command(argc, argv, cmd_id);
            }

            xperror_invalid_arg('C', 1, "command", *argv);
        }
        else
            xperror_missing_args("command", NULL);
    }

    xperror_suggestion(false);
    return FAILURE;
}


/**
 * @brief set buffering for standard output, and run the specified dit command.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @param[in]  cmd_id  index number corresponding to one of the dit commands
 * @return int  command's exit status
 *
 * @note set full buffering if a large amount of output is expected for the specified command.
 */
static int call_dit_command(int argc, char **argv, int cmd_id){
    assert(argc > 0);
    assert(argv);
    assert((cmd_id >= 0) && (cmd_id < CMDS_NUM));

    int (* const cmd_funcs[CMDS_NUM])(int, char **) = {
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

    int i = 1, mode;
    FILE *fp;

    do {
        if (i){
            fp = stdout;
            mode = _IOLBF;
#ifdef NDEBUG
            if (cmd_id == DIT_INSPECT)
                mode = _IOFBF;
#endif
        }
        else {
            fp = stderr;
            mode = _IONBF;
        }

        setvbuf(fp, NULL, mode, 0);
    } while (i--);

    assert(i == -1);
    return cmd_funcs[cmd_id](argc, argv);
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
void xperror_invalid_arg(int code_c, int state, const char * restrict desc, const char * restrict arg){
    assert(desc);

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
            addition = arg;
    }

    adjective = state ? ((state == -1) ? "ambiguous" : "invalid") : "unrecognized";

    fprintf(stderr, format, program_name, adjective, addition, desc, arg);
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
 * @param[in]  before_arg  the immediately preceding argument, if any
 *
 * @note if 'desc' is NULL, prints an error message about specifying the target file.
 */
void xperror_missing_args(const char * restrict desc, const char * restrict before_arg){
    assert(desc || (! before_arg));

    char format[] = "%s: missing %s operand after '%s'\n";
    int offset = 0;

    if (! desc){
        desc = "'-d', '-h' or '--target' option";
        offset = 14;
    }
    else if (! before_arg)
        offset = 22;

    if (offset){
        format[offset++] = '\n';
        format[offset] = '\0';
    }

    fprintf(stderr, format, program_name, desc, before_arg);
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
void xperror_message(const char * restrict msg, const char * restrict addition){
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
    const char *str1, *str2;

    if (cmd_flag){
        str1 = program_name;
        str2 = " --";
    }
    else
        str1 = (str2 = "");

    fprintf(stderr, "Try 'dit %s%shelp' for more information.\n", str1, str2);
}




/**
 * @brief print the error message about the error encountered during file handling.
 *
 * @param[in]  file_name  file name
 * @param[in]  errid  error number of an error encountered during file handling
 * @return int  -1 (error exit)
 *
 * @note can be passed as 'errfunc' in glibc 'glob' function.
 */
int xperror_file_handling(const char *file_name, int errid){
    assert(file_name);
    assert(errid == errno);

    xperror_message(strerror(errid), file_name);
    return ERROR_EXIT;
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
void xperror_file_contents(const char * restrict file_name, int lineno, const char * restrict msg){
    assert(lineno > 0);
    assert(msg);

    if (! file_name)
        file_name = "stdin";

    fprintf(stderr, "%s: %s: line %d: %s\n", program_name, file_name, lineno, msg);
}




/******************************************************************************
    * Extensions of Library Functions
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
 *
 * @note keep NULLs clustered at the end of the array.
 */
int qstrcmp(const void *a, const void *b){
    assert(a);
    assert(b);

    const char *str1, *str2;
    str1 = *((const char **) a);
    str2 = *((const char **) b);

    return str1 ? (str2 ? strcmp(str1, str2) : -1) : (str2 ? 1 : 0);
}





int xexecv(const char *cmd_path, char * const argv[]){
    assert(cmd_path);
    assert(argv);

    
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
 * @attention the array must be pre-sorted alphabetically and NULLs must be clustered at the end of it.
 */
int receive_expected_string(const char *target, const char * const *reprs, size_t size, unsigned int mode){
    assert(reprs);
    assert((size > 0) && ((size - 1) <= INT_MAX));
    assert(mode < 4);

    for (const char * const *p_repr = (reprs + size); (! *(--p_repr)) && --size;);
    assert(check_if_alphabetical_order(reprs, size));

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
 * @note if the content of 'p_id' is an index number, compares only with the corresponding instruction.
 * @note when there is a corresponding instruction, its index number is stored in 'p_id'.
 * @note if any instructions can be accepted, blank lines are also accepted as valid lines.
 */
char *receive_dockerfile_instr(char *line, int *p_id){
    assert(line);
    assert(p_id);

    char *tmp = NULL, instr[16];
    size_t instr_len = 0;

    do {
        while (isspace((unsigned char) *line))
            line++;

        if (! tmp){
            if (*line){
                tmp = line;
                assert(tmp);

                do
                    instr_len++;
                while (*(++tmp) && (! isspace((unsigned char) *tmp)));

                if (! *tmp)
                    break;

                if (instr_len == 11)
                    *p_id = ID_HEALTHCHECK;
                else if ((instr_len < 3) || ((instr_len > 7) && (instr_len != 10)))
                    break;

                memcpy(instr, line, (sizeof(char) * instr_len));
                instr[instr_len] = '\0';

                if ((*p_id >= 0) && (*p_id < DOCKER_INSTRS_NUM)){
                    if (xstrcmp_upper_case(instr, docker_instr_reprs[*p_id]))
                        break;
                }
                else {
                    *p_id = receive_expected_string(instr, docker_instr_reprs, DOCKER_INSTRS_NUM, 1);

                    if (*p_id < 0)
                        break;
                }

                line = tmp + 1;
                continue;
            }
            else if ((*p_id >= 0) && (*p_id < DOCKER_INSTRS_NUM))
                break;
        }
        else if (! *line)
            break;

        return line;
    } while (true);

    return NULL;
}




/******************************************************************************
    * Get Methods
******************************************************************************/


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
    const char *line;
    int errid = 0, i = -1;

    while ((line = xfgets_for_loop(EXIT_STATUS_FILE, NULL, &errid))){
        errid = -1;

        if ((i = receive_positive_integer(line, NULL)) >= 256)
            i = -1;
    }

    return i;
}


/**
 * @brief get the suffix of target string.
 *
 * @param[in]  target  target string
 * @param[in]  delimiter  delimiter character
 * @param[in]  retain  whether to retain target string if it has no delimiter
 * @return char*  the resulting suffix
 *
 * @note target string is of type 'char *', but its contents are not changed inside this function.
 */
char *get_suffix(char *target, int delimiter, bool retain){
    assert(target);
    assert(delimiter == ((char) delimiter));

    char *suffix, *tmp;

    suffix = (tmp = target);

    while (*tmp)
        if (*(tmp++) == delimiter)
            suffix = tmp;

    if ((! retain) && (suffix == target))
        suffix = tmp;

    return suffix;
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

static void receive_positive_integer_test(void);
static void receive_expected_string_test(void);
static void receive_dockerfile_instr_test(void);

static void get_file_size_test(void);
static void get_last_exit_status_test(void);
static void get_suffix_test(void);




void dit_test(void){
    do_test(xfgets_for_loop_test);
    do_test(xstrcmp_upper_case_test);

    do_test(receive_positive_integer_test);
    do_test(receive_expected_string_test);
    do_test(receive_dockerfile_instr_test);

    do_test(get_file_size_test);
    do_test(get_last_exit_status_test);
    do_test(get_suffix_test);
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
        assert(fprintf(fp, "%s\n", *p_line) >= 0);
    }
    assert(! fclose(fp));

#ifdef XFGETS_TEST_COMPLETE
    assert(count > 0);
    count -= rand() % count;
#endif
    assert(count > 0);
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
        assert(fprintf(stdout, "%s\n", line) >= 0);

    do {
        fputs("If everything is fine, press enter to proceed: ", stderr);

        switch (fgetc(stdin)){
            case '\n':
                fputs("Done!\n", stderr);
                break;
            case EOF:
                clearerr(stdin);
                assert(false);
            default:
                fscanf(stdin, "%*[^\n]%*c");
                assert(false);
        }

        if (start_for_stdin){
            assert(remain > 0);

            fputs("Checking if the output matches the input ...\n", stderr);

            line = start_for_stdin;

            do {
                assert(fprintf(stdout, "%s\n", line) >= 0);
                while (*(line++));
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
        assert(count > 0);

        line = start_for_file;
        p_line = lines_with_trailing_newline;

        do {
            assert(! strcmp(line, *(p_line++)));
            while (*(line++));
        } while (--count);

        free(start_for_file);
    }

    assert(! count);


    // when specifying a non-existing file

    assert(! remove(TMP_FILE1));
    assert(! xfgets_for_loop(TMP_FILE1, NULL, &errid));
    assert(errid == ENOENT);
}




static void xstrcmp_upper_case_test(void){
    // changeable part for updating test cases
    comptest_table table[] = {
        { { .name = "none"                   }, { .name = "NONE"                      }, COMPTEST_EQUAL   },
        { { .name = "hoGe-PIyO"              }, { .name = "HOGE-PIYO"                 }, COMPTEST_EQUAL   },
        { { .name = "f.lwhaeopyr;pfqwnel.FG" }, { .name = "F.LWHAEOPYR;PFQWNEL.FG"    }, COMPTEST_EQUAL   },
        { { .name = "Quit"                   }, { .name = "YES"                       }, COMPTEST_LESSER  },
        { { .name = "niPPon"                 }, { .name = "NIPPORI"                   }, COMPTEST_LESSER  },
        { { .name = "fasldhfoNLASOL>NAZHO"   }, { .name = "FASLFN/L?EF=ONLAS|OLX{ZHO" }, COMPTEST_LESSER  },
        { { .name = "SIGKILL"                }, { .name = "SIGINT"                    }, COMPTEST_GREATER },
        { { .name = "Super Sento"            }, { .name = "SUPER MARKET"              }, COMPTEST_GREATER },
        { { .name = "On your marks, Set, Go" }, { .name = "ON YOUR MARK!"             }, COMPTEST_GREATER },
        { { .name =  0                       }, { .name =  0                          },    -1            }
    };

    const char *target, *expected;

    for (int i = 0; table[i].type >= 0; i++){
        target = table[i].elem1.name;
        expected = table[i].elem2.name;
        assert(comptest_result_check(table[i].type, xstrcmp_upper_case(target, expected)));

        print_progress_test_loop('C', table[i].type, i);
        fprintf(stderr, "%-22s  %s\n", target, expected);
    }
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
            instr_repr = " \?";
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
        { "COPY ./etc/dit_install.sh /dit/etc/",    ID_ADD,         ID_COPY,        -1 },
        { "MainTainer inada",                       ID_LABEL,       ID_MAINTAINER,  -1 },
        { "form alpine:latest",                     ID_FROM,          -1,           -1 },
        { "  Volume",                                 -1,             -1,           -1 },
        { "En dit inspect",                           -1,             -1,           -1 },
        { "setcmd  [ \"/bin/bash\", \"--login\" ]",   -1,             -1,           -1 },
        { "    ",                                   ID_STOPSIGNAL,    -1,           -1 },
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
            fprintf(stderr, "  %-11s", ((id >= 0) ? docker_instr_reprs[id] : " \?"));
        }

        assert(remain == -1);
        fputc('\n', stderr);
    }
}




static void get_file_size_test(void){
    assert(sizeof(char) == 1);

    // when specifying a valid file

    // changeable part for updating test cases
    const unsigned int digit = 6;
    assert(digit <= 10);

    int i, divisor = 1;
    size_t size;
    FILE *fp;
    char *tmp;

    for (i = -1; ++i < digit; divisor *= 10) {
        size = rand() % divisor;

        assert((fp = fopen(TMP_FILE1, "wb")));
        if (size){
            assert((tmp = calloc(size, sizeof(char))));
            assert(fwrite(tmp, sizeof(char), size, fp) == size);
            free(tmp);
        }
        assert(! fclose(fp));

        assert(get_file_size(TMP_FILE1) == size);

        fprintf(stderr, "  size:  %*d\n", digit, ((int) size));
    };


    // when specifying a file that is too large

    if (system(NULL) && (! system("dd if=/dev/zero of="TMP_FILE1" bs=1M count=2K > /dev/null"))){
        assert(get_file_size(TMP_FILE1) == -2);
        assert(errno == EFBIG);
    }


    // when specifying a non-existing file

    assert(! remove(TMP_FILE1));
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
        { "%d\nto be ignored\n",    128,  128 },
        { "%d",                       1,    1 },
        { "%d\n",                 18610,   -1 },
        { "-1\ninvalid status\n",    -1,   -1 },
        { "",                        -1,   -1 },
        { "%d\n",                  curr, curr },
        {  0,                         0,    0 },
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




static void get_suffix_test(void){
    const struct {
        const char *target;
        const int delimiter;
        const bool retain;
        const char *suffix;
    }
    // changeable part for updating test cases
    table[] = {
        { "main.c",             '/',  true, "main.c"    },
        { "/etc/profile.d",     '/',  true, "profile.d" },
        { "/usr/local/bin/dit", '/',  true, "dit"       },
        { "~/.bashrc",          '/',  true, ".bashrc"   },
        { "//var//",            '/',  true, ""          },
        { "../test//main.sh",   '/',  true, "main.sh"   },
        { "main.c",             '.', false, "c"         },
        { "README.md",          '.', false, "md"        },
        { "..",                 '.', false, ""          },
        { "utils.py.test",      '.', false, "test"      },
        { "ISSUE_TEMPLATE",     '.', false, ""          },
        { ".gitignore",         '.', false, "gitignore" },
        {  0,                    0,     0,   0          }
    };

    char target[32];

    for (int i = 0; table[i].target; i++){
        memcpy(target, table[i].target, (sizeof(char) * (strlen(table[i].target) + 1)));
        assert(! strcmp(get_suffix(target, table[i].delimiter, table[i].retain), table[i].suffix));

        print_progress_test_loop('\0', -1, i);
        fprintf(stderr, "%-18s  '%s'\n", table[i].target, table[i].suffix);
    }
}




#endif // NDEBUG