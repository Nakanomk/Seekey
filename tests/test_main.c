#include "unity.h"
#include "test_helpers.h"

extern int run_config_tests(void);
extern int run_tui_tests(void);
extern int run_keynames_tests(void);
extern int run_window_state_tests(void);

void setUp(void)
{
}

void tearDown(void)
{
    test_cleanup();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    int failures = 0;
    failures += run_config_tests();
    failures += run_tui_tests();
    failures += run_keynames_tests();
    failures += run_window_state_tests();
    test_cleanup();
    return failures;
}
