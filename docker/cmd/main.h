#ifndef DOCKER_INTERACTIVE_TOOL
#define DOCKER_INTERACTIVE_TOOL


// if you want to test each command, comment out the line immediately after
#ifndef NDEBUG
#define NDEBUG
#endif


#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>

#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <pthread.h>
#include <pwd.h>
#include <regex.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "test.h"
#include "yyjson.h"




/******************************************************************************
    * commonly used Constants
******************************************************************************/

#define SUCCESS 0
#define FAILURE 1

#define NORMALLY_EXIT 1
#define ERROR_EXIT (-1)

#define POSSIBLE_ERROR 1
#define UNEXPECTED_ERROR (-1)
#define FATAL_ERROR  (UNEXPECTED_ERROR + ERROR_EXIT)

#define CMDS_NUM 14
#define ARGS_NUM 3
#define DOCKER_INSTRS_NUM 18


/******************************************************************************
    * commonly used Internal Files
******************************************************************************/

#define DIT_PROFILE "/dit/tmp/.profile"

#define DOCKER_FILE_DRAFT "/dit/mnt/Dockerfile.draft"
#define HISTORY_FILE "/dit/mnt/.dit_history"

#define CONVERT_RESULT_FILE_D "/dit/srv/convert-result.dock"
#define CONVERT_RESULT_FILE_H "/dit/srv/convert-result.hist"

#define ERASE_RESULT_FILE_D "/dit/srv/erase-result.dock"
#define ERASE_RESULT_FILE_H "/dit/srv/erase-result.hist"




/******************************************************************************
    * commonly used Expressions
******************************************************************************/

#define assign_both_or_either(target, a, b, c)  (target = (target == (a)) ? (b) : (c))

#define strchrcmp(str, c)  (((str)[0] == (c)) && (! (str)[1]))
#define check_if_stdin(file_name)  strchrcmp(file_name, '-')


/******************************************************************************
    * commonly used Functions
******************************************************************************/

#define print_target_repr(offset)  fprintf(stdout, ("\n < %s >\n" + (offset)), target_args[2 - (offset)])

#define xperror_config_arg(target)  xperror_invalid_arg('C', 0, "mode", target)
#define xperror_target_files()  xperror_missing_args(NULL, NULL)

#define xperror_individually(msg)  xperror_message(msg, NULL)
#define xperror_internal_file()  xperror_individually(NULL)


/******************************************************************************
    * IDs for each dit command
******************************************************************************/

#define DIT_CMD           0
#define DIT_CONFIG        1
#define DIT_CONVERT       2
#define DIT_COPY          3
#define DIT_ERASE         4
#define DIT_HEALTHCHECK   5
#define DIT_HELP          6
#define DIT_IGNORE        7
#define DIT_INSPECT       8
#define DIT_LABEL         9
#define DIT_ONBUILD      10
#define DIT_OPTIMIZE     11
#define DIT_PACKAGE      12
#define DIT_REFLECT      13


/******************************************************************************
    * IDs for 'receive_dockerfile_instr'
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
#define ID_MAINTAINER   10
#define ID_ONBUILD      11
#define ID_RUN          12
#define ID_SHELL        13
#define ID_STOPSIGNAL   14
#define ID_USER         15
#define ID_VOLUME       16
#define ID_WORKDIR      17




/******************************************************************************
    * commonly used Data Types
******************************************************************************/

/** Data type for achieving strings of virtually infinite length */
typedef struct {
    char *ptr;     /** a string of arbitrary length */
    size_t max;    /** the current maximum length of the string */
} inf_str;




/******************************************************************************
    * Tool-specific Functions
******************************************************************************/

int cmd(int argc, char **argv);
int config(int argc, char **argv);
int convert(int argc, char **argv);
int copy(int argc, char **argv);
int erase(int argc, char **argv);
int healthcheck(int argc, char **argv);
int help(int argc, char **argv);
int ignore(int argc, char **argv);
int inspect(int argc, char **argv);
int label(int argc, char **argv);
int onbuild(int argc, char **argv);
int optimize(int argc, char **argv);
int package(int argc, char **argv);
int reflect(int argc, char **argv);


/******************************************************************************
    * Help Functions that display each Manual
******************************************************************************/

void cmd_manual(void);
void config_manual(void);
void convert_manual(void);
void copy_manual(void);
void erase_manual(void);
void healthcheck_manual(void);
void help_manual(void);
void ignore_manual(void);
void inspect_manual(void);
void label_manual(void);
void onbuild_manual(void);
void optimize_manual(void);
void package_manual(void);
void reflect_manual(void);


/******************************************************************************
    * Functions used in separate files
******************************************************************************/

int get_config(const char *config_arg, int modes[2]);

int delete_from_dockerfile(char **patterns, size_t size, bool verbose, int assume_c);
int update_erase_logs(int reflecteds[2]);

bool check_if_ignored(const yyjson_doc *idoc, int argc, char **argv);

int reflect_to_dockerfile(char **lines, size_t size, bool verbose, int flag_c);
int read_provisional_report(int reflecteds[2]);
int write_provisional_report(int reflecteds[2]);




/******************************************************************************
    * Error Message Functions
******************************************************************************/

void xperror_invalid_arg(int code_c, int state, const char * restrict desc, const char * restrict arg);
void xperror_valid_args(const char * const *expected, size_t size);

void xperror_missing_args(const char * restrict desc, const char * restrict before_arg);
void xperror_too_many_args(int limit);

void xperror_message(const char * restrict msg, const char * restrict addition);
void xperror_suggestion(bool cmd_flag);

int xperror_standards(const char *entity, int errid);
void xperror_child_process(const char *cmd_name, int status);
void xperror_file_contents(const char * restrict file_name, int lineno, const char * restrict msg);


/******************************************************************************
    * Extensions of Library Functions
******************************************************************************/

char *xfgets_for_loop(const char *src_file, char **p_start, int *p_errid);

int xstrcmp_upper_case(const char * restrict target, const char * restrict expected);
int qstrcmp(const void *a, const void *b);

bool xstrcat_inf_len(inf_str *base, size_t base_len, const char *suf, size_t suf_len);

int execute_command(const char *cmd_file, char * const argv[], int null_redirs);


/******************************************************************************
    * String Recognizers
******************************************************************************/

int receive_positive_integer(const char *target, int *p_left);
int receive_expected_string(const char *target, const char * const *reprs, size_t size, unsigned int mode);
char *receive_dockerfile_instr(char *line, int *p_id);


/******************************************************************************
    * Get Methods
******************************************************************************/

int get_file_size(const char *file_name);
int get_last_exit_status(void);
char *get_suffix(char *target, int delimiter, bool retain);


#endif // DOCKER_INTERACTIVE_TOOL