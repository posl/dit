/**
 * @file _help.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'help', that shows information for some dit commands.
 * @author Tsukasa Inada
 * @date 2022/08/09
 */

#include "main.h"

#define HELP_CONTENTS_NUM 3

#define HELP_USAGES_STR "Usages:\n"
#define HELP_OPTIONS_STR "Options:\n"
#define HELP_REMARKS_STR "Remarks:\n"

#define DOCKER_OR_HISTORY  "Dockerfile or history-file"
#define WHEN_REFLECTING  "when reflecting a executed command line"

#define TARGET_OPTION_ARG  "  dockerfile (-d), history-file (-h), both (-dh)\n"
#define EXIT_NORMALLY  ", and exit normally\n"
#define HELP_OPTION_DESC  "display this help" EXIT_NORMALLY

#define CAN_BE_TRUNCATED  "can be truncated as long as it is unique"
#define SPECIFIED_BY_TARGET  "file must be specified explicitly by '-dh' or '--target'"


/** Data type that collects serial numbers of the contents displayed by help function */
typedef enum {
    manual,
    description,
    example
} help_conts;


static int __parse_opts(int argc, char **argv, help_conts *opt);
static void __display_cmd_list();
static int __display_version();

static int __display_help(help_conts code, const char *target);
static void __dit_manual();

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


extern const char * const cmd_reprs[3];




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
    help_conts code;

    if ((i = __parse_opts(argc, argv, &code)))
        return (i < 0) ? 1 : 0;

    const char *target = NULL;
    if ((argc -= optind) > 0){
        argv += optind;
        target = *argv;
    }
    else
        argc = 1;

    char S[] = "\n\n\n";
    do {
        i |= __display_help(code, target);

        if (--argc){
            target = *(++argv);
            S[1] = (code != manual) ? '\0' : '\n';
            fputs(S, stdout);
            fflush(stdout);
        }
        else
            return i;
    } while (1);
}




/**
 * @brief parse optional arguments.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @param[out] opt  variable to store the results of option parse
 * @return int  0 (parse success), 1 (normally exit) or -1 (error exit)
 *
 * @note the arguments are expected to be passed as-is from main function.
 */
static int __parse_opts(int argc, char **argv, help_conts *opt){
    const char *short_opts = "ademV";

    const struct option long_opts[] = {
        { "all",         no_argument, NULL, 'a' },
        { "description", no_argument, NULL, 'd' },
        { "example",     no_argument, NULL, 'e' },
        { "manual",      no_argument, NULL, 'm' },
        { "version",     no_argument, NULL, 'V' },
        { "help",        no_argument, NULL,  1  },
        {  0,             0,           0,    0  }
    };

    *opt = manual;

    int c;
    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) >= 0){
        switch (c){
            case 'a':
                __display_cmd_list();
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
            case 'V':
                return __display_version();
            case 1:
                help_manual();
                return 1;
            default:
                xperror_suggestion(true);
                return -1;
        }
    }
    return 0;
}


/**
 * @brief list all dit commands available.
 *
 * @note display the commands in the same order as 'dit help'.
 * @note array for reordering the commands is in reverse order for looping efficiency.
 */
static void __display_cmd_list(){
    const int cmd_rearange[CMDS_NUM] = {5, 7, 3, 11, 9, 4, 12, 8, 2, 6, 0, 10, 1};

    for (int i = CMDS_NUM; i--;)
        fprintf(stdout, "%s\n", cmd_reprs[cmd_rearange[i]]);
}


/**
 * @brief display the version of this tool.
 *
 * @return int  1 (normally exit) or -1 (error exit)
 */
static int __display_version(){
    FILE *fp;
    if ((fp = fopen(VERSION_FILE, "r"))){
        char S[128];
        while (fgets(S, 128, fp))
            fputs(S, stdout);

        fclose(fp);
        return 1;
    }
    xperror_internal_file();
    return -1;
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
static int __display_help(help_conts code, const char *target){
    const char *topic;
    void (* help_func)();
    int exit_status = 0;

    if (target){
        void (* const cmd_helps[HELP_CONTENTS_NUM][CMDS_NUM])() = {
            {
                config_manual,
                convert_manual,
                cp_manual,
                erase_manual,
                healthcheck_manual,
                help_manual,
                ignore_manual,
                inspect_manual,
                label_manual,
                onbuild_manual,
                optimize_manual,
                reflect_manual,
                setcmd_manual
            },
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
            topic = cmd_reprs[i];
            help_func = cmd_helps[code][i];
        }
        else {
            xperror_invalid_arg('C', i, "dit command", target);
            xperror_suggestion(false);
            exit_status = 1;
        }
    }
    else {
        void (* const dit_helps[HELP_CONTENTS_NUM])() = {
            __dit_manual,
            __dit_description,
            __dit_example
        };
        topic = "dit";
        help_func = dit_helps[code];
    }

    if (! exit_status){
        if (code != manual)
            fprintf(stdout, " < %s >\n", topic);
        help_func();
    }
    return exit_status;
}




/******************************************************************************
    * Help Functions that display each Manual
******************************************************************************/


static void __dit_manual(){
    fputs(
        HELP_USAGES_STR
        "  dit [COMMAND] [ARG]...\n"
        "Use the tool-specific functions corresponding to the specified COMMAND.\n"
        "\n"
        "Commands:\n"
        "main features of this tool:\n"
        "  convert        show how a command line is reflected in "DOCKER_OR_HISTORY"\n"
        "  optimize       do refactoring and optimization on Dockerfile based on its best practices\n"
        "\n"
        "customization of tool settings:\n"
        "  config         set the level of ignoring commands "WHEN_REFLECTING"\n"
        "  ignore         edit set of commands that are ignored "WHEN_REFLECTING"\n"
        "\n"
        "editing Dockerfile:\n"
        "  cp             copy files from the host environment and reflect this as COPY/ADD instructions\n"
        "  label          edit list of LABEL/EXPOSE instructions\n"
        "  setcmd         set CMD/ENTRYPOINT instruction\n"
        "  healthcheck    set HEALTHCHECK instruction\n"
        "  onbuild        append ONBUILD instructions\n"
        "\n"
        "utilitys:\n"
        "  reflect        append the contents of some files to "DOCKER_OR_HISTORY"\n"
        "  erase          delete some lines from "DOCKER_OR_HISTORY"\n"
        "  inspect        show some directory trees with details about each file\n"
        "  help           show information for some dit commands\n"
        "\n"
        "See 'dit help [OPTION]... [COMMAND]...' for details.\n"
    , stdout);
}


void config_manual(){
    fputs(
        HELP_USAGES_STR
        "  dit config [OPTION]... [MODE[,MODE]...]\n"
        "Set the level at which commands that should not be reflected are ignored, used\n"
        WHEN_REFLECTING" in "DOCKER_OR_HISTORY", individually.\n"
        "\n"
        HELP_OPTIONS_STR
        "  -r, --reset    reset each level with default value\n"
        "      --help     " HELP_OPTION_DESC
        "\n"
        "Modes:\n"
        "   0,  no-reflect    in the first place, do not reflect\n"
        "   1,  strict        ignore all of them, strictly\n"
        "   2,  normal        ignore them while respecting the dependencies between commands (default)\n"
        "   3,  simple        ignore them only if the command line contains only one command\n"
        "   4,  no-ignore     ignore nothing\n"
        "\n"
        HELP_REMARKS_STR
        "  - If neither OPTION nor MODE is specified, display the current settings.\n"
        "  - To specify a mode, you can use the above serial numbers and strings\n"
        "    and any of the strings "CAN_BE_TRUNCATED".\n"
        "  - If you specify an underscore instead of a mode, the current settings is inherited.\n"
        "  - Each MODE is one of the following formats and targets the files listed on the right.\n"
        "      <mode>           both files\n"
        "      [bdh]=<mode>     'b' (both files), 'd' (Dockerfile), 'h' (history-file)\n"
        "      [0-4_][0-4_]     first character (Dockerfile), second character (history-file)\n"
    , stdout);
}


void convert_manual(){
    fputs("convert manual\n", stdout);
}


void cp_manual(){
    fputs("cp manual\n", stdout);
}


void erase_manual(){
    fputs(
        HELP_USAGES_STR
        "  dit erase [OPTION]...\n"
        "Delete the lines that match the specified conditions from "DOCKER_OR_HISTORY".\n"
        "\n"
        "Options for Deleation:\n"
        "  -B, --begin-with=STR        delete the lines starting with STR\n"
        "  -C, --contain=STR           delete the lines containing STR\n"
        "  -L, --lines=ARG[,ARG]...    delete the lines with the number specified by ARGs:\n"
        "                                NUM (unique specification), [NUM]-[NUM] (range specification)\n"
        "  -Z, --undoes[=NUM]          delete the lines added within the last NUM (default is 1) times\n"
        "\n"
        "Options for Behavior:\n"
        "  -d                          delete from Dockerfile\n"
        "  -h                          delete from history-file\n"
        "      --target=FILE           determine the target file:\n"
        "                              " TARGET_OPTION_ARG
        "  -i, --ignore-case           ignore case distinctions in the STR arguments and data\n"
        "  -m, --max-count=NUM         delete at most NUM lines, counting from the most recently added\n"
        "  -r, --reset                 reset the internal log-file\n"
        "  -v, --verbose               display deleted lines\n"
        "  -y                          eliminate the confirmation work before deletion\n"
        "      --assume=Y/n            set the answer before the confirmation work\n"
        "      --help                  " HELP_OPTION_DESC
        "\n"
        HELP_REMARKS_STR
        "  - If no Options for Deleation are given, it behaves as if '-Z' is given.\n"
        "  - The line numbers for the '-L' option start from 1, and 0 is the same as specifying nothing.\n"
        "  - If NUM is omitted in the range specification of the '-L' option, it is interpreted as\n"
        "    specifying the line number representing the beginning or end depending on the position."
        "  - The FILE argument for '--target' "CAN_BE_TRUNCATED".\n"
        "  - The target "SPECIFIED_BY_TARGET".\n"
        "  - The '-Z' option uses the internal log-files that record the number of reflected lines, and\n"
        "    if there is an inconsiestency between one of that files and the target file, it is reset.\n"
        "  - Above log-files are not saved across interruptions such as exiting the container.\n"
        "  - By default, Y/n confirmation is performed using standard error output and standard input\n"
        "    as to whether it is okay to delete lines that match the specified conditions, and if you\n"
        "    answer 'Yes', delete all of the lines, if you answer 'No', delete the selected lines.\n"
    , stdout);
}


void healthcheck_manual(){
    fputs("healthcheck manual\n", stdout);
}


void help_manual(){
    fputs(
        HELP_USAGES_STR
        "  dit help [OPTION]... [COMMAND]...\n"
        "Show requested information for each specified dit COMMAND.\n"
        "\n"
        HELP_OPTIONS_STR
        "  -a, --all            list all dit commands available" EXIT_NORMALLY
        "  -d, --description    show the short descriptions\n"
        "  -e, --example        show the examples of use\n"
        "  -m, --manual         show the detailed manuals\n"
        "  -V, --version        display the version of this tool" EXIT_NORMALLY
        "      --help           " HELP_OPTION_DESC
        "\n"
        HELP_REMARKS_STR
        "  - If no COMMANDs are specified, show information about the main interface of dit commands.\n"
        "  - Each COMMAND "CAN_BE_TRUNCATED", in addition\n"
        "    'config' and 'healthcheck' can be specified by 'cfg' and 'hc' respectively.\n"
        "  - When neither '-dem' is given, it behaves as if '-m' is given.\n"
    , stdout);
}


void ignore_manual(){
    fputs("ignore manual\n", stdout);
}


void inspect_manual(){
    fputs(
        HELP_USAGES_STR
        "  dit inspect [OPTION]... [DIRECTORY]...\n"
        "List information about the files under each specified DIRECTORY in a tree format.\n"
        "\n"
        HELP_OPTIONS_STR
        "  -C, --color              colorize file name to distinguish file types\n"
        "  -F, --classify           append indicator (one of '*/=|') to each file name:\n"
        "                             to executable file, directory, socket or fifo, in order\n"
        "  -n, --numeric-uid-gid    list the corresponding IDs instead of user or group name\n"
        "  -S                       sort by file size, largest first\n"
        "  -X                       sort by file extension, alphabetically\n"
        "      --sort=WORD          replace file sorting method:\n"
        "                             name (default), size (-S), extension (-X)\n"
        "      --help               " HELP_OPTION_DESC
        "\n"
        HELP_REMARKS_STR
        "  - If no DIRECTORYs are specified, it operates as if the current directory is specified.\n"
        "  - The WORD argument for '--sort' "CAN_BE_TRUNCATED".\n"
        "  - User or group name longer than 8 characters are converted to the corresponding ID, and\n"
        "    the ID longer than 8 digits are converted to '#EXCESS' that means it is undisplayable.\n"
        "  - The units of file size are 'k,M,G,T,P,E,Z', which is powers of 1000.\n"
        "  - Undisplayable characters appearing in the file name are uniformly replaced with '?'.\n"
        "  - If standard output is not connected to a terminal, each file name is not colorized.\n"
        "\n"
        "This command is based on the 'ls' command which is a GNU one.\n"
        "See that man page for details.\n"
    , stdout);
}


void label_manual(){
    fputs("label manual\n", stdout);
}


void onbuild_manual(){
    fputs("onbuild manual\n", stdout);
}


void optimize_manual(){
    fputs("optimize manual\n", stdout);
}


void reflect_manual(){
    fputs(
        HELP_USAGES_STR
        "  dit reflect [OPTION]... [SOURCE]...\n"
        "Append the contents of each specified SOURCE to "DOCKER_OR_HISTORY".\n"
        "\n"
        HELP_OPTIONS_STR
        "  -d                   append to Dockerfile\n"
        "  -h                   append to history-file\n"
        "      --target=DEST    determine destination file:\n"
        "                       " TARGET_OPTION_ARG
        "  -p                   leave repeated empty output lines as they are\n"
        "  -s                   suppress repeated empty output lines\n"
        "      --blank=WORD     replace how to handle repeated empty output lines:\n"
        "                         ignore (default), preserve (-p), squeeze (-s)\n"
        "      --help           " HELP_OPTION_DESC
        "\n"
        HELP_REMARKS_STR
        "  - If no SOURCEs are specified, it uses the internal files generated by 'convert' command.\n"
        "  - If '-' is specified as SOURCE, it read standard input until an EOF is entered.\n"
        "  - The argument for '--target' or '--blank' "CAN_BE_TRUNCATED".\n"
        "  - Destination "SPECIFIED_BY_TARGET".\n"
        "  - If both files are destinations, only the internal files above can be used.\n"
        "  - If the size of destination file reaches the upper limit (2G), it may exit with the error.\n"
        "  - When reflecting in Dockerfile, each instruction must be on one line and\n"
        "    the line to be reflected must not contain FROM and MAINTAINER instructions.\n"
        "  - Dockerfile is adjusted so that each of CMD and ENTRYPOINT instructions is one or less.\n"
        "  - Internally, logging such as the number of reflected lines is performed.\n"
    , stdout);
}


void setcmd_manual(){
    fputs("setcmd manual\n", stdout);
}




/******************************************************************************
    * Help Functions that display each Description
******************************************************************************/


static void __dit_description(){
    fputs(
        "Use the tool-specific functions as the subcommand.\n"
    , stdout);
}

static void __config_description(){
    fputs(
        "Set the level at which commands are reflected in "DOCKER_OR_HISTORY", individually.\n"
    , stdout);
}

static void __convert_description(){
    fputs(
        "Show how a command line is transformed for reflection in the "DOCKER_OR_HISTORY".\n"
    , stdout);
}

static void __cp_description(){
    fputs(
        "Perform the processing equivalent to COPY/ADD instructions and reflect this in Dockerfile.\n"
    , stdout);
}

static void __erase_description(){
    fputs(
        "Delete the lines that match some conditions from "DOCKER_OR_HISTORY".\n"
    , stdout);
}

static void __healthcheck_description(){
    fputs(
        "Set HEALTHCHECK instruction in Dockerfile.\n"
    , stdout);
}

static void __help_description(){
    fputs(
        "Show requested information for some dit commands.\n"
    , stdout);
}

static void __ignore_description(){
    fputs(
        "Edit set of commands that should not be reflected in "DOCKER_OR_HISTORY", individually.\n"
    , stdout);
}

static void __inspect_description(){
    fputs(
        "List information about the files under some directories in a tree format.\n"
    , stdout);
}

static void __label_description(){
    fputs(
        "Edit list of LABEL/EXPOSE instructions in Dockerfile.\n"
    , stdout);
}

static void __onbuild_description(){
    fputs(
        "Append ONBUILD instructions in Dockerfile.\n"
    , stdout);
}

static void __optimize_description(){
    fputs(
        "Generate Dockerfile as the result of refactoring and optimization based on its best practices.\n"
    , stdout);
}

static void __reflect_description(){
    fputs(
        "Append the contents of some files to "DOCKER_OR_HISTORY".\n"
    , stdout);
}

static void __setcmd_description(){
    fputs(
        "Set CMD/ENTRYPOINT instruction in Dockerfile.\n"
    , stdout);
}




/******************************************************************************
    * Help Functions that display each Example
******************************************************************************/


static void __dit_example(){
    fputs("dit example\n", stdout);
}


static void __config_example(){
    fputs(
        "dit config                 Display the current settings.\n"
        "dit config no-reflect      Replace the settings with 'd=no-reflect h=no-reflect'.\n"
        "dit config d=st,h=no-ig    Replace the settings with 'd=strict h=no-ignore'.\n"
        "dit config -r _3           Reset the settings, and replace the setting of 'h' with 'simple'.\n"
    , stdout);
}


static void __convert_example(){
    fputs("convert example\n", stdout);
}


static void __cp_example(){
    fputs("cp example\n", stdout);
}


static void __erase_example(){
    fputs(
        "dit erase -dh                            Delete the lines added just before.\n"
        "dit erase -d -L -                        Delete all lines from Dockerfile.\n"
        "dit erase --cont=cat --targ=hist         Delete unnecessary 'cat' commands from history-file.\n"
        "dit erase -divy -BONBUILD > erase.out    Extract ONBUILD instructions from Dockerfile.\n"
    , stdout);
}


static void __healthcheck_example(){
    fputs("healthcheck example\n", stdout);
}


static void __help_example(){
    fputs(
        "dit help              Display the detailed manual for the main interface of dit commands.\n"
        "dit help -e inspect   Display the example of use for 'inspect'.\n"
        "dit help -d cfg ig    Display the short description for 'config' and 'ignore' respectively.\n"
        "dit help -a           List all dit commands available.\n"
    , stdout);
}


static void __ignore_example(){
    fputs("ignore example\n", stdout);
}


static void __inspect_example(){
    fputs(
        "dit inspect -S                 List files under the current directory sorted by their size.\n"
        "dit inspect --sort=ext /dit    List files under '/dit' sorted by their extension.\n"
        "dit inspect -CF /dev           List files under '/dev', decorating their name.\n"
        "dit inspect /bin /sbin         List files under '/bin' and '/sbin' respectively.\n"
    , stdout);
}


static void __label_example(){
    fputs("label example\n", stdout);
}


static void __onbuild_example(){
    fputs("onbuild example\n", stdout);
}


static void __optimize_example(){
    fputs("optimize example\n", stdout);
}


static void __reflect_example(){
    fputs(
        "dit reflect                 Error in noraml use, but used internally for logging.\n"
        "dit reflect -d erase.out    Reflect the contents of './erase.out' in Dockerfile.\n"
        "dit reflect -hs -           Reflect the input contents in history-file while suqueezing blanks.\n"
        "dit reflect --target b      Reflect the output contents of the previous 'convert' command.\n"
    , stdout);
}


static void __setcmd_example(){
    fputs("setcmd example\n", stdout);
}