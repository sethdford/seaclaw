#include "test_framework.h"
#include <stdio.h>
#include <string.h>

int hu__total = 0;
int hu__passed = 0;
int hu__failed = 0;
int hu__skipped = 0;
int hu__suite_active = 1;
const char *hu__suite_filter = NULL;
const char *hu__test_filter = NULL;
jmp_buf hu__jmp;

void run_integration_http_tests(void);
void run_integration_sqlite_tests(void);
void run_integration_imap_tests(void);

static void print_usage(const char *prog) {
    printf("Usage: %s [--suite=name] [--filter=name] [--help]\n", prog);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--suite=", 8) == 0)
            hu__suite_filter = argv[i] + 8;
        else if (strncmp(argv[i], "--filter=", 9) == 0)
            hu__test_filter = argv[i] + 9;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    printf("Human integration tests (human_core without HU_IS_TEST)\n");
    printf("=======================================================\n");
    if (hu__suite_filter)
        printf("Suite filter: %s\n", hu__suite_filter);
    if (hu__test_filter)
        printf("Test filter:  %s\n", hu__test_filter);

    HU_TEST_SUITE("Integration HTTP");
    run_integration_http_tests();
    HU_TEST_SUITE("Integration SQLite");
    run_integration_sqlite_tests();
    HU_TEST_SUITE("Integration IMAP");
    run_integration_imap_tests();

    HU_TEST_REPORT();
    HU_TEST_EXIT();
}
