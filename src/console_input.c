#include "console_input.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CI_ASYNC_BUFFER 256

typedef struct {
    char command[CI_COMMAND_MAX_LEN];
    ci_line_callback cb;
    void *user_data;
} ci_command_entry;

static pthread_t ci_thread;
static ci_line_callback ci_cb = NULL;
static void *ci_cb_data = NULL;
static const char *ci_prompt = NULL;
static volatile bool ci_running = false;
static volatile bool ci_stop_requested = false;
static ci_command_entry ci_commands[CI_MAX_COMMANDS];
static size_t ci_command_count = 0;
static pthread_mutex_t ci_cmd_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *ci_async_thread(void *arg);

/**
 * @brief Read a single line with optional prompt into a buffer.
 * @param stream Input stream to read from.
 * @param prompt Optional prompt to write to stdout before reading.
 * @param buffer Destination buffer for the line.
 * @param size Size of the destination buffer.
 * @return CI_OK on success, CI_EOF on end-of-file, CI_OVERFLOW on truncation, CI_INVALID on error.
 */
static ci_status ci_read_line_internal(FILE *stream, const char *prompt, char *buffer, size_t size) {
    if (!buffer || size == 0) return CI_INVALID;

    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }

    if (fgets(buffer, (int)size, stream) == NULL) {
        return feof(stream) ? CI_EOF : CI_INVALID;
    }

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    } else if (len + 1 == size) {
        int c;
        while ((c = fgetc(stream)) != '\n' && c != EOF) {
        }
        return CI_OVERFLOW;
    }

    return CI_OK;
}

/**
 * @brief Parse a long from a string with overflow and validation checks.
 * @param input Input string to parse.
 * @param out_value Output pointer for parsed value.
 * @return CI_OK on success, CI_OVERFLOW on range error, CI_INVALID on parse error.
 */
static ci_status ci_parse_long(const char *input, long *out_value) {
    if (!input || !out_value) return CI_INVALID;

    errno = 0;
    char *endptr = NULL;
    long val = strtol(input, &endptr, 10);

    if (errno == ERANGE || val > LONG_MAX || val < LONG_MIN) {
        return CI_OVERFLOW;
    }

    if (endptr == input || *endptr != '\0') {
        return CI_INVALID;
    }

    *out_value = val;
    return CI_OK;
}

ci_status ci_read_line(FILE *stream, char *buffer, size_t size) {
    return ci_read_line_internal(stream, NULL, buffer, size);
}

ci_status ci_prompt_line(const char *prompt, char *buffer, size_t size) {
    return ci_read_line_internal(stdin, prompt, buffer, size);
}

/**
 * @brief Prompt repeatedly until a valid numeric (long) value is entered.
 * @param prompt Prompt text to display.
 * @param out_value Output pointer for parsed long.
 * @return CI_OK on success, CI_EOF if input ends, CI_OVERFLOW on length/range issues, CI_INVALID on other errors.
 */
static ci_status ci_prompt_numeric(const char *prompt, long *out_value) {
    char buf[128];
    ci_status status;

    while (1) {
        status = ci_prompt_line(prompt, buf, sizeof(buf));
        if (status == CI_EOF) return CI_EOF;
        if (status == CI_OVERFLOW) {
            fprintf(stdout, "Input too long, try again.\n");
            continue;
        }
        if (status != CI_OK) return status;

        status = ci_parse_long(buf, out_value);
        if (status == CI_INVALID) {
            fprintf(stdout, "Invalid number, try again.\n");
            continue;
        }
        return status;
    }
}

ci_status ci_read_int(const char *prompt, int *out_value) {
    long val;
    ci_status status = ci_prompt_numeric(prompt, &val);
    if (status != CI_OK) return status;

    if (val > INT_MAX || val < INT_MIN) return CI_OVERFLOW;
    *out_value = (int)val;
    return CI_OK;
}

ci_status ci_read_long(const char *prompt, long *out_value) {
    return ci_prompt_numeric(prompt, out_value);
}

/**
 * @brief Lookup a registered command matching the given line.
 * @param line Input line to match.
 * @param out_entry Output copy of the matched entry when found.
 * @return true if a command matched, false otherwise.
 */
static bool ci_lookup_command(const char *line, ci_command_entry *out_entry) {
    if (!line || !out_entry) return false;

    bool found = false;
    pthread_mutex_lock(&ci_cmd_mutex);
    for (size_t i = 0; i < ci_command_count; i++) {
        if (strcmp(line, ci_commands[i].command) == 0) {
            *out_entry = ci_commands[i];
            found = true;
            break;
        }
    }
    pthread_mutex_unlock(&ci_cmd_mutex);
    return found;
}

/**
 * @brief Thread routine that reads lines and dispatches to commands/default callback.
 * @param arg Unused thread argument.
 * @return NULL when thread exits.
 */
static void *ci_async_thread(void *arg) {
    (void)arg;
    char buffer[CI_ASYNC_BUFFER];

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    while (ci_running && !ci_stop_requested) {
        ci_status status = ci_read_line_internal(stdin, ci_prompt, buffer, sizeof(buffer));
        if (status == CI_EOF) break;
        if (status == CI_OVERFLOW) {
            fprintf(stdout, "Input too long, try again.\n");
            continue;
        }
        if (status != CI_OK) continue;

        ci_command_entry entry;
        if (ci_lookup_command(buffer, &entry) && entry.cb) {
            entry.cb(buffer, entry.user_data);
        } else if (ci_cb) {
            ci_cb(buffer, ci_cb_data);
        }

        if (ci_stop_requested) break;
    }

    ci_running = false;
    ci_stop_requested = false;
    return NULL;
}

ci_status ci_start_async_input(const char *prompt, ci_line_callback callback, void *user_data) {
    if (!callback) return CI_INVALID;
    if (ci_running) return CI_INVALID;

    ci_cb = callback;
    ci_cb_data = user_data;
    ci_prompt = prompt;
    ci_command_count = 0;
    ci_stop_requested = false;
    ci_running = true;

    int rc = pthread_create(&ci_thread, NULL, ci_async_thread, NULL);
    if (rc != 0) {
        ci_running = false;
        return CI_INVALID;
    }

    return CI_OK;
}

ci_status ci_register_command(const char *command, ci_line_callback callback, void *user_data) {
    if (!command || !callback) return CI_INVALID;
    if (strlen(command) >= CI_COMMAND_MAX_LEN) return CI_OVERFLOW;

    pthread_mutex_lock(&ci_cmd_mutex);

    /* replace if already present */
    for (size_t i = 0; i < ci_command_count; i++) {
        if (strcmp(command, ci_commands[i].command) == 0) {
            ci_commands[i].cb = callback;
            ci_commands[i].user_data = user_data;
            pthread_mutex_unlock(&ci_cmd_mutex);
            return CI_OK;
        }
    }

    if (ci_command_count >= CI_MAX_COMMANDS) {
        pthread_mutex_unlock(&ci_cmd_mutex);
        return CI_OVERFLOW;
    }

    ci_command_entry *slot = &ci_commands[ci_command_count++];
    strncpy(slot->command, command, CI_COMMAND_MAX_LEN);
    slot->command[CI_COMMAND_MAX_LEN - 1] = '\0';
    slot->cb = callback;
    slot->user_data = user_data;

    pthread_mutex_unlock(&ci_cmd_mutex);
    return CI_OK;
}

ci_status ci_unregister_command(const char *command) {
    if (!command) return CI_INVALID;

    pthread_mutex_lock(&ci_cmd_mutex);
    for (size_t i = 0; i < ci_command_count; i++) {
        if (strcmp(command, ci_commands[i].command) == 0) {
            ci_command_count--;
            if (i != ci_command_count) {
                ci_commands[i] = ci_commands[ci_command_count];
            }
            pthread_mutex_unlock(&ci_cmd_mutex);
            return CI_OK;
        }
    }
    pthread_mutex_unlock(&ci_cmd_mutex);

    return CI_INVALID;
}

void ci_stop_async_input(void) {
    if (!ci_running) return;

    ci_stop_requested = true;
    pthread_cancel(ci_thread);
    pthread_join(ci_thread, NULL);

    ci_running = false;
    ci_cb = NULL;
    ci_cb_data = NULL;
    ci_prompt = NULL;
    ci_command_count = 0;
    ci_stop_requested = false;
}

void ci_request_stop_async_input(void) {
    ci_stop_requested = true;
}

bool ci_async_is_running(void) {
    return ci_running;
}
