# Connection Test

This is a simple test that determines whether or not the DUT is able to
establish a connection with the Golioth backend. This is useful as a basic
smoke test. The test requires an attached board and expects credentials
to be present in a YAML file with the following format:

```yaml
settings:
  wifi/ssid: MyWiFiSSID
  wifi/psk: MyWiFiPassword
  golioth/psk-id: device@project
  golioth/psk: supersecret
```

The test can be invoked by the following command:

```sh
pytest --rootdir . --port /path/to/serial/port --credentials_file /path/to/credentials/file
```
