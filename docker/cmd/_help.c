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

#define CONTENTS_NUM 3


/** Data type that collects serial numbers of the contents displayed by help function */
typedef enum {
    description,
    manual,
    example
} contents;


static int __parse_opts(int argc, char **argv, contents *opt);
static int __display_help(contents code, const char *target);

static void __dit_description();
static void __config_description();
static void __convert_description();
static void __cp_description();
static void __erase_description();
static void __healthcheck_description();
static void __help_description();
static void __ignore_description();
static void __inspect_description();
static void __label_description();
static void __onbuild_description();
static void __optimize_description();
static void __reflect_description();
static void __setcmd_description();

static void __dit_manual();
static void __help_manual();

static void __dit_example();
static void __config_example();
static void __convert_example();
static void __cp_example();
static void __erase_example();
static void __healthcheck_example();
static void __help_example();
static void __ignore_example();
static void __inspect_example();
static void __label_example();
static void __onbuild_example();
static void __optimize_example();
static void __reflect_example();
static void __setcmd_example();


extern const char * const cmd_reprs[CMDS_NUM];




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
    int i;
    contents code;

    if ((i = __parse_opts(argc, argv, &code)))
        return (i > 0) ? 0 : 1;

    setvbuf(stdout, NULL, _IOFBF, 0);

    int exit_status = 0;
    if ((argc -= optind) > 0){
        argv += optind;

        while (1){
            if (__display_help(code, *argv))
                exit_status = 1;

            if (--argc){
                argv++;
                if (code == manual)
                    puts("\n");
                else
                    putchar('\n');
                fflush(stdout);
            }
            else
                break;
        }
    }
    else
        __display_help(code, NULL);

    return exit_status;
}


/**
 * @brief parse optional arguments.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @param[out] opt  variable to store serial number identifying the display content
 * @return int  0 (parse success), 1 (normally exit) or -1 (error exit)
 *
 * @note the arguments are expected to be passed as-is from main function.
 */
static int __parse_opts(int argc, char **argv, contents *opt){
    const char *short_opts = "adem";

    const struct option long_opts[] = {
        { "all",         no_argument, NULL, 'a' },
        { "description", no_argument, NULL, 'd' },
        { "example",     no_argument, NULL, 'e' },
        { "manual",      no_argument, NULL, 'm' },
        { "help",        no_argument, NULL,  1  },
        {  0,             0,           0,    0  }
    };

    const int A[CMDS_NUM] = {1, 10, 0, 6, 2, 8, 12, 4, 9, 11, 3, 7, 5};

    *opt = manual;

    int c;
    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1){
        switch (c){
            case 'a':
                for (c = 0; c < CMDS_NUM; c++)
                    printf("%s\n", cmd_reprs[A[c]]);
                return 1;
            case 'd':
                *opt = description;
                break;
            case 'e':
                *opt = example;
                break;
            case 'm':
                *opt = manual;
                break;
            case 1:
                __help_manual();
                return 1;
            default:
                fputs("Try 'dit help --help' for more information.\n", stderr);
                return -1;
        }
    }
    return 0;
}


/**
 * @brief display requested information for the corresponding dit command on screen.
 *
 * @param[in]  code  serial number identifying the display content
 * @param[in]  target  target string
 * @return int  exit status like command's one
 *
 * @note "cfg" and "hc", which are default aliases of 'config' and 'heatlthcheck', are supported.
 */
static int __display_help(contents code, const char *target){
    if (target){
        void (* const cmd_helps[CONTENTS_NUM][CMDS_NUM])() = {
            {
                __config_description,
                __convert_description,
                __cp_description,
                __erase_description,
                __healthcheck_description,
                __help_description,
                __ignore_description,
                __inspect_description,
                __label_description,
                __onbuild_description,
                __optimize_description,
                __reflect_description,
                __setcmd_description
            },
            {
                config_manual,
                convert_manual,
                cp_manual,
                erase_manual,
                healthcheck_manual,
                __help_manual,
                ignore_manual,
                inspect_manual,
                label_manual,
                onbuild_manual,
                optimize_manual,
                reflect_manual,
                setcmd_manual
            },
            {
                __config_example,
                __convert_example,
                __cp_example,
                __erase_example,
                __healthcheck_example,
                __help_example,
                __ignore_example,
                __inspect_example,
                __label_example,
                __onbuild_example,
                __optimize_example,
                __reflect_example,
                __setcmd_example
            }
        };

        int i;
        i = (! strcmp(target, "cfg")) ? 0 :
            (! strcmp(target, "hc")) ? 4 :
            receive_expected_string(target, cmd_reprs, CMDS_NUM, 2);

        if (i >= 0){
            if (code == description)
                printf(" < %s >\n", cmd_reprs[i]);
            cmd_helps[code][i]();
        }
        else {
            fprintf(stderr, "help: %s command '%s' for dit\n", ((i == -1) ? "ambiguous" : "invalid"), target);
            fputs("Try 'dit help' for more information.\n", stderr);
            return 1;
        }
    }
    else {
        void (* const dit_helps[CONTENTS_NUM])() = {
            __dit_description,
            __dit_manual,
            __dit_example
        };

        if (code == description)
            puts(" < dit >");
        dit_helps[code]();
    }
    return 0;
}




/******************************************************************************
    * Help Functions that display each Description
******************************************************************************/


static void __dit_description(){
    puts("Use the tool-specific functions as the subcommand.");
}

static void __config_description(){
    puts("Set the level at which commands that should not be reflected are ignored, used when");
    puts("reflecting a executed command line to Dockerfile or history-file, individually.");
}

static void __convert_description(){
    puts("Show how a command line is transformed for reflection to the Dockerfile and/or history-file.");
}

static void __cp_description(){
    puts("Perform processing equivalent to COPY/ADD instructions, and reflect this to Dockerfile.");
}

static void __erase_description(){
    puts("Remove the lines that match some conditions from Dockerfile and/or history-file.");
}

static void __healthcheck_description(){
    puts("Set HEALTHCHECK instruction in Dockerfile.");
}

static void __help_description(){
    puts("Show requested information for some dit commands.");
}

static void __ignore_description(){
    puts("Edit set of commands that should not be reflected to Dockerfile or history-file, individually.");
}

static void __inspect_description(){
    puts("List information about the files under some directories in a tree format.");
}

static void __label_description(){
    puts("Edit list of LABEL/EXPOSE instructions in Dockerfile.");
}

static void __onbuild_description(){
    puts("Append ONBUILD instructions in Dockerfile.");
}

static void __optimize_description(){
    puts("Generate Dockerfile as the result of refactoring based on its best practices.");
}

static void __reflect_description(){
    puts("Append the contents of some files to Dockerfile or history-file.");
}

static void __setcmd_description(){
    puts("Set CMD/ENTRYPOINT instruction in Dockerfile.");
}




/******************************************************************************
    * Help Functions that display each Manual
******************************************************************************/


static void __dit_manual(){
    puts("Usages:");
    puts("  dit [COMMAND] [ARG]...");
    puts("Use the tool-specific functions corresponding to the specified COMMAND.");
    putchar('\n');

    puts("Commands:");
    puts("main features of this tool");
    puts("  convert        show how a command line is reflected to Dockerfile and/or history-file");
    puts("  optimize       do refactoring on Dockerfile based on its best practices");
    putchar('\n');

    puts("customization of tool settings");
    puts("  config         set the level of ignoring commands when reflecting a executed command line");
    puts("  ignore         edit set of commands that should not be reflected");
    putchar('\n');

    puts("editing your Dockerfile");
    puts("  cp             copy files from the host environment, and reflect this as COPY/ADD instructions");
    puts("  label          edit list of LABEL/EXPOSE instructions");
    puts("  setcmd         set CMD/ENTRYPOINT instruction");
    puts("  healthcheck    set HEALTHCHECK instruction");
    puts("  onbuild        append ONBUILD instructions");
    putchar('\n');

    puts("utilitys");
    puts("  reflect        append the contents of some files to Dockerfile or history-file");
    puts("  erase          remove some lines from Dockerfile and/or history-file");
    puts("  inspect        show some directory trees with details about each file");
    puts("  help           show information for some dit commands");
    putchar('\n');

    puts("See 'dit help [OPTION]... [COMMAND]...' for more details.");
}


void config_manual(){
    puts("Usages:");
    puts("  dit config [OPTION]... [MODE[,MODE]...]");
    __config_description();
    putchar('\n');

    puts("Options:");
    puts("  -r, --reset    reset each level with default value");
    puts("      --help     display this help, and exit normally");
    putchar('\n');

    puts("Modes:");
    puts("   0,  no-reflect    in the first place, reflect nothing");
    puts("   1,  strict        ignore all of them, strictly");
    puts("   2,  normal        ignore them while respecting the dependencies between commands (default)");
    puts("   3,  simple        ignore them only if the command line contains only one command");
    puts("   4,  no-ignore     ignore nothing");
    putchar('\n');

    puts("Remarks:");
    puts("  - If neither OPTION nor MODE is specified, display the current settings.");
    puts("  - To specify a mode, you can use the above serial numbers and strings,");
    puts("    and any of the strings can be truncated as long as it is unique.");
    puts("  - If you specify an underscore instead of a mode, the current settings is inherited.");
    puts("  - Each MODE is one of the following formats, and targets the files listed on the right.");
    puts("      <mode>           both files");
    puts("      [bdh]=<mode>     'b' (both files), 'd' (Dockerfile), 'h' (history-file)");
    puts("      [0-4_][0-4_]     first character (Dockerfile), second character (history-file)");
}


void convert_manual(){
    puts("convert manual");
}


void cp_manual(){
    puts("cp manual");
}


void erase_manual(){
    puts("erase manual");
}


void healthcheck_manual(){
    puts("healthcheck manual");
}


static void __help_manual(){
    puts("Usages:");
    puts("  dit help [OPTION]... [COMMAND]...");
    puts("Show requested information for each specified dit COMMAND.");
    putchar('\n');

    puts("Options:");
    puts("  -a, --all            list all dit commands available, and exit normally");
    puts("  -d, --description    show the short descriptions");
    puts("  -e, --example        show the examples of use");
    puts("  -m, --manual         show the detailed manuals");
    puts("      --help           display this help, and exit normally");
    putchar('\n');

    puts("Remarks:");
    puts("  - If no COMMANDs are specified, show information about the main interface of dit commands.");
    puts("  - Each COMMAND can be truncated as long as it is unique, in addition");
    puts("    'config' and 'healthcheck' can be specified by 'cfg' and 'hc' respectively.");
    puts("  - If neither '-dem' is specified, it operates as if '-m' is specified.");
    puts("  - Each occurrence of '-dem' overrides the previous occurrence.");
}


void ignore_manual(){
    puts("ignore manual");
}


void inspect_manual(){
    puts("Usages:");
    puts("  dit inspect [OPTION]... [DIRECTORY]...");
    puts("List information about the files under each specified DIRECTORY in a tree format.");
    putchar('\n');

    puts("Options:");
    puts("  -C, --color              colorize file name to distinguish file types");
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
    puts("  - If no DIRECTORYs are specified, it operates as if the current directory is specified.");
    puts("  - The WORD argument for '--sort' can be truncated and specified as long as it is unique.");
    puts("  - User or group name longer than 8 characters are converted to the corresponding ID,");
    puts("    and the ID longer than 8 digits are converted to '#EXCESS' that means it is undisplayable.");
    puts("  - The units of file size are 'k,M,G,T,P,E,Z', which is powers of 1000.");
    puts("  - Undisplayable characters appearing in the file name are uniformly replaced with '?'.");
    putchar('\n');

    puts("The above options are similar to the 'ls' command which is a GNU one.");
    puts("See that man page for more details.");
}


void label_manual(){
    puts("label manual");
}


void onbuild_manual(){
    puts("onbuild manual");
}


void optimize_manual(){
    puts("optimize manual");
}


void reflect_manual(){
    puts("reflect manual");
}


void setcmd_manual(){
    puts("setcmd manual");
}




/******************************************************************************
    * Help Functions that display each Example
******************************************************************************/


static void __dit_example(){
    puts("dit example");
}


static void __config_example(){
    puts("dit config                 Display the current settings.");
    puts("dit config no-reflect      Replace the settings with 'd=no-reflect h=no-reflect'.");
    puts("dit config d=st,h=no-ig    Replace the settings with 'd=strict h=no-ignore'.");
    puts("dit config -r _3           Reset the settings, and replace the setting of 'h' with 'simple'.");
}


static void __convert_example(){
    puts("convert example");
}


static void __cp_example(){
    puts("cp example");
}


static void __erase_example(){
    puts("erase example");
}


static void __healthcheck_example(){
    puts("healthcheck example");
}


static void __help_example(){
    puts("dit help              Display the detailed manual for the main interface of dit commands.");
    puts("dit help -e inspect   Display the example of use for 'inspect'.");
    puts("dit help -d cfg ig    Display the short description for 'config' and 'ignore' respectively.");
    puts("dit help -a           List all dit commands available.");
}


static void __ignore_example(){
    puts("ignore example");
}


static void __inspect_example(){
    puts("dit inspect -S                 List files under the current directory sorted by their size.");
    puts("dit inspect --sort=ext /dit    List files under '/dit' sorted by their extension.");
    puts("dit inspect -CF /dev           List files under '/dev', decorating their name.");
    puts("dit inspect /bin /sbin         List files under '/bin' and '/sbin' respectively.");
}


static void __label_example(){
    puts("label example");
}


static void __onbuild_example(){
    puts("onbuild example");
}


static void __optimize_example(){
    puts("optimize example");
}


static void __reflect_example(){
    puts("reflect example");
}


static void __setcmd_example(){
    puts("setcmd example");
}