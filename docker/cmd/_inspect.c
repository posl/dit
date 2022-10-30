/**
 * @file _inspect.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'inspect', that shows some directory trees.
 * @author Tsukasa Inada
 * @date 2022/07/18
 */

#include "main.h"

#define INSP_EXCESS_STR " #EXCESS"


/** Data type for storing comparison function used when qsort */
typedef int (* qcmp)(const void *, const void *);


/** Data type for storing the results of option parse */
typedef struct {
    bool color;         /** whether to colorize file name based on file mode */
    bool classify;      /** whether to append indicator to file name based on file mode */
    bool numeric_id;    /** whether to represent users and groups numerically */
    qcmp comp;          /** comparison function used when qsort */
} insp_opts;


/** Data type that is applied to the smallest element that makes up the directory tree */
typedef struct file_node{
    char *name;                     /** file name */
    mode_t mode;                    /** file mode */
    uid_t uid;                      /** file uid */
	gid_t gid;                      /** file gid */
    off_t size;                     /** file size */

    char *link_path;                /** file name of link destination if this is a symbolic link */
    mode_t link_mode;               /** file mode of link destination if this is a symbolic link */
    bool link_invalid;              /** whether this is a invalid symbolic link */

    struct file_node **children;    /** array for storing the children if this is a directory */
    size_t children_num;            /** the current number of the children */
    size_t children_max;            /** the current maximum length of the array */

    int errid;                      /** serial number of the error encountered */
    bool noinfo;                    /** whether the file information could not be obtained */
} file_node;


/** Data type for achieving the virtually infinite length of file path */
typedef struct {
    char *ptr;     /** file path */
    size_t max;    /** the current maximum length of file path */
} inf_path;


static int __parse_opts(int argc, char **argv, insp_opts *opt);

static file_node *__construct_dir_tree(const char *base_path, insp_opts *opt);
static file_node *__construct_recursive(inf_path *ipath, size_t ipath_len, const char *name, qcmp comp);
static bool __concat_inf_path(inf_path *ipath, size_t ipath_len, const char *suf, size_t suf_len);
static file_node *__new_file(const char * restrict path, char * restrict name);
static bool __append_file(file_node *tree, file_node *file);

static int __qcmp_name(const void *a, const void *b);
static int __qcmp_size(const void *a, const void *b);
static int __qcmp_ext(const void *a, const void *b);
static int __fcmp_name(const void *a, const void *b, int (* const addition)(file_node *, file_node *));
static int __fcmp_size(file_node *file1, file_node *file2);
static int __fcmp_ext(file_node *file1, file_node *file2);
static const char *__get_file_ext(const char *name);

static void __destruct_dir_tree(file_node *tree, insp_opts *opt);
static void __destruct_recursive(file_node *file, insp_opts *opt, unsigned int depth);
static void __print_file_mode(mode_t mode);
static void __print_file_owner(file_node *file, bool numeric_id);
static void __print_file_size(off_t size);
static void __print_file_name(file_node *file, insp_opts *opt, bool link_flag);




/******************************************************************************
    * Local Main Interface
******************************************************************************/


/**
 * @brief show the directory tree.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @return int  command's exit status
 *
 * @note treated like a normal main function.
 */
int inspect(int argc, char **argv){
    int i;
    insp_opts opt;

    if ((i = __parse_opts(argc, argv, &opt)))
        return (i < 0) ? FAILURE : SUCCESS;

    const char *path = ".";
    file_node *tree;

    if ((argc -= optind) > 0){
        argv += optind;
        path = *argv;
    }
    else
        argc = 1;

    do {
        if ((tree = __construct_dir_tree(path, &opt)))
            __destruct_dir_tree(tree, &opt);
        else
            i = FAILURE;

        if (--argc){
            path = *(++argv);
            fputc('\n', stdout);
        }
        else
            return i;
    } while (1);
}


/**
 * @brief parse optional arguments.
 *
 * @param[in]  argc  the number of command line arguments
 * @param[out] argv  array of strings that are command line arguments
 * @param[out] opt  variable to store the results of option parse
 * @return int  0 (parse success), 1 (normally exit) or -1 (error exit)
 *
 * @note the arguments are expected to be passed as-is from main function.
 */
static int __parse_opts(int argc, char **argv, insp_opts *opt){
    const char *short_opts = "CFnSX";

    const struct option long_opts[] = {
        { "color",           no_argument,       NULL, 'C' },
        { "classify",        no_argument,       NULL, 'F' },
        { "numeric-uid-gid", no_argument,       NULL, 'n' },
        { "help",            no_argument,       NULL,  1  },
        { "sort",            required_argument, NULL,  0  },
        {  0,                 0,                 0,    0  }
    };

    const char * const sort_args[ARGS_NUM] = {
        "extension",
        "name",
        "size"
    };

    opt->color = false;
    opt->classify = false;
    opt->numeric_id = false;
    opt->comp = __qcmp_name;

    int c, i;
    while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) >= 0){
        switch (c){
            case 'C':
                opt->color = true;
                break;
            case 'F':
                opt->classify = true;
                break;
            case 'n':
                opt->numeric_id = true;
                break;
            case 'S':
                opt->comp = __qcmp_size;
                break;
            case 'X':
                opt->comp = __qcmp_ext;
                break;
            case 1:
                inspect_manual();
                return NORMALLY_EXIT;
            case 0:
                if ((c = receive_expected_string(optarg, sort_args, ARGS_NUM, 2)) >= 0){
                    opt->comp = (c ? ((c == 1) ? __qcmp_name : __qcmp_size) : __qcmp_ext);
                    break;
                }
                xperror_invalid_arg('O', c, long_opts[i].name, optarg);
                xperror_valid_args(sort_args, ARGS_NUM);
            default:
                xperror_suggestion(true);
                return ERROR_EXIT;
        }
    }

    opt->color &= isatty(fileno(stdout));
    return SUCCESS;
}




/******************************************************************************
    * Construct & Sort Part
******************************************************************************/


/**
 * @brief construct the directory tree with details about each file also collected together.
 *
 * @param[in]  base_path  file path to the root of the directory tree
 * @param[in]  opt  variable containing the result of option parse
 * @return file_node*  the result of constructing
 */
static file_node *__construct_dir_tree(const char *base_path, insp_opts *opt){
    file_node *tree = NULL;

    if (base_path){
        inf_path ipath = { .ptr = NULL, .max = 0};
        tree = __construct_recursive(&ipath, 0, base_path, opt->comp);

        if (ipath.ptr)
            free(ipath.ptr);
    }
    return tree;
}


/**
 * @brief construct the directory tree, recursively.
 *
 * @param[out] ipath  file path to the file we are currently looking at
 * @param[in]  ipath_len  the length of path string
 * @param[in]  name  name of the file we are currently looking at
 * @param[in]  comp  comparison function used when qsort
 * @return file_node*  the result of sub-constructing
 *
 * @note at the same time, sort files in directory.
 */
static file_node *__construct_recursive(inf_path *ipath, size_t ipath_len, const char *name, qcmp comp){
    file_node *file = NULL;

    size_t name_len;
    if (((name_len = strlen(name) + 1) > 0) && __concat_inf_path(ipath, ipath_len, name, name_len)){
        char *dest;
        if ((dest = (char *) malloc(sizeof(char) * name_len))){
            memcpy(dest, name, (sizeof(char) * name_len));

            if ((file = __new_file(ipath->ptr, dest))){
                if (S_ISDIR(file->mode)){
                    DIR *dir;
                    if ((dir = opendir(ipath->ptr))){
                        ipath_len += name_len;

                        if (__concat_inf_path(ipath, ipath_len - 1, "/", 2)){
                            struct dirent *entry;
                            file_node *tmp;

                            while ((entry = readdir(dir))){
                                name = entry->d_name;
                                if ((name[0] == '.') && (! name[(name[1] != '.') ? 1 : 2]))
                                    continue;

                                if (! (tmp = __construct_recursive(ipath, ipath_len, name, comp)))
                                    break;
                                if (! __append_file(file, tmp)){
                                    __destruct_recursive(tmp, NULL, 0);
                                    break;
                                }
                            }

                            if (file->children)
                                qsort(file->children, file->children_num, sizeof(file_node *), comp);
                        }
                        closedir(dir);
                    }
                    else
                        file->errid = errno;
                }
            }
            else
                free(dest);
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
 * @param[in]  suf_len  the length of the string including the terminating null character
 * @return bool  successful or not
 *
 * @note path string of arbitrary length can be achieved.
 */
static bool __concat_inf_path(inf_path *ipath, size_t ipath_len, const char *suf, size_t suf_len){
    size_t curr_max, needed_len;
    curr_max = ipath->max;
    needed_len = ipath_len + suf_len;

    if (curr_max < needed_len){
        if (! curr_max)
            curr_max = 1024;

        size_t prev_max;
        while (curr_max < needed_len){
            prev_max = curr_max;
            curr_max *= 2;
            if (prev_max >= curr_max)
                return false;
        }

        void *ptr;
        if ((ptr = realloc(ipath->ptr, (sizeof(char) * curr_max)))){
            ipath->ptr = (char *) ptr;
            ipath->max = curr_max;
        }
        else
            return false;
    }

    char *dest;
    dest = ipath->ptr + ipath_len;
    memcpy(dest, suf, (sizeof(char) * suf_len));

    return true;
}


/**
 * @brief create new element that makes up the directory tree.
 *
 * @param[in]  path  file path to the file we are currently looking at
 * @param[in]  name  name of the file we are currently looking at
 * @return file_node*  new element that makes up the directory tree
 */
static file_node *__new_file(const char * restrict path, char * restrict name){
    file_node *file;
    if ((file = (file_node *) malloc(sizeof(file_node)))){
        file->name = name;
        file->mode = 0;
        file->uid = 0;
        file->gid = 0;
        file->size = 0;

        file->link_path = NULL;
        file->link_mode = 0;
        file->link_invalid = true;

        file->children = NULL;
        file->children_num = 0;
        file->children_max = 0;

        file->errid = 0;
        file->noinfo = false;

        struct stat file_stat;
        if (! lstat(path, &file_stat)){
            file->mode = file_stat.st_mode;
            file->uid = file_stat.st_uid;
            file->gid = file_stat.st_gid;
            file->size = (file_stat.st_size > 0) ? file_stat.st_size : 0;

            if (S_ISLNK(file->mode)){
                char *link_path;
                if ((link_path = (char *) malloc(sizeof(char) * (file->size + 1)))){
                    ssize_t link_len;
                    link_len = readlink(path, link_path, file->size);

                    if (link_len > 0){
                        if (! stat(path, &file_stat)){
                            file->link_mode = file_stat.st_mode;
                            file->link_invalid = false;
                        }
                        else
                            file->errid = errno;
                    }
                    else
                        link_len = 0;

                    link_path[link_len] = '\0';
                    file->link_path = link_path;
                }
            }
            else
                file->link_invalid = false;
        }
        else {
            file->errid = errno;
            file->noinfo = true;
        }
    }
    return file;
}


/**
 * @brief append file to the directory tree under construction.
 *
 * @param[out] tree  the directory tree under construction
 * @param[in]  file  file to append to the directory tree
 * @return bool  successful or not
 *
 * @note any directory can have a virtually unlimited number of files.
 */
static bool __append_file(file_node *tree, file_node *file){
    if (tree->children_num == tree->children_max){
        size_t old_size, new_size;
        old_size = tree->children_max;
        new_size = old_size ? (old_size * 2) : 32;

        void *ptr;
        if ((old_size < new_size) && (ptr = realloc(tree->children, (sizeof(file_node *) * new_size)))){
            tree->children = (file_node **) ptr;
            tree->children_max = new_size;
        }
        else
            return false;
    }

    tree->size += file->size;
    tree->children[tree->children_num++] = file;
    return true;
}




/******************************************************************************
    * Comparison Functions used when qsort
******************************************************************************/


/**
 * @brief comparison function used when sort style is unspecified or specified as name
 *
 * @param[in]  a  pointer to file1
 * @param[in]  b  pointer to file2
 * @return int  comparison result
 */
static int __qcmp_name(const void *a, const void *b){
    return __fcmp_name(a, b, NULL);
}


/**
 * @brief comparison function used when sort style is specified as size
 *
 * @param[in]  a  pointer to file1
 * @param[in]  b  pointer to file2
 * @return int  comparison result
 */
static int __qcmp_size(const void *a, const void *b){
    return __fcmp_name(a, b, __fcmp_size);
}


/**
 * @brief comparison function used when sort style is specified as extension
 *
 * @param[in]  a  pointer to file1
 * @param[in]  b  pointer to file2
 * @return int  comparison result
 */
static int __qcmp_ext(const void *a, const void *b){
    return __fcmp_name(a, b, __fcmp_ext);
}




/**
 * @brief base comparison function that compares files by their name
 *
 * @param[in]  a  pointer to file1
 * @param[in]  b  pointer to file2
 * @param[in]  addition  additional comparison function used before comparison by file name
 * @return int  comparison result
 */
static int __fcmp_name(const void *a, const void *b, int (* const addition)(file_node *, file_node *)){
    file_node *file1, *file2;
    file1 = *((file_node **) a);
    file2 = *((file_node **) b);

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
static int __fcmp_size(file_node *file1, file_node *file2){
    return (int) (file2->size - file1->size);
}


/**
 * @brief additional comparison function that compares files by their extension
 *
 * @param[in]  file1
 * @param[in]  file2
 * @return int  comparison result
 */
static int __fcmp_ext(file_node *file1, file_node *file2){
    const char *ext1, *ext2;
    ext1 = __get_file_ext(file1->name);
    ext2 = __get_file_ext(file2->name);

    return strcmp(ext1, ext2);
}


/**
 * @brief extract extension from file name.
 *
 * @param[in]  name  target file name
 * @return const char*  string representing the file extension
 */
static const char *__get_file_ext(const char *name){
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
 * @brief display the directory tree and release the dynamic memory used for it.
 *
 * @param[out] tree  pre-constructed directory tree
 * @param[in]  opt  variable containing the result of option parse
 */
static void __destruct_dir_tree(file_node *tree, insp_opts *opt){
    fputs(
        "Permission      User     Group      Size\n"
        "=========================================\n"
    , stdout);

    __destruct_recursive(tree, opt, 0);
}


/**
 * @brief display and release the directory tree, recursively.
 *
 * @param[out] file  the file we are currently trying to display
 * @param[in]  opt  variable containing the result of option parse or NULL
 * @param[in]  depth  hierarchy in the directory tree of the file we are currently trying to display
 *
 * @note at the same time, release the dynamic memory that is no longer needed.
 * @note if NULL is passed to 'opt', just release the dynamic memory that is never used.
 */
static void __destruct_recursive(file_node *file, insp_opts *opt, unsigned int depth){
    int i;

    if (opt){
        if (! (file->errid && file->noinfo)){
            __print_file_mode(file->mode);
            __print_file_owner(file, opt->numeric_id);
            __print_file_size(file->size);
        }
        else
            fputs("       ???       ???       ???       ???    ", stdout);

        if (depth){
            for (i = depth; --i;)
                fputs("|   ", stdout);
            fputs("|-- ", stdout);
        }

        __print_file_name(file, opt, false);

        if (file->link_path && *(file->link_path))
            __print_file_name(file, opt, true);

        if (file->errid)
            fprintf(stdout, " (%s)", strerror(file->errid));

        fputc('\n', stdout);
    }

    free(file->name);

    if (file->link_path)
        free(file->link_path);

    if (file->children){
        i = 0;
        depth++;
        do
            __destruct_recursive(file->children[i++], opt, depth);
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
    char mode_str[11];
    mode_str[0] =
        S_ISREG(mode) ? '-' :
        S_ISDIR(mode) ? 'd' :
        S_ISCHR(mode) ? 'c' :
        S_ISBLK(mode) ? 'b' :
        S_ISFIFO(mode) ? 'p' :
        S_ISLNK(mode) ? 'l' :
        S_ISSOCK(mode) ? 's' :
        '\?';

    mode_str[1] = (mode & S_IRUSR) ? 'r' : '-';
    mode_str[2] = (mode & S_IWUSR) ? 'w' : '-';
    mode_str[3] = (mode & S_ISUID) ? ((mode & S_IXUSR) ? 's' : 'S') : ((mode & S_IXUSR) ? 'x' : '-');
    mode_str[4] = (mode & S_IRGRP) ? 'r' : '-';
    mode_str[5] = (mode & S_IWGRP) ? 'w' : '-';
    mode_str[6] = (mode & S_ISGID) ? ((mode & S_IXGRP) ? 's' : 'S') : ((mode & S_IXGRP) ? 'x' : '-');
    mode_str[7] = (mode & S_IROTH) ? 'r' : '-';
    mode_str[8] = (mode & S_IWOTH) ? 'w' : '-';
    mode_str[9] = (mode & S_ISVTX) ? ((mode & S_IXOTH) ? 't' : 'T') : ((mode & S_IXOTH) ? 'x' : '-');
    mode_str[10] = '\0';

    fprintf(stdout, "%s  ", mode_str);
}


/**
 * @brief display file owner on screen.
 *
 * @param[in]  file the file we are currently trying to display
 * @param[in]  numeric_id  whether to represent users and groups numerically
 *
 * @attention if the id exceeds 8 digits, print a string that represents a numeric excess.
 */
static void __print_file_owner(file_node *file, bool numeric_id){
    int i = 1;
    unsigned int id;
    struct passwd *passwd;
    struct group *group;
    const char *tmp = NULL;

    do {
        if (i){
            id = file->uid;
            if ((! numeric_id) && (passwd = getpwuid(id)))
                tmp = passwd->pw_name;
        }
        else {
            id = file->gid;
            if ((! numeric_id) && (group = getgrgid(id)))
                tmp = group->gr_name;
        }

        if (! (tmp && (strlen(tmp) <= 8))){
            if (id < 100000000){
                fprintf(stdout, "%8u  ", id);
                continue;
            }
            tmp = INSP_EXCESS_STR;
        }
        fprintf(stdout, "%8s  ", tmp);
    } while (i--);
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
        fprintf(stdout, "%6d B    ", ((int) size));
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
            fprintf(stdout, "%3d.%1d %cB    ", ((int) size), rem, units[i]);
        }
        else
            fputs(INSP_EXCESS_STR "    ", stdout);
    }
}


/**
 * @brief display file name on screen.
 *
 * @param[in]  file  the file we are currently trying to display
 * @param[in]  opt  variable containing the result of option parse
 * @param[in]  link_flag  whether or not to display the information of the link destination
 */
static void __print_file_name(file_node *file, insp_opts *opt, bool link_flag){
    char *name, *tmp, *format;
    mode_t mode;
    int offset = 0;

    if (! link_flag){
        name = file->name;
        mode = file->mode;
        offset = 4;
    }
    else {
        name = file->link_path;
        mode = file->link_mode;
    }

    tmp = name;
    while (*tmp){
        if (iscntrl(*tmp))
            *tmp = '\?';
        tmp++;
    }

    if (opt->color){
        tmp =
            file->link_invalid ? "31" :
            S_ISREG(mode) ? (
                (mode & S_ISUID) ? "37;41" :
                (mode & S_ISGID) ? "30;43" :
                (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) ? "1;32" :
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

        format = " -> \033[%sm%s\033[0m";
    }
    else {
        tmp = name;
        format = " -> %s";
    }

    fprintf(stdout, (format + offset), tmp, name);

    if (opt->classify){
        int indicator;
        indicator =
            (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH))) ? '*' :
            S_ISDIR(mode) ? '/' :
            S_ISFIFO(mode) ? '|' :
            S_ISSOCK(mode) ? '=' :
                '\0';

        if (indicator)
            fputc(indicator, stdout);
    }
}