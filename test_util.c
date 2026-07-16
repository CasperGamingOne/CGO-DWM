#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "util.h"

// Helper function to run a function in a child process, capture stderr, and check exit status and output
int run_death_test(void (*test_fn)(void), const char *expected_stderr_prefix, const char *expected_stderr_suffix, int expected_exit_status) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 0;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 0;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]);
        dup2(pipefd[1], STDERR_FILENO); // Redirect stderr to pipe
        dup2(pipefd[1], STDOUT_FILENO); // Also redirect stdout so printf doesn't repeat for tests
        close(pipefd[1]);

        test_fn();
        exit(0); // Should not reach here if test_fn calls die()
    } else {
        // Parent process
        close(pipefd[1]);

        char buffer[4096];
        ssize_t nbytes;
        size_t total_bytes = 0;

        while ((nbytes = read(pipefd[0], buffer + total_bytes, sizeof(buffer) - total_bytes - 1)) > 0) {
            total_bytes += nbytes;
            if (total_bytes >= sizeof(buffer) - 1) {
                fprintf(stderr, "Buffer overflow: stderr output too long\n");
                close(pipefd[0]);
                return 0;
            }
        }
        buffer[total_bytes] = '\0';
        close(pipefd[0]);

        int status;
        waitpid(pid, &status, 0);

        if (!WIFEXITED(status)) {
            fprintf(stderr, "Child did not exit normally\n");
            return 0;
        }

        if (WEXITSTATUS(status) != expected_exit_status) {
            fprintf(stderr, "Expected exit status %d, got %d\n", expected_exit_status, WEXITSTATUS(status));
            return 0;
        }

        // Check expected output
        if (expected_stderr_prefix) {
            if (strncmp(buffer, expected_stderr_prefix, strlen(expected_stderr_prefix)) != 0) {
                fprintf(stderr, "Expected prefix '%s', got '%s'\n", expected_stderr_prefix, buffer);
                return 0;
            }
        }

        if (expected_stderr_suffix) {
            size_t len = strlen(buffer);
            size_t suffix_len = strlen(expected_stderr_suffix);
            if (len < suffix_len || strcmp(buffer + len - suffix_len, expected_stderr_suffix) != 0) {
                fprintf(stderr, "Expected suffix '%s', got '%s'\n", expected_stderr_suffix, buffer);
                return 0;
            }
        }

        return 1;
    }
}

// Test functions
void test_normal_string() {
    die("hello");
}

void test_format_string() {
    die("hello %s", "world");
}

void test_errno_append() {
    errno = ENOENT;
    die("error:");
}

void test_errno_append_no_errno() {
    errno = 0;
    die("error:");
}

void test_empty_string() {
    die("");
}

void test_just_colon() {
    errno = EACCES;
    die(":");
}

int main() {
    int passed = 0;
    int total = 6;

    // Disable stdout buffering to prevent repeated output when fork is called
    setbuf(stdout, NULL);

    printf("Running test_normal_string...\n");
    if (run_death_test(test_normal_string, "hello\n", "hello\n", 1)) {
        passed++;
    } else {
        printf("FAILED\n");
    }

    printf("Running test_format_string...\n");
    if (run_death_test(test_format_string, "hello world\n", "hello world\n", 1)) {
        passed++;
    } else {
        printf("FAILED\n");
    }

    printf("Running test_errno_append...\n");
    char expected_errno_msg[1024];
    snprintf(expected_errno_msg, sizeof(expected_errno_msg), "error: %s\n", strerror(ENOENT));
    if (run_death_test(test_errno_append, expected_errno_msg, expected_errno_msg, 1)) {
        passed++;
    } else {
        printf("FAILED\n");
    }

    printf("Running test_errno_append_no_errno...\n");
    char expected_errno_msg_no_errno[1024];
    snprintf(expected_errno_msg_no_errno, sizeof(expected_errno_msg_no_errno), "error: %s\n", strerror(0));
    if (run_death_test(test_errno_append_no_errno, expected_errno_msg_no_errno, expected_errno_msg_no_errno, 1)) {
        passed++;
    } else {
        printf("FAILED\n");
    }

    printf("Running test_empty_string...\n");
    if (run_death_test(test_empty_string, "\n", "\n", 1)) {
        passed++;
    } else {
        printf("FAILED\n");
    }

    printf("Running test_just_colon...\n");
    char expected_colon_msg[1024];
    snprintf(expected_colon_msg, sizeof(expected_colon_msg), ": %s\n", strerror(EACCES));
    if (run_death_test(test_just_colon, expected_colon_msg, expected_colon_msg, 1)) {
        passed++;
    } else {
        printf("FAILED\n");
    }

    printf("\nTests passed: %d/%d\n", passed, total);
    return passed == total ? 0 : 1;
}
