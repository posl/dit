/**
 * @file _config.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'config', that edits the modes of the dit command 'convert'.
 * @author Tsukasa Inada
 * @date 2022/08/29
 *
 * @note In the config-file, a 2-digit integer in quinary notation is stored as unsigned char.
 */

#include "main.h"

#define CONFIG_FILE "/dit/var/config.stat"

#define CONF_ISRSTFLG 0b001
#define CONF_ISWRTFLG 0b010
#define CONF_ISHASARG 0b100

#define CONF_RESET_OR_SHOW(reset_flag)  ((reset_flag) | ((reset_flag) << 1))
#define CONF_SET_OR_UPDATE(reset_flag)  ((reset_flag) | CONF_ISWRTFLG | CONF_ISHASARG)
#define CONF_GET_FROM_CONVERT  (CONF_ISHASARG)

#define CONF_MODES_NUM 5
#define CONF_DEFAULT_MODE 2
#define CONF_STAT_FORMULA(mode2d, mode2h)  (CONF_MODES_NUM * (mode2d) + (mode2h))
#define CONF_INITIAL_STAT  CONF_STAT_FORMULA(CONF_DEFAULT_MODE, CONF_DEFAULT_MODE)
#define CONF_EXCEED_STAT  (CONF_MODES_NUM * CONF_MODES_NUM)


static int parse_opts(int argc, char **argv, unsigned int *opt);
static int config_contents(unsigned int code, ...);

static bool receive_mode(const char *config_arg, unsigned char result[2]);
static int receive_mode_integer(int c, int spare);


/** array of strings representing each mode in alphabetical order */
static const char * const mode_reprs[CONF_MODES_NUM] = {
    [1] = "no-reflect",  // mode = 0
    [4] = "strict",      // mode = 1
    [2] = "normal",      // mode = 2
    [3] = "simple",      // mode = 3
    [0] = "no-ignore"    // mode = 4
};

/** array for converting mode numbers to index numbers of above string array */
static const int mode2idx[CONF_MODES_NUM] = {1, 4, 2, 3, 0};

/** array for converting index numbers of above string array to mode numbers */
static const int idx2mode[CONF_MODES_NUM] = {4, 0, 2, 3, 1};




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief edit the modes of the dit command 'convert'.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note treated like a normal main function.
 */
int config(int argc, char **argv){
    int i, exit_status = FAILURE;
    unsigned int reset_flag;

    if (! (i = parse_opts(argc, argv, &reset_flag))){
        switch ((argc - optind)){
            case 0:
                assert(CONF_RESET_OR_SHOW(reset_flag) == (reset_flag ? 0b011 : 0b000));
                exit_status = config_contents(CONF_RESET_OR_SHOW(reset_flag));
                break;
            case 1:
                assert(CONF_SET_OR_UPDATE(reset_flag) == (reset_flag ? 0b111 : 0b110));
                if ((exit_status = config_contents(CONF_SET_OR_UPDATE(reset_flag), argv[optind])) > 0)
                    xperror_config_arg(argv[optind]);
                break;
            default:
                xperror_too_many_args(1);
        }
    }
    else if (i > 0)
        exit_status = SUCCESS;

    if (exit_status){
        if (exit_status < 0){
            exit_status = FAILURE;
            xperror_internal_file();
        }
        xperror_suggestion(true);
    }
    return exit_status;
}


/**
 * @brief parse optional arguments.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @param[out] opt  variable to store boolean representing whether to reset the config-file
 * @return int  0 (parse success), 1 (normally exit) or -1 (error exit)
 *
 * @note the arguments are expected to be passed as-is from main function.
 */
static int parse_opts(int argc, char **argv, unsigned int *opt){
    assert(opt);

    const char *short_opts = "r";

    const struct option long_opts[] = {
        { "reset", no_argument, NULL, 'r' },
        { "help",  no_argument, NULL,  1  },
        {  0,       0,           0,    0  }
    };

    *opt = false;

    int c;
    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) >= 0)
        switch (c){
            case 'r':
                *opt = true;
                break;
            case 1:
                config_manual();
                return NORMALLY_EXIT;
            default:
                return ERROR_EXIT;
        }

    assert(*opt == ((bool) *opt));
    return SUCCESS;
}




/**
 * @brief function that contains all the individual functions that handle the config-file
 *
 * @param[in]  code  some flags (bit 1: wheter to reset, bit 2: whether to write, bit 3: argument existence)
 * @param[out] ...  if necessary, string for determining the modes and array for storing store them
 * @return int  0 (success), 1 (argument recognition error) or -1 (unexpected error)
 */
static int config_contents(unsigned int code, ...){
    assert(code < 8);

    bool reset_flag, write_flag, has_arg;
    FILE *fp;
    int exit_status = UNEXPECTED_ERROR;

    reset_flag = code & CONF_ISRSTFLG;
    write_flag = code & CONF_ISWRTFLG;
    has_arg = code & CONF_ISHASARG;

    if ((fp = fopen(CONFIG_FILE, (reset_flag ? "wb" : "rb+")))){
        unsigned char status = CONF_INITIAL_STAT, modes[2] = { CONF_DEFAULT_MODE, CONF_DEFAULT_MODE };

        exit_status = SUCCESS;

        if (! reset_flag){
            if ((fread(&status, sizeof(status), 1, fp) == 1) && (status < CONF_EXCEED_STAT)){
                div_t tmp;
                tmp = div(status, CONF_MODES_NUM);
                modes[1] = tmp.quot;
                modes[0] = tmp.rem;
            }
            else {
                exit_status = UNEXPECTED_ERROR;
                status = CONF_INITIAL_STAT;
            }

            if (! has_arg)
                fprintf(
                    stdout, "d=%s\nh=%s\n",
                    mode_reprs[mode2idx[modes[1]]],
                    mode_reprs[mode2idx[modes[0]]]
                );
        }

        if (has_arg){
            va_list sp;
            va_start(sp, code);

            if ((receive_mode(va_arg(sp, const char *), modes))){
                if (write_flag)
                    status = CONF_STAT_FORMULA(modes[1], modes[0]);
                else
                    memcpy(va_arg(sp, unsigned char *), modes, (sizeof(unsigned char) * 2));
            }
            else
                exit_status = POSSIBLE_ERROR;

            va_end(sp);
        }

        switch (exit_status){
            case SUCCESS:
                if (! write_flag)
                    break;
            case UNEXPECTED_ERROR:
                if (! reset_flag)
                    fseek(fp, 0, SEEK_SET);
                fwrite(&status, sizeof(status), 1, fp);
        }

        fclose(fp);
    }

    return exit_status;
}


/**
 * @brief get the modes of the dit command 'convert' and hand over it to the command.
 *
 * @param[in]  config_arg  string for determining the modes
 * @param[out] modes  array for storing the modes used when reflecting in Dockerfile and history-file
 * @return int  0 (success), 1 (argument recognition error) or -1 (unexpected error)
 *
 * @note the modes are stored in order, one for history-file and one for Dockerfile.
 */
int get_config(const char *config_arg, unsigned char modes[2]){
    assert(CONF_GET_FROM_CONVERT == 0b100);
    return config_contents(CONF_GET_FROM_CONVERT, config_arg, modes);
}




/******************************************************************************
    * String Recognizers
******************************************************************************/


/**
 * @brief parse the passed string, and generate next modes.
 *
 * @param[in]  config_arg  string for determining the modes
 * @param[out] results  array for storing the modes used when reflecting in Dockerfile and history-file
 * @return bool  successful or not
 */
static bool receive_mode(const char *config_arg, unsigned char results[2]){
    assert(results);
    assert(results[1] < CONF_MODES_NUM);
    assert(results[0] < CONF_MODES_NUM);

    if (config_arg){
        char *arg_copy;
        unsigned char modes[2];
        const char *token;
        int target_c, i, tmp;

        size_t size;
        size = strlen(config_arg) + 1;

        char buf[size];
        memcpy(buf, config_arg, (sizeof(char) * size));

        arg_copy = buf;
        assert(arg_copy);

        memcpy(modes, results, (sizeof(unsigned char) * 2));

        while ((token = strtok(arg_copy, ","))){
            arg_copy = NULL;
            target_c = 'b';
            size = strlen(token);

            if (size == 2){
                for (i = 2; i--;){
                    if ((tmp = receive_mode_integer(token[i ^ 1], modes[i])) < 0){
                        assert(target_c == 'b');
                        goto strcmp;
                    }
                    assert((tmp >= 0) && (tmp < CONF_MODES_NUM));
                    modes[i] = tmp;
                }
                assert(i == -1);
                continue;
            }
            else if (size > 2){
                if (token[1] == '='){
                    if ((token[0] != 'b') && (token[0] != 'd') && (token[0] != 'h'))
                        return false;
                    target_c = token[0];
                    token += 2;

                    if (! token[1])
                        goto chrcmp;
                }
                goto strcmp;
            }

        chrcmp:
            switch ((i = receive_mode_integer(token[0], -2))){
                case -1:
                    break;
                case -2:
                    continue;
                default:
                    goto assign;
            }

        strcmp:
            if ((i = receive_expected_string(token, mode_reprs, CONF_MODES_NUM, 2)) < 0)
                return false;
            i = idx2mode[i];

        assign:
            assert((i >= 0) && (i < CONF_MODES_NUM));
            if (target_c != 'h')
                modes[1] = i;
            if (target_c != 'd')
                modes[0] = i;
        }

        memcpy(results, modes, (sizeof(unsigned char) * 2));
        assert(results[1] < CONF_MODES_NUM);
        assert(results[0] < CONF_MODES_NUM);
    }

    return true;
}


/**
 * @brief receive the passed character as integer corresponding to a mode.
 *
 * @param[in]  c  target character
 * @param[in]  spare  integer to return if an underscore is passed
 * @return int  the resulting integer, spare integer, or -1
 */
static int receive_mode_integer(int c, int spare){
    int i, mode = -1;

    if ((i = c - '0') >= 0){
        if (i < CONF_MODES_NUM)
            mode =  i;
        else if (c == '_')
            mode = spare;
    }
    return mode;
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/


static void receive_mode_test(void);
static void receive_mode_integer_test(void);




void config_test(void){
    do_test(receive_mode_integer_test);
    do_test(receive_mode_test);
}




static void receive_mode_test(void){
    const struct {
        const char * const input;
        const int result;
    }
    // changeable part for updating test cases
    table[] = {
        { "",                CONF_INITIAL_STAT       },
        { "_",               CONF_INITIAL_STAT       },
        { "0,1,2,3,4",       CONF_STAT_FORMULA(4, 4) },
        { "0_",              CONF_STAT_FORMULA(0, 4) },
        { "d=2,h=3",         CONF_STAT_FORMULA(2, 3) },
        { "h=1,0_",          CONF_STAT_FORMULA(0, 1) },
        { "h=no-ig",         CONF_STAT_FORMULA(0, 4) },
        { "strict,d=simple", CONF_STAT_FORMULA(3, 1) },
        { "1,,h=no-refl",    CONF_STAT_FORMULA(1, 0) },
        { "b=norm,3_",       CONF_STAT_FORMULA(3, 2) },
        { "m",                   -1                  },
        { "0,1,2,3,4,5",         -1                  },
        { "-4",                  -1                  },
        { "no-",                 -1                  },
        { "d=norm,h=struct",     -1                  },
        {  0,                     0                  }
    };

    int rand2d, rand2h, i, type;
    unsigned char modes[2] = { CONF_DEFAULT_MODE, CONF_DEFAULT_MODE };
    div_t tmp;
    char format[] = "%-15s  %d  %d\n";

    rand2d = CONF_DEFAULT_MODE;
    rand2h = CONF_DEFAULT_MODE;


    for (i = 0; table[i].input; i++){
        type = SUCCESS;

        if (table[i].result < 0){
            modes[1] = (rand2d = rand() % CONF_MODES_NUM);
            modes[0] = (rand2h = rand() % CONF_MODES_NUM);
            type = FAILURE;
        }

        assert(receive_mode(table[i].input, modes) == (type == SUCCESS));

        if (type == SUCCESS){
            tmp = div(table[i].result, CONF_MODES_NUM);
            assert(modes[1] == tmp.quot);
            assert(modes[0] == tmp.rem);

            format[5] = ' ';
            format[6] = ' ';
        }
        else {
            assert(modes[1] == rand2d);
            assert(modes[0] == rand2h);

            format[5] = '\n';
            format[6] = '\0';
        }

        print_progress_test_loop('S', type, i);
        fprintf(stderr, format, table[i].input, modes[1], modes[0]);
    }
}




static void receive_mode_integer_test(void){
    const int spare = -2;

    const struct {
        const int input;
        const int result;
    }
    // changeable part for updating test cases
    table[] = {
        { '0',   0   },
        { '2',   2   },
        { '3',   3   },
        { '4',   4   },
        { '_', spare },
        { '5',  -1   },
        { '-',  -1   },
        { 'i',  -1   },
        { 'o',  -1   },
        { ' ',  -1   },
        {  0,    0   }
    };

    for (int i = 0; table[i].input; i++){
        assert(receive_mode_integer(table[i].input, spare) == table[i].result);

        print_progress_test_loop('S', ((table[i].result != -1) ? SUCCESS : FAILURE), i);
        fprintf(stderr, "'%c'  % d\n", table[i].input, table[i].result);
    }
}


#endif // NDEBUG