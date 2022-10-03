/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * Golioth modified, loosely based on the bleprph example in esp-idf:
 *
 * https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/nimble/bleprph/main/main.c
 */
#include "ble.h"
#include "nvs.h"

#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/ans/ble_svc_ans.h"
#include "esp_log.h"

#define TAG "golioth_ble"

#define PREFERRED_ATT_MTU 500
#define GATT_SERVER_SVC_ALERT_UUID 0x1811

// Forward declarations of external structs/functions
struct ble_hs_cfg;
struct ble_gatt_register_ctxt;
struct ble_gatt_svc_def;
void ble_store_config_init(void);

typedef const char* (*nvs_read_fn)(void);

static void ble_advertise(void);

// clang-format off
/// Golioth Provisioning Service
/// 1f45bcd1-4700-4b82-b278-745c86226373
static const ble_uuid128_t gatt_svr_svc_golioth_prov_uuid =
    BLE_UUID128_INIT(0x73, 0x63, 0x22, 0x86, 0x5c, 0x74, 0x78, 0xb2,
                     0x82, 0x4b, 0x00, 0x47, 0xd1, 0xbc, 0x45, 0x1f);

/// Golioth Provisioning Service, Characteristic WiFi SSID
/// 8d69e681-ea3a-488b-b88b-dd9fa552585a
static const ble_uuid128_t gatt_svr_chr_golioth_prov_wifi_ssid_uuid =
    BLE_UUID128_INIT(0x5a, 0x58, 0x52, 0xa5, 0x9f, 0xdd, 0x8b, 0xb8,
                     0x8b, 0x48, 0x3a, 0xea, 0x81, 0xe6, 0x69, 0x8d);

/// Golioth Provisioning Service, Characteristic WiFi PSK
/// bb4e7294-57e2-4a13-9322-c35e6bd6dc49
static const ble_uuid128_t gatt_svr_chr_golioth_prov_wifi_psk_uuid =
    BLE_UUID128_INIT(0x49, 0xdc, 0xd6, 0x6b, 0x5e, 0xc3, 0x22, 0x93,
                     0x13, 0x4a, 0xe2, 0x57, 0x94, 0x72, 0x4e, 0xbb);

/// Golioth Provisioning Service, Characteristic Golioth PSK-ID
/// 54afcfe6-57f3-4b2e-b812-37ec84c4067a
static const ble_uuid128_t gatt_svr_chr_golioth_prov_golioth_psk_id_uuid =
    BLE_UUID128_INIT(0x7a, 0x06, 0xc4, 0x84, 0xec, 0x37, 0x12, 0xb8,
                     0x2e, 0x4b, 0xf3, 0x57, 0xe6, 0xcf, 0xaf, 0x54);

/// Golioth Provisioning Service, Characteristic Golioth PSK
/// 96b7479b-368f-430e-bf47-b3404f28a2c8
static const ble_uuid128_t gatt_svr_chr_golioth_prov_golioth_psk_uuid =
    BLE_UUID128_INIT(0xc8, 0xa2, 0x28, 0x4f, 0x40, 0xb3, 0x47, 0xbf,
                     0x0e, 0x43, 0x8f, 0x36, 0x9b, 0x47, 0xb7, 0x96);

static int golioth_prov_service_cb(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt,
                                   void *arg);

static const struct ble_gatt_svc_def _svcs[] = {
    {
        // Service: Golioth Provisioning
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_golioth_prov_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[])
        {
            {
                // Characteristic: WiFi SSID
                .uuid = &gatt_svr_chr_golioth_prov_wifi_ssid_uuid.u,
                .access_cb = golioth_prov_service_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            {
                // Characteristic: WiFi PSK
                .uuid = &gatt_svr_chr_golioth_prov_wifi_psk_uuid.u,
                .access_cb = golioth_prov_service_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            {
                // Characteristic: Golioth PSK-ID
                .uuid = &gatt_svr_chr_golioth_prov_golioth_psk_id_uuid.u,
                .access_cb = golioth_prov_service_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            {
                // Characteristic: WiFi PSK
                .uuid = &gatt_svr_chr_golioth_prov_golioth_psk_uuid.u,
                .access_cb = golioth_prov_service_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            {
                0, // No more characteristics in this service.
            }
        },
    },
    {
        0, // No more services.
    },
};
// clang-format on

static int read_credential_from_nvs(struct ble_gatt_access_ctxt* ctxt, nvs_read_fn read_fn) {
    const char* value = read_fn();
    int rc = os_mbuf_append(ctxt->om, value, strlen(value));
    return (rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES);
}

static int write_credential_to_nvs(struct ble_gatt_access_ctxt* ctxt, const char* nvs_key) {
    static char buf[512];
    memset(buf, 0, sizeof(buf));
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, NULL);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    ESP_LOGD(TAG, "Setting %s to %s", nvs_key, buf);
    bool success = nvs_write_str(nvs_key, buf);
    return (success ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES);
}

static int golioth_prov_service_cb(
        uint16_t conn_handle,
        uint16_t attr_handle,
        struct ble_gatt_access_ctxt* ctxt,
        void* arg) {
    const ble_uuid_t* uuid = ctxt->chr->uuid;

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_golioth_prov_wifi_ssid_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            return read_credential_from_nvs(ctxt, nvs_read_wifi_ssid);
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            return write_credential_to_nvs(ctxt, NVS_WIFI_SSID_KEY);
        } else {
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    } else if (ble_uuid_cmp(uuid, &gatt_svr_chr_golioth_prov_wifi_psk_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            return read_credential_from_nvs(ctxt, nvs_read_wifi_password);
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            return write_credential_to_nvs(ctxt, NVS_WIFI_PASS_KEY);
        } else {
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    } else if (ble_uuid_cmp(uuid, &gatt_svr_chr_golioth_prov_golioth_psk_id_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            return read_credential_from_nvs(ctxt, nvs_read_golioth_psk_id);
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            return write_credential_to_nvs(ctxt, NVS_GOLIOTH_PSK_ID_KEY);
        } else {
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    } else if (ble_uuid_cmp(uuid, &gatt_svr_chr_golioth_prov_golioth_psk_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            return read_credential_from_nvs(ctxt, nvs_read_golioth_psk);
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            return write_credential_to_nvs(ctxt, NVS_GOLIOTH_PSK_KEY);
        } else {
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    } else {
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static int gatt_server_init(const struct ble_gatt_svc_def* services) {
    int rc = 0;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_ans_init();

    rc = ble_gatts_count_cfg(services);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg error: %d", rc);
        return rc;
    }

    rc = ble_gatts_add_svcs(services);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs error: %d", rc);
        return rc;
    }

    return 0;
}

static void gatt_server_register_cb(struct ble_gatt_register_ctxt* ctxt, void* arg) {
    char buf[BLE_UUID_STR_LEN] = {};

    switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGD(
                    TAG,
                    "registered service %s with handle=%d",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
            break;

        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGD(
                    TAG,
                    "registering characteristic %s with "
                    "def_handle=%d val_handle=%d",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
            break;

        case BLE_GATT_REGISTER_OP_DSC:
            ESP_LOGD(
                    TAG,
                    "registering descriptor %s with handle=%d",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
            break;

        default:
            break;
    }
}

static void print_connection_desc(const struct ble_gap_conn_desc* desc) {
    ESP_LOGD(
            TAG,
            "handle=%d our_ota_addr_type=%d our_ota_addr=%02x:%02x:%02x:%02x:%02x:%02x",
            desc->conn_handle,
            desc->our_ota_addr.type,
            desc->our_ota_addr.val[5],
            desc->our_ota_addr.val[4],
            desc->our_ota_addr.val[3],
            desc->our_ota_addr.val[2],
            desc->our_ota_addr.val[1],
            desc->our_ota_addr.val[0]);

    ESP_LOGD(
            TAG,
            "our_id_addr_type=%d our_id_addr=%02x:%02x:%02x:%02x:%02x:%02x",
            desc->our_id_addr.type,
            desc->our_id_addr.val[5],
            desc->our_id_addr.val[4],
            desc->our_id_addr.val[3],
            desc->our_id_addr.val[2],
            desc->our_id_addr.val[1],
            desc->our_id_addr.val[0]);

    ESP_LOGD(
            TAG,
            "peer_ota_addr_type=%d peer_ota_addr=%02x:%02x:%02x:%02x:%02x:%02x",
            desc->peer_ota_addr.type,
            desc->peer_ota_addr.val[5],
            desc->peer_ota_addr.val[4],
            desc->peer_ota_addr.val[3],
            desc->peer_ota_addr.val[2],
            desc->peer_ota_addr.val[1],
            desc->peer_ota_addr.val[0]);

    ESP_LOGD(
            TAG,
            "peer_id_addr_type=%d peer_id_addr=%02x:%02x:%02x:%02x:%02x:%02x",
            desc->peer_id_addr.type,
            desc->peer_id_addr.val[5],
            desc->peer_id_addr.val[4],
            desc->peer_id_addr.val[3],
            desc->peer_id_addr.val[2],
            desc->peer_id_addr.val[1],
            desc->peer_id_addr.val[0]);

    ESP_LOGI(
            TAG,
            "conn_itvl=%d conn_latency=%d supervision_timeout=%d "
            "encrypted=%d authenticated=%d bonded=%d",
            desc->conn_itvl,
            desc->conn_latency,
            desc->supervision_timeout,
            desc->sec_state.encrypted,
            desc->sec_state.authenticated,
            desc->sec_state.bonded);
}

// NimBLE calls this when a GAP event occurs (for all connections).
// Return 0 on success, non-zero on failure (event-dependent).
static int on_ble_gap_event(struct ble_gap_event* event, void* arg) {
    struct ble_gap_conn_desc desc = {};
    int rc = 0;

    ESP_LOGD(TAG, "GAP event: %d", event->type);

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            // A new connection was established or a connection attempt failed
            ESP_LOGI(
                    TAG,
                    "connection %s; status=%d ",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);
            if (event->connect.status == 0) {
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                if (rc == 0) {
                    print_connection_desc(&desc);

                    // Initiate GAP security procedure
                    rc = ble_gap_security_initiate(event->connect.conn_handle);
                    if (rc != 0) {
                        ESP_LOGE(TAG, "ble_gap_security_inititate failed: %d", rc);
                    }
                } else {
                    ESP_LOGE(TAG, "ble_gap_conn_find failed: %d", rc);
                }
            }

            // Resume advertising, to allow another connection
            ble_advertise();
            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "disconnect; reason=%d ", event->disconnect.reason);
            print_connection_desc(&event->disconnect.conn);
            // Connection terminated; resume advertising
            ble_advertise();
            return 0;

        case BLE_GAP_EVENT_CONN_UPDATE:
            // The central has updated the connection parameters
            ESP_LOGI(TAG, "connection updated; status=%d ", event->conn_update.status);
            rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
            if (rc == 0) {
                print_connection_desc(&desc);
            } else {
                ESP_LOGE(TAG, "ble_gap_conn_find failed: %d", rc);
            }
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "advertise complete; reason=%d", event->adv_complete.reason);
            ble_advertise();
            return 0;

        case BLE_GAP_EVENT_ENC_CHANGE:
            // Encryption has been enabled or disabled for this connection
            ESP_LOGI(TAG, "encryption change event; status=%d ", event->enc_change.status);
            rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
            assert(rc == 0);
            print_connection_desc(&desc);
            return 0;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(
                    TAG,
                    "subscribe event; conn_handle=%d attr_handle=%d "
                    "reason=%d prevn=%d curn=%d previ=%d curi=%d",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle,
                    event->subscribe.reason,
                    event->subscribe.prev_notify,
                    event->subscribe.cur_notify,
                    event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);
            return 0;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(
                    TAG,
                    "mtu update event; conn_handle=%d cid=%d mtu=%d",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
            return 0;

        case BLE_GAP_EVENT_REPEAT_PAIRING:
            // We already have a bond with the peer, but it is attempting to
            // establish a new secure link. This app sacrifices security for
            // convenience: just throw away the old bond and accept the new link.

            // Delete the old bond
            rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            assert(rc == 0);
            ble_store_util_delete_peer(&desc.peer_id_addr);

            // Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
            // continue with the pairing operation.
            return BLE_GAP_REPEAT_PAIRING_RETRY;

        default:
            ESP_LOGD(TAG, "Unhandled GAP event: %d", event->type);
            return 0;
    }

    return 0;
}

static void ble_advertise(void) {
    int rc = 0;

    // Set the default advertisement data included in our advertisements:
    //      * Flags (indicates advertisement type and other general info)
    //      * Advertising tx power
    //      * Device name
    //      * 16-bit service UUIDs (alert notifications)
    struct ble_hs_adv_fields fields = {};

    // Advertise two flags:
    //     * Discoverability in forthcoming advertisement (general)
    //     * BLE-only (BR/EDR unsupported)
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Indicate that the TX power level field should be included; have the
    // stack fill this value automatically.  This is done by assigning the
    // special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    const char* name = ble_svc_gap_device_name();
    fields.name = (const uint8_t*)name;
    fields.name_len = (uint8_t)strnlen(name, 255);
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]){BLE_UUID16_INIT(GATT_SERVER_SVC_ALERT_UUID)};
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement fields; rc=%d", rc);
        return;
    }

    // Begin advertising
    struct ble_gap_adv_params adv_params = {
            .conn_mode = BLE_GAP_CONN_MODE_UND,
            .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };
    rc = ble_gap_adv_start(
            BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, on_ble_gap_event, NULL);
    if (rc != 0) {
        if (rc == BLE_HS_ENOMEM) {
            // This is expected if we have reached the maximum number of connections,
            // so don't log an error here.
        } else if (rc == BLE_HS_EALREADY) {
            // Harmless - already advertising
        } else {
            ESP_LOGE(TAG, "error starting advertisement; rc=%d", rc);
        }
        return;
    }
}

static void on_ble_reset(int reason) {
    // If this gets called, all connections have been dropped, and we should wait
    // until on_ble_sync is called again to resume operation.
    ESP_LOGW(TAG, "Resetting state; reason = %d", reason);
}

static void on_ble_sync(void) {
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr error: %d", rc);
        return;
    }

    // Figure out address to use while advertising
    uint8_t own_addr_type = BLE_OWN_ADDR_PUBLIC;
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type: %d", rc);
        return;
    }

    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    ESP_LOGD(
            TAG,
            "Device Address = %02x:%02x:%02x:%02x:%02x:%02x",
            addr_val[5],
            addr_val[4],
            addr_val[3],
            addr_val[2],
            addr_val[1],
            addr_val[0]);

    ble_advertise();
}

static void ble_host_task(void* param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    // This function will return only when nimble_port_stop() is executed
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_init(const char* device_name) {
    esp_err_t err = esp_nimble_hci_and_controller_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_nimble_hci_and_controller_init error: %d", err);
        return;
    }

    nimble_port_init();

    // Initialize the NimBLE host configuration
    ble_hs_cfg.reset_cb = on_ble_reset;
    ble_hs_cfg.sync_cb = on_ble_sync;
    ble_hs_cfg.gatts_register_cb = gatt_server_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_sc = 1;                        // use secure pairing if partner supports it
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;  // no input/output, aka "Just Works"
    ble_hs_cfg.sm_bonding = 1;                   // enable bonding
    ble_hs_cfg.sm_our_key_dist = 1;
    ble_hs_cfg.sm_their_key_dist = 1;
    ble_hs_cfg.sm_mitm = 0;  // no authentication

    // Initialize the GATT server
    if (gatt_server_init(_svcs) != 0) {
        ESP_LOGE(TAG, "Failed to initialize GATT server");
        return;
    }

    // Set the default device name
    if (ble_svc_gap_device_name_set(device_name) != 0) {
        ESP_LOGE(TAG, "Failed to set GAP device name");
        return;
    }

    int rc = ble_att_set_preferred_mtu(PREFERRED_ATT_MTU);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set preferred MTU");
        return;
    }

    ble_store_config_init();

    nimble_port_freertos_init(ble_host_task);
}
