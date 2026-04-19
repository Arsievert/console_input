#include "console_input.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* ---- Platform mutex abstraction ---- */

#ifdef CI_PLATFORM_FREERTOS
#include "FreeRTOS.h"
#include "semphr.h"
static SemaphoreHandle_t ci_cmd_mutex;
#define ci_mutex_lock(mp)   xSemaphoreTake(*(mp), portMAX_DELAY)
#define ci_mutex_unlock(mp) xSemaphoreGive(*(mp))
#else
#include <pthread.h>
static pthread_mutex_t ci_cmd_mutex = PTHREAD_MUTEX_INITIALIZER;
#define ci_mutex_lock(mp)   pthread_mutex_lock(mp)
#define ci_mutex_unlock(mp) pthread_mutex_unlock(mp)
#endif

/* ---- Shared state ---- */

typedef struct {
    char command[CI_COMMAND_MAX_LEN];
    ci_line_callback cb;
    void *user_data;
} ci_command_entry;

static ci_line_callback ci_cb = NULL;
static void *ci_cb_data = NULL;
static ci_command_entry ci_commands[CI_MAX_COMMANDS];
static size_t ci_command_count = 0;

/* ---- Portable API ---- */

void ci_init(void) {
#ifdef CI_PLATFORM_FREERTOS
    ci_cmd_mutex = xSemaphoreCreateMutex();
#endif
}

ci_status ci_set_default_callback(ci_line_callback callback, void *user_data) {
    if (!callback) return CI_INVALID;
    ci_cb = callback;
    ci_cb_data = user_data;
    return CI_OK;
}

static bool ci_lookup_command(const char *line, ci_command_entry *out_entry) {
    if (!line || !out_entry) return false;

    bool found = false;
    ci_mutex_lock(&ci_cmd_mutex);
    for (size_t i = 0; i < ci_command_count; i++) {
        if (strcmp(line, ci_commands[i].command) == 0) {
            *out_entry = ci_commands[i];
            found = true;
            break;
        }
    }
    ci_mutex_unlock(&ci_cmd_mutex);
    return found;
}

ci_status ci_process_line(const char *line) {
    if (!line) return CI_INVALID;

    ci_command_entry entry;
    if (ci_lookup_command(line, &entry) && entry.cb) {
        entry.cb(line, entry.user_data);
    } else if (ci_cb) {
        ci_cb(line, ci_cb_data);
    }
    return CI_OK;
}

ci_status ci_register_command(const char *command, ci_line_callback callback, void *user_data) {
    if (!command || !callback) return CI_INVALID;
    if (strlen(command) >= CI_COMMAND_MAX_LEN) return CI_OVERFLOW;

    ci_mutex_lock(&ci_cmd_mutex);

    for (size_t i = 0; i < ci_command_count; i++) {
        if (strcmp(command, ci_commands[i].command) == 0) {
            ci_commands[i].cb = callback;
            ci_commands[i].user_data = user_data;
            ci_mutex_unlock(&ci_cmd_mutex);
            return CI_OK;
        }
    }

    if (ci_command_count >= CI_MAX_COMMANDS) {
        ci_mutex_unlock(&ci_cmd_mutex);
        return CI_OVERFLOW;
    }

    ci_command_entry *slot = &ci_commands[ci_command_count++];
    strncpy(slot->command, command, CI_COMMAND_MAX_LEN);
    slot->command[CI_COMMAND_MAX_LEN - 1] = '\0';
    slot->cb = callback;
    slot->user_data = user_data;

    ci_mutex_unlock(&ci_cmd_mutex);
    return CI_OK;
}

ci_status ci_unregister_command(const char *command) {
    if (!command) return CI_INVALID;

    ci_mutex_lock(&ci_cmd_mutex);
    for (size_t i = 0; i < ci_command_count; i++) {
        if (strcmp(command, ci_commands[i].command) == 0) {
            ci_command_count--;
            if (i != ci_command_count) {
                ci_commands[i] = ci_commands[ci_command_count];
            }
            ci_mutex_unlock(&ci_cmd_mutex);
            return CI_OK;
        }
    }
    ci_mutex_unlock(&ci_cmd_mutex);

    return CI_INVALID;
}

/* ---- POSIX-only implementation ---- */

#ifndef CI_PLATFORM_FREERTOS

#include <errno.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>

#define CI_ASYNC_BUFFER 256

static pthread_t ci_thread;
static const char *ci_prompt = NULL;
static atomic_bool ci_running = false;
static atomic_bool ci_stop_requested = false;
static bool ci_thread_valid = false;

static void *ci_async_thread(void *arg);

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

static ci_status ci_parse_long(const char *input, long *out_value) {
    if (!input || !out_value) return CI_INVALID;

    errno = 0;
    char *endptr = NULL;
    long val = strtol(input, &endptr, 10);

    if (errno == ERANGE) {
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

        ci_process_line(buffer);

        if (ci_stop_requested) break;
    }

    ci_running = false;
    ci_stop_requested = false;
    return NULL;
}

ci_status ci_start_async_input(const char *prompt, ci_line_callback callback, void *user_data) {
    if (!callback) return CI_INVALID;
    if (ci_running) return CI_INVALID;

    ci_set_default_callback(callback, user_data);
    ci_prompt = prompt;
    ci_command_count = 0;
    ci_stop_requested = false;
    ci_running = true;

    int rc = pthread_create(&ci_thread, NULL, ci_async_thread, NULL);
    if (rc != 0) {
        ci_running = false;
        return CI_INVALID;
    }

    ci_thread_valid = true;
    return CI_OK;
}

void ci_stop_async_input(void) {
    if (!ci_thread_valid) return;

    ci_stop_requested = true;
    if (ci_running) {
        pthread_cancel(ci_thread);
    }
    pthread_join(ci_thread, NULL);

    ci_thread_valid = false;
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

#endif /* CI_PLATFORM_FREERTOS */
