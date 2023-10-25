This is a cmake project for running unit tests on the host
machine.

To run tests:

```
./test.sh
```


To run tests with code coverage, make sure you've installed lcov:

```
sudo apt install lcov
```

Then run

```
CODECOV=1 ./test.sh
```
