# Golioth Basics Example

This example will connect to Golioth and demonstrate Logging, Over-the-Air (OTA)
firmware updates, and sending data to both LightDB state and Stream.
Please see the comments in app_main.c for a thorough explanation of the expected
behavior.

## Adding credentials

After building and flashing this app, you will need to add WiFi and Golioth
credentials using the device shell.

### Shell Credentials

```console
settings set wifi/ssid "YourWiFiAccessPointName"
settings set wifi/psk "YourWiFiPassword"
settings set golioth/psk-id "YourGoliothDevicePSK-ID"
settings set golioth/psk "YourGoliothDevicePSK"
```

Type `reset` to restart the app with the new credentials.
