# ci_input

A small C library to read user input safely from console streams. Supports synchronous reads and an async thread to capture input while a main loop keeps working.

## Building

```
make
```

## Example

```
make example
./examples/basic
```

This starts an async input thread with prompt `(q/ping/any)>`. The main thread continues a simulated busy loop; typing `q` stops the loop. The example also registers `ping` to respond with `pong`, and a default handler prints any other line.

## API

```c
#include "ci_input.h"

/* Blocking convenience helpers (sync) */
ci_status ci_read_line(FILE *stream, char *buffer, size_t size);
ci_status ci_prompt_line(const char *prompt, char *buffer, size_t size);
ci_status ci_read_int(const char *prompt, int *out_value);
ci_status ci_read_long(const char *prompt, long *out_value);

/* Async input helpers */
typedef void (*ci_line_callback)(const char *line, void *user_data);
ci_status ci_start_async_input(const char *prompt,
                                ci_line_callback callback,
                                void *user_data);
void ci_stop_async_input(void);
bool ci_async_is_running(void);

/* Command callbacks */
ci_status ci_register_command(const char *command,
                               ci_line_callback callback,
                               void *user_data);
ci_status ci_unregister_command(const char *command);
```
