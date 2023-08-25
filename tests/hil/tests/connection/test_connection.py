import argparse
import sys
import os
import serial
from time import time
import re
import yaml

def wait_for_regex_in_line(ser, regex, timeout_s=20, log=True):
    start_time = time()
    while True:
        line = ser.readline().decode('utf-8', errors='replace').replace("\r\n", "")
        if line != "" and log:
            print(line)
        if time() - start_time > timeout_s:
            raise RuntimeError('Timeout')
        regex_search = re.search(regex, line)
        if regex_search:
            return regex_search

def set_setting(ser, key, value):
    ser.write('\r\n'.encode())
    wait_for_regex_in_line(ser, 'uart:', log=False)
    ser.write('settings set {} {}\r\n'.format(key, value).encode())
    wait_for_regex_in_line(ser, 'saved', log=False)

def set_credentials(ser, file):
    with open(file, 'r') as f:
        credentials = yaml.safe_load(f)

    print('===== Setting credentials via CLI (logging disabled) ====')
    settings = credentials['settings']
    for key, value in settings.items():
        set_setting(ser, key, value)

def reset(ser):
    ser.write('\r\n'.encode())
    wait_for_regex_in_line(ser, 'uart:')
    ser.write('kernel reboot cold\r\n'.encode())
    # Wait for string that prints on next boot
    wait_for_regex_in_line(ser, 'Booting Zephyr OS')
    wait_for_regex_in_line(ser, 'uart:')

def test_connect(port, baud, credentials_file):
    # Connect to the device over serial and use the shell CLI to interact and run tests
    ser = serial.Serial(port, baud, timeout=1)

    set_credentials(ser, credentials_file)

    reset(ser)

    assert None != wait_for_regex_in_line(ser, 'Golioth CoAP client connected')
