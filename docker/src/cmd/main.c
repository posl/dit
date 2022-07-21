#include "main.h"

#define CMDS_NUM 10


static int (* const __get_subcmd(char *))(int, char **);



int main(int argc, char **argv){
    if (--argc <= 0){
        fputs("dit: requires one subcommand. See 'dit help'.\n", stderr);
        return 1;
    }

    int (* subcmd)(int, char **);
    if (! (subcmd = __get_subcmd(*(++argv)))){
        fprintf(stderr, "dit: '%s' is not a dit command. See 'dit help'.\n", *argv);
        return 1;
    }

    return subcmd(argc, argv);
}


static int (* const __get_subcmd(char *target))(int, char **){
    const char *cmds_str[CMDS_NUM] = {
        "commit",
        "config",
        "convert",
        "erase",
        "healthcheck",
        "help",
        "ignore",
        "inspect",
        "label",
        "optimize"
    };
    int (* const cmds_func[CMDS_NUM])(int, char **) = {
        commit,
        config,
        convert,
        erase,
        healthcheck,
        help,
        ignore,
        inspect,
        label,
        optimize
    };

    int min, max, mid, tmp;
    min = 0;
    max = CMDS_NUM - 1;

    while (min < max){
        mid = (min + max) / 2;
        tmp = strcmp(target, cmds_str[mid]);
        if (tmp)
            if (tmp > 0)
                min = mid + 1;
            else
                max = mid - 1;
        else
            return cmds_func[mid];
    }

    if ((min == max) && (! strcmp(target, cmds_str[min])))
        return cmds_func[min];
    return NULL;
}






char *fget_line(FILE *stream, int block_size){
    char *line, *tmp;
    line = (tmp = (char *) malloc(sizeof(char) * block_size));

    int len = 0;
    while ((! fgets(tmp, block_size, stream)) && ((len += (int) strlen(tmp)) > 0)){
        if (line[len - 1] != '\n')
            if (! feof(stream)){
                line = (char *) realloc(line, sizeof(char) * (len + block_size));
                tmp = &line[len];
                continue;
            }
        else
            line[len - 1] = '\0';
        return line;
    }

    free(line);
    if (ferror(stream)){
        perror("fgets");
        exit(EXIT_FAILURE);
    }
    return NULL;
}