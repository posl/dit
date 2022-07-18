#ifndef DOCKER_INTERACTIVE_TOOL
#define DOCKER_INTERACTIVE_TOOL


#include <stdio.h>
#include <string.h>



/*
tool-specific commands
*/

int commit(int, char **);
int config(int, char **);
int convert(int, char **);
int erase(int, char **);
int healthcheck(int, char **);
int help(int, char **);
int ignore(int, char **);
int inspect(int, char **);
int label(int, char **);
int optimize(int, char **);



/*
utility functions
*/

int arg_bsearch(const char *, const char **, int);


#endif