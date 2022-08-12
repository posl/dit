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

static void __help_dit();


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
        __help_dit();
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
        return help_config;
    }
    if (! strcmp(target, "hc")){
        puts(" < healthcheck >");
        return help_healthcheck;
    }

    void (* const help_funcs[CMDS_NUM])() = {
        help_config,
        help_convert,
        help_cp,
        help_erase,
        help_healthcheck,
        help_help,
        help_ignore,
        help_inspect,
        help_label,
        help_onbuild,
        help_optimize,
        help_setcmd
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


void __help_dit(){
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


void help_config(){
    puts("help config");
}


void help_convert(){
    puts("help convert");
}


void help_cp(){
    puts("help cp");
}


void help_erase(){
    puts("help erase");
}


void help_healthcheck(){
    puts("help healthcheck");
}


void help_help(){
    puts("Usage: dit help [SUBCOMMAND]...");
    puts("Show detailed usage of each subcommand.");
}


void help_ignore(){
    puts("help ignore");
}


void help_inspect(){
    puts("Usage: dit inspect [OPTION]... [DIRECTORY]...");
    puts("List information about the files under the specified DIRECTORYs in a tree format.\n");
    puts("Options:");
    puts("  -c, --color              colorize the output to distinguish file types");
    puts("  -F, --classify           append indicator (one of */=|) to each file name:");
    puts("                             to executable file, directory, socket or fifo, in order");
    puts("  -n, --numeric-uid-gid    list the corresponding IDs instead of user or group name");
    puts("  -S, --sort=size          sort by file size, largest first");
    puts("  -X, --sort=extension     sort by file extension, alphabetically");
    puts("      --help               display this help, and exit normally\n");
    puts("Each directory is sorted alphabetically unless otherwise specified.");
    puts("Prefixs representing file size are k,M,G,T,P,E,Z, which is powers of 1000.");
    puts("Undisplayable characters appearing in the file name are uniformly replaced with '?'.");
    puts("Two hyphens can mark the end of the option like any other command.\n");
    puts("The above options are similar to the 'ls' command which is a GNU one.");
    puts("See that man page for more details.");
}


void help_label(){
    puts("help label");
}


void help_onbuild(){
    puts("help onbuild");
}


void help_optimize(){
    puts("help optimize");
}


void help_setcmd(){
    puts("help setcmd");
}