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
#define WHEN_REFLECTING  "when reflecting an executed command line"

#define TARGET_OPTION_ARGS  "  dockerfile (-d), history-file (-h), both (-dh)\n"
#define BLANK_OPTION_DESC  "replace how to handle the empty lines:\n"

#define EXIT_NORMALLY  ", and exit normally\n"
#define HELP_OPTION_DESC  "display this help" EXIT_NORMALLY

#define CAN_BE_TRUNCATED  "can be truncated as long as it is unique"
#define CASE_INSENSITIVE  "without regard to case"
#define SPECIFIED_BY_TARGET  "file must be specified explicitly by '-dh' or '--target'"


/** Data type that collects serial numbers of the contents displayed by help function */
typedef enum {
    manual,
    description,
    example
} help_conts;


static int parse_opts(int argc, char **argv, help_conts *opt);
static void display_cmd_list(void);
static int display_version(void);

static bool display_help(help_conts code, const char *target);
static void dit_manual(void);

static void dit_description(void);
static void cmd_description(void);
static void config_description(void);
static void convert_description(void);
static void copy_description(void);
static void erase_description(void);
static void healthcheck_description(void);
static void help_description(void);
static void ignore_description(void);
static void inspect_description(void);
static void label_description(void);
static void onbuild_description(void);
static void optimize_description(void);
static void package_description(void);
static void reflect_description(void);

static void dit_example(void);
static void cmd_example(void);
static void config_example(void);
static void convert_example(void);
static void copy_example(void);
static void erase_example(void);
static void healthcheck_example(void);
static void help_example(void);
static void ignore_example(void);
static void inspect_example(void);
static void label_example(void);
static void onbuild_example(void);
static void optimize_example(void);
static void package_example(void);
static void reflect_example(void);


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
    int i, exit_status = SUCCESS;
    help_conts code;

    if ((i = parse_opts(argc, argv, &code))){
        if (i < 0){
            exit_status = FAILURE;
            xperror_suggestion(true);
        }
        return exit_status;
    }

    const char *target = NULL;
    char newlines[] = "\n\n\n";

    if ((argc -= optind) > 0){
        argv += optind;
        target = *argv;
    }
    else
        argc = 1;

    do {
        if (! display_help(code, target))
            exit_status = FAILURE;

        assert(argc > 0);
        if (--argc){
            target = *(++argv);
            newlines[1] = (code != manual) ? '\0' : '\n';
            fputs(newlines, stdout);
        }
        else
            return exit_status;
    } while (true);
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
static int parse_opts(int argc, char **argv, help_conts *opt){
    assert(opt);

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
    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) >= 0)
        switch (c){
            case 'a':
                display_cmd_list();
                return NORMALLY_EXIT;
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
                return display_version();
            case 1:
                help_manual();
                return NORMALLY_EXIT;
            default:
                return ERROR_EXIT;
        }

    return SUCCESS;
}


/**
 * @brief list all dit commands available.
 *
 * @note display the commands in the same order as 'dit help'.
 */
static void display_cmd_list(void){
    const int cmd_rearange[] = {
        DIT_CONVERT,
        DIT_OPTIMIZE,
        DIT_CONFIG,
        DIT_IGNORE,
        DIT_PACKAGE,
        DIT_COPY,
        DIT_LABEL,
        DIT_CMD,
        DIT_HEALTHCHECK,
        DIT_ONBUILD,
        DIT_REFLECT,
        DIT_ERASE,
        DIT_INSPECT,
        DIT_HELP,
            -1
    };

    for (const int *p_id = cmd_rearange; *p_id >= 0; p_id++){
        assert(*p_id < CMDS_NUM);
        fprintf(stdout, "%s\n", cmd_reprs[*p_id]);
    }
}


/**
 * @brief display the version of this tool on screen.
 *
 * @return int  1 (normally exit) or -1 (error exit)
 */
static int display_version(void){
    const char *line;
    int errid = 0, exit_status = NORMALLY_EXIT;

    while ((line = xfgets_for_loop(VERSION_FILE, NULL, &errid)))
        fprintf(stdout, "%s\n", line);

    if (errid){
        xperror_internal_file();
        exit_status = ERROR_EXIT;
    }
    return exit_status;
}




/**
 * @brief display requested information for the corresponding dit command on screen.
 *
 * @param[in]  code  serial number identifying the display content
 * @param[in]  target  target string
 * @return bool  successful or not
 */
static bool display_help(help_conts code, const char *target){
    assert((code >= 0) && (code < HELP_CONTENTS_NUM));

    const char *topic;
    void (* help_func)(void);

    if (target){
        void (* const cmd_helps[HELP_CONTENTS_NUM][CMDS_NUM])(void) = {
            {
                cmd_manual,
                config_manual,
                convert_manual,
                copy_manual,
                erase_manual,
                healthcheck_manual,
                help_manual,
                ignore_manual,
                inspect_manual,
                label_manual,
                onbuild_manual,
                optimize_manual,
                package_manual,
                reflect_manual
            },
            {
                cmd_description,
                config_description,
                convert_description,
                copy_description,
                erase_description,
                healthcheck_description,
                help_description,
                ignore_description,
                inspect_description,
                label_description,
                onbuild_description,
                optimize_description,
                package_description,
                reflect_description
            },
            {
                cmd_example,
                config_example,
                convert_example,
                copy_example,
                erase_example,
                healthcheck_example,
                help_example,
                ignore_example,
                inspect_example,
                label_example,
                onbuild_example,
                optimize_example,
                package_example,
                reflect_example
            }
        };

        int i;
        if ((i = receive_expected_string(target, cmd_reprs, CMDS_NUM, 2)) >= 0){
            assert(i < CMDS_NUM);
            topic = cmd_reprs[i];
            help_func = cmd_helps[code][i];
        }
        else {
            xperror_invalid_arg('C', i, "dit command", target);
            xperror_suggestion(false);
            return false;
        }
    }
    else {
        void (* const dit_helps[HELP_CONTENTS_NUM])(void) = {
            dit_manual,
            dit_description,
            dit_example
        };

        topic = "dit";
        help_func = dit_helps[code];
    }

    if (code != manual)
        fprintf(stdout, " < %s >\n", topic);
    help_func();

    return true;
}




/******************************************************************************
    * Help Functions that display each Manual
******************************************************************************/


static void dit_manual(void){
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
        "  package        install packages in an optimized manner and reflect this as a RUN instruction\n"
        "  copy           copy files from the host environment and reflect this as COPY/ADD instructions\n"
        "  label          edit list of LABEL/EXPOSE instructions\n"
        "  cmd            set a CMD/ENTRYPOINT instruction\n"
        "  healthcheck    set a HEALTHCHECK instruction\n"
        "  onbuild        append ONBUILD instructions\n"
        "\n"
        "utilities:\n"
        "  reflect        append the contents of some files to "DOCKER_OR_HISTORY"\n"
        "  erase          delete some lines from "DOCKER_OR_HISTORY"\n"
        "  inspect        show some directory trees with details about each file\n"
        "  help           show information for some dit commands\n"
        "\n"
        "See 'dit help [OPTION]... [COMMAND]...' for details.\n"
    , stdout);
}


void cmd_manual(void){
    fputs(
        HELP_USAGES_STR
        "  dit cmd [OPTION]...\n"
    , stdout);
}


void config_manual(void){
    fputs(
        HELP_USAGES_STR
        "  dit config [OPTION]... [MODE[,MODE]...]\n"
        "Set the level at which commands that should not be reflected are ignored, used\n"
        WHEN_REFLECTING" in "DOCKER_OR_HISTORY", individually.\n"
        "\n"
        HELP_OPTIONS_STR
        "  -r, --reset    reset each level with the default value\n"
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
        "  - If you specify an underscore instead of a mode, the current setting is inherited.\n"
        "  - Each MODE is one of the following formats and targets the files listed on the right.\n"
        "      <mode>           both files\n"
        "      [bdh]=<mode>     'b' (both files), 'd' (Dockerfile), 'h' (history-file)\n"
        "      [0-4_][0-4_]     first character (Dockerfile), second character (history-file)\n"
    , stdout);
}


void convert_manual(void){
    fputs(
        HELP_USAGES_STR
        "  dit convert [OPTION]...\n"
    , stdout);
}


void copy_manual(void){
    fputs(
        HELP_USAGES_STR
        "  dit copy [OPTION]...\n"
    , stdout);
}


void erase_manual(void){
    fputs(
        HELP_USAGES_STR
        "  dit erase [OPTION]...\n"
        "Delete the lines that match the specified conditions from "DOCKER_OR_HISTORY".\n"
        "\n"
        "Options for Deletion:\n"
        "  -E, --extended-regexp=PTN     delete the lines containing extended regular expression pattern\n"
        "  -N, --numbers=ARG[,ARG]...    delete the lines with the numbers specified by ARGs:\n"
        "                                  NUM (unique specification), [NUM]-[NUM] (range specification)\n"
        "  -Z, --undoes[=NUM]            delete the lines added within the last NUM (1 by default) times\n"
        "\n"
        "Options for Behavior:\n"
        "  -d                            delete from Dockerfile\n"
        "  -h                            delete from history-file\n"
        "      --target=FILE             determine the target file:\n"
        "                                " TARGET_OPTION_ARGS
        "  -i, --ignore-case             ignore case distinctions in the STR arguments and data\n"
        "  -m, --max-count=NUM           delete at most NUM lines, counting from the most recently added\n"
        "  -r, --reset                   reset the internal log-files\n"
        "  -s                            suppress repeated empty lines\n"
        "  -t                            truncate all empty lines\n"
        "      --blank=WORD              " BLANK_OPTION_DESC
        "                                  preserve (default), squeeze (-s), truncate (-t)\n"
        "  -v, --verbose                 display deleted lines\n"
        "  -y                            eliminate the confirmation before deletion\n"
        "      --assume=Y/n              set the answer to the confirmation before deletion:\n"
        "                                  YES (-y), NO (delete selected lines), QUIT (stop deleting)\n"
        "      --help                    " HELP_OPTION_DESC
        "\n"
        "Remarks about Deletion:\n"
        "  - When no options are given for deletion including '-st' or '--blank', if '-v' is given, it\n"
        "    shows the previous deleted lines and exit normally, otherwise it behaves as if '-Z' is given.\n"
        "  - When multiple Options for Deletion are given, the specified conditions are ANDed together.\n"
        "  - The line numbers for '-N' start from 1, and 0 is the same as specifying nothing.\n"
        "  - In the range specification of '-N', if nothing is specified for NUM, the first or last\n"
        "    line number will be assigned depending on the position, and if left NUM is greater than\n"
        "    right NUM, it is interpreted as two range specifications, each without NUM on one side.\n"
        "  - The internal log-files that record the number of reflected lines are used by '-Z',\n"
        "    and if there is an inconsistency between one of that files and the target file,\n"
        "    it resets the files that had the problem and behaves as if '-Z' had not been given.\n"
        "  - Information that the number of reflected lines is 0 is not retained in that log-files,\n"
        "    and '-Z' counts the timing when some lines are added to the target file as one time.\n"
        "  - The internal log-files are not saved across interruptions such as exiting the container.\n"
        "\n"
        "Remarks about Behavior:\n"
        "  - The argument for '--target' or '--blank' "CAN_BE_TRUNCATED".\n"
        "  - The target "SPECIFIED_BY_TARGET".\n"
        "  - The deletion of empty lines is not performed unless '-st' or '--blank' is given, and always\n"
        "    precedes the other deletion when one of the options is given, except when an error occurs.\n"
        "  - The argument for '--assume' "CAN_BE_TRUNCATED" "CASE_INSENSITIVE".\n"
        "  - By default, Y/n confirmation is performed using standard error output and standard input\n"
        "    as to whether it is okay to delete all the lines that match the specified conditions.\n"
        "  - If you answer 'YES' to above confirmation, delete all the lines, if you answer 'NO', delete\n"
        "    lines you select in the same way as specifying the line numbers with '-N', and if you answer\n"
        "    'QUIT', stop deleting lines for which above confirmation has not yet been completed.\n"
        "  - If the answer to above confirmation is set, it skips that step and perform the same process.\n"
        "\n"
        "We take no responsibility for using regular expression pattern that uses excessive resources.\n"
        "See man page of 'REGEX' for details.\n"
    , stdout);
}


void healthcheck_manual(void){
    fputs(
        HELP_USAGES_STR
        "  dit healthcheck [OPTION]...\n"
    , stdout);
}


void help_manual(void){
    fputs(
        HELP_USAGES_STR
        "  dit help [OPTION]... [COMMAND]...\n"
        "Show requested information for each specified dit COMMAND.\n"
        "\n"
        HELP_OPTIONS_STR
        "  -a, --all            list all dit commands available" EXIT_NORMALLY
        "  -d, --description    show the short descriptions\n"
        "  -e, --example        show the examples of use\n"
        "  -m, --manual         show the detailed manuals (default)\n"
        "  -V, --version        display the version of this tool" EXIT_NORMALLY
        "      --help           " HELP_OPTION_DESC
        "\n"
        HELP_REMARKS_STR
        "  - If no COMMANDs are specified, show information about the main interface of the dit commands.\n"
        "  - Each COMMAND "CAN_BE_TRUNCATED".\n"
    , stdout);
}


void ignore_manual(void){
    fputs(
        HELP_USAGES_STR
        "  dit ignore [OPTION]... [NAME]...\n"
        "  dit ignore [OPTION]... -A NAME [SHORT_OPTS [LONG_OPTS]...] [OPTARG]... [FIRST_ARG]...\n"
        "Edit set of commands that should not be reflected and the conditions for ignoring each element,\n"
        "used "WHEN_REFLECTING" in "DOCKER_OR_HISTORY", individually.\n"
        "\n"
        HELP_OPTIONS_STR
        "  -d                            edit the settings when reflecting in Dockerfile\n"
        "  -h                            edit the settings when reflecting in history-file\n"
        "      --target=FILE             determine the target file:\n"
        "                                " TARGET_OPTION_ARGS
        "  -n, --unset                   remove the setting for each specified NAME\n"
        "  -p, --print                   display the current of default settings without any editing\n"
        "  -r, --reset                   reset the settings\n"
        "  -A, --additional-settings     accept the specification of the conditions for ignoring\n"
        "      --same-as-nothing=TEXT    replace the string meaning no arguments:\n"
        "                                  NONE (default), used in OPTARG or FIRST_ARG\n"
        "      --help                    " HELP_OPTION_DESC
        "\n"
        "Conditions:\n"
        "  SHORT_OPTS    specify short options the same as the first argument of 'getopts' except that:\n"
        "                  - Optstring cannot start with ':' and must not contain '='.\n"
        "  LONG_OPTS     specify long options the same as SHORT_OPTS except that:\n"
        "                  - Options with no arguments must be split using multiple positional arguments.\n"
        "  OPTARG        specify the argument to be specified for an option as follows:\n"
        "                  - Specify by a string containing one or more '='s that doesn't start with '='.\n"
        "                  - When separating the strings with '=', the first element is an option,\n"
        "                    the last element is its argument, and the others are aliases of the option.\n"
        "                  - A condition that the option has no arguments can be also specified.\n"
        "  FIRST_ARG     specify the first non-optional argument as follows:\n"
        "                  - Specify by a string that does not contain '='.\n"
        "                  - A condition that there are no non-optional arguments can be also specified.\n"
        "\n"
        HELP_REMARKS_STR
        "  - If no NAMEs are specified and '-r' is not given, it behaves as if '-p' is given.\n"
        "  - If the number of the non-optional arguments is less than 2, '-A' makes no sense, and\n"
        "    if either of '-np' is given, the non-optional arguments other than NAME make no sense.\n"
        "  - The argument for '--target' "CAN_BE_TRUNCATED".\n"
        "  - The target "SPECIFIED_BY_TARGET".\n"
        "  - When appending the setting, it always overwrites the previous setting.\n"
        "  - When '-p' is given, '-r' toggles between displaying the current and default settings,\n"
        "    and NAMEs narrow down the settings to be displayed to only those specified.\n"
        "  - The argument for '--same-as-nothing' can be any string that does not\n"
        "    contain '=', and the string to be specified here is case insensitive.\n"
        "  - When specifying LONG_OPTSs, it must come after specifying one SHORT_OPTS, so even if\n"
        "    you don't want to specify SHORT_OPTS, you must specify an empty string instead.\n"
        "  - OPTARG is distinguished from other conditions by whether or not it contains '='.\n"
        "  - Aliases of the option in OPTARG are used when you want to define a short option and a\n"
        "    long option that have the same meaning, or when you want to give an alias that does not\n"
        "    match any options to a long option that requires specifying many arguments as conditions.\n"
        "  - If you want to specify FIRST_ARG without specifying OPTARG, use one '=' instead of OPTARG.\n"
        "  - The settings here are recorded in the ignore-file in json format, and as long as you use this\n"
        "    command, no invalid, incorrect or meaningless settings will be recorded, but even if you edit\n"
        "    the file in another way, the contents will be checked so that no problems will occur.\n"
    , stdout);
}


void inspect_manual(void){
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
        "  - The argument for '--sort' "CAN_BE_TRUNCATED".\n"
        "  - If standard output is not connected to a terminal, each file name is not colorized.\n"
        "  - User or group name longer than 8 characters are converted to the corresponding ID, and\n"
        "    the ID longer than 8 digits are converted to '#EXCESS' that means it is undisplayable.\n"
        "  - The units of file size are 'k,M,G,T,P,E,Z', which is powers of 1000.\n"
        "  - Undisplayable characters appearing in the file name are uniformly replaced with '?'.\n"
        "\n"
        "This command is based on the 'ls' command which is a GNU one.\n"
        "See that man page for details.\n"
    , stdout);
}


void label_manual(void){
    fputs(
        HELP_USAGES_STR
        "  dit label [OPTION]...\n"
    , stdout);
}


void onbuild_manual(void){
    fputs(
        HELP_USAGES_STR
        "  dit onbuild [OPTION]...\n"
    , stdout);
}


void optimize_manual(void){
    fputs(
        HELP_USAGES_STR
        "  dit optimize [OPTION]...\n"
    , stdout);
}


void package_manual(void){
    fputs(
        HELP_USAGES_STR
        "  dit package [OPTION]...\n"
    , stdout);
}


void reflect_manual(void){
    fputs(
        HELP_USAGES_STR
        "  dit reflect [OPTION]... [SOURCE]...\n"
        "Append the contents of each specified SOURCE to "DOCKER_OR_HISTORY".\n"
        "\n"
        HELP_OPTIONS_STR
        "  -d                   append to Dockerfile\n"
        "  -h                   append to history-file\n"
        "      --target=DEST    determine destination file:\n"
        "                       " TARGET_OPTION_ARGS
        "  -p                   leave the empty lines as they are\n"
        "  -s                   suppress repeated empty lines\n"
        "      --blank=WORD     " BLANK_OPTION_DESC
        "                         preserve (-p), squeeze (-s), truncate (default)\n"
        "  -v, --verbose        display reflected lines\n"
        "      --help           " HELP_OPTION_DESC
        "\n"
        HELP_REMARKS_STR
        "  - If no SOURCEs are specified, it uses the results of the previous dit command 'convert'.\n"
        "  - If '-' is specified as SOURCE, it read standard input until 'EOF' is entered.\n"
        "  - The argument for '--target' or '--blank' "CAN_BE_TRUNCATED".\n"
        "  - Destination "SPECIFIED_BY_TARGET".\n"
        "  - If both files are destination, the reflection contents cannot be specified by SOURCEs.\n"
        "  - If the size of destination file exceeds the upper limit (2G), it exits with the error.\n"
        "  - When reflecting in Dockerfile, each instruction must be on one line.\n"
        "  - Dockerfile is adjusted so that each of CMD and ENTRYPOINT instructions is 1 or less.\n"
        "  - Internally, logging such as the number of reflected lines is performed.\n"
    , stdout);
}




/******************************************************************************
    * Help Functions that display each Description
******************************************************************************/


static void dit_description(void){
    fputs(
        "Use the tool-specific functions as the subcommand.\n"
    , stdout);
}

static void cmd_description(void){
    fputs(
        "Set CMD/ENTRYPOINT instruction in Dockerfile.\n"
    , stdout);
}

static void config_description(void){
    fputs(
        "Set the level at which commands are reflected in "DOCKER_OR_HISTORY", individually.\n"
    , stdout);
}

static void convert_description(void){
    fputs(
        "Show how a command line is transformed for reflection in the "DOCKER_OR_HISTORY".\n"
    , stdout);
}

static void copy_description(void){
    fputs(
        "Perform the processing equivalent to COPY/ADD instructions and reflect this in Dockerfile.\n"
    , stdout);
}

static void erase_description(void){
    fputs(
        "Delete the lines that match some conditions from "DOCKER_OR_HISTORY".\n"
    , stdout);
}

static void healthcheck_description(void){
    fputs(
        "Set HEALTHCHECK instruction in Dockerfile.\n"
    , stdout);
}

static void help_description(void){
    fputs(
        "Show requested information for some dit commands.\n"
    , stdout);
}

static void ignore_description(void){
    fputs(
        "Edit set of commands that should not be reflected in "DOCKER_OR_HISTORY", individually.\n"
    , stdout);
}

static void inspect_description(void){
    fputs(
        "List information about the files under some directories in a tree format.\n"
    , stdout);
}

static void label_description(void){
    fputs(
        "Edit list of LABEL/EXPOSE instructions in Dockerfile.\n"
    , stdout);
}

static void onbuild_description(void){
    fputs(
        "Append ONBUILD instructions in Dockerfile.\n"
    , stdout);
}

static void optimize_description(void){
    fputs(
        "Generate Dockerfile as the result of refactoring and optimization based on its best practices.\n"
    , stdout);
}

static void package_description(void){
    fputs(
        "Perform the package installation in an optimized manner and reflect this in Dockerfile.\n"
    , stdout);
}

static void reflect_description(void){
    fputs(
        "Append the contents of some files to "DOCKER_OR_HISTORY".\n"
    , stdout);
}




/******************************************************************************
    * Help Functions that display each Example
******************************************************************************/


static void dit_example(void){
    fputs(
        "dit package  \n"
        "dit copy     \n"
        "dit erase    \n"
        "dit optimize \n"
    , stdout);
}


static void cmd_example(void){
    fputs(
        "dit cmd \n"
        "dit cmd \n"
        "dit cmd \n"
        "dit cmd \n"
    , stdout);
}


static void config_example(void){
    fputs(
        "dit config                 Display the current settings.\n"
        "dit config no-reflect      Replace the settings with 'd=no-reflect h=no-reflect'.\n"
        "dit config d=st,h=no-ig    Replace the settings with 'd=strict h=no-ignore'.\n"
        "dit config -r _3           Reset the settings, and replace the setting of 'h' with 'simple'.\n"
    , stdout);
}


static void convert_example(void){
    fputs(
        "dit convert \n"
        "dit convert \n"
        "dit convert \n"
        "dit convert \n"
    , stdout);
}


static void copy_example(void){
    fputs(
        "dit copy \n"
        "dit copy \n"
        "dit copy \n"
        "dit copy \n"
    , stdout);
}


static void erase_example(void){
    fputs(
        "dit erase -dh                              Delete the lines added just before.\n"
        "dit erase -diy -E '^ONBUILD[[:space:]]'    Delete all ONBUILD instructions from Dockerfile.\n"
        "dit erase -hm10 -N -                       Delete last 10 lines from history-file.\n"
        "dit erase -v --target both                 Display the previous deleted lines.\n"
    , stdout);
}


static void healthcheck_example(void){
    fputs(
        "dit healthcheck \n"
        "dit healthcheck \n"
        "dit healthcheck \n"
        "dit healthcheck \n"
    , stdout);
}


static void help_example(void){
    fputs(
        "dit help              Display the detailed manual for the main interface of the dit commands.\n"
        "dit help -e inspect   Display the example of use for 'inspect'.\n"
        "dit help -d cfg ig    Display the short description for 'config' and 'ignore' respectively.\n"
        "dit help -a           List all dit commands available.\n"
    , stdout);
}


static void ignore_example(void){
    fputs(
        "dit ignore -d diff vim             Prevent 'diff' and 'vim' from being reflected in Dockerfile.\n"
        "dit ignore -hn declare             Make 'declare' a command that is reflected in history-file.\n"
        "dit ignore -dhA export p = none    Set the detailed conditions for ignoring 'export'.\n"
        "dit ignore -dhpr sed awk           Display the default ignore settings for 'sed' and 'awk'.\n"
    , stdout);
}


static void inspect_example(void){
    fputs(
        "dit inspect -S                 List the files under the current directory sorted by their size.\n"
        "dit inspect --sort=ext /dit    List the files under '/dit' sorted by their extension.\n"
        "dit inspect -CF /dev           List the files under '/dev', decorating their name.\n"
        "dit inspect /bin /sbin         List the files under '/bin' and '/sbin' respectively.\n"
    , stdout);
}


static void label_example(void){
    fputs(
        "dit label \n"
        "dit label \n"
        "dit label \n"
        "dit label \n"
    , stdout);
}


static void onbuild_example(void){
    fputs(
        "dit onbuild \n"
        "dit onbuild \n"
        "dit onbuild \n"
        "dit onbuild \n"
    , stdout);
}


static void optimize_example(void){
    fputs(
        "dit optimize \n"
        "dit optimize \n"
        "dit optimize \n"
        "dit optimize \n"
    , stdout);
}


static void package_example(void){
    fputs(
        "dit package \n"
        "dit package \n"
        "dit package \n"
        "dit package \n"
    , stdout);
}


static void reflect_example(void){
    fputs(
        "dit reflect          Error in noraml use, but used internally for logging.\n"
        "dit reflect -d in    Reflect the contents of './in' in Dockerfile.\n"
        "dit reflect -hp -    Reflect the input contents in history-file while keeping the empty lines.\n"
        "dit reflect -dhv     Reflect the output contents of the previous 'convert', and report them.\n"
    , stdout);
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/


void help_test(void){
    no_test();
}


#endif // NDEBUG