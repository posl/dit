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

#define INSP_INITIAL_DIRS_MAX 32

#define INSP_EXCESS_STR " #EXCESS"


/** Data type for storing the results of option parse */
typedef struct {
    unsigned int color;    /** whether to colorize file name based on file mode */
    bool classify;         /** whether to append i to file name based on file mode */
    bool numeric_id;       /** whether to represent users and groups numerically */
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
    bool link_invalid;              /** whether this is an invalid symbolic link */

    struct file_node **children;    /** array for storing the children if this is a directory */
    size_t children_num;            /** the current number of the children */
    size_t children_max;            /** the current maximum length of the array */

    int errid;                      /** serial number of the error encountered */
    bool noinfo;                    /** whether the file information could not be obtained */
} file_node;


static int parse_opts(int argc, char **argv, insp_opts *opt);

static file_node *construct_dir_tree(const char *root);
static file_node *construct_recursive(const char *name);
static file_node *new_file(char *name);
static bool append_file(file_node *tree, file_node *file);

static int qcmp_name(const void *a, const void *b);
static int qcmp_size(const void *a, const void *b);
static int qcmp_ext(const void *a, const void *b);
static int fcmp_name(const void *a, const void *b, int (* fcmp)(const file_node *, const file_node *));
static int fcmp_size(const file_node *file1, const file_node *file2);
static int fcmp_ext(const file_node *file1, const file_node *file2);

static void destruct_dir_tree(file_node *tree, const insp_opts *opt);
static void destruct_recursive(file_node *file, const insp_opts *opt, size_t depth);
static void print_file_mode(mode_t mode);
static void print_file_owner(const file_node *file, bool numeric_id);
static void print_file_size(off_t size);
static void print_file_name(const file_node *file, const insp_opts *opt, bool link_flag);


/** comparison function used when qsort */
static int (* qcmp)(const void *, const void *) = qcmp_name;

/** global variable for interrupting the recursive processing */
static bool fatal_error = false;




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

    if ((i = parse_opts(argc, argv, &opt)))
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
        if ((tree = construct_dir_tree(path)))
            destruct_dir_tree(tree, &opt);
        else
            i = FAILURE;

        assert(argc > 0);
        if (--argc && (! fatal_error)){
            path = *(++argv);
            fputc('\n', stdout);
        }
        else
            return i;
    } while (true);
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
static int parse_opts(int argc, char **argv, insp_opts *opt){
    assert(opt);

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

    int c, i;
    while ((c = getopt_long(argc, argv, short_opts, long_opts, &i)) >= 0)
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
                qcmp = qcmp_size;
                break;
            case 'X':
                qcmp = qcmp_ext;
                break;
            case 1:
                inspect_manual();
                return NORMALLY_EXIT;
            case 0:
                if ((c = receive_expected_string(optarg, sort_args, ARGS_NUM, 2)) >= 0){
                    qcmp = c ? ((c == 1) ? qcmp_name : qcmp_size) : qcmp_ext;
                    break;
                }
                xperror_invalid_arg('O', c, long_opts[i].name, optarg);
                xperror_valid_args(sort_args, ARGS_NUM);
            default:
                xperror_suggestion(true);
                return ERROR_EXIT;
        }

    opt->color &= (unsigned int) isatty(fileno(stdout));
    assert(opt->color == ((bool) opt->color));

    return SUCCESS;
}




/******************************************************************************
    * Construct & Sort Phase
******************************************************************************/


/**
 * @brief construct the directory tree with details about each file also collected together.
 *
 * @param[in]  root  file path to the root of the directory tree
 * @return file_node*  the result of constructing
 */
static file_node *construct_dir_tree(const char *root){
    file_node *tree = NULL;

    if (root){
        tree = construct_recursive(root);

        if (tree && fatal_error){
            destruct_recursive(tree, NULL, 0);
            tree = NULL;
        }
    }
    return tree;
}


/**
 * @brief construct the directory tree, recursively.
 *
 * @param[in]  name  name of the file we are currently looking at
 * @return file_node*  the result of sub-constructing
 *
 * @note at the same time, sorts files in directory.
 */
static file_node *construct_recursive(const char *name){
    assert(name);

    file_node *file = NULL;
    size_t name_len;

    if ((name_len = strlen(name) + 1) > 0){
        char *dest;
        if ((dest = (char *) malloc(sizeof(char) * name_len))){
            memcpy(dest, name, (sizeof(char) * name_len));

            if ((file = new_file(dest))){
                if (S_ISDIR(file->mode)){
                    int status;
                    DIR *dir;

                    if ((! (status = chdir(name))) && (dir = opendir("."))){
                        struct dirent *entry;
                        file_node *tmp;

                        while ((entry = readdir(dir))){
                            name = entry->d_name;
                            assert(name);

                            if (check_if_valid_dirent(name)){
                                if ((! (tmp = construct_recursive(name))) || fatal_error)
                                    break;
                                if (! append_file(file, tmp)){
                                    destruct_recursive(tmp, NULL, 0);
                                    break;
                                }
                            }
                        }

                        closedir(dir);

                        if (fatal_error)
                            goto exit;

                        if (file->children)
                            qsort(file->children, file->children_num, sizeof(file_node *), qcmp);
                    }
                    else
                        file->errid = errno;

                    if ((! status) && chdir("..")){
                        fatal_error = true;
                        xperror_standards(NULL, errno);
                    }
                }
            }
            else
                free(dest);
        }
    }

exit:
    return file;
}




/**
 * @brief create new element that makes up the directory tree.
 *
 * @param[in]  name  name of the file we are currently looking at
 * @return file_node*  new element that makes up the directory tree
 */
static file_node *new_file(char *name){
    assert(name);

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
        if (! lstat(name, &file_stat)){
            file->mode = file_stat.st_mode;
            file->uid = file_stat.st_uid;
            file->gid = file_stat.st_gid;
            file->size = (file_stat.st_size > 0) ? file_stat.st_size : 0;

            if (S_ISLNK(file->mode)){
                char *link_path;
                if ((link_path = (char *) malloc(sizeof(char) * (file->size + 1)))){
                    ssize_t link_len;
                    link_len = readlink(name, link_path, file->size);

                    if (link_len > 0){
                        if (! stat(name, &file_stat)){
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
static bool append_file(file_node *tree, file_node *file){
    assert(tree);
    assert(file);

    if (tree->children_num == tree->children_max){
        size_t old_size, new_size;
        void *ptr;

        old_size = tree->children_max;
        new_size = old_size ? (old_size * 2) : INSP_INITIAL_DIRS_MAX;

        if ((old_size < new_size) && (ptr = realloc(tree->children, (sizeof(file_node *) * new_size)))){
            tree->children = (file_node **) ptr;
            tree->children_max = new_size;
        }
        else
            return false;
    }

    assert(tree->children);

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
static int qcmp_name(const void *a, const void *b){
    return fcmp_name(a, b, NULL);
}


/**
 * @brief comparison function used when sort style is specified as size
 *
 * @param[in]  a  pointer to file1
 * @param[in]  b  pointer to file2
 * @return int  comparison result
 */
static int qcmp_size(const void *a, const void *b){
    return fcmp_name(a, b, fcmp_size);
}


/**
 * @brief comparison function used when sort style is specified as extension
 *
 * @param[in]  a  pointer to file1
 * @param[in]  b  pointer to file2
 * @return int  comparison result
 */
static int qcmp_ext(const void *a, const void *b){
    return fcmp_name(a, b, fcmp_ext);
}




/**
 * @brief base comparison function that compares files by their name
 *
 * @param[in]  a  pointer to file1
 * @param[in]  b  pointer to file2
 * @param[in]  fcmp  additional comparison function used before comparison by file name
 * @return int  comparison result
 */
static int fcmp_name(const void *a, const void *b, int (* fcmp)(const file_node *, const file_node *)){
    assert(a);
    assert(b);

    file_node *file1, *file2;
    int i;

    file1 = *((file_node **) a);
    file2 = *((file_node **) b);
    assert(file1);
    assert(file2);

    return (fcmp && (i = fcmp(file1, file2))) ? i : strcmp(file1->name, file2->name);
}


/**
 * @brief additional comparison function that compares files by their size
 *
 * @param[in]  file1
 * @param[in]  file2
 * @return int  comparison result
 */
static int fcmp_size(const file_node *file1, const file_node *file2){
    assert(file1);
    assert(file2);

    int i = 0;

    if (file1->size != file2->size)
        i = (file1->size < file2->size) ? 1 : -1;

    return i;
}


/**
 * @brief additional comparison function that compares files by their extension
 *
 * @param[in]  file1
 * @param[in]  file2
 * @return int  comparison result
 */
static int fcmp_ext(const file_node *file1, const file_node *file2){
    assert(file1);
    assert(file2);

    const char *ext1, *ext2;

    ext1 = get_suffix(file1->name, '.', false);
    ext2 = get_suffix(file2->name, '.', false);

    return strcmp(ext1, ext2);
}




/******************************************************************************
    * Display & Release Phase
******************************************************************************/


/**
 * @brief display the directory tree and release the dynamic memory used for it.
 *
 * @param[out] tree  pre-constructed directory tree
 * @param[in]  opt  variable to store the results of option parse
 */
static void destruct_dir_tree(file_node *tree, const insp_opts *opt){
    assert(tree);
    assert(opt);

    fputs(
        "Permission      User     Group      Size\n"
        "=========================================\n"
    , stdout);

    destruct_recursive(tree, opt, 0);
}


/**
 * @brief display and release the directory tree, recursively.
 *
 * @param[out] file  the file we are currently trying to display
 * @param[in]  opt  variable to store the results of option parse or NULL
 * @param[in]  depth  hierarchy in the directory tree of the file we are currently trying to display
 *
 * @note at the same time, releases the dynamic memory that is no longer needed.
 * @note if NULL is passed to 'opt', just releases the dynamic memory that is never used.
 *
 * @attention note that if 'depth' wraps around, the hierarchical representation of directories is disturbed.
 */
static void destruct_recursive(file_node *file, const insp_opts *opt, size_t depth){
    assert(file);
    assert(file->name);

    size_t size;

    if (opt){
        if (! file->noinfo){
            print_file_mode(file->mode);
            print_file_owner(file, opt->numeric_id);
            print_file_size(file->size);
        }
        else
            fputs("       ???       ???       ???       ???    ", stdout);

        if (depth){
            for (size = depth; --size;)
                fputs("|   ", stdout);
            fputs("|-- ", stdout);
        }

        print_file_name(file, opt, false);

        if (file->link_path && *(file->link_path))
            print_file_name(file, opt, true);

        if (file->errid)
            fprintf(stdout, " (%s)", strerror(file->errid));

        fputc('\n', stdout);
    }

    free(file->name);

    if (file->link_path)
        free(file->link_path);

    if (file->children){
        size = file->children_num;
        depth++;

        for (file_node * const *p_file = file->children; size--; p_file++)
            destruct_recursive(*p_file, opt, depth);

        free(file->children);
    }
    free(file);
}




/**
 * @brief display file mode on screen.
 *
 * @param[in]  mode  file mode
 */
static void print_file_mode(mode_t mode){
    char output[] = "?rw-rw-rw-  ";

    switch ((mode & S_IFMT)){
        case S_IFREG:
            *output = '-';
            break;
        case S_IFDIR:
            *output = 'd';
            break;
        case S_IFCHR:
            *output = 'c';
            break;
        case S_IFBLK:
            *output = 'b';
            break;
        case S_IFIFO:
            *output = 'p';
            break;
        case S_IFLNK:
            *output = 'l';
            break;
        case S_IFSOCK:
            *output = 's';
    }


#define unset_rwprm(idx, mask) \
    if (! (mode & (mask))) \
        output[idx] = '-';

#define update_xprm(idx, mask, st_mask, st_only, st_too) \
    switch ((mode & ((mask) | (st_mask)))){ \
        case (mask): \
            output[idx] = 'x'; \
            break; \
        case (st_mask): \
            output[idx] = (st_only); \
            break; \
        case ((mask) | (st_mask)): \
            output[idx] = (st_too); \
            break; \
    }

    unset_rwprm(1, S_IRUSR)
    unset_rwprm(2, S_IWUSR)
    update_xprm(3, S_IXUSR, S_ISUID, 'S', 's')

    unset_rwprm(4, S_IRGRP)
    unset_rwprm(5, S_IWGRP)
    update_xprm(6, S_IXGRP, S_ISGID, 'S', 's')

    unset_rwprm(7, S_IROTH)
    unset_rwprm(8, S_IWOTH)
    update_xprm(9, S_IXOTH, S_ISVTX, 'T', 't')


    fputs(output, stdout);
}




/**
 * @brief display file owner on screen.
 *
 * @param[in]  file  the file we are currently trying to display
 * @param[in]  numeric_id  whether to represent users and groups numerically
 *
 * @attention if the id exceeds 8 digits, prints a string that represents a numeric excess.
 */
static void print_file_owner(const file_node *file, bool numeric_id){
    assert(file);
    assert((file->uid >= 0) && (file->uid <= UINT_MAX));
    assert((file->gid >= 0) && (file->gid <= UINT_MAX));

    const char *name;
    int i = 1;
    unsigned int id;
    struct passwd *passwd;
    struct group *group;

    do {
        name = NULL;

        if (i){
            id = file->uid;
            if ((! numeric_id) && (passwd = getpwuid(id)))
                name = passwd->pw_name;
        }
        else {
            id = file->gid;
            if ((! numeric_id) && (group = getgrgid(id)))
                name = group->gr_name;
        }

        if (! (name && (strlen(name) <= 8))){
            if (id < 100000000){
                fprintf(stdout, "%8u  ", id);
                continue;
            }
            name = INSP_EXCESS_STR;
        }
        fprintf(stdout, "%8s  ", name);
    } while (i--);

    assert(i == -1);
}




/**
 * @brief display file size on screen.
 *
 * @param[in]  size  file size
 *
 * @attention cannot be handle file size exceeding the upper limit of integer type.
 */
static void print_file_size(off_t size){
    assert(size >= 0);

    unsigned int i = 0, rem = 0;
    char unit = '\0';
    const char *format;

    while (size >= 1000){
        i++;
        rem = size % 1000;
        size /= 1000;
    }

    if (i < 8){
        if (i){
            rem /= 100;
            unit = " kMGTPEZ"[i];
            format = "%3u.%1u %cB    ";
        }
        else
            format = "%6u B    ";

        fprintf(stdout, format, ((unsigned int) size), rem, unit);
    }
    else
        fputs(INSP_EXCESS_STR "    ", stdout);
}




/**
 * @brief display file name on screen.
 *
 * @param[in]  file  the file we are currently trying to display
 * @param[in]  opt  variable to store the results of option parse
 * @param[in]  link_flag  whether or not to display the information of the link destination
 */
static void print_file_name(const file_node *file, const insp_opts *opt, bool link_flag){
    assert(file);
    assert(opt);

    char *name, *tmp;
    mode_t mode;
    int i = 0;
    const char *format;

    if (! link_flag){
        name = file->name;
        mode = file->mode;
        i = 4;
    }
    else {
        name = file->link_path;
        mode = file->link_mode;
    }

    assert(name);
    for (tmp = name; *tmp; tmp++)
        if (iscntrl((unsigned char) *tmp))
            *tmp = '\?';

    if (opt->color){
        if (! file->link_invalid)
            switch ((mode & S_IFMT)){
                case S_IFREG:
                    tmp =
                        (mode & S_ISUID) ? "37;41" :
                        (mode & S_ISGID) ? "30;43" :
                        (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) ? "1;32" :
                            "0";
                    break;
                case S_IFDIR:
                    switch ((mode & (S_ISVTX | S_IWOTH))){
                        case 0:
                            tmp = "1;34";
                            break;
                        case S_ISVTX:
                            tmp = "37;44";
                            break;
                        case S_IWOTH:
                            tmp = "34;42";
                            break;
                        default:
                            tmp = "30;42";
                    }
                    break;
                case S_IFCHR:
                case S_IFBLK:
                    tmp = "1;33";
                    break;
                case S_IFIFO:
                    tmp = "33";
                    break;
                case S_IFLNK:
                    tmp = "1;36";
                    break;
                case S_IFSOCK:
                    tmp = "1;35";
                    break;
                default:
                    tmp = "0";
            }
        else
            tmp = "31";

        format = " -> \033[%sm%s\033[0m";
    }
    else {
        tmp = name;
        format = " -> %s";
    }

    fprintf(stdout, (format + i), tmp, name);

    if (opt->classify){
        switch ((mode & S_IFMT)){
            case S_IFREG:
                if (! (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
                    return;
                i = '*';
                break;
            case S_IFDIR:
                i = '/';
                break;
            case S_IFIFO:
                i = '|';
                break;
            case S_IFSOCK:
                i = '=';
                break;
            default:
                return;
        }
        assert(i);
        fputc(i, stdout);
    }
}




#ifndef NDEBUG


/******************************************************************************
    * Unit Test Functions
******************************************************************************/


static void new_file_test(void);
static void append_file_test(void);

static void fcmp_name_test(void);
static void fcmp_size_test(void);
static void fcmp_ext_test(void);




void inspect_test(void){
    do_test(new_file_test);
    do_test(append_file_test);

    do_test(fcmp_name_test);
    do_test(fcmp_size_test);
    do_test(fcmp_ext_test);
}




static void new_file_test(void){
    uid_t uid;
    gid_t gid;
    file_node *file;
    mode_t mode;

    uid = geteuid();
    gid = getegid();


    // when specifying a regular file

    int fd;

    assert((fd = open(TMP_FILE1, (O_RDWR | O_CREAT | O_TRUNC))) != -1);
    assert(! close(fd));

    assert((file = new_file(TMP_FILE1)));
    assert(! strcmp(file->name, TMP_FILE1));

    mode = file->mode;
    assert(S_ISREG(mode));

    assert(file->uid == uid);
    assert(file->gid == gid);
    assert(! file->size);

    assert(! (file->link_path || file->link_mode || file->link_invalid));
    assert(! (file->children || file->children_num || file->children_max));
    assert(! (file->errid || file->noinfo));

    free(file);


    // when specifying a symbolic link

    assert(! symlink(TMP_FILE1, TMP_FILE2));

    assert((file = new_file(TMP_FILE2)));
    assert(! strcmp(file->name, TMP_FILE2));

    assert(S_ISLNK(file->mode));
    assert(file->uid == uid);
    assert(file->gid == gid);
    assert(file->size == strlen(TMP_FILE1));

    assert(! strcmp(file->link_path, TMP_FILE1));
    assert(file->link_mode == mode);
    assert(! file->link_invalid);

    assert(! (file->children || file->children_num || file->children_max));
    assert(! (file->errid || file->noinfo));

    free(file->link_path);
    free(file);


    // when specifying a non-existing file

    assert(! unlink(TMP_FILE1));

    assert((file = new_file(TMP_FILE1)));
    assert(! strcmp(file->name, TMP_FILE1));

    assert(! (file->mode || file->uid || file->gid || file->size));

    assert(! file->link_path);
    assert(! file->link_mode);
    assert(file->link_invalid);

    assert(! (file->children || file->children_num || file->children_max));

    assert(file->errid == ENOENT);
    assert(file->noinfo);

    free(file);


    // when specifying an invalid symbolic link

    assert((file = new_file(TMP_FILE2)));
    assert(! strcmp(file->name, TMP_FILE2));

    assert(S_ISLNK(file->mode));
    assert(file->uid == uid);
    assert(file->gid == gid);
    assert(file->size == strlen(TMP_FILE1));

    assert(! strcmp(file->link_path, TMP_FILE1));
    assert(! file->link_mode);
    assert(file->link_invalid);

    assert(! (file->children || file->children_num || file->children_max));

    assert(file->errid == ENOENT);
    assert(! file->noinfo);

    free(file->link_path);
    free(file);


    assert(! unlink(TMP_FILE2));
}




static void append_file_test(void){
    file_node node, *file;
    int i = 0;
    off_t total_size = 0;

    node.size = 0;
    node.children = NULL;
    node.children_num = 0;
    node.children_max = 0;


    // when storing file nodes in order

    do {
        assert((file = (file_node *) malloc(sizeof(file_node))));
        file->size = rand() / (INSP_INITIAL_DIRS_MAX * 2);

        fprintf(stderr, "  Appending the %2dth element of size %d ...\n", i, ((int) file->size));

        assert(append_file(&node, file));

        total_size += file->size;
        assert(node.size == total_size);

        assert(node.children);
        assert(node.children[i++] == file);
        assert(node.children_num == i);

        free(file);

        if (i <= INSP_INITIAL_DIRS_MAX)
            assert(node.children_max == INSP_INITIAL_DIRS_MAX);
        else {
            assert(node.children_max == (INSP_INITIAL_DIRS_MAX * 2));
            break;
        }
    } while (true);

    assert(i == (INSP_INITIAL_DIRS_MAX + 1));

    free(node.children);
}




static void fcmp_name_test(void){
    // changeable part for updating test cases
    comptest_table table[] = {
        { { .name = "dit_version"   }, { .name = "dit_version"  }, equal   },
        { { .name = "su-exec"       }, { .name = "su-exec"      }, equal   },
        { { .name = ".vscode"       }, { .name = ".vscode"      }, equal   },
        { { .name = "etc"           }, { .name = "mnt"          }, lesser  },
        { { .name = ".bashrc"       }, { .name = ".profile"     }, lesser  },
        { { .name = "."             }, { .name = ".."           }, lesser  },
        { { .name = ".dockerignore" }, { .name = ".dit_history" }, greater },
        { { .name = "abc.txt"       }, { .name = "abc.csv"      }, greater },
        { { .name = "123 456"       }, { .name = "123\t456"     }, greater },
        { { .name =  0              }, { .name =  0             }, end     }
    };

    int i;
    file_node *file1, *file2, node1, node2;

    file1 = &node1;
    file2 = &node2;

    for (i = 0; table[i].type != end; i++){
        node1.name = table[i].elem1.name;
        node2.name = table[i].elem2.name;
        assert(check_if_correct_cmp_result(table[i].type, fcmp_name(&file1, &file2, NULL)));

        print_progress_test_loop('C', table[i].type, i);
        fprintf(stderr, "%-13s  %s\n", node1.name, node2.name);
    }
}




static void fcmp_size_test(void){
    // changeable part for updating test cases
    comptest_table table[] = {
        { { .size =    0 }, { .size =    0 }, equal   },
        { { .size =   32 }, { .size =   32 }, equal   },
        { { .size =  195 }, { .size =  195 }, equal   },
        { { .size =    8 }, { .size =    0 }, lesser  },
        { { .size = 1270 }, { .size =   15 }, lesser  },
        { { .size = 2048 }, { .size = 1024 }, lesser  },
        { { .size =   60 }, { .size =  122 }, greater },
        { { .size =  672 }, { .size = 3572 }, greater },
        { { .size =    5 }, { .size =    6 }, greater },
        { { .size =    0 }, { .size =    0 }, end     }
    };

    int i;
    file_node node1, node2;

    for (i = 0; table[i].type != end; i++){
        node1.size = table[i].elem1.size;
        node2.size = table[i].elem2.size;
        assert(check_if_correct_cmp_result(table[i].type, fcmp_size(&node1, &node2)));

        print_progress_test_loop('C', table[i].type, i);
        fprintf(stderr, "%4d  %4d\n", ((int) node1.size), ((int) node2.size));
    }
}




static void fcmp_ext_test(void){
    // changeable part for updating test cases
    comptest_table table[] = {
        { { .name = "config.stat"      }, { .name = "optimize.stat"            }, equal   },
        { { .name = "properties.json"  }, { .name = "tasks.json"               }, equal   },
        { { .name = "bin"              }, { .name = "sbin"                     }, equal   },
        { { .name = "ignore.json.dock" }, { .name = "ignore.json.hist"         }, lesser  },
        { { .name = "build"            }, { .name = "docker-compose.build.yml" }, lesser  },
        { { .name = "main.c"           }, { .name = "main.o"                   }, lesser  },
        { { .name = "Dockerfile.draft" }, { .name = ".dockerignore"            }, greater },
        { { .name = "exec.sh"          }, { .name = "exec.bash"                }, greater },
        { { .name = "index.html"       }, { .name = "html"                     }, greater },
        { { .name =  0                 }, { .name =  0                         }, end     }
    };

    int i;
    file_node node1, node2;

    for (i = 0; table[i].type != end; i++){
        node1.name = table[i].elem1.name;
        node2.name = table[i].elem2.name;
        assert(check_if_correct_cmp_result(table[i].type, fcmp_ext(&node1, &node2)));

        print_progress_test_loop('C', table[i].type, i);
        fprintf(stderr, "%-16s  %s\n", node1.name, node2.name);
    }
}




#endif // NDEBUG