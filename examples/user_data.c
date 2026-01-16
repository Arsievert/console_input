#include "console_input.h"

#include <stdio.h>
#include <unistd.h>

typedef struct {
    int count;
} line_counter;

static volatile int keep_running = 1;

static void on_line(const char *line, void *user_data) {
    line_counter *counter = (line_counter *)user_data;
    if (counter) {
        counter->count++;
    }

    printf("[%d] Received: %s\n", counter ? counter->count : -1, line ? line : "(null)");
    fflush(stdout);

    if (line && line[0] == 'q') {
        keep_running = 0;
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    line_counter counter = {0};

    if (ci_start_async_input("(q to quit)> ", on_line, &counter) != CI_OK) {
        puts("Failed to start async input");
        return 1;
    }

    int iterations = 0;
    while (keep_running) {
        printf("Working... %d\n", iterations++);
        usleep(100000);
    }

    ci_stop_async_input();
    printf("Total lines received: %d\n", counter.count);
    return 0;
}
