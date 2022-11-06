#ifndef DIT_UNIT_TESTS
#define DIT_UNIT_TESTS

#ifndef NDEBUG


/******************************************************************************
    * Interface for Unit Test
******************************************************************************/

void test(int argc, char **argv, int cmd_id);


/******************************************************************************
    * Unit Test Functions
******************************************************************************/

int dit_test(void);
int config_test(void);
int convert_test(void);
int cp_test(void);
int erase_test(void);
int healthcheck_test(void);
int help_test(void);
int ignore_test(void);
int inspect_test(void);
int label_test(void);
int onbuild_test(void);
int optimize_test(void);
int reflect_test(void);
int setcmd_test(void);


#endif // NDEBUG

#endif // DIT_UNIT_TESTS