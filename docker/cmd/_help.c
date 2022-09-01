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
 * @brief show information for some dit commands.
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
 * @return int  0 (parse success), 1 (normally exit) or -1 (error exit)
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
            default:
                fputs("Try 'dit help --help' for more information.\n", stderr);
                return -1;
        }
    }
    return 0;
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
    puts("  dit [COMMAND] [ARG]...");
    puts("By specifying one of the following COMMANDs, you can use the tool-specific functions.");
    putchar('\n');
    puts("Commands:");
    puts("main features of this tool");
    puts("  convert        show how a command line is reflected to Dockerfile and history-file");
    puts("  optimize       do refactoring on Dockerfile based on its best practices");
    putchar('\n');
    puts("customization of tool settings");
    puts("  config         set the level of ignoring commands when reflecting a executed command line");
    puts("  ignore         edit set of commands that are ignored when reflecting a executed command line");
    putchar('\n');
    puts("editing your Dockerfile");
    puts("  cp             copy files from the shared directory, and reflect this as COPY/ADD instructions");
    puts("  label          edit the metadata listed as LABEL/EXPOSE instructions");
    puts("  setcmd         set the CMD/ENTRYPOINT instruction generated based on a command line");
    puts("  healthcheck    set the HEALTHCHECK instruction generated based on a command line");
    puts("  onbuild        add the ONBUILD instructions generated based on a command line");
    putchar('\n');
    puts("utilitys");
    puts("  erase          remove the specific lines from Dockerfile and/or history-file");
    puts("  inspect        show the directory tree with details about each file");
    puts("  help           show information for some dit commands");
    putchar('\n');
    puts("See 'dit [COMMAND] --help' or 'dit help [COMMAND]...' for more details.");
}


void config_usage(){
    puts("Usages:");
    puts("  dit config [OPTION]... [MODE[,MODE]...]");
    puts("Decide at what level to ignore commands in a executed command line that should not be reflected.");
    putchar('\n');
    puts("Options:");
    puts("  -r, --reset    reset the mode with default value");
    puts("      --help     display this help, and exit normally");
    putchar('\n');
    puts("Modes:");
    puts("   0,  no-reflect    in the first place, do not reflect");
    puts("   1,  strict        ignore all, strictly");
    puts("   2,  normal        ignore while respecting each command's dependencies");
    puts("   3,  simple        ignore only if the command line contains only one command");
    puts("   4,  no-ignore     ignore nothing");
    putchar('\n');
    puts("Remarks:");
    puts("  - If neither OPTION nor MODE is specified, display the current setting information.");
    puts("  - To specify the mode, you can use the above serial numbers and strings,");
    puts("    and any of the strings can be truncated as long as it is unique.");
    puts("  - If you specify an underscore instead of the mode, the current setting is inherited.");
    puts("  - You can change the modes used when reflecting to Dockerfile and/or history-file.");
    puts("  - Each MODE is one of the following formats, and targets the files listed on the right.");
    puts("      <mode>           both files");
    puts("      [abdh]=<mode>    'a''b' (both files), 'd' (Dockerfile), 'h' (history-file)");
    puts("      [0-4_][0-4_]     first character (Dockerfile), second character (history-file)");
    putchar('\n');
    puts("Examples:");
    puts("  dit config strict");
    puts("  dit config d=no-ig,h=sim");
    puts("  dit config _3");
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
    puts("Show detailed usage of each specified dit COMMAND.");
    putchar('\n');
    puts("Options:");
    puts("  -l, --list    list all dit commands available, and exit normally");
    puts("      --help    display this help, and exit normally");
    putchar('\n');
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
    puts("List information about the files under the specified DIRECTORYs in a tree format.");
    putchar('\n');
    puts("Options:");
    puts("  -C, --color              colorize the output to distinguish file types");
    puts("  -F, --classify           append indicator (one of '*/=|') to each file name:");
    puts("                             to executable file, directory, socket or fifo, in order");
    puts("  -n, --numeric-uid-gid    list the corresponding IDs instead of user or group name");
    puts("  -S                       sort by file size, largest first");
    puts("  -X                       sort by file extension, alphabetically");
    puts("      --sort=WORD          replace file sorting method:");
    puts("                             name (default), size (-S), extension (-X)");
    puts("      --help               display this help, and exit normally");
    putchar('\n');
    puts("Remarks:");
    puts("  - If no DIRECTORY is specified, it operates as if the current directory is specified.");
    puts("  - The WORD argument for '--sort' can be truncated and specified as long as it is unique.");
    puts("  - User or group name longer than 8 characters are converted to corresponding ID,");
    puts("    and IDs longer than 8 digits are converted to '#EXCESS' as undisplayable.");
    puts("  - The units of file size are 'k,M,G,T,P,E,Z', which is powers of 1000.");
    puts("  - Undisplayable characters appearing in the file name are uniformly replaced with '?'.");
    putchar('\n');
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