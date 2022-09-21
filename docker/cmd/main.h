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

#define DOCKERFILE "/dit/usr/Dockerfile.draft"
#define HISTORY_FILE "/dit/usr/.cmd_history"



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
    * Help Functions for each dit command
******************************************************************************/

void config_manual();
void convert_manual();
void cp_manual();
void erase_manual();
void healthcheck_manual();
void ignore_manual();
void inspect_manual();
void label_manual();
void onbuild_manual();
void optimize_manual();
void reflect_manual();
void setcmd_manual();



/******************************************************************************
    * Utilitys
******************************************************************************/

char *xstrndup(const char *src, size_t n);
int strcmp_upper_case(const char *target, const char *expected);



/******************************************************************************
    * Argument Parsers
******************************************************************************/

int receive_positive_integer(const char *target);
int receive_expected_string(const char *target, const char * const expected[], int size, int mode);
int receive_yes_or_no(const char *target);
int receive_target_file(const char *target);



/******************************************************************************
    * Functions used in separate files
******************************************************************************/

int get_config(const char *config_arg, int *p_mode2d, int *p_mode2h);


#endif