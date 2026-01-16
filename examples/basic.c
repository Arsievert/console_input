#include "console_input.h"
#include "console_input.h"

#include <stdio.h>
#include <unistd.h>

static volatile int keep_running = 1;

static void on_any_line(const char *line, void *user_data) {
    (void)user_data;
    if (line && line[0] != '\0') {
        printf("Async received (default): %s\n", line);
        fflush(stdout);
    }
}

static void on_quit(const char *line, void *user_data) {
    (void)line;
    (void)user_data;
    printf("Quit command received.\n");
    fflush(stdout);
    keep_running = 0;
    ci_request_stop_async_input();
}

static void on_ping(const char *line, void *user_data) {
    (void)line;
    (void)user_data;
    printf("pong\n");
    fflush(stdout);
}

int main(void) {
    /* ensure both threads flush immediately */
    setvbuf(stdout, NULL, _IONBF, 0);

    if (ci_start_async_input("(q/ping/any)> ", on_any_line, NULL) != CI_OK) {
        puts("Failed to start async input");
        return 1;
    }

    ci_register_command("q", on_quit, NULL);
    ci_register_command("ping", on_ping, NULL);

    int iterations = 0;
    while (keep_running) {
        printf("Working... %d\n", iterations++);
        usleep(100000); // simulate busy loop work
    }

    ci_stop_async_input();
    return 0;
}
