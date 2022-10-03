/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_chip_info.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "shell.h"
#include "util.h"
#include "wifi.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "ping/ping_sock.h"

#define TAG "shell"
#define PROMPT_STR "esp32"

#define MAX_NUM_CUSTOM_COMMANDS 16

static int heap(int argc, char** argv);
static int version(int argc, char** argv);
static int reset(int argc, char** argv);
static int tasks(int argc, char** argv);
static int settings(int argc, char** argv);

static struct {
    struct arg_dbl* timeout;
    struct arg_dbl* interval;
    struct arg_int* data_size;
    struct arg_int* count;
    struct arg_int* tos;
    struct arg_str* host;
    struct arg_end* end;
} _ping_args;

static const esp_console_cmd_t _cmds[] = {
        {
                .command = "heap",
                .help = "Get the current size of free heap memory, and min ever heap size",
                .hint = NULL,
                .func = heap,
        },
        {
                .command = "version",
                .help = "Get version of chip and SDK",
                .hint = NULL,
                .func = version,
        },
        {
                .command = "reset",
                .help = "Software reset of the chip",
                .hint = NULL,
                .func = reset,
        },
        {
                .command = "tasks",
                .help = "Get information about running tasks and stack high watermark (HWM)",
                .hint = NULL,
                .func = tasks,
        },
        {
                .command = "settings",
                .help = "Get/Set/Erase settings by key",
                .hint = NULL,
                .func = settings,
        },
        // There's a "ping" command too, see register_ping_command()
};

// Commands added with shell_register_commands
static esp_console_cmd_t _custom_cmds[MAX_NUM_CUSTOM_COMMANDS];
static size_t _num_custom_cmds;

static int heap(int argc, char** argv) {
    printf("Free: %d, Free low watermark: %d\n",
           esp_get_free_heap_size(),
           heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
    return 0;
}

static int version(int argc, char** argv) {
    esp_chip_info_t info;
    esp_chip_info(&info);
    printf("IDF Version:%s\r\n", esp_get_idf_version());
    printf("Chip info:\r\n");
    printf("\tmodel:%s\r\n", info.model == CHIP_ESP32 ? "ESP32" : "Unknown");
    printf("\tcores:%d\r\n", info.cores);
    printf("\tfeature:%s%s%s%s%d%s\r\n",
           info.features & CHIP_FEATURE_WIFI_BGN ? "/802.11bgn" : "",
           info.features & CHIP_FEATURE_BLE ? "/BLE" : "",
           info.features & CHIP_FEATURE_BT ? "/BT" : "",
           info.features & CHIP_FEATURE_EMB_FLASH ? "/Embedded-Flash:" : "/External-Flash:",
           spi_flash_get_chip_size() / (1024 * 1024),
           " MB");
    printf("\trevision number:%d\r\n", info.revision);
    return 0;
}

static int reset(int argc, char** argv) {
    ESP_LOGI(TAG, "Resetting");
    esp_restart();
}

static int tasks(int argc, char** argv) {
    const size_t bytes_per_task = 40;
    char* task_list_buffer = malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_list_buffer == NULL) {
        ESP_LOGE(TAG, "failed to allocate buffer for vTaskList output");
        return 1;
    }
    fputs("Task Name\tStatus\tPrio\tHWM\tTask#", stdout);
    fputs("\n", stdout);
    vTaskList(task_list_buffer);
    fputs(task_list_buffer, stdout);
    free(task_list_buffer);
    return 0;
}

static void cmd_ping_on_ping_success(esp_ping_handle_t hdl, void* args) {
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    printf("%d bytes from %s icmp_seq=%d ttl=%d time=%d ms\n",
           recv_len,
           ipaddr_ntoa((ip_addr_t*)&target_addr),
           seqno,
           ttl,
           elapsed_time);
}

static void cmd_ping_on_ping_timeout(esp_ping_handle_t hdl, void* args) {
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    printf("From %s icmp_seq=%d timeout\n", ipaddr_ntoa((ip_addr_t*)&target_addr), seqno);
}

static void cmd_ping_on_ping_end(esp_ping_handle_t hdl, void* args) {
    ip_addr_t target_addr;
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    uint32_t loss = (uint32_t)((1 - ((float)received) / transmitted) * 100);
    if (IP_IS_V4(&target_addr)) {
        printf("\n--- %s ping statistics ---\n", inet_ntoa(*ip_2_ip4(&target_addr)));
    } else {
        printf("\n--- %s ping statistics ---\n", inet6_ntoa(*ip_2_ip6(&target_addr)));
    }
    printf("%d packets transmitted, %d received, %d%% packet loss, time %dms\n",
           transmitted,
           received,
           loss,
           total_time_ms);
    // delete the ping sessions, so that we clean up all resources and can create a new ping session
    // we don't have to call delete function in the callback, instead we can call delete function
    // from other tasks
    esp_ping_delete_session(hdl);
}

static int ping(int argc, char** argv) {
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();

    int nerrors = arg_parse(argc, argv, (void**)&_ping_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, _ping_args.end, argv[0]);
        return 1;
    }

    if (_ping_args.timeout->count > 0) {
        config.timeout_ms = (uint32_t)(_ping_args.timeout->dval[0] * 1000);
    }

    if (_ping_args.interval->count > 0) {
        config.interval_ms = (uint32_t)(_ping_args.interval->dval[0] * 1000);
    }

    if (_ping_args.data_size->count > 0) {
        config.data_size = (uint32_t)(_ping_args.data_size->ival[0]);
    }

    if (_ping_args.count->count > 0) {
        config.count = (uint32_t)(_ping_args.count->ival[0]);
    }

    if (_ping_args.tos->count > 0) {
        config.tos = (uint32_t)(_ping_args.tos->ival[0]);
    }

    // parse IP address
    struct sockaddr_in6 sock_addr6;
    ip_addr_t target_addr;
    memset(&target_addr, 0, sizeof(target_addr));

    if (inet_pton(AF_INET6, _ping_args.host->sval[0], &sock_addr6.sin6_addr) == 1) {
        /* convert ip6 string to ip6 address */
        ipaddr_aton(_ping_args.host->sval[0], &target_addr);
    } else {
        struct addrinfo hint;
        struct addrinfo* res = NULL;
        memset(&hint, 0, sizeof(hint));
        /* convert ip4 string or hostname to ip4 or ip6 address */
        if (getaddrinfo(_ping_args.host->sval[0], NULL, &hint, &res) != 0) {
            printf("ping: unknown host %s\n", _ping_args.host->sval[0]);
            return 1;
        }
        if (res->ai_family == AF_INET) {
            struct in_addr addr4 = ((struct sockaddr_in*)(res->ai_addr))->sin_addr;
            inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
        } else {
            struct in6_addr addr6 = ((struct sockaddr_in6*)(res->ai_addr))->sin6_addr;
            inet6_addr_to_ip6addr(ip_2_ip6(&target_addr), &addr6);
        }
        freeaddrinfo(res);
    }
    config.target_addr = target_addr;

    /* set callback functions */
    esp_ping_callbacks_t cbs = {
            .cb_args = NULL,
            .on_ping_success = cmd_ping_on_ping_success,
            .on_ping_timeout = cmd_ping_on_ping_timeout,
            .on_ping_end = cmd_ping_on_ping_end};
    esp_ping_handle_t ping;
    esp_ping_new_session(&config, &cbs, &ping);
    esp_ping_start(ping);

    return 0;
}

// Map from CLI key to internal NVS keys.
// Can't use CLI keys directly in NVS due to character length
// limitation in NVS.
static const char* cli_key_to_nvs_key(const char* key) {
    if (0 == strcmp(key, "wifi/ssid")) {
        return NVS_WIFI_SSID_KEY;
    } else if (0 == strcmp(key, "wifi/psk")) {
        return NVS_WIFI_PASS_KEY;
    } else if (0 == strcmp(key, "golioth/psk-id")) {
        return NVS_GOLIOTH_PSK_ID_KEY;
    } else if (0 == strcmp(key, "golioth/psk")) {
        return NVS_GOLIOTH_PSK_KEY;
    } else {
        return "unknown";
    }
}

static int settings(int argc, char** argv) {
    const char* usage =
            "usage:\n"
            "  settings get <key> [--json]\n"
            "  settings set <key> <value> [--json]\n"
            "  settings erase <key>\n";

    if (argc < 3) {
        printf(usage);
        return 1;
    }

    const char* command = argv[1];
    const char* cli_key = argv[2];
    const char* nvs_key = cli_key_to_nvs_key(cli_key);

    if (0 == strcmp(command, "get")) {
        char valuebuf[128] = {};
        const char* value = nvs_read_str(nvs_key, valuebuf, sizeof(valuebuf));
        bool was_not_found = (0 == strcmp(value, NVS_DEFAULT_STR));
        bool json_output = ((argc >= 4) && (0 == strcmp(argv[3], "--json")));
        if (json_output) {
            if (was_not_found) {
                printf("{\"status\": \"failed\", \"msg\": \"setting not found\"}\n");
            } else {
                printf("{\"status\": \"success\", \"value\": \"%s\"}\n", value);
            }
        } else {
            if (was_not_found) {
                printf("Setting not found\n");
            } else {
                printf("%s\n", value);
            }
        }
    } else if (0 == strcmp(command, "set")) {
        if (argc < 4) {
            printf(usage);
            return 1;
        }
        const char* value = argv[3];
        bool success = nvs_write_str(nvs_key, value);
        bool json_output = ((argc >= 5) && (0 == strcmp(argv[4], "--json")));
        if (json_output) {
            if (success) {
                printf("{\"status\": \"success\", \"msg\": \"setting %s saved as %s\"}\n",
                       cli_key,
                       value);
            } else {
                printf("{\"status\": \"failed\", \"msg\": \"failed to save setting %s:%s\"}\n",
                       cli_key,
                       value);
            }
        } else {
            if (success) {
                printf("Setting %s saved\n", cli_key);
            } else {
                printf("Failed to save setting %s\n", cli_key);
            }
        }
    } else if (0 == strcmp(command, "erase")) {
        bool success = nvs_erase_str(nvs_key);
        if (success) {
            printf("Erased key %s from NVS\n", cli_key);
        }
    } else {
        printf(usage);
        return 1;
    }

    return 0;
}

static void register_ping_command(void) {
    _ping_args.timeout = arg_dbl0("W", "timeout", "<t>", "Time to wait for a response, in seconds");
    _ping_args.interval =
            arg_dbl0("i", "interval", "<t>", "Wait interval seconds between sending each packet");
    _ping_args.data_size =
            arg_int0("s", "size", "<n>", "Specify the number of data bytes to be sent");
    _ping_args.count = arg_int0("c", "count", "<n>", "Stop after sending count packets");
    _ping_args.tos =
            arg_int0("Q", "tos", "<n>", "Set Type of Service related bits in IP datagrams");
    _ping_args.host = arg_str1(NULL, NULL, "<host>", "Host address");
    _ping_args.end = arg_end(1);
    const esp_console_cmd_t ping_cmd = {
            .command = "ping",
            .help = "send ICMP ECHO_REQUEST to network hosts",
            .hint = NULL,
            .func = &ping,
            .argtable = &_ping_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&ping_cmd));
}

static void initialize_console(void) {
    fflush(stdout);
    fsync(fileno(stdout));
    setvbuf(stdin, NULL, _IONBF, 0);
    esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
#ifndef CONFIG_ESP_CONSOLE_USB_CDC
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
        .source_clk = UART_SCLK_REF_TICK,
#else
        .source_clk = UART_SCLK_XTAL,
#endif
    };
    ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
#endif
    esp_console_config_t console_config = {
            .max_cmdline_args = 8, .max_cmdline_length = 256, .hint_color = atoi(LOG_COLOR_CYAN)};
    ESP_ERROR_CHECK(esp_console_init(&console_config));
    linenoiseSetMultiLine(1);
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*)&esp_console_get_hint);
    linenoiseHistorySetMaxLen(100);
    linenoiseSetMaxLineLen(console_config.max_cmdline_length);
    linenoiseAllowEmpty(false);
}

static void shell_task(void* arg) {
    initialize_console();
    esp_console_register_help_command();

    for (int i = 0; i < COUNT_OF(_cmds); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&_cmds[i]));
    }

    for (int i = 0; i < _num_custom_cmds; i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&_custom_cmds[i]));
    }

    // Ping command requires args to be setup, so register it seperately
    register_ping_command();

    const char* prompt = LOG_COLOR_I PROMPT_STR "> " LOG_RESET_COLOR;
    printf("\n"
           "Type 'help' to get the list of commands.\n"
           "Use UP/DOWN arrows to navigate through command history.\n"
           "Press TAB when typing command name to auto-complete.\n"
           "Press Enter or Ctrl+C will terminate the console environment.\n");

    int probe_status = linenoiseProbe();
    if (probe_status) { /* zero indicates success */
        printf("\n"
               "Your terminal application does not support escape sequences.\n"
               "Line editing and history features are disabled.\n"
               "On Windows, try using Putty instead.\n");
        linenoiseSetDumbMode(1);
        prompt = PROMPT_STR "> ";
    }

    while (true) {
        char* line = linenoise(prompt);
        if (line == NULL) {
            continue;
        }

        if (strlen(line) > 0) {
            linenoiseHistoryAdd(line);
        }

        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // command was empty
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
        } else if (err != ESP_OK) {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        /* linenoise allocates line buffer on the heap, so need to free it */
        linenoiseFree(line);
    }
}

void shell_start(void) {
    static bool initialized = false;
    if (!initialized) {
        bool task_created = xTaskCreate(
                shell_task,
                "shell",
                7168,
                NULL,  // task arg
                2,     // pri
                NULL);
        if (!task_created) {
            ESP_LOGE(TAG, "Failed to create shell task");
        } else {
            initialized = true;
        }
    }
}

void shell_register_command(const esp_console_cmd_t* cmd) {
    if (_num_custom_cmds >= MAX_NUM_CUSTOM_COMMANDS) {
        ESP_LOGE(TAG, "Can't register more than %d custom commands", MAX_NUM_CUSTOM_COMMANDS);
        return;
    }
    _custom_cmds[_num_custom_cmds++] = *cmd;
}
