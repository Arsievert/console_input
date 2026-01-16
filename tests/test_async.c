#include "console_input.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static volatile int default_calls = 0;
static volatile int cmd_calls = 0;
static volatile int quit_calls = 0;

static void default_cb(const char *line, void *user_data) {
    (void)user_data;
    if (line) {
        default_calls++;
    }
}

static void cmd_cb(const char *line, void *user_data) {
    (void)user_data;
    (void)line;
    cmd_calls++;
}

static void quit_cb(const char *line, void *user_data) {
    (void)user_data;
    (void)line;
    quit_calls++;
    ci_request_stop_async_input();
}

static void reset_counters(void) {
    default_calls = 0;
    cmd_calls = 0;
    quit_calls = 0;
}

static void test_async_default_receives(void) {
    reset_counters();
    int saved_fd, write_fd;
    replace_stdin_with_pipe(NULL, 0, &saved_fd, &write_fd);

    ci_status st = ci_start_async_input("", default_cb, NULL);
    ASSERT_STATUS(CI_OK, st);
    ASSERT_TRUE(ci_async_is_running(), "async should be running");

    write(write_fd, "hello\n", 6);
    close(write_fd);

    wait_millis(50);
    ci_request_stop_async_input();
    ci_stop_async_input();
    ASSERT_TRUE(!ci_async_is_running(), "async should stop");
    ASSERT_EQ_INT(1, default_calls);

    restore_stdin_from_fd(saved_fd);
}

static void test_command_dispatch_and_default(void) {
    reset_counters();
    int saved_fd, write_fd;
    replace_stdin_with_pipe(NULL, 0, &saved_fd, &write_fd);

    ci_status st = ci_start_async_input("", default_cb, NULL);
    ASSERT_STATUS(CI_OK, st);
    ASSERT_STATUS(CI_OK, ci_register_command("ping", cmd_cb, NULL));

    write(write_fd, "ping\nother\n", 10);
    close(write_fd);

    /* wait for both callbacks or timeout */
    for (int i = 0; i < 20; i++) {
        if (cmd_calls >= 1 && default_calls >= 1) break;
        if (!ci_async_is_running() && (cmd_calls + default_calls) >= 2) break;
        wait_millis(10);
    }

    ci_stop_async_input();

    ASSERT_EQ_INT(1, cmd_calls);
    ASSERT_EQ_INT(1, default_calls); /* for 'other' */

    restore_stdin_from_fd(saved_fd);
}

static void test_command_replace_and_unregister(void) {
    reset_counters();
    int saved_fd, write_fd;
    replace_stdin_with_pipe(NULL, 0, &saved_fd, &write_fd);

    ci_status st = ci_start_async_input("", default_cb, NULL);
    ASSERT_STATUS(CI_OK, st);

    /* first registration */
    ASSERT_STATUS(CI_OK, ci_register_command("ping", cmd_cb, NULL));
    /* replace with quit_cb to distinguish */
    ASSERT_STATUS(CI_OK, ci_register_command("ping", quit_cb, NULL));

    write(write_fd, "ping\n", 5);
    close(write_fd);

    wait_millis(50);
    ci_stop_async_input();

    ASSERT_EQ_INT(0, cmd_calls);
    ASSERT_EQ_INT(1, quit_calls);

    /* unregister and ensure default handles */
    reset_counters();
    replace_stdin_with_pipe(NULL, 0, &saved_fd, &write_fd);

    st = ci_start_async_input("", default_cb, NULL);
    ASSERT_STATUS(CI_OK, st);
    ASSERT_STATUS(CI_OK, ci_register_command("ping", cmd_cb, NULL));
    ASSERT_STATUS(CI_OK, ci_unregister_command("ping"));

    write(write_fd, "ping\n", 5);
    close(write_fd);
    wait_millis(50);
    ci_stop_async_input();

    ASSERT_EQ_INT(1, default_calls);
    ASSERT_EQ_INT(0, cmd_calls);

    restore_stdin_from_fd(saved_fd);
}

static void test_stop_via_request(void) {
    reset_counters();
    int saved_fd, write_fd;
    replace_stdin_with_pipe(NULL, 0, &saved_fd, &write_fd);

    ci_status st = ci_start_async_input("", default_cb, NULL);
    ASSERT_STATUS(CI_OK, st);
    ASSERT_STATUS(CI_OK, ci_register_command("q", quit_cb, NULL));

    write(write_fd, "q\n", 2);
    close(write_fd);

    wait_millis(80);
    ci_stop_async_input();

    ASSERT_EQ_INT(1, quit_calls);
    ASSERT_TRUE(!ci_async_is_running(), "async stopped");

    restore_stdin_from_fd(saved_fd);
}

static void test_command_capacity_limit(void) {
    reset_counters();
    /* Ensure registry is clean */
    ci_start_async_input("", default_cb, NULL);
    ci_stop_async_input();

    for (int i = 0; i < CI_MAX_COMMANDS; i++) {
        char name[16];
        snprintf(name, sizeof(name), "cmd%d", i);
        ci_status st = ci_register_command(name, cmd_cb, NULL);
        ASSERT_STATUS(CI_OK, st);
    }
    ci_status overflow = ci_register_command("extra", cmd_cb, NULL);
    ASSERT_STATUS(CI_OVERFLOW, overflow);

    /* cleanup */
    for (int i = 0; i < CI_MAX_COMMANDS; i++) {
        char name[16];
        snprintf(name, sizeof(name), "cmd%d", i);
        ci_unregister_command(name);
    }

    /* reset state */
    quit_calls = 0;
    default_calls = 0;
    cmd_calls = 0;
}

int main(void) {
    test_async_default_receives();
    test_command_dispatch_and_default();
    test_command_replace_and_unregister();
    test_stop_via_request();
    test_command_capacity_limit();
    printf("test_async passed\n");
    return 0;
}
