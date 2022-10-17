/**
 * @file _config.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'config', that edits the modes of dit command 'convert'.
 * @author Tsukasa Inada
 * @date 2022/08/29
 *
 * @note In the config-file, a 2-digit integer in quinary notation is stored as signed char type.
 */

#include "main.h"

#define CONFIG_FILE "/dit/var/config.stat"

#define CONF_ISRSTFLG 1
#define CONF_ISWRTFLG 2
#define CONF_ISHASARG 4

#define CONF_RESET_OR_SHOW(reset_flag)  (((reset_flag) << 1) | (reset_flag))
#define CONF_SET_OR_UPDATE(reset_flag)  (CONF_ISWRTFLG | CONF_ISHASARG | (reset_flag))
#define CONF_GET_FROM_CONVERT  (CONF_ISHASARG)

#define CONF_MODES_NUM 5
#define CONF_DEFAULT_MODE 2
#define CONF_STAT_FORMULA(mode2d, mode2h)  (CONF_MODES_NUM * (mode2d) + (mode2h))
#define CONF_INITIAL_STAT  CONF_STAT_FORMULA(CONF_DEFAULT_MODE, CONF_DEFAULT_MODE)
#define CONF_EXCEED_STAT  (CONF_MODES_NUM * CONF_MODES_NUM)


static int __parse_opts(int argc, char **argv, int *opt);
static int __config_contents(int code, ...);

static bool __receive_mode(const char *config_arg, int * restrict p_mode2d, int * restrict p_mode2h);
static int __receive_mode_integer(int c, int spare);


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
 * @brief edit the modes of dit command 'convert'.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note treated like a normal main function.
 */
int config(int argc, char **argv){
    int i, reset_flag = 0;

    if ((i = __parse_opts(argc, argv, &reset_flag)))
        return (i < 0);

    switch ((argc - optind)){
        case 0:
            i = __config_contents(CONF_RESET_OR_SHOW(reset_flag));
            break;
        case 1:
            if ((i = __config_contents(CONF_SET_OR_UPDATE(reset_flag), argv[optind])) < 0)
                xperror_invalid_arg('C', 0, "mode", argv[optind]);
            break;
        default:
            i = -1;
            xperror_too_many_args(1);
    }

    if (i){
        if (i < 0)
            i = 1;
        else
            xperror_internal_file();
        xperror_suggestion(true);
    }
    return i;
}


/**
 * @brief parse optional arguments.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @param[out] opt  variable to store integer representing whether to reset config-file
 * @return int  0 (parse success), 1 (normally exit) or -1 (error exit)
 *
 * @note the arguments are expected to be passed as-is from main function.
 */
static int __parse_opts(int argc, char **argv, int *opt){
    const char *short_opts = "r";

    const struct option long_opts[] = {
        { "reset", no_argument, NULL, 'r' },
        { "help",  no_argument, NULL,  1  },
        {  0,       0,           0,    0  }
    };

    int c;
    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) >= 0){
        switch (c){
            case 'r':
                *opt = 1;
                break;
            case 1:
                config_manual();
                return 1;
            default:
                return -1;
        }
    }
    return 0;
}


/**
 * @brief function that combines the individual functions to handle config-file into one
 *
 * @param[in]  code  some flags (bit 1: wheter to reset, bit 2: whether to write, bit 3: argument existence)
 * @param[out] ...  if necessary, string for determining the modes and variable to store them
 * @return int  0 (success), 1 (unexpected error) or -1 (argument recognition error)
 */
static int __config_contents(int code, ...){
    bool reset_flag, write_flag, has_arg;

    reset_flag = code & CONF_ISRSTFLG;
    write_flag = code & CONF_ISWRTFLG;
    has_arg = code & CONF_ISHASARG;

    FILE *fp;
    int exit_status = 0;

    if ((fp = fopen(CONFIG_FILE, (reset_flag ? "wb" : "rb+")))){
        signed char c = CONF_INITIAL_STAT;
        int mode2d = CONF_DEFAULT_MODE,
            mode2h = CONF_DEFAULT_MODE;

        if (! reset_flag){
            if ((fread(&c, sizeof(c), 1, fp) == 1) && (c >= 0) && (c < CONF_EXCEED_STAT)){
                div_t tmp;
                tmp = div(c, CONF_MODES_NUM);
                mode2d = tmp.quot;
                mode2h = tmp.rem;
            }
            else {
                exit_status = 1;
                c = CONF_INITIAL_STAT;
            }

            if (! has_arg)
                fprintf(stdout, "d=%s\nh=%s\n", mode_reprs[mode2idx[mode2d]], mode_reprs[mode2idx[mode2h]]);
        }

        if (has_arg){
            va_list sp;
            va_start(sp, code);

            if ((__receive_mode(va_arg(sp, const char *), &mode2d, &mode2h))){
                if (write_flag){
                    signed char d;
                    if (c != (d = CONF_STAT_FORMULA(mode2d, mode2h)))
                        c = d;
                    else if (! reset_flag)
                        write_flag = false;
                }
                else {
                    *(va_arg(sp, int *)) = mode2d;
                    *(va_arg(sp, int *)) = mode2h;
                }
            }
            else
                exit_status = -1;

            va_end(sp);
        }

        switch (exit_status){
            case 0:
                if (! write_flag)
                    break;
            case 1:
                if (! reset_flag)
                    fseek(fp, 0, SEEK_SET);

                exit_status |= (fwrite(&c, sizeof(c), 1, fp) != 1);
        }
        fclose(fp);
    }
    else
        exit_status = 1;

    return exit_status;
}


/**
 * @brief get the modes of dit command 'convert', and hand over it to the command.
 *
 * @param[in]  config_arg  string for determining the modes
 * @param[out] p_mode2d  variable to store the mode used when reflecting to Dockerfile
 * @param[out] p_mode2h  variable to store the mode used when reflecting to history-file
 * @return int  0 (success), 1 (unexpected error) or -1 (argument recognition error)
 */
int get_config(const char *config_arg, int * restrict p_mode2d, int * restrict p_mode2h){
    return __config_contents(CONF_GET_FROM_CONVERT, config_arg, p_mode2d, p_mode2h);
}




/******************************************************************************
    * String Recognizers
******************************************************************************/


/**
 * @brief parse the passed string, and generate next modes.
 *
 * @param[in]  config_arg  string for determining the modes
 * @param[out] p_mode2d  variable to store the mode used when reflecting to Dockerfile
 * @param[out] p_mode2h  variable to store the mode used when reflecting to history-file
 * @return bool  successful or not
 */
static bool __receive_mode(const char *config_arg, int * restrict p_mode2d, int * restrict p_mode2h){
    char *S;
    if ((S = xstrndup(config_arg, strlen(config_arg)))){
        const char *token;

        if ((token = strtok(S, ","))){
            int mode2d, mode2h, mode, offset, target, i;
            mode2d = *p_mode2d;
            mode2h = *p_mode2h;

            do {
                mode = -1;
                offset = 0;
                target = 'b';

                if (! (i = strlen(token) - 2)){
                    if ((i = __receive_mode_integer(token[0], mode2d)) >= 0){
                        if ((mode = __receive_mode_integer(token[1], mode2h)) >= 0){
                            mode2d = i;
                            mode2h = mode;
                            continue;
                        }
                    }
                    else
                        i = 0;
                }
                else if (i > 0){
                    if (token[1] == '='){
                        if (! strchr("bdh", token[0]))
                            return false;
                        if (token[3] == '\0')
                            i = -1;

                        offset = 2;
                        target = token[0];
                    }
                }

                token += offset;
                if ((i < 0) && ((mode = __receive_mode_integer(token[0], -2)) < -1))
                    continue;
                if ((mode < 0) && ((i = receive_expected_string(token, mode_reprs, CONF_MODES_NUM, 2)) >= 0))
                    mode = idx2mode[i];

                if (mode >= 0){
                    switch (target){
                        case 'b':
                            mode2h = mode;
                        case 'd':
                            mode2d = mode;
                            break;
                        case 'h':
                            mode2h = mode;
                    }
                }
                else
                    return false;
            } while ((token = strtok(NULL, ",")));

            *p_mode2d = mode2d;
            *p_mode2h = mode2h;
        }
        return true;
    }
    return false;
}


/**
 * @brief receive the passed character as integer corresponding to a mode.
 *
 * @param[in]  c  target character
 * @param[in]  spare  integer to return if an underscore is passed
 * @return int  the resulting integer, spare integer, or -1
 */
static int __receive_mode_integer(int c, int spare){
    int i;
    if ((i = c - '0') >= 0){
        if (i < CONF_MODES_NUM)
            return i;
        if (c == '_')
            return spare;
    }
    return -1;
}