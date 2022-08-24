/**
 * @file _help.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'help', that shows information for some dit commands
 * @author Tsukasa Inada
 * @date 2022/08/09
 */

#include "main.h"


static int __parse_opts(int argc, char **argv);
static int __display_usage(const char *target, int is_plural);

static void __dit_usage();
static void __help_usage();


extern const char * const dit_cmds[CMDS_NUM];




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief show information for some dit commands
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note treated like a normal main function.
 */
int help(int argc, char **argv){
    setvbuf(stdout, NULL, _IOFBF, 0);

    int i;
    if ((i = __parse_opts(argc, argv)))
        return (i > 0) ? 0 : 1;

    int error_occurred = 0;
    if ((argc -= optind) > 0){
        argv += optind;
        i = (argc > 1);

        while (1){
            if (__display_usage(*argv, i))
                error_occurred = 1;

            if (--argc){
                argv++;
                putchar('\n');
            }
            else
                break;
        }
    }
    else
        __dit_usage();

    return error_occurred;
}


/**
 * @brief parse optional arguments.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  parse result (zero: parse success, positive: help success, negative: parse failure)
 *
 * @note the arguments are expected to be passed as-is from main function.
 */
static int __parse_opts(int argc, char **argv){
    optind = 1;
    opterr = 0;

    const char *short_opts = ":l";
    const struct option long_opts[] = {
        { "list", no_argument, NULL, 'l' },
        { "help", no_argument, NULL,  1  },
        {  0,      0,           0,    0  }
    };

    int A[CMDS_NUM] = {1, 10, 0, 6, 2, 8, 11, 4, 9, 3, 7, 5};

    int c;
    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1){
        switch (c){
            case 'l':
                for (c = 0; c < CMDS_NUM; c++)
                    printf("%s\n", dit_cmds[A[c]]);
                return 1;
            case 1:
                __help_usage();
                return 1;
            case '\?':
                if (argv[--optind][1] == '-')
                    fprintf(stderr, "help: unrecognized option '%s'\n", argv[optind]);
                else
                    fprintf(stderr, "help: invalid option '-%c'\n", optopt);
                goto error_occurred;
        }
    }
    return 0;

error_occurred:
    fputs("Try 'dit help --help' for more information.\n", stderr);
    return -1;
}


/**
 * @brief display usage for the corresponding dit command on screen.
 *
 * @param[in]  target  target string
 * @param[in]  is_plural  whether command line arguments contain multiple dit commands
 * @return int  exit status like command's one
 *
 * @note "cfg" and "hc", which are default aliases of 'config' and 'heatlthcheck', are supported.
 */
static int __display_usage(const char *target, int is_plural){
    void (* const help_funcs[CMDS_NUM])() = {
        config_usage,
        convert_usage,
        cp_usage,
        erase_usage,
        healthcheck_usage,
        __help_usage,
        ignore_usage,
        inspect_usage,
        label_usage,
        onbuild_usage,
        optimize_usage,
        setcmd_usage
    };

    int i;
    i = (! strcmp(target, "cfg")) ? 0 :
        (! strcmp(target, "hc"))  ? 4 :
        receive_expected_string(target, dit_cmds, CMDS_NUM, 2);

    if (i >= 0){
        if (is_plural)
            printf(" < %s >\n", dit_cmds[i]);
        help_funcs[i]();
        return 0;
    }
    else {
        FILE *fp;
        fp = is_plural ? stdout : stderr;
        fprintf(fp, "help: %s command '%s' for dit\n", ((i == -1) ? "ambiguous" : "invalid"), target);
        fputs("Try 'dit help' for more information.\n", fp);
        return 1;
    }
}




/******************************************************************************
    * Help Functions
******************************************************************************/


static void __dit_usage(){
    puts("Usages:");
    puts("  dit [COMMAND] [ARGS]...");
    puts("By specifying one of the following COMMANDs, you can use the tool-specific functions.\n");
    puts("Commands:");
    puts("main features of this tool");
    puts("  convert        show how a command line is reflected to Dockerfile and history-file");
    puts("  optimize       refactoring Dockerfile based on its best practices\n");
    puts("customize tool settings");
    puts("  config         make changes the specification of how a executed command line is reflected");
    puts("  ignore         edit set of commands that is ignored when reflecting a command line\n");
    puts("edit your Dockerfile");
    puts("  cp             copy files from shared directory, and reflect this as COPY or ADD instruction");
    puts("  label          make changes about LABEL or EXPOSE instruction");
    puts("  setcmd         make changes about CMD or ENTRYPOINT instruction");
    puts("  healthcheck    make changes about HEALTHCHECK instruction");
    puts("  onbuild        make changes about ONBUILD instruction\n");
    puts("utilitys");
    puts("  erase          remove specific lines from Dockerfile and/or history-file");
    puts("  inspect        show the directory tree with details about each file");
    puts("  help           show information for some dit commands\n");
    puts("See 'dit [COMMAND] --help' or 'dit help [COMMAND]...' for more details.");
}


void config_usage(){
    puts("help config");
}


void convert_usage(){
    puts("help convert");
}


void cp_usage(){
    puts("help cp");
}


void erase_usage(){
    puts("help erase");
}


void healthcheck_usage(){
    puts("help healthcheck");
}


static void __help_usage(){
    puts("Usages:");
    puts("  dit help [OPTION]... [COMMAND]...");
    puts("Show detailed usage of each specified dit COMMAND.\n");
    puts("Options:");
    puts("  -l, --list    list all commands available, and exit normally");
    puts("      --help    display this help, and exit normally\n");
    puts("Remarks:");
    puts("  - If no SUBCOMMAND is specified, list a summary of each dit command.");
    puts("  - Each SUBCOMMAND can be truncated as long as it is unique,");
    puts("    and for 'config' and 'healthcheck' can be specified by 'cfg' and 'hc'.");
}


void ignore_usage(){
    puts("help ignore");
}


void inspect_usage(){
    puts("Usages:");
    puts("  dit inspect [OPTION]... [DIRECTORY]...");
    puts("List information about the files under the specified DIRECTORYs in a tree format.\n");
    puts("Options:");
    puts("  -C, --color              colorize the output to distinguish file types");
    puts("  -F, --classify           append indicator (one of '*/=|') to each file name:");
    puts("                             to executable file, directory, socket or fifo, in order");
    puts("  -n, --numeric-uid-gid    list the corresponding IDs instead of user or group name");
    puts("  -S                       sort by file size, largest first");
    puts("  -X                       sort by file extension, alphabetically");
    puts("      --sort=WORD          replace file sorting method:");
    puts("                             name(default), size(-S), extension(-X)");
    puts("      --help               display this help, and exit normally\n");
    puts("Remarks:");
    puts("  - If no DIRECTORY is specified, it operates as if the current directory is specified.");
    puts("  - The WORD argument for '--sort' can be truncated and specified as long as it is unique.");
    puts("  - User or group name longer than 8 characters are converted to corresponding ID,");
    puts("    and IDs longer than 8 digits are converted to '#EXCESS' as undisplayable.");
    puts("  - The units of file size are 'k,M,G,T,P,E,Z', which is powers of 1000.");
    puts("  - Undisplayable characters appearing in the file name are uniformly replaced with '?'.\n");
    puts("The above options are similar to the 'ls' command which is a GNU one.");
    puts("See that man page for more details.");
}


void label_usage(){
    puts("help label");
}


void onbuild_usage(){
    puts("help onbuild");
}


void optimize_usage(){
    puts("help optimize");
}


void setcmd_usage(){
    puts("help setcmd");
}