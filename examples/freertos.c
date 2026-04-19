/*
 * Example: using console_input on FreeRTOS.
 *
 * Build with -DCI_PLATFORM_FREERTOS and your FreeRTOS port headers.
 * This file won't compile standalone — it's a reference for integration.
 */
#define CI_PLATFORM_FREERTOS
#include "console_input.h"

/* Stub: replace with your UART/USB-CDC read function. */
extern int uart_read_line(char *buf, int max_len);

static void on_line(const char *line, void *user_data) {
    (void)user_data;
    /* handle unrecognized input */
}

static void on_reboot(const char *line, void *user_data) {
    (void)line;
    (void)user_data;
    /* NVIC_SystemReset(); */
}

void input_task(void *params) {
    (void)params;
    char buf[128];

    ci_init();
    ci_set_default_callback(on_line, NULL);
    ci_register_command("reboot", on_reboot, NULL);

    for (;;) {
        int n = uart_read_line(buf, sizeof(buf));
        if (n > 0) {
            ci_process_line(buf);
        }
    }
}
