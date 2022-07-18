#include "main.h"

#define CMDS_NUM 10


int main(int argc, char **argv){
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

    if (argc > 1){
        int cmd_index;
        cmd_index = arg_bsearch(argv[1], cmds_str, CMDS_NUM);

        if (cmd_index >= 0)
            return cmds_func[cmd_index](argc - 1, &argv[1]);
        else
            printf("dit: '%s' is not a dit command. See 'dit help'.\n", argv[1]);
    }
    else if (argc == 1)
        return help(1, argv);

    return -1;
}



int arg_bsearch(const char *target, const char **candidates, int size){
    int min, max, mid, tmp;
    min = 0;
    max = size - 1;

    while (min < max){
        mid = (min + max) / 2;
        tmp = strcmp(target, candidates[mid]);
        if (tmp){
            if (tmp > 0)
                min = mid + 1;
            else
                max = mid - 1;
        }
        else
            return mid;
    }

    if ((min == max) && (! strcmp(target, candidates[min])))
        return min;
    return -1;
}