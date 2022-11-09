#ifndef DIT_UNIT_TESTS
#define DIT_UNIT_TESTS

#ifndef NDEBUG


/******************************************************************************
    * commonly used Functions
******************************************************************************/

#define do_test(func) \
    do { \
        fprintf(stdout, "Testing %s:%u: "#func" ...\n", __FILE__, __LINE__); \
        func(); \
        fputs("Passed all unit tests.\n\n", stdout); \
    } while(false)




/******************************************************************************
    * Interface for Unit Test
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


#endif // NDEBUG

#endif // DIT_UNIT_TESTS