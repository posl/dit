#include "main.h"



int convert(int argc, char **argv){
    FILE* fp;
    if ((fp = fopen("/dit/tmp/last-convert-result", "w"))){
        fputs(" < Dockerfile >\n", fp);
        fputs("ENV abc=123\n", fp);
        fputc('\n', fp);
        fputs(" < history-file >\n", fp);
        fputs("export abc=123\n", fp);
        fclose(fp);
        puts("convert");
        return 0;
    }
    else
        return 1;
}