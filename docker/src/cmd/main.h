#ifndef DOCKER_INTERACTIVE_TOOL
#define DOCKER_INTERACTIVE_TOOL


#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMDS_NUM 10



/******************************************************************************
    * Tool-specific Functions
******************************************************************************/

int commit(int argc, char **argv);
int config(int argc, char **argv);
int convert(int argc, char **argv);
int erase(int argc, char **argv);
int healthcheck(int argc, char **argv);
int help(int argc, char **argv);
int ignore(int argc, char **argv);
int inspect(int argc, char **argv);
int label(int argc, char **argv);
int optimize(int argc, char **argv);



/******************************************************************************
    * Help Function for each Subcommand
******************************************************************************/

void help_commit();
void help_config();
void help_convert();
void help_erase();
void help_healthcheck();
void help_help();
void help_ignore();
void help_inspect();
void help_label();
void help_optimize();



/******************************************************************************
    * Extensions of Standard Library Functions
******************************************************************************/

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);



/******************************************************************************
    * Extra Implementation of Library Functions
******************************************************************************/

char *xstrndup(const char *src, size_t n);



/******************************************************************************
    * Utilitys
******************************************************************************/

int bsearch_subcmds(const char *target, int (* const comp)(const char *, const char *));
int strcmp_forward_match(const char *target, const char *expected);


#endif