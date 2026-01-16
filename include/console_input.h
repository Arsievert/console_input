#ifndef CI_INPUT_H
#define CI_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum {
    CI_OK = 0,
    CI_EOF = -1,
    CI_OVERFLOW = -2,
    CI_INVALID = -3
} ci_status;
typedef void (*ci_line_callback)(const char *line, void *user_data);

#define CI_COMMAND_MAX_LEN 64
#define CI_MAX_COMMANDS 32

/**
 * @brief Read a single line from the given stream.
 * @param stream Input stream (e.g., stdin).
 * @param buffer Destination buffer for the line.
 * @param size Size of the destination buffer.
 * @return CI_OK on success, CI_EOF on end-of-file, CI_OVERFLOW if truncated, CI_INVALID on error.
 * @note Blocking convenience helper for synchronous use.
 */
ci_status ci_read_line(FILE *stream, char *buffer, size_t size);

/**
 * @brief Prompt and read a line from stdin.
 * @param prompt Prompt text to display (can be NULL).
 * @param buffer Destination buffer for the line.
 * @param size Size of the destination buffer.
 * @return CI_OK on success, CI_EOF on end-of-file, CI_OVERFLOW if truncated, CI_INVALID on error.
 * @note Blocking convenience helper for synchronous use.
 */
ci_status ci_prompt_line(const char *prompt, char *buffer, size_t size);

/**
 * @brief Prompt for and read an int value.
 * @param prompt Prompt text to display.
 * @param out_value Output pointer for the parsed int.
 * @return CI_OK on success, CI_EOF on end-of-file, CI_OVERFLOW on range/length issues, CI_INVALID on parse error.
 * @note Blocking convenience helper for synchronous use.
 */
ci_status ci_read_int(const char *prompt, int *out_value);

/**
 * @brief Prompt for and read a long value.
 * @param prompt Prompt text to display.
 * @param out_value Output pointer for the parsed long.
 * @return CI_OK on success, CI_EOF on end-of-file, CI_OVERFLOW on range/length issues, CI_INVALID on parse error.
 * @note Blocking convenience helper for synchronous use.
 */
ci_status ci_read_long(const char *prompt, long *out_value);

/**
 * @brief Start an async input thread that forwards lines to a callback.
 * @param prompt Prompt text to display before each read (can be NULL).
 * @param callback Callback invoked for each full line.
 * @param user_data User pointer passed to the callback.
 * @return CI_OK on start, CI_INVALID on bad args or if already running.
 */
ci_status ci_start_async_input(const char *prompt,
                                ci_line_callback callback,
                                void *user_data);

/**
 * @brief Stop the async input thread and join it.
 */
void ci_stop_async_input(void);

/**
 * @brief Check whether the async input thread is running.
 * @return true if running, false otherwise.
 */
bool ci_async_is_running(void);

/* Command callbacks. Thread-safe for registration while async input runs. */
/**
 * @brief Register (or replace) a command string callback.
 * @param command Exact command string to match (max CI_COMMAND_MAX_LEN-1 chars).
 * @param callback Callback to invoke when the command matches.
 * @param user_data User pointer passed to the callback.
 * @return CI_OK on success, CI_OVERFLOW if table is full/command too long, CI_INVALID on bad args.
 */
ci_status ci_register_command(const char *command,
                               ci_line_callback callback,
                               void *user_data);

/**
 * @brief Remove a previously registered command callback.
 * @param command Command string to remove.
 * @return CI_OK if removed, CI_INVALID if not found or bad args.
 */
ci_status ci_unregister_command(const char *command);

#endif
