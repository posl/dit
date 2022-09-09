/**
 * @file _config.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'config', that edits the modes of dit command 'convert'
 * @author Tsukasa Inada
 * @date 2022/08/29
 *
 * @note In the config-file, a 2-digit integer in quinary notation is stored as signed char type.
 */

#include "main.h"

#define CONFIG_FILE "/dit/conf/current-spec.conv"
#define CONFIGS_NUM 5


/** Data type that collects serial numbers of the contents related to config-file */
typedef enum {
    init,
    display,
    update,
    get
} contents;


static int __parse_opts(int argc, char **argv, bool *opt);

static int __config_contents(contents code, ...);
static int __init_config();
static int __display_config();
static int __update_config(const char *config_arg);

static bool __receive_config(const char *config_arg, int *p_spec2d, int *p_spec2h);
static int __receive_config_integer(int c, int spare);
static int __receive_config_string(const char *token);




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
    bool reset = false;

    if ((i = __parse_opts(argc, argv, &reset)))
        return (i > 0) ? 0 : 1;

    bool err_flag = true;
    if (! (argc -= optind)){
        if (! (reset ? __init_config() : __display_config()))
            return 0;
    }
    else {
        if (reset)
            i = __init_config();

        if (! i){
            if (! (argc -= 1)){
                const char *config_arg;
                config_arg = argv[optind];

                if (! (i = __update_config(config_arg)))
                    return 0;
                if (i < 0){
                    err_flag = false;
                    fprintf(stderr, "config: unrecognized argument '%s'\n", config_arg);
                }
            }
            else if (argc > 0) {
                err_flag = false;
                fputs("config: allow up to one non-optional argument\n", stderr);
            }
        }
    }

    if (err_flag)
        fputs("config: unexpected error\n", stderr);
    fputs("Try 'dit config --help' for more information.\n", stderr);
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
    optind = 1;
    opterr = 0;

    const char *short_opts = ":r";
    const struct option long_opts[] = {
        { "reset", no_argument, NULL, 'r' },
        { "help",  no_argument, NULL,  1  },
        {  0,       0,           0,    0  }
    };

    int c;
    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1){
        switch (c){
            case 'r':
                *opt = true;
                break;
            case 1:
                config_usage();
                return 1;
            case '\?':
                if (argv[--optind][1] == '-')
                    fprintf(stderr, "config: unrecognized option '%s'\n", argv[optind]);
                else
                    fprintf(stderr, "config: invalid option '-%c'\n", optopt);
            default:
                fputs("Try 'dit config --help' for more information.\n", stderr);
                return -1;
        }
    }
    return 0;
}




/******************************************************************************
    * Contents
******************************************************************************/


/**
 * @brief function that combines the individual functions to use config-file into one
 *
 * @param[in]  code  serial number identifying the content
 * @return int  exit status tailored to caller
 */
static int __config_contents(contents code, ...){
    signed char c = 12;
    bool write_flag = true;
    FILE *fp;
    int exit_status = 0;

    if (code != init){
        if ((fp = fopen(CONFIG_FILE, "rb"))){
            int spec2d = 2, spec2h = 2;

            if ((fread(&c, sizeof(c), 1, fp) == 1) && (c >= 0) && (c < 25)){
                write_flag = false;

                div_t tmp;
                tmp = div(c, 5);
                spec2d = tmp.quot;
                spec2h = tmp.rem;
            }
            else if (code == get)
                c = 12;
            else
                exit_status = 1;

            if (! exit_status){
                if (code == display){
                    const char *config_strs[CONFIGS_NUM] = {
                        "no-reflect",
                        "strict",
                        "normal",
                        "simple",
                        "no-ignore"
                    };
                    printf("d=%s\n", config_strs[spec2d]);
                    printf("h=%s\n", config_strs[spec2h]);
                }
                else {
                    va_list sp;
                    va_start(sp, code);

                    if (__receive_config(va_arg(sp, const char *), &spec2d, &spec2h)){
                        if (code == update){
                            signed char d;
                            if (c != (d = 5 * spec2d + spec2h)){
                                c = d;
                                write_flag = true;
                            }
                        }
                        else {
                            *(va_arg(sp, int *)) = spec2d;
                            *(va_arg(sp, int *)) = spec2h;
                        }
                    }
                    else
                        exit_status = -1;

                    va_end(sp);
                }
            }
            fclose(fp);
        }
        else
            exit_status = 1;
    }

    if ((! exit_status) && write_flag){
        if ((fp = fopen(CONFIG_FILE, "wb"))){
            if (fwrite(&c, sizeof(c), 1, fp) != 1)
                exit_status = 1;
            fclose(fp);
        }
        else
            exit_status = 1;
    }
    return exit_status;
}




/**
 * @brief initialize the modes of dit command 'convert'.
 *
 * @return int  exit status like command's one
 */
static int __init_config(){
    return __config_contents(init);
}


/**
 * @brief display the current modes of dit command 'convert' on screen.
 *
 * @return int  exit status like command's one
 */
static int __display_config(){
    return __config_contents(display);
}


/**
 * @brief update the modes of dit command 'convert'.
 *
 * @param[in]  config_arg  string for determining the modes
 * @return int  0 (success), 1 (unexpected error) or -1 (recognition failure)
 */
static int __update_config(const char *config_arg){
    return __config_contents(update, config_arg);
}


/**
 * @brief get the modes of dit command 'convert', and hand over it to the command.
 *
 * @param[in]  config_arg  string for determining the modes
 * @param[out] p_spec2d  variable to store the setting information used when reflecting to Dockerfile
 * @param[out] p_spec2h  variable to store the setting information used when reflecting to history-file
 * @return int  0 (success), 1 (unexpected error) or -1 (recognition failure)
 */
int get_config(const char *config_arg, int *p_spec2d, int *p_spec2h){
    return __config_contents(get, config_arg, p_spec2d, p_spec2h);
}




/******************************************************************************
    * Argument Parsers
******************************************************************************/


/**
 * @brief parse the passed passed string, and generate next modes.
 *
 * @param[in]  config_arg  string for determining the modes
 * @param[out] p_spec2d  variable to store the setting information used when reflecting to Dockerfile
 * @param[out] p_spec2h  variable to store the setting information used when reflecting to history-file
 * @return bool  successful or not
 */
static bool __receive_config(const char *config_arg, int *p_spec2d, int *p_spec2h){
    size_t len;
    len = strlen(config_arg) + 1;

    char A[len];
    strcpy(A, config_arg);

    const char *token;
    if ((token = strtok(A, ","))){
        int spec2d, spec2h, i, j;
        spec2d = *p_spec2d;
        spec2h = *p_spec2h;

        do {
            i = -1;
            if (! (j = strlen(token) - 2)){
                if (! (((spec2d = __receive_config_integer(token[0], spec2d)) < 0)
                    || ((spec2h = __receive_config_integer(token[1], spec2h)) < 0)))
                        continue;
            }
            else if (j > 0){
                if (token[1] == '='){
                    if (token[3] == '\0'){
                        i = __receive_config_integer(token[2], 5);

                        if ((i == 5) && strchr("abdh", *token))
                            continue;
                    }

                    if (i < 0)
                        i = __receive_config_string(token + 2);

                    if ((i >= 0) && (i < 5)){
                        switch (*token){
                            case 'a':
                            case 'b':
                                spec2d = (spec2h = i);
                                continue;
                            case 'd':
                                spec2d = i;
                                continue;
                            case 'h':
                                spec2h = i;
                                continue;
                        }
                    }
                    i = 5;
                }
            }
            else
                i = __receive_config_integer(*token, -1);

            if (i < 0)
                i = __receive_config_string(token);

            if ((i >= 0) && (i < 5))
                spec2d = (spec2h = i);
            else
                return false;
        } while ((token = strtok(NULL, ",")));

        *p_spec2d = spec2d;
        *p_spec2h = spec2h;
    }

    return true;
}


/**
 * @brief receive the passed character as integer corresponding to a mode.
 *
 * @param[in]  c  target character
 * @param[in]  spare  integer to return if a underscore is passed
 * @return int  the resulting integer, arbitrary integer, or -1
 */
static int __receive_config_integer(int c, int spare){
    return (((c >= '0') && (c < '5')) ? (c - '0') : ((c == '_') ? spare : -1));
}


/**
 * @brief generate integer corresponding to a mode from the passed string.
 *
 * @param[in]  token  target string
 * @return int  the resulting integer or -1
 */
static int __receive_config_string(const char *token){
    const char * const config_strs[CONFIGS_NUM] = {
        "no-ignore",
        "no-reflect",
        "normal",
        "simple",
        "strict"
    };
    int A[CONFIGS_NUM] = {4, 0, 2, 3, 1};

    int i;
    return (((i = receive_expected_string(token, config_strs, CONFIGS_NUM, 2)) >= 0) ? A[i] : -1);
}