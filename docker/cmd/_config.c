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

#define CONF_MODES_NUM 5
#define CONF_DEFAULT_MODE 2
#define CONF_STAT_FORMULA(mode2d, mode2h)  (CONF_MODES_NUM * mode2d + mode2h)
#define CONF_INITIAL_STAT  CONF_STAT_FORMULA(CONF_DEFAULT_MODE, CONF_DEFAULT_MODE)
#define CONF_EXCEED_STAT  (CONF_MODES_NUM * CONF_MODES_NUM)


/** Data type that collects serial numbers of the contents that handle the config-file */
typedef enum {
    reset,
    display,
    set,
    update,
    get
} conf_conts;


static int __parse_opts(int argc, char **argv, bool *opt);
static int __config_contents(conf_conts code, ...);

static bool __receive_mode(const char *config_arg, int *p_mode2d, int *p_mode2h);
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
    int i;
    bool reset_flag = false;

    if ((i = __parse_opts(argc, argv, &reset_flag))){
        if (i > 0)
            return 0;
    }
    else if (! (argc -= optind)){
        if (! __config_contents((reset_flag ? reset : display)))
            return 0;
    }
    else if (! (argc -= 1)){
        const char *config_arg;
        config_arg = argv[optind];

        if (! (i = __config_contents((reset_flag ? set : update), config_arg)))
            return 0;
        if (i < 0)
            xperror_invalid_cmdarg(0, "mode", config_arg);
    }
    else if (argc > 0)
        xperror_too_many_args(1);

    xperror_suggestion(true);
    return 1;
}


/**
 * @brief parse optional arguments.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @param[out] opt  variable to store boolean representing whether to reset config-file
 * @return int  0 (parse success), 1 (normally exit) or -1 (error exit)
 *
 * @note the arguments are expected to be passed as-is from main function.
 */
static int __parse_opts(int argc, char **argv, bool *opt){
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
                *opt = true;
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
 * @brief function that combines the individual functions to use config-file into one
 *
 * @param[in]  code  serial number identifying the operation content
 * @return int  0 (success), 1 (unexpected error) or -1 (argument recognition error)
 */
static int __config_contents(conf_conts code, ...){
    FILE *fp;
    signed char c = CONF_INITIAL_STAT;
    bool write_flag = true;

    if (code != reset){
        int mode2d = CONF_DEFAULT_MODE, mode2h = CONF_DEFAULT_MODE;

        if (code != set){
            if ((fp = fopen(CONFIG_FILE, "rb"))){
                if ((fread(&c, sizeof(c), 1, fp) == 1) && (c >= 0) && (c < CONF_EXCEED_STAT)){
                    write_flag = false;

                    div_t tmp;
                    tmp = div(c, CONF_MODES_NUM);
                    mode2d = tmp.quot;
                    mode2h = tmp.rem;
                }
                else
                    c = CONF_INITIAL_STAT;

                fclose(fp);
            }
            if (write_flag && (code != get))
                return 1;
        }

        if (code == display){
            printf(" d=%s\n", mode_reprs[mode2idx[mode2d]]);
            printf(" h=%s\n", mode_reprs[mode2idx[mode2h]]);
        }
        else {
            va_list sp;
            va_start(sp, code);

            bool success_flag;
            if ((success_flag = __receive_mode(va_arg(sp, const char *), &mode2d, &mode2h))){
                if (code != get){
                    signed char d;
                    if (c != (d = CONF_STAT_FORMULA(mode2d, mode2h))){
                        c = d;
                        write_flag = true;
                    }
                }
                else {
                    *(va_arg(sp, int *)) = mode2d;
                    *(va_arg(sp, int *)) = mode2h;
                }
            }
            va_end(sp);

            if (! success_flag)
                return -1;
        }
    }

    int exit_status = 0;
    if (write_flag){
        if ((fp = fopen(CONFIG_FILE, "wb"))){
            exit_status = (fwrite(&c, sizeof(c), 1, fp) != 1);
            fclose(fp);
        }
        else
            exit_status = 1;
    }
    return exit_status;
}


/**
 * @brief get the modes of dit command 'convert', and hand over it to the command.
 *
 * @param[in]  config_arg  string for determining the modes
 * @param[out] p_mode2d  variable to store the setting used when reflecting to Dockerfile
 * @param[out] p_mode2h  variable to store the setting used when reflecting to history-file
 * @return int  0 (success), 1 (unexpected error) or -1 (argument recognition error)
 */
int get_config(const char *config_arg, int *p_mode2d, int *p_mode2h){
    return __config_contents(get, config_arg, p_mode2d, p_mode2h);
}




/******************************************************************************
    * String Recognizers
******************************************************************************/


/**
 * @brief parse the passed string, and generate next modes.
 *
 * @param[in]  config_arg  string for determining the modes
 * @param[out] p_mode2d  variable to store the setting used when reflecting to Dockerfile
 * @param[out] p_mode2h  variable to store the setting used when reflecting to history-file
 * @return bool  successful or not
 */
static bool __receive_mode(const char *config_arg, int *p_mode2d, int *p_mode2h){
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
 * @param[in]  spare  integer to return if a underscore is passed
 * @return int  the resulting integer, arbitrary integer, or -1
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