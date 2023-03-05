#ifndef DIT_COPY_SRCGLOB
#define DIT_COPY_SRCGLOB


#define SRCGLOB_FILE_B "/dit/tmp/srcglob.buf"
#define SRCGLOB_FILE_D "/dit/tmp/srcglob.dat"


/** Data type for storing the information about the results of source path expansion */
typedef struct {
    size_t total_num;       /** the total number of copy sources */
    size_t dirs_num;        /** the number of directories in copy sources */
    size_t written_size;    /** size of strings written to the rest of the binary file */
} srcglob_info;


#endif // DIT_COPY_SRCGLOB