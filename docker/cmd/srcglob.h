#ifndef DIT_COPY_SRCGLOB
#define DIT_COPY_SRCGLOB


/******************************************************************************
    * Macros
******************************************************************************/

#define SRCGLOB_FILE "/dit/tmp/srcglob.bin"




/******************************************************************************
    * Data Types
******************************************************************************/

/** Data type for storing the information about the results of source path expansion */
typedef struct {
    size_t total_num;       /** the total number of copy sources */
    size_t dirs_num;        /** the number of directories in copy sources */
    size_t written_size;    /** size of strings written to the rest of the binary file */
} srcglob_info;


/** Data type for storing the error information */
typedef struct {
    int type;               /** error type ('S' (standard) or 'O' (original)) */
    int code;               /** error code ('errno' or 'errcode') */
    size_t written_size;    /** size of a string written to the rest of the binary file */
} srcglob_err;


#endif // DIT_COPY_SRCGLOB