#ifndef DOCKER_INTERACTIVE_TOOL
#define DOCKER_INTERACTIVE_TOOL


#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



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

int strcmp_forward_match(const char *target, const char *expected);


#endif