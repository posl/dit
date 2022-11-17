#ifndef DIT_UNIT_TESTS
#define DIT_UNIT_TESTS

#ifndef NDEBUG


/******************************************************************************
    * commonly used Macros
******************************************************************************/

#define COMPTESTS_NUM 3

#define COMPTEST_EQUAL    0
#define COMPTEST_LESSER   1
#define COMPTEST_GREATER  2


#define TMP_DIR "/dit/tmp/"

#define TMP_NAME1 "test1.tmp"
#define TMP_NAME2 "test2.tmp"

#define TMP_FILE1  TMP_DIR TMP_NAME1
#define TMP_FILE2  TMP_DIR TMP_NAME2


#define do_test(func) \
    do { \
        fprintf(stderr, "Testing %s:%u: "#func" ...\n", __FILE__, __LINE__); \
        func(); \
        fputs("Passed all tests!\n\n", stderr); \
    } while(false)

#define no_test()  fputs("No unit tests.\n\n", stderr)




/******************************************************************************
    * commonly used Data Types
******************************************************************************/

typedef const union {
    char *name;
    size_t size;
} comptest_elem;


typedef const struct {
    comptest_elem elem1;
    comptest_elem elem2;
    const int type;
} comptest_table;




/******************************************************************************
    * Interface for all Unit Tests
******************************************************************************/

void test(int argc, char **argv, int cmd_id);


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

void dit_test(void);
void config_test(void);
void convert_test(void);
void cp_test(void);
void erase_test(void);
void healthcheck_test(void);
void help_test(void);
void ignore_test(void);
void inspect_test(void);
void label_test(void);
void onbuild_test(void);
void optimize_test(void);
void reflect_test(void);
void setcmd_test(void);


/******************************************************************************
    * Utilities
******************************************************************************/

bool check_if_alphabetical_order(const char * const reprs[], size_t size);

bool comptest_result_check(int type, int result);

void print_progress_test_loop(int code_c, int type, int count);


#endif // NDEBUG

#endif // DIT_UNIT_TESTS