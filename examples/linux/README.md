# Linux

## Install System Dependencies

```sh
sudo apt install libssl-dev
```

## Build and install libcoap

We build from source in order to use a specific commit of `libcoap`.

```sh
cd external/libcoap
cmake -E remove_directory build
cmake -E make_directory build
cd build
cmake ..
cmake --build .
sudo cmake --build . -- install
```

## Build and run

Build:

```sh
cd examples/linux/golioth_basics
./build.sh
```

Run:

```sh
build/golioth_basics
```
