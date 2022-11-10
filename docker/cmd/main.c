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


/** Data type for storing the information for one loop for 'xfgets_for_loop' */
typedef struct {
    const char *src_file;    /** source file name or NULL */
    FILE *fp;                /** handler for the source file */
    char *dest;              /** pointer to the beginning of the dynamic memory allocated */
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
    assert((code_c == 'O') || (code_c == 'N') || (code_c == 'C'));
    assert(desc);
    assert(arg);

    const char *format, *addition = "", *adjective;

    switch (code_c){
        case 'O':
            format = "%s: %s argument '%s' for '--%s'\n";
            addition = arg;
            break;
        case 'N':
            addition = " number of";
        default:
            format = "%s: %s%s %s: '%s'\n";
    }

    adjective = state ? ((state == -1) ? "ambiguous" : "invalid") : "unrecognized";

    assert(program_name);
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

    for (int i = 0; i < size; i++)
        fprintf(stderr, "  - '%s'\n", reprs[i]);
}




/**
 * @brief print an error message to stderr that some arguments are missing.
 *
 * @param[in]  desc  the description for the arguments or NULL
 * @param[in]  before_arg  the immediately preceding argument, if any
 *
 * @note if 'desc' is NULL, print an error message about specifying the target file.
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
        assert(offset < (sizeof(format) - 1));
        format[offset++] = '\n';
        format[offset] = '\0';
    }

    assert(program_name);
    fprintf(stderr, format, program_name, desc, before_arg);
}


/**
 * @brief print an error message to stderr that the number of arguments is too many.
 *
 * @param[in]  limit  the maximum number of arguments or negative integer
 *
 * @note if 'limit' is negative integer, print an error message about specifying both files as targets.
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
            assert(sizeof(format) > 27);
            format[26] = '\n';
            format[27] = '\0';
    }

    assert(program_name);
    fprintf(stderr, format, program_name, adjective);
}




/**
 * @brief print the specified error message to stderr.
 *
 * @param[in]  msg  the error message or NULL
 * @param[in]  addition  additional information, if any
 *
 * @note if 'msg' is NULL, print an error message about manipulating an internal file.
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

    assert(program_name);
    fprintf(stderr, ("%s: %s: %s\n" + offset), program_name, addition, msg);
}


/**
 * @brief print a suggenstion message to stderr.
 *
 * @param[in]  cmd_flag  whether to suggest displaying the manual of each dit command
 */
void xperror_suggestion(bool cmd_flag){
    const char *tmp1, *tmp2;

    if (cmd_flag){
        assert(program_name);
        tmp1 = program_name;
        tmp2 = " --";
    }
    else
        tmp1 = (tmp2 = "");

    fprintf(stderr, "Try 'dit %s%shelp' for more information.\n", tmp1, tmp2);
}




/******************************************************************************
    * Extensions of Standard Library Functions
******************************************************************************/


/**
 * @brief read the contents of the specified file exactly one line at a time.
 *
 * @param[in]  src_file  source file name or NULL
 * @param[in]  preserve_flag  whether to preserve or overwrite the previously read lines
 * @param[out] p_errid  variable to store the error ID when an error occurs
 * @return char*  the resulting line or NULL
 *
 * @note read to the end of the file by using it as a conditional expression in a loop statement.
 * @note this function can be nested by passing different 'src_file' to a depth of 'XFGETS_NESTINGS_MAX'.
 * @note all lines of the file whose size is too large to be represented by int type cannot be preserved.
 * @note this function updates the contents of 'p_errid' only when an error occurs while opening the file.
 * @note if the contents of 'p_errid' was set to something other than 0, perform finish processing.
 * @note the trailing newline character of the line that is the return value is stripped.
 *
 * @attention the string returned at the first call should be released at the end if you preserve read lines.
 * @attention this function must be called until NULL is returned, since it uses dynamic memory internally.
 */
char *xfgets_for_loop(const char *src_file, bool preserve_flag, int *p_errid){
    static int info_idx = -1;
    static xfgets_info info_list[XFGETS_NESTINGS_MAX];

    xfgets_info *p_info;
    bool allocate_flag = false;

    p_info = info_list + info_idx;

    if ((info_idx < 0) || (p_info->src_file != src_file)){
        FILE *fp = stdin;
        errno = 0;

        if ((info_idx < (XFGETS_NESTINGS_MAX - 1)) && ((! src_file) || (fp = fopen(src_file, "r")))){
            info_idx++;
            p_info++;

            assert((info_idx >= 0) && (info_idx < XFGETS_NESTINGS_MAX));
            assert(p_info == (info_list + info_idx));

            p_info->src_file = src_file;
            p_info->fp = fp;
            p_info->dest = NULL;
            p_info->curr_size = 1023;
            p_info->curr_length = 0;

            allocate_flag = true;
        }
        else {
            if (p_errid)
                *p_errid = errno;
            return NULL;
        }
    }

    size_t length;
    char *start;
    unsigned int tmp;
    bool reset_flag = true;

    length = p_info->curr_length;

    do {
        if (allocate_flag){
            if ((start = (char *) realloc(p_info->dest, (sizeof(char) * p_info->curr_size)))){
                p_info->dest = start;
                allocate_flag = false;
                continue;
            }
        }
        else if ((! (p_errid && *p_errid)) && ((tmp = p_info->curr_size - length) > 1)){
            start = p_info->dest + length;

            if (fgets(start, tmp, p_info->fp)){
                length += strlen(start);

                if (p_info->dest[length - 1] != '\n'){
                    tmp = p_info->curr_size + 1;
                    if ((tmp <<= 1)){
                        p_info->curr_size = tmp - 1;
                        allocate_flag = true;
                        continue;
                    }
                }
                else {
                    p_info->dest[--length] = '\0';
                    reset_flag = false;
                }
            }
            else if (length != p_info->curr_length)
                reset_flag = false;
        }

        if (reset_flag){
            if (src_file)
                fclose(p_info->fp);
            else
                clearerr(p_info->fp);

            if ((! preserve_flag) && p_info->dest)
                free(p_info->dest);

            start = NULL;
            info_idx--;
        }
        else if (preserve_flag){
            start = p_info->dest + p_info->curr_length;
            p_info->curr_length = length + 1;
        }
        else
            start = p_info->dest;

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
    assert(mode < 4);

    if (target && (size > 0)){
        const char *expecteds[size];
        memcpy(expecteds, reprs, (sizeof(const char *) * size));

        bool upper_case, forward_match, break_flag = false;
        int min, max, tmp, c, mid;

        upper_case = mode & 0b01;
        forward_match = mode & 0b10;

        min = 0;
        max = size - 1;
        tmp = -max;

        while ((c = (unsigned char) *(target++))){
            if (upper_case)
                c = toupper(c);

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

            if (break_flag)
                break_flag = false;
            else if (c != ((unsigned char) *(expecteds[min]++)))
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
 * @note if the content of 'p_id' is an index number, compare only with corresponding instruction.
 * @note when there is a corresponding instruction, its index number is stored in 'p_id'.
 * @attention the instruction must not contain unnecessary leading white spaces.
 */
char *receive_dockerfile_instruction(char *line, int *p_id){
    assert(line);
    assert(p_id);

    char *tmp;
    size_t instr_len = 0;

    tmp = line;
    while (! isspace((unsigned char) *(tmp++)))
        instr_len++;

    char instr[instr_len + 1];
    memcpy(instr, line, (sizeof(char) * instr_len));
    instr[instr_len] = '\0';

    if ((*p_id >= 0) && (*p_id < DOCKER_INSTRS_NUM)){
        if (xstrcmp_upper_case(instr, docker_instr_reprs[*p_id]))
            tmp = NULL;
    }
    else if ((*p_id = receive_expected_string(instr, docker_instr_reprs, DOCKER_INSTRS_NUM, 1)) < 0)
        tmp = NULL;

    if (tmp)
        while (isspace((unsigned char) *tmp))
            tmp++;

    return tmp;
}




/******************************************************************************
    * Check Functions
******************************************************************************/


/**
 * @brief check if the size of the specified file is not too large to be represented by int type.
 *
 * @param[in]  file_name  target file name
 * @return int  the resulting file size, -1 (unexpected error) or -2 (too large)
 *
 * @note if the file is too large, set an error number indicating that.
 */
int check_file_size(const char *file_name){
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
int check_last_exit_status(void){
    const char *line;
    int errid = 0, i = -1;

    while ((line = xfgets_for_loop(EXIT_STATUS_FILE, false, &errid))){
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

static void check_file_size_test(void);
static void check_last_exit_status_test(void);




void dit_test(void){
    do_test(xfgets_for_loop_test);
    do_test(xstrcmp_upper_case_test);

    do_test(receive_positive_integer_test);
    do_test(receive_expected_string_test);
    do_test(receive_dockerfile_instruction_test);

    do_test(check_file_size_test);
    do_test(check_last_exit_status_test);
}




static void xfgets_for_loop_test(void){
    FILE *fp;
    int errid = 0;


    // when specifying an empty file

    assert((fp = fopen(TMP_FILE, "w")));
    assert(! fclose(fp));

    assert(! xfgets_for_loop(TMP_FILE, false, &errid));
    assert(! errid);


    // when specifying the file whose contents do not end with a newline

    const char *line = "tonari no kyaku ha yoku kaki kuu kyaku da";

    assert((fp = fopen(TMP_FILE, "w")));
    assert(fputs(line, fp) != EOF);
    assert(! fclose(fp));

    assert(! strcmp(xfgets_for_loop(TMP_FILE, false, &errid), line));
    assert(! errid);

    assert(! xfgets_for_loop(TMP_FILE, false, &errid));
    assert(! errid);


    // when accepting input from standard input while reading 'r' lines of a non-empty file

    const char * const *tmp;
    size_t n;

    const char * lines[6] = {
        "abc",
        "def ghij",
        "kl mn",
        "op qr",
        "stu v w xyz",
        NULL
    };

    assert((fp = fopen(TMP_FILE, "w")));
    for (tmp = lines; *tmp; tmp++)
        assert(fprintf(fp, "%s\n", *tmp) >= 0);
    assert(! fclose(fp));

    n = rand();
    n %= 6;

    for (tmp = lines;; tmp++){
        if (n--){
            assert(! strcmp(xfgets_for_loop(TMP_FILE, false, &errid), *tmp));
            assert(! errid);
        }
        else {
            fputs("Checking if it works the same as 'cat -' ...\n", stderr);

            while ((line = xfgets_for_loop(NULL, false, NULL)))
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
            assert(! xfgets_for_loop(TMP_FILE, false, &errid));
            assert(errid == -1);
            break;
        }
    }


    // when specifying a non-existing file

    assert(! remove(TMP_FILE));
    assert(! xfgets_for_loop(TMP_FILE, false, &errid));
    assert(errid == ENOENT);
}




static void xstrcmp_upper_case_test(void){
    // equal
    assert(! xstrcmp_upper_case("none", "NONE"));
    assert(! xstrcmp_upper_case("hoGe-PIyO", "HOGE-PIYO"));
    assert(! xstrcmp_upper_case("f.lwhaeopyr;pfqwnel.FGQHP934", "F.LWHAEOPYR;PFQWNEL.FGQHP934"));

    // lower than
    assert(xstrcmp_upper_case("Quit", "YES") < 0);
    assert(xstrcmp_upper_case("niPPon", "NIPPORI") < 0);
    assert(xstrcmp_upper_case("fasldhfoNLASOL>NAZHO", "FASLFN/L?EF=ONLAS|OLX{ZHO") < 0);
    assert(xstrcmp_upper_case("SaKurA", "SAKURA, HIRAHIRA") < 0);

    // greater than
    assert(xstrcmp_upper_case("SIGKILL", "SIGINT") > 0);
    assert(xstrcmp_upper_case("Super Sento", "SUPER MARKET") > 0);
    assert(xstrcmp_upper_case(";LZQERXT;,EM;W ; ERGJ'VXWP,9MG", ";LZQERXT;,EM;W 4CQ; ERGJ'VXWP,9MG") > 0);
    assert(xstrcmp_upper_case("On your marks, Set, Go!", "ON YOUR MARK") > 0);
}




static void receive_positive_integer_test(void){
    int left;

    // successful cases

    assert(! receive_positive_integer("0", NULL));
    assert(receive_positive_integer("23", NULL) == 23);
    assert(receive_positive_integer("0601", NULL) == 601);

    left = -1;
    assert(receive_positive_integer("456", &left) == 456);
    assert(left == -1);

    assert(receive_positive_integer("89-12", &left) == 12);
    assert(left == 89);

    assert(receive_positive_integer("-2022", &left) == 2022);
    assert(! left);

    assert(! receive_positive_integer("03629-", &left));
    assert(left == 3629);

    assert(! receive_positive_integer("-", &left));
    assert(! left);


    // failure cases

    assert(receive_positive_integer("2o1", NULL) == -1);
    assert(receive_positive_integer("integer", NULL) == -1);
    assert(receive_positive_integer("-29", NULL) == -1);
    assert(receive_positive_integer("4294967295", NULL) == -1);

    left = -1;
    assert(receive_positive_integer("_73", &left) == -1);
    assert(left == -1);

    assert(receive_positive_integer("3278o-3y28", &left) == -1);
    assert(left == -1);

    assert(receive_positive_integer("zwei-vier", &left) == -1);
    assert(left == -1);


    assert(receive_positive_integer("7-ten", &left) == -1);
    assert(left == 7);

    assert(receive_positive_integer("--", &left) == -1);
    assert(! left);
}




static void receive_expected_string_test(void){
    // successful cases

    assert(receive_expected_string("COPY", docker_instr_reprs, DOCKER_INSTRS_NUM, 0) == ID_COPY);
    assert(receive_expected_string("WORKDIR", docker_instr_reprs, DOCKER_INSTRS_NUM, 0) == ID_WORKDIR);
    assert(receive_expected_string("SHELL", docker_instr_reprs, DOCKER_INSTRS_NUM, 0) == ID_SHELL);
    assert(receive_expected_string("LABEL", docker_instr_reprs, DOCKER_INSTRS_NUM, 0) == ID_LABEL);

    assert(receive_expected_string("Volume", docker_instr_reprs, DOCKER_INSTRS_NUM, 1) == ID_VOLUME);
    assert(receive_expected_string("from", docker_instr_reprs, DOCKER_INSTRS_NUM, 1) == ID_FROM);
    assert(receive_expected_string("add", docker_instr_reprs, DOCKER_INSTRS_NUM, 1) == ID_ADD);
    assert(receive_expected_string("ExPose", docker_instr_reprs, DOCKER_INSTRS_NUM, 1) == ID_EXPOSE);

    assert(receive_expected_string("HEA", docker_instr_reprs, DOCKER_INSTRS_NUM, 2) == ID_HEALTHCHECK);
    assert(receive_expected_string("R", docker_instr_reprs, DOCKER_INSTRS_NUM, 2) == ID_RUN);
    assert(receive_expected_string("ENT", docker_instr_reprs, DOCKER_INSTRS_NUM, 2) == ID_ENTRYPOINT);
    assert(receive_expected_string("AR", docker_instr_reprs, DOCKER_INSTRS_NUM, 2) == ID_ARG);

    assert(receive_expected_string("env", docker_instr_reprs, DOCKER_INSTRS_NUM, 3) == ID_ENV);
    assert(receive_expected_string("main", docker_instr_reprs, DOCKER_INSTRS_NUM, 3) == ID_MAINTAINER);
    assert(receive_expected_string("sTo", docker_instr_reprs, DOCKER_INSTRS_NUM, 3) == ID_STOPSIGNAL);
    assert(receive_expected_string("Cmd", docker_instr_reprs, DOCKER_INSTRS_NUM, 3) == ID_CMD);


    // failure cases

    assert(receive_expected_string("copy", docker_instr_reprs, DOCKER_INSTRS_NUM, 0) == -2);
    assert(receive_expected_string("WORK DIR", docker_instr_reprs, DOCKER_INSTRS_NUM, 0) == -2);
    assert(receive_expected_string("S", docker_instr_reprs, DOCKER_INSTRS_NUM, 0) == -1);
    assert(receive_expected_string("QWERT", docker_instr_reprs, DOCKER_INSTRS_NUM, 0) == -2);

    assert(receive_expected_string("vol", docker_instr_reprs, DOCKER_INSTRS_NUM, 1) == -2);
    assert(receive_expected_string("Form", docker_instr_reprs, DOCKER_INSTRS_NUM, 1) == -2);
    assert(receive_expected_string("a", docker_instr_reprs, DOCKER_INSTRS_NUM, 1) == -1);
    assert(receive_expected_string("Lap-Top", docker_instr_reprs, DOCKER_INSTRS_NUM, 1) == -2);

    assert(receive_expected_string("health", docker_instr_reprs, DOCKER_INSTRS_NUM, 2) == -2);
    assert(receive_expected_string("Run", docker_instr_reprs, DOCKER_INSTRS_NUM, 2) == -2);
    assert(receive_expected_string("EN", docker_instr_reprs, DOCKER_INSTRS_NUM, 2) == -1);
    assert(receive_expected_string("STRAW", docker_instr_reprs, DOCKER_INSTRS_NUM, 2) == -2);

    assert(receive_expected_string("environ", docker_instr_reprs, DOCKER_INSTRS_NUM, 3) == -2);
    assert(receive_expected_string("Main Tain", docker_instr_reprs, DOCKER_INSTRS_NUM, 3) == -2);
    assert(receive_expected_string("", docker_instr_reprs, DOCKER_INSTRS_NUM, 3) == -1);
    assert(receive_expected_string("2.3],fm';", docker_instr_reprs, DOCKER_INSTRS_NUM, 3) == -2);
}




static void receive_dockerfile_instruction_test(void){
    char *line;
    int instr_id;

    // successful cases

    line = "ADD abc.tar.gz ./";
    instr_id = ID_ADD;
    assert(receive_dockerfile_instruction(line, &instr_id) == (line + 4));
    assert(instr_id == ID_ADD);

    line = "HealthCheck\tCmd sh /bin/check-running";
    instr_id = ID_HEALTHCHECK;
    assert(receive_dockerfile_instruction(line, &instr_id) == (line + 12));
    assert(instr_id == ID_HEALTHCHECK);

    line = "EXPOSE  80/tcp 80/udp";
    instr_id = -1;
    assert(receive_dockerfile_instruction(line, &instr_id) == (line + 8));
    assert(instr_id == ID_EXPOSE);

    line = "OnBuild\tWorkDir /";
    instr_id = -1;
    assert(receive_dockerfile_instruction(line, &instr_id) == (line + 8));
    assert(instr_id == ID_ONBUILD);


    // failure cases

    instr_id = ID_FROM;
    assert(! receive_dockerfile_instruction("form alpine:latest", &instr_id));

    instr_id = ID_STOPSIGNAL;
    assert(! receive_dockerfile_instruction("SIGNAL SIGINT", &instr_id));

    instr_id = ID_ADD;
    assert(! receive_dockerfile_instruction("COPY ./etc/dit_install.sh /dit/etc/", &instr_id));

    instr_id = -1;
    assert(! receive_dockerfile_instruction("setcmd [ \"/bin/bash\", \"--login\" ]", &instr_id));

    instr_id = -1;
    assert(! receive_dockerfile_instruction("LAVEL author inada tsukasa", &instr_id));

    instr_id = -1;
    assert(! receive_dockerfile_instruction("ENT dit inspect", &instr_id));
}




static void check_file_size_test(void){
    FILE *fp;
    size_t size;

    // when specifying an empty file

    assert((fp = fopen(TMP_FILE, "wb")));
    assert(! fclose(fp));

    assert(! check_file_size(TMP_FILE));


    // when specifying a non-empty file

    assert((fp = fopen(TMP_FILE, "wb")));

    size = rand();
    size = size % 50 + 1;
    assert(fwrite("123456789 123456789 123456789 123456789 123456789", sizeof(char), size, fp) == size);

    assert(! fclose(fp));

    size *= sizeof(char);
    assert(check_file_size(TMP_FILE) == size);


    // when specifying a file that is too large

    if (system(NULL) && (! system("dd if=/dev/zero of="TMP_FILE" bs=1M count=2K"))){
        assert(check_file_size(TMP_FILE) == -2);
        assert(errno == EFBIG);
    }


    // when specifying a non-existing file

    assert(! remove(TMP_FILE));
    assert(check_file_size(TMP_FILE) == -1);
    assert(errno == ENOENT);
}




static void check_last_exit_status_test(void){
    int last_exit_status, tmp;
    FILE *fp;
    unsigned int i;

    // if the exit status of last executed command line is valid

    last_exit_status = check_last_exit_status();
    assert((last_exit_status >= 0) && (last_exit_status < 256));


    assert((fp = fopen(EXIT_STATUS_FILE, "w")));

    i = rand();
    i %= 256;
    assert(fprintf(fp, "%u\n", i) >= 0);
    assert(fputs("abc\n", fp) != EOF);

    assert(! fclose(fp));

    tmp = check_last_exit_status();
    assert((tmp >= 0) && (tmp < 256));


    // if the exit status of last executed command line is invalid

    assert((fp = fopen(EXIT_STATUS_FILE, "w")));
    assert(fprintf(fp, "%u\n", (i + 256)) >= 0);
    assert(! fclose(fp));
    assert(check_last_exit_status() == -1);


    assert((fp = fopen(EXIT_STATUS_FILE, "w")));
    assert(fputs("huga\n", fp) != EOF);
    assert(! fclose(fp));
    assert(check_last_exit_status() == -1);


    // restore the contents of the file

    assert((fp = fopen(EXIT_STATUS_FILE, "w")));
    assert(fprintf(fp, "%d\n", last_exit_status) >= 0);
    assert(! fclose(fp));
}


#endif // NDEBUG