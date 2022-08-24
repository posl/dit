#ifndef DOCKER_INTERACTIVE_TOOL
#define DOCKER_INTERACTIVE_TOOL


#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CMDS_NUM 12



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
int setcmd(int argc, char **argv);



/******************************************************************************
    * Help Function for each dit command
******************************************************************************/

void config_usage();
void convert_usage();
void cp_usage();
void erase_usage();
void healthcheck_usage();
void ignore_usage();
void inspect_usage();
void label_usage();
void onbuild_usage();
void optimize_usage();
void setcmd_usage();



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


#endif