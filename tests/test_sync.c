#include "console_input.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void test_read_line_ok(void) {
    const char *input = "hello world\n";
    int saved_fd;
    replace_stdin_with_pipe(input, strlen(input), &saved_fd, NULL);

    char buf[32];
    ci_status st = ci_read_line(stdin, buf, sizeof(buf));
    ASSERT_STATUS(CI_OK, st);
    ASSERT_STR_EQ("hello world", buf);

    restore_stdin_from_fd(saved_fd);
}

static void test_read_line_overflow(void) {
    const char *input = "toolongline\nrest";
    int saved_fd;
    replace_stdin_with_pipe(input, strlen(input), &saved_fd, NULL);

    char buf[6];
    ci_status st = ci_read_line(stdin, buf, sizeof(buf));
    ASSERT_STATUS(CI_OVERFLOW, st);
    ASSERT_STR_EQ("toolo", buf);

    /* ensure remaining input can still be read */
    char leftover[8];
    st = ci_read_line(stdin, leftover, sizeof(leftover));
    ASSERT_STATUS(CI_OK, st);
    ASSERT_STR_EQ("rest", leftover);

    restore_stdin_from_fd(saved_fd);
}

static void test_read_line_eof(void) {
    const char *input = "";
    int saved_fd;
    replace_stdin_with_pipe(input, strlen(input), &saved_fd, NULL);

    char buf[8];
    ci_status st = ci_read_line(stdin, buf, sizeof(buf));
    ASSERT_STATUS(CI_EOF, st);

    restore_stdin_from_fd(saved_fd);
}

static void test_prompt_line_overflow_and_retry(void) {
    /* first prompt should overflow */
    const char *long_input = "thisiswaytoolongforthesize\n";
    int saved_fd;
    replace_stdin_with_pipe(long_input, strlen(long_input), &saved_fd, NULL);

    char buf[6];
    ci_status st = ci_prompt_line("? ", buf, sizeof(buf));
    ASSERT_STATUS(CI_OVERFLOW, st);
    ASSERT_STR_EQ("thisi", buf);
    restore_stdin_from_fd(saved_fd);

    /* second prompt should succeed with short input */
    const char *short_input = "short\n";
    char buf2[8];
    replace_stdin_with_pipe(short_input, strlen(short_input), &saved_fd, NULL);
    st = ci_prompt_line("? ", buf2, sizeof(buf2));
    ASSERT_STATUS(CI_OK, st);
    ASSERT_STR_EQ("short", buf2);
    restore_stdin_from_fd(saved_fd);
}

static void test_read_int_valid(void) {
    const char *input = "42\n";
    int saved_fd;
    replace_stdin_with_pipe(input, strlen(input), &saved_fd, NULL);

    int value = 0;
    ci_status st = ci_read_int("num: ", &value);
    ASSERT_STATUS(CI_OK, st);
    ASSERT_EQ_INT(42, value);

    restore_stdin_from_fd(saved_fd);
}

static void test_read_int_invalid_then_valid(void) {
    const char *input = "abc\n5\n";
    int saved_fd;
    replace_stdin_with_pipe(input, strlen(input), &saved_fd, NULL);

    int value = 0;
    int saved_stdout = suppress_stdout();
    ci_status st = ci_read_int("num: ", &value);
    restore_stdout(saved_stdout);

    ASSERT_STATUS(CI_OK, st);
    ASSERT_EQ_INT(5, value);

    restore_stdin_from_fd(saved_fd);
}

static void test_read_int_overflow(void) {
    const char *input = "999999999999999\n";
    int saved_fd;
    replace_stdin_with_pipe(input, strlen(input), &saved_fd, NULL);

    int value = 0;
    ci_status st = ci_read_int("num: ", &value);
    ASSERT_STATUS(CI_OVERFLOW, st);

    restore_stdin_from_fd(saved_fd);
}

static void test_read_long_valid(void) {
    const char *input = "123456\n";
    int saved_fd;
    replace_stdin_with_pipe(input, strlen(input), &saved_fd, NULL);

    long value = 0;
    ci_status st = ci_read_long("num: ", &value);
    ASSERT_STATUS(CI_OK, st);
    ASSERT_EQ_INT(123456, (int)value);

    restore_stdin_from_fd(saved_fd);
}

int main(void) {
    test_read_line_ok();
    test_read_line_overflow();
    test_read_line_eof();
    test_prompt_line_overflow_and_retry();
    test_read_int_valid();
    test_read_int_invalid_then_valid();
    test_read_int_overflow();
    test_read_long_valid();
    printf("test_sync passed\n");
    return 0;
}
