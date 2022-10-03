/******************************************************************************
* File Name:   golioth_main.c
*
* Description: This file contains task and functions related to Golioth main
*              operation.
*
********************************************************************************
* Copyright 2020-2021, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/* Header file includes. */
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include <inttypes.h>

/* FreeRTOS header file. */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

/* Cypress secure socket header file. */
#include "cy_secure_sockets.h"

/* Wi-Fi connection manager header files. */
#include "cy_wcm.h"
#include "cy_wcm_error.h"

#include "golioth_main.h"
#include "golioth.h"

#define TAG "golioth_main"

// Configurable via LightDB State at path "desired/my_config"
int32_t _my_config = 0;

// Configurable via Settings service, key = "LOOP_DELAY_S"
int32_t _loop_delay_s = 10;

// Given if/when the we have a connection to Golioth
static SemaphoreHandle_t _connected_sem;

/*******************************************************************************
 * Function Name: connect_to_wifi_ap()
 *******************************************************************************
 * Summary:
 *  Connects to Wi-Fi AP using the user-configured credentials, retries up to a
 *  configured number of times until the connection succeeds.
 *
 *******************************************************************************/
cy_rslt_t connect_to_wifi_ap(void)
{
    cy_rslt_t result;

    /* Variables used by Wi-Fi connection manager.*/
    cy_wcm_connect_params_t wifi_conn_param;

    cy_wcm_config_t wifi_config = { .interface = CY_WCM_INTERFACE_TYPE_STA };

    cy_wcm_ip_address_t ip_address;

     /* Initialize Wi-Fi connection manager. */
    result = cy_wcm_init(&wifi_config);

    if (result != CY_RSLT_SUCCESS)
    {
        printf("Wi-Fi Connection Manager initialization failed!\n");
        return result;
    }
    printf("Wi-Fi Connection Manager initialized.\r\n");

     /* Set the Wi-Fi SSID, password and security type. */
    memset(&wifi_conn_param, 0, sizeof(cy_wcm_connect_params_t));
    memcpy(wifi_conn_param.ap_credentials.SSID, WIFI_SSID, sizeof(WIFI_SSID));
    memcpy(wifi_conn_param.ap_credentials.password, WIFI_PASSWORD, sizeof(WIFI_PASSWORD));
    wifi_conn_param.ap_credentials.security = WIFI_SECURITY_TYPE;

    /* Join the Wi-Fi AP. */
    for(uint32_t conn_retries = 0; conn_retries < MAX_WIFI_CONN_RETRIES; conn_retries++ )
    {
        result = cy_wcm_connect_ap(&wifi_conn_param, &ip_address);

        if(result == CY_RSLT_SUCCESS)
        {
            printf("Successfully connected to Wi-Fi network '%s'.\n",
                                wifi_conn_param.ap_credentials.SSID);
            printf("IP Address Assigned: %d.%d.%d.%d\n", (uint8)ip_address.ip.v4,
                    (uint8)(ip_address.ip.v4 >> 8), (uint8)(ip_address.ip.v4 >> 16),
                    (uint8)(ip_address.ip.v4 >> 24));
            return result;
        }

        printf("Connection to Wi-Fi network failed with error code %d."
               "Retrying in %d ms...\n", (int)result, WIFI_CONN_RETRY_INTERVAL_MSEC);

        vTaskDelay(pdMS_TO_TICKS(WIFI_CONN_RETRY_INTERVAL_MSEC));
    }

    /* Stop retrying after maximum retry attempts. */
    printf("Exceeded maximum Wi-Fi connection attempts\n");

    return result;
}

static void on_client_event(golioth_client_t client, golioth_client_event_t event, void* arg) {
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected) {
        xSemaphoreGive(_connected_sem);
    }
    GLTH_LOGI(TAG, "Golioth client %s", is_connected ? "connected" : "disconnected");
}

// Callback function for asynchronous get request of LightDB path "my_int"
static void on_get_my_int(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg) {
    // It's a good idea to check the response status, to make sure the request didn't time out.
    if (response->status != GOLIOTH_OK) {
        GLTH_LOGE(TAG, "on_get_my_int status = %s", golioth_status_to_str(response->status));
        return;
    }

    // Now we can use a helper function to convert the binary payload to an integer.
    int32_t value = golioth_payload_as_int(payload, payload_size);
    GLTH_LOGI(TAG, "Callback got my_int = %"PRId32, value);
}

// Callback function for asynchronous observation of LightDB path "desired/my_config"
static void on_my_config(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg) {
    if (response->status != GOLIOTH_OK) {
        return;
    }

    // Payload might be null if desired/my_config is deleted, so ignore that case
    if (golioth_payload_is_null(payload, payload_size)) {
        return;
    }

    int32_t desired_value = golioth_payload_as_int(payload, payload_size);
    GLTH_LOGI(TAG, "Cloud desires %s = %"PRId32". Setting now.", path, desired_value);
    _my_config = desired_value;
    golioth_lightdb_delete_async(client, path, NULL, NULL);
}

static golioth_rpc_status_t on_double(
        const char* method,
        const cJSON* params,
        uint8_t* detail,
        size_t detail_size,
        void* callback_arg) {
    if (cJSON_GetArraySize(params) != 1) {
        return RPC_INVALID_ARGUMENT;
    }
    int num_to_double = cJSON_GetArrayItem(params, 0)->valueint;
    snprintf((char*)detail, detail_size, "{ \"value\": %d }", 2 * num_to_double);
    return RPC_OK;
}

static golioth_settings_status_t on_setting(
        const char* key,
        const golioth_settings_value_t* value) {
    GLTH_LOGD(TAG, "Received setting: key = %s, type = %d", key, value->type);

    if (0 == strcmp(key, "LOOP_DELAY_S")) {
        // This setting is expected to be an int, return an error if it's not
        if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT) {
            return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
        }

        // This setting must be in range [1, 100], return an error if it's not
        if (value->i32 < 1 || value->i32 > 100) {
            return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
        }

        // Setting has passed all checks, so apply it to the loop delay
        GLTH_LOGI(TAG, "Setting loop delay to %"PRId32" s", value->i32);
        _loop_delay_s = value->i32;
        return GOLIOTH_SETTINGS_SUCCESS;
    }

    // If the setting is not recognized, we should return an error
    return GOLIOTH_SETTINGS_KEY_NOT_RECOGNIZED;
}

void golioth_main_task(void *arg) {
    /* Connect to Wi-Fi AP */
    if(connect_to_wifi_ap() != CY_RSLT_SUCCESS) {
        printf("\n Failed to connect to Wi-FI AP.\n");
        CY_ASSERT(0);
    }

    /* Secure Sockets initialized */
    cy_rslt_t result = cy_socket_init();
    if (result != CY_RSLT_SUCCESS) {
        printf("Secure Sockets initialization failed!\n");
        CY_ASSERT(0);
    }
    printf("Secure Sockets initialized\n");

    _connected_sem = xSemaphoreCreateBinary();

    // Now we are ready to connect to the Golioth cloud.
    //
    // To start, we need to create a client. The function golioth_client_create will
    // dynamically create a client and return a handle to it.
    //
    // The client itself runs in a separate task, so once this function returns,
    // there will be a new task running in the background.
    //
    // As soon as the task starts, it will try to connect to Golioth using the
    // CoAP protocol over DTLS, with the PSK ID and PSK for authentication.
    const char* psk_id = GOLIOTH_PSK_ID;
    const char* psk = GOLIOTH_PSK;

    golioth_client_config_t config = {
            .credentials = {
                    .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
                    .psk = {
                            .psk_id = psk_id,
                            .psk_id_len = strlen(psk_id),
                            .psk = psk,
                            .psk_len = strlen(psk),
                    }}};
    golioth_client_t client = golioth_client_create(&config);
    assert(client);

    // Register a callback function that will be called by the client task when
    // connect and disconnect events happen.
    //
    // This is optional, but can be useful for synchronizing operations on connect/disconnect
    // events. For this example, the on_client_event callback will simply log a message.
    golioth_client_register_event_callback(client, on_client_event, NULL);

    // Wait for Golioth connection, up to 30 seconds
    GLTH_LOGI(TAG, "Waiting to Golioth to connect...");
    xSemaphoreTake(_connected_sem, portMAX_DELAY);

    // At this point, we have a client that can be used to interact with Golioth services:
    //      Logging
    //      Over-the-Air (OTA) firmware updates
    //      LightDB state
    //      LightDB stream

    // We'll start by logging a message to Golioth.
    //
    // This is an "asynchronous" function, meaning that this log message will be
    // copied into a queue for later transmission by the client task, and this function
    // will return immediately. Any functions provided by this SDK ending in _async
    // will have the same meaning.
    //
    // The last two arguments are for an optional callback, in case the user wants to
    // be notified of when the log has been received by the Golioth server. In this
    // case we set them to NULL, which makes this a "fire-and-forget" log request.
    golioth_log_info_async(client, "app_main", "Hello, World!", NULL, NULL);

    // We can also log messages "synchronously", meaning the function will block
    // until one of 3 things happen (whichever comes first):
    //
    //  1. We receive a response to the request from the server
    //  2. The user-provided timeout expires
    //  3. The default client task timeout expires (GOLIOTH_COAP_RESPONSE_TIMEOUT_S)
    //
    // In this case, we will block for up to 2 seconds waiting for the server response.
    // We'll check the return code to know whether a timeout happened.
    //
    // Any function provided by this SDK ending in _sync will have the same meaning.
    golioth_status_t status = golioth_log_warn_sync(client, "app_main", "Sync log", 5);
    if (status != GOLIOTH_OK) {
        GLTH_LOGE(TAG, "Error in golioth_log_warn_sync: %s", golioth_status_to_str(status));
    }

    // For OTA, we will spawn a background task that will listen for firmware
    // updates from Golioth and automatically update firmware on the device.
    //
    // This is optional, but most real applications will probably want to use this.
    char current_version[sizeof("999.999.999")];
    snprintf(current_version, sizeof(current_version), "%d.%d.%d",
            APP_VERSION_MAJOR,
            APP_VERSION_MINOR,
            APP_VERSION_BUILD);
    golioth_fw_update_init(client, current_version);

    // There are a number of different functions you can call to get and set values in
    // LightDB state, based on the type of value (e.g. int, bool, float, string, JSON).
    golioth_lightdb_set_int_async(client, "my_int", 42, NULL, NULL);
    status = golioth_lightdb_set_string_sync(client, "my_string", "asdf", 4, 5);
    if (status != GOLIOTH_OK) {
        GLTH_LOGE(TAG, "Error setting string: %s", golioth_status_to_str(status));
    }

    // Read back the integer we set above
    int32_t readback_int = 0;
    status = golioth_lightdb_get_int_sync(client, "my_int", &readback_int, 5);
    if (status == GOLIOTH_OK) {
        GLTH_LOGI(TAG, "Synchronously got my_int = %"PRId32, readback_int);
    } else {
        GLTH_LOGE(TAG, "Synchronous get my_int failed: %s", golioth_status_to_str(status));
    }

    // To asynchronously get a value from LightDB, a callback function must be provided
    golioth_lightdb_get_async(client, "my_int", on_get_my_int, NULL);

    // We can also "observe" paths in LightDB state. The Golioth cloud will notify
    // our client whenever the resource at that path changes, without needing
    // to poll.
    //
    // This can be used to implement the "digital twin" concept that is common in IoT.
    //
    // In this case, we will observe the path desired/my_config for changes.
    // The callback will read the value, update it locally, then delete the path
    // to indicate that the desired state was processed (the "twins" should be
    // in sync at that point).
    //
    // If you want to try this out, log into Golioth console (console.golioth.io),
    // go to the "LightDB State" tab, and add a new item for desired/my_config.
    // Once set, the on_my_config callback function should be called here.
    golioth_lightdb_observe_async(client, "desired/my_config", on_my_config, NULL);

    // LightDB stream functions are nearly identical to LightDB state.
    golioth_lightdb_stream_set_int_async(client, "my_stream_int", 15, NULL, NULL);

    // We can register Remote Procedure Call (RPC) methods. RPCs allow
    // remote users to "call a function" on the device.
    //
    // In this case, the device provides a "double" method, which takes an integer input param,
    // doubles it, then returns the resulting value.
    golioth_rpc_register(client, "double", on_double, NULL);

    // We can register a callback for persistent settings. The Settings service
    // allows remote users to manage and push settings to devices that will
    // be stored in device flash.
    //
    // When the cloud has new settings for us, the on_setting function will be called
    // for each setting.
    golioth_settings_register_callback(client, on_setting);

    // Now we'll just sit in a loop and update a LightDB state variable every
    // once in a while.
    GLTH_LOGI(TAG, "Entering endless loop");
    int32_t counter = 0;
    char sbuf[32];
    while (1) {
        golioth_lightdb_set_int_async(client, "counter", counter, NULL, NULL);
        snprintf(sbuf, sizeof(sbuf), "Sending hello! %"PRId32, counter);
        golioth_log_info_async(client, "app_main", sbuf, NULL, NULL);
        counter++;
        vTaskDelay(_loop_delay_s * 1000 / portTICK_PERIOD_MS);
    };

    // That pretty much covers the basics of this SDK!
    //
    // If you log into the Golioth console, you should see the log messages and
    // LightDB state should look something like this:
    //
    // {
    //      "counter": 10,
    //      "my_int": 42,
    //      "my_string": "asdf"
    // }
}

/* [] END OF FILE */
