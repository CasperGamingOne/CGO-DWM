#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>

#include "util.h"

// Simple test framework macro
#define TEST(name) void name()

TEST(test_ecalloc_success) {
    size_t nmemb = 10;
    size_t size = sizeof(int);

    // Allocate memory
    int *p = (int *)ecalloc(nmemb, size);

    // Check pointer is not NULL
    assert(p != NULL);

    // Check memory is zeroed out
    for (size_t i = 0; i < nmemb; i++) {
        assert(p[i] == 0);
    }

    free(p);
    printf("test_ecalloc_success: PASS\n");
}

TEST(test_ecalloc_failure) {
    fflush(stdout);
    pid_t pid = fork();
    assert(pid >= 0); // fork shouldn't fail

    if (pid == 0) {
        // Child process
        // Close stderr so we don't spam the console with the "die" message
        fclose(stderr);

        // Attempt to allocate an impossible amount of memory
        // This should cause calloc to fail, which in turn calls die() which exits with 1
        ecalloc(SIZE_MAX, SIZE_MAX);

        // If we reach here, ecalloc didn't die! That's a failure.
        exit(0);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);

        // The child should have exited normally (via exit(1) in die())
        assert(WIFEXITED(status));
        // The exit status should be 1
        assert(WEXITSTATUS(status) == 1);

        printf("test_ecalloc_failure: PASS\n");
    }
}

int main(void) {
    printf("Running util.c tests...\n");
    test_ecalloc_success();
    test_ecalloc_failure();
    printf("All tests passed!\n");
    return 0;
}
