#ifndef DOCKER_INTERACTIVE_TOOL
#define DOCKER_INTERACTIVE_TOOL


#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


#define CMDS_NUM 13

#define ASSUMES_NUM 2
#define DISPLAYS_NUM 3
#define TARGETS_NUM 3


#define VERSION_FILE "/dit/etc/dit_version"

#define DOCKER_FILE_DRAFT "/dit/mnt/Dockerfile.draft"
#define HISTORY_FILE "/dit/mnt/.dit_history"

#define CONVERT_FILE_D "/dit/tmp/convert-result.dock"
#define CONVERT_FILE_H "/dit/tmp/convert-result.hist"



/******************************************************************************
    * commonly used Expressions
******************************************************************************/

#define assign_both_or_either(target, a, b, c)  (target = (target == (a)) ? (b) : (c))

#define check_string_of_length1(str, c)  ((str[0] == c) && (! str[1]))
#define check_string_isstdin(file_name)  check_string_of_length1(file_name, '-')
#define check_string_isnewline(line)  check_string_of_length1(line, '\n')


/******************************************************************************
    * commonly used Error Message Functions
******************************************************************************/

#define xperror_config_arg(target)  xperror_invalid_arg('C', 0, "mode", target)
#define xperror_target_files()  xperror_missing_args(NULL, NULL)

#define xperror_standards(errid, addition)  xperror_message(strerror(errid), addition, false)
#define xperror_individually(msg)  xperror_message(msg, NULL, false)
#define xperror_internal_file()  xperror_individually("unexpected error while manipulating an internal file")


/******************************************************************************
    * IDs for 'receive_dockerfile_instruction'
******************************************************************************/

#define ID_ADD           0
#define ID_ARG           1
#define ID_CMD           2
#define ID_COPY          3
#define ID_ENTRYPOINT    4
#define ID_ENV           5
#define ID_EXPOSE        6
#define ID_FROM          7
#define ID_HEALTHCHECK   8
#define ID_LABEL         9
#define ID_ONBUILD      10
#define ID_RUN          11
#define ID_SHELL        12
#define ID_STOPSIGNAL   13
#define ID_USER         14
#define ID_VOLUME       15
#define ID_WORKDIR      16



/******************************************************************************
    * Tool-specific Functions
******************************************************************************/

int config(int argc, char **argv);
int convert(int argc, char **argv);
int cp(int argc, char **argv);
int erase(int argc, char **argv);
int healthcheck(int argc, char **argv);
int help(int argc, char **argv);
int ignore(int argc, char **argv);
int inspect(int argc, char **argv);
int label(int argc, char **argv);
int onbuild(int argc, char **argv);
int optimize(int argc, char **argv);
int reflect(int argc, char **argv);
int setcmd(int argc, char **argv);


/******************************************************************************
    * Help Functions that display each Manual
******************************************************************************/

void config_manual();
void convert_manual();
void cp_manual();
void erase_manual();
void healthcheck_manual();
void help_manual();
void ignore_manual();
void inspect_manual();
void label_manual();
void onbuild_manual();
void optimize_manual();
void reflect_manual();
void setcmd_manual();


/******************************************************************************
    * Functions used in separate files
******************************************************************************/

int get_config(const char *config_arg, int * restrict p_mode2d, int * restrict p_mode2h);

int reflect_to_Dockerfile(const char *line);
int read_provisional_report(unsigned short prov_reflecteds[2]);
int write_provisional_report(unsigned short prov_reflecteds[2]);



/******************************************************************************
    * Error Message Functions
******************************************************************************/

void xperror_invalid_arg(int code_c, int state, const char * restrict desc, const char * restrict arg);
void xperror_valid_args(const char * const expected[], size_t size);

void xperror_missing_args(const char * restrict desc, const char * restrict before_arg);
void xperror_too_many_args(int limit);

void xperror_message(const char * restrict msg, const char * restrict addition, bool omit_newline);
void xperror_suggestion(bool cmd_flag);


/******************************************************************************
    * Utilitys
******************************************************************************/

char *xfgets_for_loop(const char *file_name, bool preserve_flag, int *p_errid);
int xstrcmp_upper_case(const char * restrict target, const char * restrict expected);


/******************************************************************************
    * String Recognizers
******************************************************************************/

int receive_positive_integer(const char *target);
int receive_expected_string(const char *target, const char * const expected[], size_t size, int mode);
char *receive_dockerfile_instruction(const char *line, int *p_id);


/******************************************************************************
    * Check Functions
******************************************************************************/

int check_file_size(const char *file_name);
int check_last_exit_status();


#endif