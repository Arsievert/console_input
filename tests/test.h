#ifndef TEST_H
#define TEST_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ASSERT_TRUE(cond, msg)                                                       \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "Assertion failed: %s (at %s:%d): %s\n", #cond, __FILE__, \
                    __LINE__, msg);                                                  \
            exit(1);                                                                 \
        }                                                                            \
    } while (0)

#define ASSERT_EQ_INT(expected, actual)                                              \
    do {                                                                             \
        if ((expected) != (actual)) {                                                \
            fprintf(stderr, "Assertion failed: expected %d, got %d (%s:%d)\n",     \
                    (int)(expected), (int)(actual), __FILE__, __LINE__);             \
            exit(1);                                                                 \
        }                                                                            \
    } while (0)

#define ASSERT_STATUS(expected, actual)                                              \
    do {                                                                             \
        if ((expected) != (actual)) {                                                \
            fprintf(stderr, "Status mismatch: expected %d, got %d (%s:%d)\n",      \
                    (int)(expected), (int)(actual), __FILE__, __LINE__);             \
            exit(1);                                                                 \
        }                                                                            \
    } while (0)

#define ASSERT_STR_EQ(expected, actual)                                              \
    do {                                                                             \
        if (strcmp((expected), (actual)) != 0) {                                     \
            fprintf(stderr, "String mismatch: expected '%s', got '%s' (%s:%d)\n",   \
                    (expected), (actual), __FILE__, __LINE__);                       \
            exit(1);                                                                 \
        }                                                                            \
    } while (0)

/* Replace stdin with a pipe. Optionally preload data and return write end. */
static inline int replace_stdin_with_pipe(const char *data, size_t len, int *saved_fd, int *write_fd_out) {
    int fds[2];
    if (pipe(fds) != 0) {
        return -1;
    }

    int original = dup(STDIN_FILENO);
    if (original < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }

    if (dup2(fds[0], STDIN_FILENO) < 0) {
        close(fds[0]);
        close(fds[1]);
        close(original);
        return -1;
    }

    /* reset EOF/error flags on the new stdin */
    clearerr(stdin);
    setvbuf(stdin, NULL, _IONBF, 0);

    close(fds[0]);

    if (data && len > 0) {
        (void)write(fds[1], data, len);
    }

    if (saved_fd) {
        *saved_fd = original;
    } else {
        close(original);
    }

    if (write_fd_out) {
        *write_fd_out = fds[1];
    } else {
        close(fds[1]);
    }

    return 0;
}

static inline void restore_stdin_from_fd(int saved_fd) {
    if (saved_fd >= 0) {
        dup2(saved_fd, STDIN_FILENO);
        close(saved_fd);
    }
}

static inline void wait_millis(int millis) {
    usleep((useconds_t)millis * 1000);
}

/* Temporarily redirect stdout to /dev/null; returns previous fd, or -1 on error. */
static inline int suppress_stdout(void) {
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull < 0) return -1;
    int saved = dup(STDOUT_FILENO);
    if (saved < 0) {
        close(devnull);
        return -1;
    }
    if (dup2(devnull, STDOUT_FILENO) < 0) {
        close(devnull);
        close(saved);
        return -1;
    }
    close(devnull);
    return saved;
}

static inline void restore_stdout(int saved_fd) {
    if (saved_fd >= 0) {
        dup2(saved_fd, STDOUT_FILENO);
        close(saved_fd);
    }
}

#endif /* TEST_H */
