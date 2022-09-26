/**
 * @file _inspect.c
 *
 * Copyright (c) 2022 Tsukasa Inada
 *
 * @brief Described the dit command 'inspect', that shows the directory tree.
 * @author Tsukasa Inada
 * @date 2022/07/18
 */

#include "main.h"

#define SORTS_NUM 3
#define comp(sort_style)  __comp_func_##sort_style

#define HEADER_LEN 42
#define EXCESS_STR " #EXCESS"


/** Data type for storing comparison function used when qsort */
typedef int (* comp_func)(const void *, const void *);


/** Data type for storing the results of option parse */
typedef struct {
    bool color;         /** whether to colorize file name based on file mode */
    bool classify;      /** whether to append indicator to file name based on file mode */
    bool numeric_id;    /** whether to represent users and groups numerically */
    comp_func comp;     /** comparison function used when qsort */
} insp_opts;


/** Data type that is applied to the smallest element that makes up the directory tree */
typedef struct file_node{
    char *name;
    mode_t mode;
    uid_t uid;
	gid_t gid;
    off_t size;

    char *link_path;                /** file name of link destination if this is a symbolic link */
    mode_t link_mode;               /** file mode of link destination if this is a symbolic link */
    bool link_invalid;              /** whether this is a invalid symbolic link */

    struct file_node **children;    /** array for storing the children if this is a directory */
    size_t children_num;            /** the current number of the children */
    size_t children_max;            /** the current maximum length of the array */

    int err_id;                     /** serial number of the error encountered */
    bool noinfo;                    /** whether the file information could not be obtained */
} file_node;


/** Data type for achieving the virtually infinite length of file path */
typedef struct {
    char *ptr;     /** file path */
    size_t max;    /** the current maximum length of file path */
} inf_path;


static int __parse_opts(int argc, char **argv, insp_opts *opt);

static file_node *__construct_dir_tree(const char *base_path, insp_opts *opt);
static file_node *__construct_recursive(inf_path *ipath, size_t ipath_len, char *name, comp_func comp);
static bool __concat_inf_path(inf_path *ipath, size_t ipath_len, const char *suf, size_t suf_len);
static file_node *__new_file(char *path, char *name);
static bool __append_file(file_node *tree, file_node *file);

static int __comp_func_name(const void *a, const void *b);
static int __comp_func_size(const void *a, const void *b);
static int __comp_func_extension(const void *a, const void *b);
static int __fcmp_name(const void *a, const void *b, int (* const addition)(file_node *, file_node *));
static int __fcmp_size(file_node *file1, file_node *file2);
static int __fcmp_extension(file_node *file1, file_node *file2);
static const char *__get_file_extension(const char *name);

static void __display_dir_tree(file_node *tree, insp_opts *opt);
static void __display_recursive(file_node *file, insp_opts *opt, unsigned int depth);
static void __print_file_mode(mode_t mode);
static void __print_file_user(uid_t uid, bool numeric_id);
static void __print_file_group(gid_t gid, bool numeric_id);
static void __print_file_size(off_t size);
static void __print_file_name(char *name, mode_t mode, bool link_invalid, bool color, bool classify);




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
 * @attention try this with '--help' option for more information.
 */
int inspect(int argc, char **argv){
    int i;
    insp_opts opt;

    if ((i = __parse_opts(argc, argv, &opt))){
        if (i > 0)
            return 0;

        xperror_suggestion(true);
        return 1;
    }

    setvbuf(stdout, NULL, _IOFBF, 0);

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

    file_node *tree;
    int exit_status = 0;

    do {
        if ((tree = __construct_dir_tree(path, &opt)))
            __display_dir_tree(tree, &opt);
        else
            exit_status = 1;

        if (--argc){
            path = *(++argv);
            putchar('\n');
        }
        else
            return exit_status;
    } while(1);
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

    const char * const sort_args[SORTS_NUM] = {
        "extension",
        "name",
        "size"
    };

    opt->color = false;
    opt->classify = false;
    opt->numeric_id = false;
    opt->comp = comp(name);

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
                opt->comp = comp(size);
                break;
            case 'X':
                opt->comp = comp(extension);
                break;
            case 1:
                inspect_manual();
                return 1;
            case 0:
                if ((c = receive_expected_string(optarg, sort_args, SORTS_NUM, 2)) >= 0){
                    opt->comp = (c ? ((c == 1) ? comp(name) : comp(size)) : comp(extension));
                    break;
                }
                xperror_invalid_optarg(c, long_opts[i].name, optarg);
                xperror_valid_args(sort_args, SORTS_NUM);
            default:
                return -1;
        }
    }
    return 0;
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

    size_t path_len;
    if ((path_len = strlen(base_path)) > 0){
        inf_path ipath;
        ipath.max = 1024;

        if ((ipath.ptr = (char *) malloc(sizeof(char) * ipath.max))){
            if (__concat_inf_path(&ipath, 0, base_path, path_len)){
                char *name;
                if ((name = xstrndup(base_path, path_len))){
                    if (! (tree = __construct_recursive(&ipath, path_len, name, opt->comp)))
                        free(name);
                }
            }
            free(ipath.ptr);
        }
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
static file_node *__construct_recursive(inf_path *ipath, size_t ipath_len, char *name, comp_func comp){
    file_node *file;
    file = __new_file(ipath->ptr, name);

    if (file && S_ISDIR(file->mode)){
        DIR *dir;
        if ((dir = opendir(ipath->ptr))){
            if (__concat_inf_path(ipath, ipath_len++, "/", 1)){
                struct dirent *entry;
                size_t name_len;
                file_node *tmp;

                while ((entry = readdir(dir))){
                    name = entry->d_name;
                    if ((name[0] == '.') && (! name[1 + ((name[1] != '.') ? 0 : 1)]))
                        continue;

                    name_len = strlen(name);
                    if ((name = xstrndup(name, name_len))){
                        if (__concat_inf_path(ipath, ipath_len, name, name_len)){
                            if ((tmp = __construct_recursive(ipath, ipath_len + name_len, name, comp))){
                                if (__append_file(file, tmp))
                                    continue;
                                free(tmp);
                            }
                        }
                        free(name);
                    }
                    break;
                }

                if (file->children)
                    qsort(file->children, file->children_num, sizeof(file_node *), comp);
            }
            closedir(dir);
        }
        else
            file->err_id = errno;
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
 * @return file_node*  new element that makes up the directory tree
 */
static file_node *__new_file(char *path, char *name){
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

        file->err_id = 0;
        file->noinfo = false;

        struct stat file_stat;
        if (! lstat(path, &file_stat)){
            file->mode = file_stat.st_mode;
            file->uid = file_stat.st_uid;
            file->gid = file_stat.st_gid;
            file->size = file_stat.st_size;

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
        }
        else {
            file->err_id = errno;
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
        void *ptr;
        if (! tree->children){
            tree->children_max = 128;
            ptr = malloc(sizeof(file_node *) * tree->children_max);
        }
        else {
            tree->children_max *= 2;
            ptr = realloc(tree->children, sizeof(file_node *) * tree->children_max);
        }

        if (ptr)
            tree->children = (file_node **) ptr;
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
static int __comp_func_name(const void *a, const void *b){
    return __fcmp_name(a, b, NULL);
}


/**
 * @brief comparison function used when sort style is specified as size
 *
 * @param[in]  a  pointer to file1
 * @param[in]  b  pointer to file2
 * @return int  comparison result
 */
static int __comp_func_size(const void *a, const void *b){
    return __fcmp_name(a, b, __fcmp_size);
}


/**
 * @brief comparison function used when sort style is specified as extension
 *
 * @param[in]  a  pointer to file1
 * @param[in]  b  pointer to file2
 * @return int  comparison result
 */
static int __comp_func_extension(const void *a, const void *b){
    return __fcmp_name(a, b, __fcmp_extension);
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
static int __fcmp_extension(file_node *file1, file_node *file2){
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
 * @brief display the directory tree, and release the heap space used for it.
 *
 * @param[out] tree  pre-constructed directory tree
 * @param[in]  opt  variable containing the result of option parse
 */
static void __display_dir_tree(file_node *tree, insp_opts *opt){
    const char header[HEADER_LEN] = " Permission      User     Group      Size";
    puts(header);

    for (int i = HEADER_LEN; i--;)
        putchar('=');
    putchar('\n');

    __display_recursive(tree, opt, 0);
}


/**
 * @brief display and release the directory tree, recursively.
 *
 * @param[out] file  the file we are currently trying to display
 * @param[in]  opt  variable containing the result of option parse
 * @param[in]  depth  hierarchy in the directory tree of the file we are currently trying to display
 *
 * @note at the same time, release the data that is no longer needed.
 */
static void __display_recursive(file_node *file, insp_opts *opt, unsigned int depth){
    int i;

    if (! (file->err_id && file->noinfo)){
        __print_file_mode(file->mode);
        __print_file_user(file->uid, opt->numeric_id);
        __print_file_group(file->gid, opt->numeric_id);
        __print_file_size(file->size);
    }
    else {
        putchar(' ');
        for (i = 4; i--;)
            fputs("       ???", stdout);
    }
    fputs("    ", stdout);

    if (depth){
        for (i = depth; --i;)
            fputs("|   ", stdout);
        fputs("|-- ", stdout);
    }

    __print_file_name(file->name, file->mode, file->link_invalid, opt->color, opt->classify);
    free(file->name);

    if (file->link_path){
        fputs(" -> ", stdout);
        __print_file_name(file->link_path, file->link_mode, file->link_invalid, opt->color, opt->classify);
        free(file->link_path);
    }
    else if (file->err_id)
        printf("  (%s)", strerror(file->err_id));

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
        '\?';
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

    printf(" %s  ", S);
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
    do {
        if (! numeric_id){
            struct passwd *passwd;
            if ((passwd = getpwuid(uid)) && (strlen(passwd->pw_name) <= 8)){
                printf("%8s", passwd->pw_name);
                break;
            }
        }
        if (uid < 100000000)
            printf("%8d", uid);
        else
            fputs(EXCESS_STR, stdout);
    } while (0);

    fputs("  ", stdout);
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
    do {
        if (! numeric_id){
            struct group *group;
            if ((group = getgrgid(gid)) && (strlen(group->gr_name) <= 8)){
                printf("%8s", group->gr_name);
                break;
            }
        }
        if (gid < 100000000)
            printf("%8d", gid);
        else
            fputs(EXCESS_STR, stdout);
    } while (0);

    fputs("  ", stdout);
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
        printf("%6d B", (int) size);
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
            printf("%3d.%1d %cB", (int) size, rem, units[i]);
        }
        else
            fputs(EXCESS_STR, stdout);
    }
}


/**
 * @brief display file name on screen.
 *
 * @param[out] name  file name
 * @param[in]  mode  file mode
 * @param[in]  link_invalid  whether file name is relative to a file that is an invalid symlink
 * @param[in]  color  whether to colorize file name based on file mode
 * @param[in]  classify  whether to append indicator to file name based on file mode
 */
static void __print_file_name(char *name, mode_t mode, bool link_invalid, bool color, bool classify){
    char *tmp;
    tmp = name;
    while (*tmp){
        if (iscntrl(*tmp))
            *tmp = '\?';
        tmp++;
    }

    if (color){
        tmp =
            link_invalid ? "31" :
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
        printf("\033[%sm%s\033[0m", tmp, name);
    }
    else
        fputs(name, stdout);

    if (classify){
        int indicator;
        indicator =
            (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH))) ? '*' :
            S_ISDIR(mode) ? '/' :
            S_ISFIFO(mode) ? '|' :
            S_ISSOCK(mode) ? '=' :
            '\0';
        if (indicator)
            putchar(indicator);
    }
}