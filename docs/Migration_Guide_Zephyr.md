# Migrating your code from the Golioth Zephyr SDK to the Golioth Firmware SDK

Golioth support for Zephyr RTOS is included in the [Golioth Firmware
SDK](https://github.com/golioth/golioth-firmware-sdk). All new projects,
and updates to existing projects, should use this SDK.

All the great Zephyr support you know and love is now packaged in one
Golioth repository to rule them all. From the API standpoint, the change
is mostly naming and syntax. We'll walk you through how to make those
updates in your code. Under the hood, this change means we can develop,
test, and rollout new features and better performance to all of our
supported platforms so you always have access to the best Golioth has to
offer.

The [Golioth Zephyr SDK](https://github.com/golioth/golioth-zephyr-sdk)
will be deprecated later in 2024. This migration guide details the
actions necessary to move an existing Zephyr project to the Golioth
Firmware SDK.

#### Additional resources to help migration:
- [Golioth Firmware SDK Doxygen
  reference](https://firmware-sdk-docs.golioth.io/)
- [Golioth Firmware SDK example
  code](https://github.com/golioth/golioth-firmware-sdk/tree/main/examples/zephyr)
- [Golioth Docs: Golioth Firmware
  SDK](https://docs.golioth.io/firmware/golioth-firmware-sdk/)
- [Contact the Golioth Developer Relations
  team](mailto:devrel@golioth.io) with migration questions.

## Migration Overview

1. [Update `west` manifest](#update-west-manifest)
2. [Update Kconfig files](#update-kconfig-files)
3. [Update header file includes](#update-header-file-includes)
4. [Change Golioth API calls](#change-golioth-api-calls)
5. [Special case: Changes for OTA Firmware
   Updates](#special-case-changes-for-ota-firmware-updates)
6. [Special case: File Descriptors](#special-case-file-descriptors)

## Update `west` manifest

### 1. Add `golioth-firmware-sdk` to `west.yml`

Add the Golioth Firmware SDK as an entry in the `projects` section of
your `west.yml` manifest file:

```yml
mainfest:
  projects:
    # Golioth repository.
    - name: golioth
      path: modules/lib/golioth-firmware-sdk
      revision: main
      url: https://github.com/golioth/golioth-firmware-sdk.git
      submodules: true
```

- The `submodules: true` is important to ensure Golioth Firmware SDK
  dependencies are updated by `west`.
- If your manifest file previously included an `import` section under
  the Golioth project block, it may be added here.

### 2. Remove `golioth-zephyr-sdk` from `west.yml`

Remove the Golioth Zephyr SDK entry from your `west.yml` file

```yaml
# Remove this block from your manifest file:
- name: golioth
  path: modules/lib/golioth
  revision: main
  url: https://github.com/golioth/golioth-zephyr-sdk.git
  import: west-external.yml
```

### 3. Remove the Golioth Zephyr SDK folder

Remove the `modules/lib/golioth` folder. The updated entry for Golioth
(from step 1) will install the SDK in
`modules/lib/golioth-firmware-sdk`.

### 4. Run `west update`

Run `west update` to checkout the Golioth Firmware SDK along with its
dependencies.

## Update Kconfig files

Update your Kconfig files to match the new Golioth symbol names.
Primarily this will involve changes to your `prj.conf` and perhaps
`<boardname>.conf` files.

### 1. Kconfig symbol needed for every project using Golioth

You must select this Kconfig symbol to include Golioth in your build:

```config
CONFIG_GOLIOTH_FIRMWARE_SDK=y
```

### 2. Kconfig symbols that enable Golioth services

Each Golioth service is selected individually with its own
symbol. Two new symbols have been added to enable Stream data and
LightDB State data:

```config
# Two newly added symbols
CONFIG_GOLIOTH_STREAM=y
CONFIG_GOLIOTH_LIGHTDB_STATE=y
```

These symbols for Golioth services are unchanged:

```config
# Symbols that remain unchanged
CONFIG_LOG_BACKEND_GOLIOTH=y
CONFIG_GOLIOTH_RPC=y
CONFIG_GOLIOTH_SETTINGS=y
```

Include only the services you have used in your project. The `prj.conf`
files [in Golioth example
code](https://github.com/golioth/golioth-firmware-sdk/tree/main/examples/zephyr)
is a good resource to confirm the symbols needed in your project.

Zephyr uses [the menuconfig
system](https://docs.zephyrproject.org/latest/build/kconfig/menuconfig.html)
to deliver a terminal-based graphical interface for configuration. From
the main menu, go to `Modules` &rarr; `golioth-firmware-sdk` to view all
Golioth-specific Kconfig symbols. You may type `?` when viewing a symbol
to see the symbol name and more information about how it is selected.

### 3. Sample library Kconfig symbols

If you are using the sample library in your application, change the main
symbol to the updated name which uses the singular `SAMPLE`:

```config
/* Remove this from your existing code */
CONFIG_GOLIOTH_SAMPLES_COMMON=y

/* Replace it with this updated syntax */
CONFIG_GOLIOTH_SAMPLE_COMMON=y
```

> :warning: **The symbol name has changed**: The symbol used to enable the
Golioth sample library has dropped the `S` from the end of `SAMPLES`.

Other symbol changes/additions in the Sample library are less commonly
used by our customers and  beyond the scope of this guide. Please review
the Golioth example code for guidance.

## Update header file includes

Update the Golioth header files included in your project's source code.

All Golioth Firmware SDK header files are now prefixed with `golioth`. A
complete [list of the available header
files](https://github.com/golioth/golioth-firmware-sdk/tree/main/include/golioth)
is available in the top-level `include` directory.

For example, you will always need to include the Golioth client in your
builds, here is a comparison of the old and new naming:

```c
/* Remove this from your existing code */
#include <net/golioth/system_client.h>

/* Replace it with this updated syntax */
#include <golioth/client.h>
```

The naming scheme for these files has been updated so that it is easier
to understand what includes do. This comes with the added benefit of
making it easier to intuitively know what header file you need.

### Sample header files

Golioth example code uses a common sample library. This is not
technically a part of the core SDK (it serves as a guide for developing
your own applications based on the SDK). Header files for sample library
largely retain the same naming scheme, though several new headers have
been added.

```c
/* This include remains unchanged */
#include <samples/common/net_connect.h>
```

## Change Golioth API calls

Most of the API names and formats have changed. The majority of the
migration work will be in correctly updating these calls. This section
highlights the most commonly needed changes.

> :information_source: **Compare Sample Code**: The most direct
> comparison of these changes is found by comparing samples between the
> deprecated [Golioth Zephyr
> SDK](https://github.com/golioth/golioth-zephyr-sdk/tree/main/samples)
> and the [Golioth Firmware
> SDK](https://github.com/golioth/golioth-firmware-sdk/tree/main/examples/zephyr)
> that replaces it.

### 1. Creating the Golioth client

- The Golioth client now requires a `golioth_client_config` struct that
  contains a credentials for authenticating with Golioth. This is used
  for either certificate authentication, pre-shared key authentication,
  or credential tag authentication.

    ```c
    /* Config for Certificate authentication */
    golioth_client_config_t client_config = {
        .credentials =
            {
                .auth_type = GOLIOTH_TLS_AUTH_TYPE_PKI,
                .pki =
                    {
                        .ca_cert = tls_ca_crt,
                        .ca_cert_len = sizeof(tls_ca_crt),
                        .public_cert = tls_client_crt,
                        .public_cert_len = tls_client_crt_len,
                        .private_key = tls_client_key,
                        .private_key_len = tls_client_key_len,
                    },
            },
    };

    /* Config for PSK authentication */
    golioth_client_config_t client_config = {
        .credentials =
            {
                .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
                .psk =
                    {
                        .psk_id = client_psk_id,
                        .psk_id_len = client_psk_id_len,
                        .psk = client_psk,
                        .psk_len = client_psk_len,
                    },
            },
    };

    /* Config for credential tag authentication */
    golioth_client_config_t client_config = {
        .credentials =
            {
                .auth_type = GOLIOTH_TLS_AUTH_TYPE_TAG,
                .tag = YOUR_CREDENTIALS_TAG,
            },
    };
    ```

- Pass a pointer to the configuration when creating the Golioth client:

    ```c
    golioth_client_t client = golioth_client_create(&client_config);
    ```

- The callback that runs when Golioth connects is now registered using a
  function call, and the SDK now passes a `golioth_client_event` into
  the callback:

    ```c
    /* Remove this from your code */
    client->on_connect = golioth_on_connect;

    /* Replace it with this updated syntax */
    golioth_client_register_event_callback(client, on_client_event, NULL);
    ```

    Replace your callback up the updated callback format as follow:

    ```c
    /* Replace your callback with one of this format: */
    static void on_client_event(struct golioth_client *client,
                                enum golioth_client_event event,
                                void *arg) {
        bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
        if (is_connected) {
            /* this was called due to a "connected" event */
        }
    }
    ```

- The Golioth sample common library includes helper functions that
  return certificate or PSK configurations for hardcoded and runtime
  credentials. View the [example code in the Golioth Firmware
  SDK](https://github.com/golioth/golioth-firmware-sdk/tree/main/examples/zephyr)
  to see how these are used.

> :link: Please see the [Golioth Client Doxygen
> page](https://firmware-sdk-docs.golioth.io/group__golioth__client.html)
> for details.

### 2. Replacing LightDB Stream calls with Stream calls

- The naming for LightDB Stream calls has been changed, most notably by
  removing the word `lightdb` and just using `stream` to differentiate
  these calls from LightDB State.

  The argument format remains similar, but the content type names have
  changed:

  - `GOLIOTH_CONTENT_TYPE_JSON`
  - `GOLIOTH_CONTENT_TYPE_CBOR`

- Here are examples of synchronous and
  asynchronous calls using JSON and CBOR content types:

  ```c
  /* Synchronous using JSON */
  char sbuf[] = "{\"mykey\":42}";
  #define APP_TIMEOUT_S 1

  golioth_stream_set_sync(client,
                          "path/to/set",
                          GOLIOTH_CONTENT_TYPE_JSON,
                          json_buf,
                          sizeof(json_buf),
                          APP_TIMEOUT_S);

  /* Asynchronous using CBOR */
  char cbor_buf[] = {0xA1, 0x65, 0x6D, 0x79, 0x6B, 0x65, 0x79, 0x18, 0x2A};

  golioth_stream_set_async(client,
                          "path/to/set",
                          GOLIOTH_CONTENT_TYPE_CBOR,
                          cbor_buf,
                          sizeof(cbor_buf),
                          name_of_your_callback,
                          pointer_to_optional_callback_arg);
  ```

- Many type-specific Stream calls have been added.
- Please see the [Callbacks](#6.-callbacks) section of this guide for
  information on updating Stream callbacks.

> :link: Please see the [Golioth Stream Doxygen
> page](https://firmware-sdk-docs.golioth.io/group__golioth__stream.html)
> for details.

### 3. Updating LightDB State calls

- The naming for the LightDB State calls has changed. The argument
  format remains similar, but the content type names have changed:

  - `GOLIOTH_CONTENT_TYPE_JSON`
  - `GOLIOTH_CONTENT_TYPE_CBOR`

  Here are examples of synchronous and
  asynchronous calls using JSON and CBOR content types:

  ```c
  /* Synchronous using JSON */
  char sbuf[] = "{\"mykey\":42}";
  #define APP_TIMEOUT_S 1

  golioth_lightdb_set_sync(client,
                          "path/to/set",
                          GOLIOTH_CONTENT_TYPE_JSON,
                          json_buf,
                          sizeof(json_buf),
                          APP_TIMEOUT_S);


  /* Asynchronous using CBOR */
  char cbor_buf[] = {0xA1, 0x65, 0x6D, 0x79, 0x6B, 0x65, 0x79, 0x18, 0x2A};

  golioth_lightdb_set_async(client,
                            "path/to/set",
                            GOLIOTH_CONTENT_TYPE_CBOR,
                            cbor_buf,
                            sizeof(cbor_buf),
                            name_of_your_callback,
                            pointer_to_optional_callback_arg);
  ```

- Many type-specific LightDB State calls have been added.
- Please see the [Callbacks](#6.-callbacks) section of this guide for
  information on updating LightDB State callbacks.

> :link: Please see the [Golioth LightDB State Doxygen
> page](https://firmware-sdk-docs.golioth.io/group__golioth__lightdb__state.html)
> for details.

### 4. Updating RPCs

- Registering an RPC now requires a `golioth_rpc` struct be passed as
  the first parameter:

    ```c
    struct golioth_rpc *rpc = golioth_rpc_init(client);

    golioth_rpc_register(rpc, "multiply", on_multiply, NULL);
    ```

- An observation call is no longer needed for RPC and can be removed
  from your application:

    ```c
    /* Remove RPC observations call like this one */
    golioth_rpc_observe(client);
    ```

- RPC callbacks remain unchanged.

> :link: Please see the [Golioth RPC Doxygen
> page](https://firmware-sdk-docs.golioth.io/group__golioth__rpc.html)
> for details.

### 5. Updating Settings calls

* A `golioth_settings` struct is now a required parameter for
  registering a Settings callback:

    ```c
    struct golioth_settings *settings = golioth_settings_init(client);
    ```

* The type is now included in the name of the function that registers a
  Settings callback:

    ```c
    golioth_settings_register_int(settings,
                                  "name_of_setting",
                                  name_of_your_callback,
                                  pointer_to_optional_callback_arg);
    ```

    Functions exist for `_bool`, `_float` `_int`, `_string`.

    There is one additional settings register function that accepts
    parameters for bounding an integer setting:
    `golioth_settings_register_int_with_range()`.

* Callbacks for settings all take the same format, with the first
  parameter matching the type of the function that registered it:

    ```c
    static enum golioth_settings_status name_of_your_callback(int32_t new_value,
                                                              void* arg)
    ```

> :link: Please see the [Golioth Settings Doxygen
> page](https://firmware-sdk-docs.golioth.io/group__golioth__lightdb__state.html)
> for details.

### 6. Callbacks

- There are two types of callbacks related to working with data: `get`
  and `set`. These callbacks use the same format whether they're
  registered with a Stream or a LightDB State call. The name of the
  callback function is arbitrary. It must match the name given when
  registering the callback.

    ```c
    /* callback for a `set` async call */
    static void counter_set_handler(
            struct golioth_client* client,
            const struct golioth_response* response,
            const char* path,
            void* arg) {
        if (response->status != GOLIOTH_OK) {
            LOG_WRN("Failed to set counter: %d", response->status);
            return;
        }

        LOG_DBG("Counter successfully set");

        return;
    }

    /* callback for a `get` async call */
    static void counter_get_handler(
            struct golioth_client* client,
            const struct golioth_response* response,
            const char* path,
            const uint8_t* payload,
            size_t payload_size,
            void* arg) {
        if ((response->status != GOLIOTH_OK) || golioth_payload_is_null(payload, payload_size)) {
            LOG_WRN("Failed to get counter (async): %d", response->status);
        }
        else
        {
            LOG_INF("Counter (async): %d", golioth_payload_as_int(payload, payload_size));
        }
    }
    ```

- The `response` parameter includes a `status` member that returns
  `GOLIOTH_OK` or [one of the error values declared by
  `golioth_status.h`](https://firmware-sdk-docs.golioth.io/golioth__status_8h_source.html#l00009)

> :link: Please see the [Golioth Client Doxygen
> page](https://firmware-sdk-docs.golioth.io/group__golioth__client.html)
> for details about these callback typedefs.

## Special case: Changes for OTA Firmware Updates

For Over-the-Air (OTA) Firmware Updates, the Golioth Firmware SDK has
moved most of the structural code out of user space. This means that
adding OTA to a project is much easier. Adapting existing code to this
approach involves a few steps:

1. Remove the `flash.c` and `flash.h` from your application
2. Update the necessary header files
3. Update the necessary Kconfig symbols
4. Update the API calls

This section serves as a mini-guide for updating your firmware update
code.

> :information_source: **The example code name has changed**: Please
> note that name of the code example for Golioth's OTA service has
> changed from `dfu` to `fw_update`.

### 1. Remove flash files

The following files used for OTA updates are not included in the SDK and
no longer needed in your application. Please remove them, and remove
references to them from your `CMakeLists.txt` files:

- `flash.c`
- `flash.h`

### 2. Firmware update header files

Update the include necessary for firmware update:

```c
/* Remove this from your existing code */
#include <net/golioth/fw.h>

/* Replace it with this updated syntax */
#include <golioth/fw_update.h>
```

### 3. Change firmware update Kconfig symbols

Update the name of the Kconfig symbol for firmware update:

```config
# Remove this from your existing code
CONFIG_GOLIOTH_FW=y

# Replace it with this updated syntax
CONFIG_GOLIOTH_FW_UPDATE=y
```

The version number of your application can now be specified using a
Kconfig symbol. Our example places this in `prj.conf`:

```config
# Firmware version used in FW Update process
CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION="1.2.3"
```

> :information_source: **Flash-related symbols are still needed**: The
> [Kconfig symbols used to enable flash memory
> write/erase](https://github.com/golioth/golioth-firmware-sdk/blob/81e7e1d2ec7d9758be6aec260ad17a52833ed5dd/examples/zephyr/fw_update/prj.conf#L10-L14)
> processes are still needed. However, you no longer need to select
> `CONFIG_REBOOT=y` as this is now selected by the SDK.

### 4. Firmware Update API calls

The API calls used for firmware update are greatly simplified. Your
firmware only needs to call just one function that takes the semantic
version number as a string:

```c
// Current firmware version; update in prj.conf or via build argument
static const char *_current_version = CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION;

int main(void) {
  /* create the Golioth client before registering for firmware update*/

  /* register for firmware updates */
  golioth_fw_update_init(client, _current_version);

  /* continue to the loop */
}
```

## Special case: File Descriptors

The Golioth Firmware SDK uses an event-based system that reduces
latency. This relies on Zephyr's file descriptor system.

We value your device resources and have kept the Golioth footprint as
low as possible. You may find that there are not enough file descriptors
allocated for your program, indicated by an error message like the
following:

```log
[00:00:07.342,000] <err> golioth_sys_zephyr: eventfd creation failed, errno: 12
[00:00:07.342,000] <err> golioth_sys_zephyr: eventfd creation failed, errno: 12
[00:00:07.342,000] <err> golioth_sys_zephyr: eventfd creation failed, errno: 12
```

This means you have exceeded the number of available file descriptors.
These limits may be increased using the following Kconfig symbols which
should be increased in tandem to meet your application's needs:

```config
# Default FD settings
CONFIG_EVENTFD_MAX=6
CONFIG_POSIX_MAX_FDS=16
```

