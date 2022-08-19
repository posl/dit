/**
 * @file _help.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described subcommand that shows how to use dit command
 * @author Tsukasa Inada
 * @date 2022/08/09
 */

#include "main.h"


static void (* const __get_help_func(const char *target))();

static void __dit_usage();


extern const char * const subcmds[CMDS_NUM];




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief show how to use dit command
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note treated like a normal main function.
 */
int help(int argc, char **argv){
    if (--argc > 0){
        void (* help_func)();
        bool error_only = true;

        while (1){
            if ((help_func = __get_help_func(*(++argv)))){
                help_func();
                error_only = false;
            }
            if (--argc)
                putchar('\n');
            else
                break;
        }
        if (error_only)
            return 1;
    }
    else
        __dit_usage();
    return 0;
}


/**
 * @brief extract the corresponding help function.
 *
 * @param[in]  target  string that is the argument passed on the command line
 * @return int(* const)(int, char**)  desired help function or NULL
 *
 * @note "cfg" and "hc", which are default aliases of config and heatlthcheck, are supported.
 */
static void (* const __get_help_func(const char *target))(){
    if (! strcmp(target, "cfg")){
        puts(" < config >");
        return config_usage;
    }
    if (! strcmp(target, "hc")){
        puts(" < healthcheck >");
        return healthcheck_usage;
    }

    void (* const help_funcs[CMDS_NUM])() = {
        config_usage,
        convert_usage,
        cp_usage,
        erase_usage,
        healthcheck_usage,
        help_usage,
        ignore_usage,
        inspect_usage,
        label_usage,
        onbuild_usage,
        optimize_usage,
        setcmd_usage
    };

    int i;
    char *desc;
    if ((i = bsearch_subcmds(target, strcmp_forward_match)) >= 0){
        if (((! i) || strcmp_forward_match(target, subcmds[i - 1])) \
            && ((i == CMDS_NUM - 1) || strcmp_forward_match(target, subcmds[i + 1]))){
            printf(" < %s >\n", subcmds[i]);
            return help_funcs[i];
        }

        desc = "ambiguous";
    }
    else
        desc = "invalid";

    fprintf(stderr, "help: %s subcommand '%s'\n", desc, target);
    fputs("Try 'dit help' for more information.\n", stderr);
    return NULL;
}



/******************************************************************************
    * Help Functions
******************************************************************************/


void __dit_usage(){
    puts("Usage: dit [SUBCOMMAND] [ARGS]...");
    puts("By specifying one of the following SUBCOMMANDs, you can use the tool-specific functions.\n");
    puts("Subcommands:\n");
    puts("main features of this tool");
    puts("  convert        output how a command line is reflected to Dockerfile and history-file");
    puts("  optimize       refactoring Dockerfile based on its best practices\n");
    puts("customize tool settings");
    puts("  config         change the specification of how a executed command line is reflected");
    puts("  ignore         edit set of commands that is ignored when reflecting a command line\n");
    puts("edit the Dockerfile");
    puts("  cp             copy files from shared directory, and reflect this as COPY or ADD instruction");
    puts("  label          edit LABEL or EXPOSE instruction");
    puts("  setcmd         reflect the last command line as CMD or ENTRYPOINT instruction");
    puts("  healthcheck    reflect the last command line as HEALTHCHECK instruction");
    puts("  onbuild        reflect the last command line as ONBUILD instruction\n");
    puts("utilitys");
    puts("  erase          remove specific lines from a tool-specific file");
    puts("  inspect        display the directory tree under the specified file path");
    puts("  help           show how to use dit command\n");
    puts("See 'dit help [SUBCOMMAND]...' for more details.");
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


void help_usage(){
    puts("Usage: dit help [SUBCOMMAND]...");
    puts("Show detailed usage of each subcommand.");
}


void ignore_usage(){
    puts("help ignore");
}


void inspect_usage(){
    puts("Usage: dit inspect [OPTION]... [DIRECTORY]...");
    puts("List information about the files under the specified DIRECTORYs in a tree format.\n");
    puts("Options:");
    puts("  -C, --color              colorize the output to distinguish file types");
    puts("  -F, --classify           append indicator (one of */=|) to each file name:");
    puts("                             to executable file, directory, socket or fifo, in order");
    puts("  -n, --numeric-uid-gid    list the corresponding IDs instead of user or group name");
    puts("  -S                       sort by file size, largest first");
    puts("  -X                       sort by file extension, alphabetically");
    puts("      --sort=WORD          replace file sorting method:");
    puts("                             name(default), size(-S), extension(-X)");
    puts("      --help               display this help, and exit normally\n");
    puts("Each directory is sorted alphabetically unless otherwise specified.");
    puts("User or group name longer than 8 characters are converted to corresponding ID,");
    puts("  and IDs longer than 8 digits are converted to #EXCESS as undisplayable.");
    puts("Prefixs representing file size are k,M,G,T,P,E,Z, which is powers of 1000.");
    puts("Undisplayable characters appearing in the file name are uniformly replaced with '?'.");
    puts("Two hyphens can mark the end of the option like any other command.\n");
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