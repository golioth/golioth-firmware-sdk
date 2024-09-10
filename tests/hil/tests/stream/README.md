# Stream Test

This tests the stream service on the device.

## Test data for block upload

Test data is stored in `test_data.json`. If this file is updated, the
`test_data.h` file must also be updated using:

```
xxd -i test_data.json > test_data.h
```
