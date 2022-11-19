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

#define EXIT_STATUS_FILE "/dit/tmp/last-exit-status"

#define XFGETS_NESTINGS_MAX 2
#define XFGETS_INITIAL_SIZE 1023


/** Data type for storing the information for one loop for 'xfgets_for_loop' */
typedef struct {
    const char *src_file;    /** source file name or NULL */
    FILE *fp;                /** handler for the source file */
    char **p_start;          /** pointer to the beginning of a series of strings or NULL */
    char *dest;              /** pointer to the beginning of dynamic memory allocated */
    size_t curr_size;        /** the size of dynamic memory currently in use */
    size_t curr_length;      /** the total length of the preserved strings including null characters */
} xfgets_info;


static int call_dit_command(int argc, char **argv, int cmd_id);


/** array of strings in alphabetical order representing each dit command */
const char * const cmd_reprs[CMDS_NUM] = {
    "config",
    "convert",
    "cp",
    "erase",
    "healthcheck",
    "help",
    "ignore",
    "inspect",
    "label",
    "onbuild",
    "optimize",
    "reflect",
    "setcmd"
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
static const char *program_name = NULL;




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
 * @note leave the command name in a global variable for error message output.
 * @note if unit tests were performed in 'test', it will not return to this function.
 */
int main(int argc, char **argv){
    if (argc > 0){
        assert(argv);

        program_name = *argv;
        assert(program_name);

        const char *target = NULL;
        int cmd_id = -3;

        if (--argc){
            target = *(++argv);
            cmd_id = receive_expected_string(target, cmd_reprs, CMDS_NUM, 0);
        }

#ifndef NDEBUG
        srand((unsigned int) time(NULL));

        if (argc)
            test(argc, argv, cmd_id);
#endif

        if (cmd_id >= 0){
            program_name = target;
            assert(program_name);

            return call_dit_command(argc, argv, cmd_id);
        }

        const char *desc = "command";
        if (target)
            xperror_invalid_arg('C', 1, desc, target);
        else
            xperror_missing_args(desc, NULL);
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
        config,
        convert,
        cp,
        erase,
        healthcheck,
        help,
        ignore,
        inspect,  // cmd_id = 7
        label,
        onbuild,
        optimize,
        reflect,
        setcmd
    };

    int i = 1, mode;
    FILE *fp;

    do {
        if (i){
            fp = stdout;
            mode = _IOLBF;
#ifdef NDEBUG
            if (cmd_id == 7)
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
 * @param[in]  code  error code ('O' (for optarg), 'N' (for number) or 'C' (for cmdarg))
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
void xperror_valid_args(const char * const reprs[], size_t size){
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

    char format[] = "%s: No %sarguments allowed when reflecting in both files\n";
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
 * @note all lines of the file whose size is too large to be represented by int type cannot be preserved.
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
            p_info->curr_length = 0;

            allocate_flag = true;
        }
        else {
            if (p_errid)
                *p_errid = errno;
            return NULL;
        }
    }

    char *start;
    size_t length;
    unsigned int tmp;
    bool reset_flag = true;

    start = p_info->dest;
    length = p_info->curr_length;

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
            start += length;

            tmp = p_info->curr_size - length;
            assert((tmp > 0) && (tmp <= INT_MAX));

            if (fgets(start, tmp, p_info->fp)){
                tmp = strlen(start);
                length += tmp;

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
                    length--;
                    start[tmp] = '\0';
                    reset_flag = false;
                }
            }
            else if (length != p_info->curr_length)
                reset_flag = false;
        }

        start = p_info->dest;

        if (reset_flag){
            assert(src_file == p_info->src_file);

            if (src_file)
                fclose(p_info->fp);
            else
                clearerr(p_info->fp);

            if ((! (p_info->p_start && p_info->curr_length)) && start){
                free(start);

                if (p_info->p_start)
                    *(p_info->p_start) = NULL;
            }

            info_idx--;
            start = NULL;
        }
        else if (p_info->p_start){
            start += p_info->curr_length;
            p_info->curr_length = length + 1;
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
 * @attention array of expected strings must be pre-sorted alphabetically.
 */
int receive_expected_string(const char *target, const char * const reprs[], size_t size, unsigned int mode){
    assert(reprs);
    assert((size - 1) <= INT_MAX);
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
                        tmp = mid;
                        while ((--tmp >= min) && (c == ((unsigned char) *(expecteds[tmp]++))));
                        min = ++tmp;

                        tmp = mid;
                        while ((++tmp <= max) && (c == ((unsigned char) *(expecteds[tmp]++))));
                        max = --tmp;

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

        if (! tmp)
            return (forward_match || (! *(expecteds[min]))) ? min : -2;
    }
    return -1;
}


/**
 * @brief analyze which instruction in Dockerfile the specified line corresponds to.
 *
 * @param[in]  line  target line with only one trailing newline character
 * @param[out] p_id  variable to store index number of the instruction to be compared
 * @return char*  substring that is the argument for the instruction in the target line or NULL
 *
 * @note if the content of 'p_id' is an index number, compares only with corresponding instruction.
 * @note when there is a corresponding instruction, its index number is stored in 'p_id'.
 * @attention the instruction must not contain unnecessary leading white spaces.
 */
char *receive_dockerfile_instruction(char *line, int *p_id){
    assert(line);
    assert(p_id);

    char *args;
    size_t instr_len = 0;

    args = line;
    while (! isspace((unsigned char) *(args++)))
        instr_len++;

    char instr[instr_len + 1];
    memcpy(instr, line, (sizeof(char) * instr_len));
    instr[instr_len] = '\0';

    if ((*p_id >= 0) && (*p_id < DOCKER_INSTRS_NUM)){
        if (xstrcmp_upper_case(instr, docker_instr_reprs[*p_id]))
            args = NULL;
    }
    else if ((*p_id = receive_expected_string(instr, docker_instr_reprs, DOCKER_INSTRS_NUM, 1)) < 0)
        args = NULL;

    if (args)
        while (isspace((unsigned char) *args))
            args++;

    return args;
}




/******************************************************************************
    * Get Methods
******************************************************************************/


/**
 * @brief check if the size of the specified file is not too large to be represented by int type.
 *
 * @param[in]  file_name  target file name
 * @return int  the resulting file size, -1 (unexpected error) or -2 (too large)
 *
 * @note if the file is too large, sets an error number indicating that.
 */
int get_file_size(const char *file_name){
    assert(file_name);

    struct stat file_stat;
    int i = -1;

    if (! stat(file_name, &file_stat)){
        if ((file_stat.st_size >= 0) && (file_stat.st_size <= INT_MAX))
            i = (int) file_stat.st_size;
        else {
            i = -2;
            errno = EFBIG;
        }
    }

    return i;
}


/**
 * @brief check the exit status of last executed command line.
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




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/


static void xfgets_for_loop_test(void);
static void xstrcmp_upper_case_test(void);

static void receive_positive_integer_test(void);
static void receive_expected_string_test(void);
static void receive_dockerfile_instruction_test(void);

static void get_file_size_test(void);
static void get_last_exit_status_test(void);




void dit_test(void){
    do_test(xfgets_for_loop_test);
    do_test(xstrcmp_upper_case_test);

    do_test(receive_positive_integer_test);
    do_test(receive_expected_string_test);
    do_test(receive_dockerfile_instruction_test);

    do_test(get_file_size_test);
    do_test(get_last_exit_status_test);
}




static void xfgets_for_loop_test(void){
    FILE *fp;
    int errid = 0;

    // when specifying an empty file

    assert((fp = fopen(TMP_FILE1, "w")));
    assert(! fclose(fp));

    assert(! xfgets_for_loop(TMP_FILE1, NULL, &errid));
    assert(! errid);


    // when specifying the file whose contents do not end with a newline

    const char *line = "tonari no kyaku ha yoku kaki kuu kyaku da";

    assert((fp = fopen(TMP_FILE1, "w")));
    assert(fputs(line, fp) != EOF);
    assert(! fclose(fp));

    assert((line = xfgets_for_loop(TMP_FILE1, NULL, &errid)));
    assert(! strcmp(line, line));
    assert(! errid);

    assert(! xfgets_for_loop(TMP_FILE1, NULL, &errid));
    assert(! errid);


    // when accepting input from standard input while reading 'r' lines of a non-empty file

    const char * const lines[6] = {
        "abc",
        "def ghij",
        "kl mn",
        "op qr",
        "stu v w xyz",
        NULL
    };

    const char * const *p_line;
    size_t n;

    assert((fp = fopen(TMP_FILE1, "w")));
    for (p_line = lines; *p_line; p_line++)
        assert(fprintf(fp, "%s\n", *p_line) >= 0);
    assert(! fclose(fp));

    n = rand();
    n %= 6;

    for (p_line = lines;; p_line++){
        if (n--){
            assert(! strcmp(xfgets_for_loop(TMP_FILE1, NULL, &errid), *p_line));
            assert(! errid);
            fprintf(stderr, "  %-11s  (%d lines left)\n", *p_line, ((int) n));
        }
        else {
            fputs("Checking if it works the same as 'cat -' ...\n", stderr);

            while ((line = xfgets_for_loop(NULL, NULL, NULL)))
                assert(fprintf(stdout, "%s\n", line) >= 0);

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

            errid = -1;
            assert(! xfgets_for_loop(TMP_FILE1, NULL, &errid));
            assert(errid == -1);
            break;
        }
    }


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
        {     0,                                    0,                                      -1            }
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




static void receive_dockerfile_instruction_test(void){
    const struct {
        char * const line;
        const int expected_id;
        const int actual_id;
        const int offset;
    }
    // changeable part for updating test cases
    table[] = {
        { "ADD abc.tar.gz ./",                      ID_ADD,         ID_ADD,          4 },
        { "USER root",                              ID_USER,        ID_USER,         5 },
        { "HealthCheck  Cmd sh /bin/check-running", ID_HEALTHCHECK, ID_HEALTHCHECK, 13 },
        { "EXPOSE 80/tcp 80/udp",                     -1,           ID_EXPOSE,       7 },
        { "RUN make && make clean",                   -1,           ID_RUN,          4 },
        { "OnBuild  WorkDir /",                       -1,           ID_ONBUILD,      9 },
        { "COPY ./etc/dit_install.sh /dit/etc/",    ID_ADD,         ID_COPY,        -1 },
        { "MainTainer inada",                       ID_LABEL,       ID_MAINTAINER,  -1 },
        { "form alpine:latest",                     ID_FROM,          -1,           -1 },
        { "  Volume [ \"/myapp\" ]",                  -1,             -1,           -1 },
        { "ENT dit inspect",                          -1,             -1,           -1 },
        { "setcmd  [ \"/bin/bash\", \"--login\" ]",   -1,             -1,           -1 },
        {  0,                                          0,              0,            0 }
    };

    int i, id, remain;
    char *line, *args;


    for (i = 0; (line = table[i].line); i++){
        id = table[i].expected_id;
        args = receive_dockerfile_instruction(line, &id);

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
    FILE *fp;
    size_t size;

    // when specifying an empty file

    assert((fp = fopen(TMP_FILE1, "wb")));
    assert(! fclose(fp));

    assert(! get_file_size(TMP_FILE1));


    // when specifying a non-empty file

    assert((fp = fopen(TMP_FILE1, "wb")));

    size = rand();
    size = size % 32 + 1;
    assert(fwrite("1234567 1234567 1234567 1234567", sizeof(char), size, fp) == size);
    size *= sizeof(char);

    assert(! fclose(fp));

    assert(get_file_size(TMP_FILE1) == size);


    // when specifying a file that is too large

    if (system(NULL) && (! system("dd if=/dev/zero of="TMP_FILE1" bs=1M count=2K"))){
        assert(get_file_size(TMP_FILE1) == -2);
        assert(errno == EFBIG);
    }


    // when specifying a non-existing file

    assert(! remove(TMP_FILE1));
    assert(get_file_size(TMP_FILE1) == -1);
    assert(errno == ENOENT);
}




static void get_last_exit_status_test(void){
    int last_exit_status;
    FILE *fp;
    unsigned int i;

    // if the exit status of last executed command line is valid

    last_exit_status = get_last_exit_status();
    assert((last_exit_status >= 0) && (last_exit_status < 256));


    assert((fp = fopen(EXIT_STATUS_FILE, "w")));

    i = rand();
    i %= 256;
    assert(fprintf(fp, "%u\n", i) >= 0);
    assert(fputs("to be ignored\n", fp) != EOF);

    assert(! fclose(fp));

    assert(((unsigned int) get_last_exit_status()) < 256);


    // if the exit status of last executed command line is invalid

    assert((fp = fopen(EXIT_STATUS_FILE, "w")));
    assert(fprintf(fp, "%u\n", (i + 256)) >= 0);
    assert(! fclose(fp));
    assert(get_last_exit_status() == -1);


    assert((fp = fopen(EXIT_STATUS_FILE, "w")));
    assert(fputs("exit status\n", fp) != EOF);
    assert(! fclose(fp));
    assert(get_last_exit_status() == -1);


    // restore the contents of the file

    assert((fp = fopen(EXIT_STATUS_FILE, "w")));
    assert(fprintf(fp, "%d\n", last_exit_status) >= 0);
    assert(! fclose(fp));
}


#endif // NDEBUG