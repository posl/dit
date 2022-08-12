/**
 * @file _inspect.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described subcommand that displays the directory tree
 * @author Tsukasa Inada
 * @date 2022/07/18
 */

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include "main.h"

#ifndef S_IXUGO
#define S_IXUGO  (S_IXUSR | S_IXGRP | S_IXOTH)
#endif


/** Data type for storing the results of option parse */
typedef struct {
    bool color;       /** whether to colorize file name depending on file mode */
    bool classify;    /** whether to append indicator to file name depending on file mode */
    bool numeric_id;  /** whether to represent users and groups numerically */
    enum {
        unspecified,
        size,
        extension
    } sort_style;     /** sort style for each directory */
} options;


/** Data type that is applied to the smallest element that makes up the directory tree */
typedef struct file_tree{
    char  *name;
    mode_t mode;
    uid_t  uid;
	gid_t  gid;
    off_t  size;

    char  *link_path;
    mode_t link_mode;
    bool   link_invalid;

    struct file_tree \
         **children;
    size_t children_num;
    size_t children_max;
} file_tree;


/** Data type for achieving a virtually infinite path length */
typedef struct {
    char  *ptr;
    size_t max;
} inf_path;


/** Data type for storing comparison function used when qsort */
typedef int (* comp_func)(const void *, const void *);


static int __parse_args(int argc, char **argv, options *opt);

static file_tree *__construct_file_tree(const char *base_path, options *opt);
static file_tree *__construct_recursive(inf_path *ipath, size_t ipath_len, char *name, comp_func comp);
static bool __concat_inf_path(inf_path *ipath, size_t ipath_len, const char *suf, size_t suf_len);
static file_tree *__new_file(char *path, char *name);
static bool __append_file(file_tree *dir, file_tree *file);

static comp_func __get_comp_func(int sort_style);
static int __comp_func_unspecified(const void *a, const void *b);
static int __comp_func_size(const void *a, const void *b);
static int __comp_func_extension(const void *a, const void *b);
static int __comp_file_name(const void *a, const void *b, int (* const addition)(file_tree *, file_tree *));
static int __comp_file_size(file_tree *file1, file_tree *file2);
static int __comp_file_extension(file_tree *file1, file_tree *file2);
static const char *__get_file_extension(const char *name);

static void __display_file_tree(file_tree *tree, options *opt);
static void __display_recursive(file_tree *file, options *opt, int depth);
static void __print_file_mode(mode_t mode);
static void __print_file_user(uid_t uid, bool numeric_id);
static void __print_file_group(gid_t gid, bool numeric_id);
static void __print_file_size(off_t size);
static void __print_file_name(char *name, mode_t mode, bool link_invalid, bool color, bool classify);




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief display the directory tree.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note treated like a normal main function.
 * @attention try this with '--help' option for more information.
 */
int inspect(int argc, char **argv){
    if (setvbuf(stdout, NULL, _IOFBF, 0))
        perror("inspect: setvbuf");

    int i;
    options opt;

    if ((i = __parse_args(argc, argv, &opt)))
        return (i < 0) ? 1 : 0;

    const char *path;
    if (argc <= optind){
        argc = 1;
        path = ".";
    }
    else {
        argc -= optind;
        argv += optind;
        path = *argv;
    }

    file_tree *tree;
    bool error_only = true;

    while (1){
        if ((tree = __construct_file_tree(path, &opt))){
            __display_file_tree(tree, &opt);
            error_only = false;
        }
        if (--argc > 0){
            path = *(++argv);
            putchar('\n');
        }
        else
            return error_only ? 1 : 0;
    }
}


/**
 * @brief parse optional arguments.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @param[out] opt  variable to store the results of option parse
 * @return int  parse result (zero : parse success, positive : help success, negative : parse failure)
 *
 * @note the arguments are expected to be passed as-is from main function.
 */
static int __parse_args(int argc, char **argv, options *opt){
    optind = 1;
    opterr = 0;

    const char *short_opts = ":cFnSX";
    const struct option long_opts[] = {
        { "color",           no_argument,       NULL, 'c' },
        { "classify",        no_argument,       NULL, 'F' },
        { "numeric-uid-gid", no_argument,       NULL, 'n' },
        { "sort",            required_argument, NULL,  1  },
        { "help",            no_argument,       NULL,  2  },
        {  0,                 0,                 0,    0  }
    };

    opt->color = false;
    opt->classify = false;
    opt->numeric_id = false;
    opt->sort_style = unspecified;

    int c;
    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1){
        switch (c){
            case 'c':
                opt->color = true;
                break;
            case 'F':
                opt->classify = true;
                break;
            case 'n':
                opt->numeric_id = true;
                break;
            case 'S':
                opt->sort_style = size;
                break;
            case 'X':
                opt->sort_style = extension;
                break;
            case 1:
                if (optarg){
                    if (strlen(optarg)){
                        if (! strcmp_forward_match(optarg, "size"))
                            opt->sort_style = size;
                        else if (! strcmp_forward_match(optarg, "extension"))
                            opt->sort_style = extension;
                        else {
                            fprintf(stderr, "inspect: invalid argument '%s' for '--sort'\n", optarg);
                            fputs("Valid arguments are:\n  - 'size'\n  - 'extension'\n", stderr);
                            goto error_occurred;
                        }
                    }
                    else {
                        fputs("inspect: ambiguous argument '' for '--sort'\n", stderr);
                        goto error_occurred;
                    }
                }
                break;
            case 2:
                help_inspect();
                return 1;
            case ':':
                fputs("inspect: option '--sort' requires an argument\n", stderr);
                goto error_occurred;
            case '?':
                if (argv[--optind][1] == '-')
                    fprintf(stderr, "inspect: unrecognized option '%s'\n", argv[optind]);
                else
                    fprintf(stderr, "inspect: invalid option '-%c'\n", optopt);
                goto error_occurred;
        }
    }
    return 0;

error_occurred:
    fputs("Try 'dit inspect --help' for more information.\n", stderr);
    return -1;
}




/******************************************************************************
    * Construct & Sort Part
******************************************************************************/


/**
 * @brief construct the directory tree to display.
 *
 * @param[in]  base_path  file path to the root of the directory tree
 * @param[in]  opt  variable containing the result of option parse
 * @return file_tree*  the result of constructing
 */
static file_tree *__construct_file_tree(const char *base_path, options *opt){
    file_tree *tree = NULL;

    size_t path_len;
    if ((path_len = strlen(base_path)) > 0){
        inf_path ipath;
        ipath.max = 1024;

        if ((ipath.ptr = (char *) malloc(sizeof(char) * ipath.max))){
            if (__concat_inf_path(&ipath, 0, base_path, path_len)){
                char *name;
                if ((name = xstrndup(base_path, path_len))){
                    comp_func comp;
                    if (! ((comp = __get_comp_func(opt->sort_style)) \
                        && (tree = __construct_recursive(&ipath, path_len, name, comp))))
                        free(name);
                }
            }
            free(ipath.ptr);
        }
    }
    return tree;
}


/**
 * @brief construct the directory tree recursively.
 *
 * @param[out] ipath  file path to the file we are currently looking at
 * @param[in]  ipath_len  the length of path string
 * @param[in]  name  name of the file we are currently looking at
 * @param[in]  comp  comparison function used when qsort
 * @return file_tree*  the result of sub-constructing
 *
 * @note at the same time, sort files in directory.
 */
static file_tree *__construct_recursive(inf_path *ipath, size_t ipath_len, char *name, comp_func comp){
    file_tree *file;
    file = __new_file(ipath->ptr, name);

    if (file && S_ISDIR(file->mode)){
        DIR *dir;
        if (! (dir = opendir(ipath->ptr))){
            fputs("inspect: ", stderr);
            perror(ipath->ptr);
        }
        else if (__concat_inf_path(ipath, ipath_len++, "/", 1)){
            struct dirent *entry;
            while ((entry = readdir(dir))){
                name = entry->d_name;
                if ((name[0] == '.') && (name[1 + ((name[1] == '.') ? 1 : 0)] == '\0'))
                    continue;

                size_t name_len;
                name_len = strlen(name);

                if ((name = xstrndup(name, name_len))){
                    bool continue_flag = false;
                    if (__concat_inf_path(ipath, ipath_len, name, name_len)){
                        file_tree *tmp;
                        if (tmp = __construct_recursive(ipath, ipath_len + name_len, name, comp)){
                            if (__append_file(file, tmp))
                                continue;
                        }
                        else
                            continue_flag = true;
                    }
                    free(name);
                    if (continue_flag)
                        continue;
                }
                break;
            }

            if (file->children)
                qsort(file->children, file->children_num, sizeof(file_tree *), comp);
            closedir(dir);
        }
    }
    return file;
}




/**
 * @brief concatenate any string to the end of base path string.
 *
 * @param[out] ipath  base path string
 * @param[in]  ipath_len  the length of base path string
 * @param[in]  suf  string to concatenate to the end of base path string
 * @param[in]  suf_len  the length of the string
 * @return bool  successful or not
 *
 * @note path string of arbitrary length can be achieved.
 */
static bool __concat_inf_path(inf_path *ipath, size_t ipath_len, const char *suf, size_t suf_len){
    size_t needed_len;
    needed_len = ipath_len + suf_len + 1;
    if (ipath->max < needed_len){
        do
            ipath->max *= 2;
        while (ipath->max < needed_len);

        if (! (ipath->ptr = (char *) realloc(ipath->ptr, sizeof(char) * ipath->max)))
            return false;
    }

    char *path;
    path = ipath->ptr + ipath_len;
    while (suf_len--)
        *(path++) = *(suf++);
    *path = '\0';

    return true;
}


/**
 * @brief create new element that makes up the directory tree.
 *
 * @param[in]  path  file path to the file we are currently looking at
 * @param[in]  name  name of the file we are currently looking at
 * @return file_tree*  new element that makes up the directory tree
 */
static file_tree *__new_file(char *path, char *name){
    struct stat file_stat;
    if (lstat(path, &file_stat)){
        fputs("inspect: ", stderr);
        perror(path);
        return NULL;
    }

    file_tree *file;
    if ((file = (file_tree *) malloc(sizeof(file_tree)))){
        file->name = name;
        file->mode = file_stat.st_mode;
        file->uid = file_stat.st_uid;
        file->gid = file_stat.st_gid;
        file->size = file_stat.st_size;

        file->link_path = NULL;
        file->link_mode = 0;
        file->link_invalid = true;

        if (S_ISLNK(file->mode)){
            char *link_path;
            if ((link_path = (char *) malloc(sizeof(char) * (file->size + 1)))){
                size_t link_len;
                if ((link_len = readlink(path, link_path, file->size)) > 0){
                    link_path[link_len] = '\0';
                    file->link_path = link_path;

                    if (! stat(path, &file_stat)){
                        file->link_mode = file_stat.st_mode;
                        file->link_invalid = false;
                    }
                }
                else
                    free(link_path);
            }
        }
        else
            file->link_invalid = false;

        file->children = NULL;
        file->children_num = 0;
        file->children_max = 0;
    }
    return file;
}


/**
 * @brief append file to the directory tree under construction.
 *
 * @param[out] dir  the directory tree under construction
 * @param[in]  file  file to append to the directory tree
 * @return bool  successful or not
 *
 * @note any directory can have a virtually unlimited number of files.
 */
static bool __append_file(file_tree *dir, file_tree *file){
    if (dir->children_num == dir->children_max){
        void *ptr;
        if (! dir->children){
            dir->children_max = 128;
            ptr = malloc(sizeof(file_tree *) * dir->children_max);
        }
        else {
            dir->children_max *= 2;
            ptr = realloc(dir->children, sizeof(file_tree *) * dir->children_max);
        }

        if (ptr)
            dir->children = (file_tree **) ptr;
        else
            return false;
    }

    dir->size += file->size;
    dir->children[dir->children_num++] = file;
    return true;
}




/******************************************************************************
    * Comparison Functions used when qsort
******************************************************************************/


/**
 * @brief extract the comparison function used when qsort based on option parse.
 *
 * @param[in]  sort_style  file sorting style determined by option parse
 * @return comp_func  the desired comparison function or NULL
 */
static comp_func __get_comp_func(int sort_style){
    switch (sort_style){
        case unspecified:
            return __comp_func_unspecified;
        case size:
            return __comp_func_size;
        case extension:
            return __comp_func_extension;
        default:
            return NULL;
    }
}


/**
 * @brief comparison function used when sort style is unspecified
 *
 * @param[in]  a  pointer to file1
 * @param[in]  b  pointer to file2
 * @return int  comparison result
 */
static int __comp_func_unspecified(const void *a, const void *b){
    return __comp_file_name(a, b, NULL);
}


/**
 * @brief comparison function used when sort style is specified as size
 *
 * @param[in]  a  pointer to file1
 * @param[in]  b  pointer to file2
 * @return int  comparison result
 */
static int __comp_func_size(const void *a, const void *b){
    return __comp_file_name(a, b, __comp_file_size);
}


/**
 * @brief comparison function used when sort style is specified as extension
 *
 * @param[in]  a  pointer to file1
 * @param[in]  b  pointer to file2
 * @return int  comparison result
 */
static int __comp_func_extension(const void *a, const void *b){
    return __comp_file_name(a, b, __comp_file_extension);
}




/**
 * @brief base comparison function that compares files by their name
 *
 * @param[in]  a  pointer to file1
 * @param[in]  b  pointer to file2
 * @param[in]  addition  additional comparison function used before comparison by file name
 * @return int  comparison result
 */
static int __comp_file_name(const void *a, const void *b, int (* const addition)(file_tree *, file_tree *)){
    file_tree *file1 = *((file_tree **) a);
    file_tree *file2 = *((file_tree **) b);

    int i;
    return (addition && (i = addition(file1, file2))) ? i : strcmp(file1->name, file2->name);
}


/**
 * @brief additional comparison function that compares files by their size
 *
 * @param[in]  file1
 * @param[in]  file2
 * @return int  comparison result
 */
static int __comp_file_size(file_tree *file1, file_tree *file2){
    return (int) (file2->size - file1->size);
}


/**
 * @brief additional comparison function that compares files by their extension
 *
 * @param[in]  file1
 * @param[in]  file2
 * @return int  comparison result
 */
static int __comp_file_extension(file_tree *file1, file_tree *file2){
    const char *ext1, *ext2;
    ext1 = __get_file_extension(file1->name);
    ext2 = __get_file_extension(file2->name);
    return strcmp(ext1, ext2);
}


/**
 * @brief extract extension from file name.
 *
 * @param[in]  name  target file name
 * @return const char*  string representing the file extension
 */
static const char *__get_file_extension(const char *name){
    const char *ext;
    ext = name;
    while (*name)
        if (*(name++) == '.')
            ext = name;
    return ext;
}




/******************************************************************************
    * Display & Release Part
******************************************************************************/


/**
 * @brief actually display the directory tree.
 *
 * @param[out] tree  pre-constructed directory tree
 * @param[in]  opt  variable containing the result of option parse
 */
static void __display_file_tree(file_tree *tree, options *opt){
    puts("Permission      User     Group      Size");
    puts("=========================================");
    __display_recursive(tree, opt, 0);
}


/**
 * @brief display the directory tree recursively.
 *
 * @param[out] file  the file we are currently trying to display
 * @param[in]  opt  variable containing the result of option parse
 * @param[in]  depth  hierarchy in the directory tree of the file we are currently trying to display
 *
 * @note at the same time, release the data that is no longer needed.
 */
static void __display_recursive(file_tree *file, options *opt, int depth){
    __print_file_mode(file->mode);
    __print_file_user(file->uid, opt->numeric_id);
    __print_file_group(file->gid, opt->numeric_id);
    __print_file_size(file->size);

    int i;
    if (depth){
        i = 0;
        while (++i < depth)
            printf("|   ");
        printf("|-- ");
    }
    __print_file_name(file->name, file->mode, file->link_invalid, opt->color, opt->classify);
    free(file->name);

    if (file->link_path){
        printf(" -> ");
        __print_file_name(file->link_path, file->link_mode, file->link_invalid, opt->color, opt->classify);
        free(file->link_path);
    }
    putchar('\n');

    if (file->children){
        i = 0;
        depth++;
        do
            __display_recursive(file->children[i++], opt, depth);
        while (i < file->children_num);

        free(file->children);
    }
    free(file);
}




/**
 * @brief display file mode on screen.
 *
 * @param[in]  mode  file mode
 */
static void __print_file_mode(mode_t mode){
    char S[11];
    S[0] =
        S_ISREG(mode) ? '-' :
        S_ISDIR(mode) ? 'd' :
        S_ISCHR(mode) ? 'c' :
        S_ISBLK(mode) ? 'b' :
        S_ISFIFO(mode) ? 'p' :
        S_ISLNK(mode) ? 'l' :
        S_ISSOCK(mode) ? 's' :
        '?';
    S[1] = (mode & S_IRUSR) ? 'r' : '-';
    S[2] = (mode & S_IWUSR) ? 'w' : '-';
    S[3] = (mode & S_ISUID) ? ((mode & S_IXUSR) ? 's' : 'S') : ((mode & S_IXUSR) ? 'x' : '-');
    S[4] = (mode & S_IRGRP) ? 'r' : '-';
    S[5] = (mode & S_IWGRP) ? 'w' : '-';
    S[6] = (mode & S_ISGID) ? ((mode & S_IXGRP) ? 's' : 'S') : ((mode & S_IXGRP) ? 'x' : '-');
    S[7] = (mode & S_IROTH) ? 'r' : '-';
    S[8] = (mode & S_IWOTH) ? 'w' : '-';
    S[9] = (mode & S_ISVTX) ? ((mode & S_IXOTH) ? 't' : 'T') : ((mode & S_IXOTH) ? 'x' : '-');
    S[10] = '\0';

    printf("%s  ", S);
}


/**
 * @brief display file user on screen.
 *
 * @param[in]  uid  file uid
 * @param[in]  numeric_id  whether to represent users and groups numerically
 *
 * @attention if the uid exceeds 8 digits, print a string that represents a numeric excess.
 */
static void __print_file_user(uid_t uid, bool numeric_id){
    if (! numeric_id){
        struct passwd *passwd;
        if ((passwd = getpwuid(uid)) && (strlen(passwd->pw_name) <= 8)){
            printf("%8s  ", passwd->pw_name);
            return;
        }
    }
    if (uid < 100000000)
        printf("%8d  ", uid);
    else
        puts(" #EXCESS  ");
}


/**
 * @brief display file group on screen.
 *
 * @param[in]  gid  file gid
 * @param[in]  numeric_id  whether to represent users and groups numerically
 *
 * @attention if the gid exceeds 8 digits, print a string that represents a numeric excess.
 */
static void __print_file_group(gid_t gid, bool numeric_id){
    if (! numeric_id){
        struct group *group;
        if ((group = getgrgid(gid)) && (strlen(group->gr_name) <= 8)){
            printf("%8s  ", group->gr_name);
            return;
        }
    }
    if (gid < 100000000)
        printf("%8d  ", gid);
    else
        puts(" #EXCESS  ");
}


/**
 * @brief display file size on screen.
 *
 * @param[in]  size  file size
 *
 * @attention cannot be handle file size exceeding the upper limit of integer type.
 */
static void __print_file_size(off_t size){
    if (size < 1000)
        printf("%6d B    ", (int) size);
    else {
        int i = -1, rem;
        char *units = "kMGTPEZ";

        do {
            i++;
            rem = size % 1000;
            size /= 1000;
        } while (size >= 1000);

        if (i <= 6){
            rem /= 100;
            printf("%3d.%1d %cB    ", (int) size, rem, units[i]);
        }
        else
            puts(" #EXCESS    ");
    }
}


/**
 * @brief display file name on screen.
 *
 * @param[out] name  file name
 * @param[in]  mode  file mode
 * @param[in]  link_invalid  whether file name is relative to a file that is an invalid symlink
 * @param[in]  color  whether to colorize file name depending on file mode
 * @param[in]  classify  whether to append indicator to file name depending on file mode
 */
static void __print_file_name(char *name, mode_t mode, bool link_invalid, bool color, bool classify){
    char *tmp;
    tmp = name;
    while (*tmp){
        if (iscntrl((int) *tmp))
            *tmp = '?';
        tmp++;
    }

    if (color){
        tmp =
            link_invalid ? "31" :
            S_ISREG(mode) ? (
                (mode & S_ISUID) ? "37;41" :
                (mode & S_ISGID) ? "30;43" :
                (mode & S_IXUGO) ? "1;32" :
                "0"
            ) :
            S_ISDIR(mode) ? (
                (mode & S_IWOTH) ? (
                    (mode & S_ISVTX) ? "30;42" :
                    "34;42"
                ) :
                (mode & S_ISVTX) ? "37;44" :
                "1;34"
            ) :
            (S_ISCHR(mode) || S_ISBLK(mode)) ? "1;33" :
            S_ISFIFO(mode) ? "33" :
            S_ISLNK(mode) ? "1;36" :
            S_ISSOCK(mode) ? "1;35" :
            "0";
        printf("\033[%sm%s\033[0m", tmp, name);
    }
    else
        printf(name);

    if (classify){
        char indicator;
        indicator =
            (S_ISREG(mode) && (mode & S_IXUGO)) ? '*' :
            S_ISDIR(mode) ? '/' :
            S_ISFIFO(mode) ? '|' :
            S_ISSOCK(mode) ? '=' :
            '\0';
        if (indicator)
            putchar(indicator);
    }
}