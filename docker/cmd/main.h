#ifndef DOCKER_INTERACTIVE_TOOL
#define DOCKER_INTERACTIVE_TOOL


#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
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
#define CONVERT_FILE_D "/dit/tmp/convert-result.dock"
#define CONVERT_FILE_H "/dit/tmp/convert-result.hist"
#define SETCMD_FILE "/dit/var/setcmd.log"

#define assign_both_or_either(target, a, b, c)  (target = (target == (a)) ? (b) : (c))

#define xperror_target_files()  xperror_missing_args(NULL, NULL)

#define xperror_standards(addition)  xperror_message(strerror(errno), addition)
#define xperror_individually(msg)  xperror_message(msg, NULL)
#define xperror_internal_file()  xperror_individually("unexpected error while manipulating an internal file")


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
    * Error Handling Functions
******************************************************************************/

void xperror_invalid_arg(int code, int state, const char * restrict desc, const char * restrict arg);
void xperror_valid_args(const char * const expected[], size_t size);

void xperror_missing_args(const char * restrict desc, const char * restrict before_arg);
void xperror_too_many_args(unsigned int limit);

void xperror_message(const char * restrict msg, const char * restrict addition);
void xperror_suggestion(bool cmd_flag);


/******************************************************************************
    * Utilitys
******************************************************************************/

char *xstrndup(const char *src, size_t n);
int strcmp_upper_case(const char * restrict target, const char * restrict expected);


/******************************************************************************
    * String Recognizers
******************************************************************************/

int receive_positive_integer(const char *target);
int receive_expected_string(const char *target, const char * const expected[], size_t size, int mode);


/******************************************************************************
    * Check Functions
******************************************************************************/

int check_file_isempty(const char *file_name);
int check_last_exit_status();


/******************************************************************************
    * Functions used in separate files
******************************************************************************/

int get_config(const char *config_arg, int * restrict p_mode2d, int * restrict p_mode2h);

int manage_provisional_report(unsigned short *prov_reflects, int mode_c, int keep_c);


#endif