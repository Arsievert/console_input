# console_input

A small C library for safe console input in C programs. It provides overflow-aware line reads, validated numeric parsing, and an async input thread with command callbacks so your main loop can keep working while input is handled promptly.

## Building

- Requirements: C99 toolchain, pthreads (POSIX).
- Build library and examples:
  ```
  make example
  ```
  Artifacts: `libconsole_input.a`, `examples/basic`, `examples/user_data`

## Quick start

Sync (blocking convenience):
```c
#include "console_input.h"

char line[128];
if (ci_prompt_line("Name: ", line, sizeof(line)) == CI_OK) {
    printf("Hello, %s\n", line);
}

int value;
if (ci_read_int("Enter a number: ", &value) == CI_OK) {
    printf("You entered %d\n", value);
}
```

Async with a default callback and clean shutdown:
```c
#include "console_input.h"
#include <unistd.h>

static volatile int running = 1;

static void on_line(const char *line, void *user) {
    (void)user;
    if (line && line[0] == 'q') {
        running = 0;
        ci_request_stop_async_input();
        return;
    }
    printf("got: %s\n", line);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (ci_start_async_input("(q to quit)> ", on_line, NULL) != CI_OK) return 1;

    while (running) {
        // busy work
        usleep(100000);
    }

    ci_stop_async_input();
    return 0;
}
```

## Async commands

Register command-specific callbacks; a default callback handles everything else:
```c
ci_register_command("q", on_quit, NULL);
ci_register_command("ping", on_ping, NULL);
```
Inside `on_quit`, call `ci_request_stop_async_input()` (and set your own flag) to exit promptly. The main thread should still call `ci_stop_async_input()` before exiting.

## Using user_data

Pass a context pointer to callbacks (see `examples/user_data.c`):
```c
typedef struct { int count; } line_counter;

static void on_line(const char *line, void *user_data) {
    line_counter *c = user_data;
    c->count++;
    printf("[%d] %s\n", c->count, line);
    if (line && line[0] == 'q') {
        ci_request_stop_async_input();
    }
}
```

## API summary

```c
#include "console_input.h"

typedef enum { CI_OK = 0, CI_EOF = -1, CI_OVERFLOW = -2, CI_INVALID = -3 } ci_status;
typedef void (*ci_line_callback)(const char *line, void *user_data);

/* Blocking convenience (sync) */
ci_status ci_read_line(FILE *stream, char *buffer, size_t size);
ci_status ci_prompt_line(const char *prompt, char *buffer, size_t size);
ci_status ci_read_int(const char *prompt, int *out_value);
ci_status ci_read_long(const char *prompt, long *out_value);

/* Async */
ci_status ci_start_async_input(const char *prompt, ci_line_callback callback, void *user_data);
void ci_stop_async_input(void);
bool ci_async_is_running(void);
void ci_request_stop_async_input(void);

/* Commands */
ci_status ci_register_command(const char *command, ci_line_callback callback, void *user_data);
ci_status ci_unregister_command(const char *command);

/* Limits */
#define CI_COMMAND_MAX_LEN 64
#define CI_MAX_COMMANDS 32
```

## Behavior notes
- Sync helpers block the caller; they validate length and numeric ranges.
- Async thread uses `fgets`; prompts are flushed before read.
- Command registry is mutex-protected; callbacks run on the async thread.
- To stop promptly from a command, set your own loop flag and call `ci_request_stop_async_input`; then `ci_stop_async_input` will join the thread.

## Examples
- `examples/basic`: async with `q` (quit) and `ping` (prints `pong`).
- `examples/user_data`: demonstrates `user_data` by counting lines.

Run examples:
```
make example
./examples/basic
./examples/user_data
```
