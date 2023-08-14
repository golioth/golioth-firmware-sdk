# Linux

## Install System Dependencies

```sh
sudo apt install libssl-dev
```

## Configure PSK and PSK-ID credentials

For the `golioth_basics` example, credentials are sourced from the
`GOLIOTH_PSK_ID` and `GOLIOTH_PSK` environment variables.

See the `certificate_auth` example [README.md](./certificate_auth/README.md) for
information on how to configure certificates.

## Build and run

Build:

```sh
cd examples/linux/<example>
./build.sh
```

Run:

```sh
build/<example>
```
