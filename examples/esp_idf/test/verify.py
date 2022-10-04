import sys
import os
import serial
from time import time
import re
import yaml

def wait_for_str_in_line(ser, str, timeout_s=20, log=True):
    start_time = time()
    while True:
        line = ser.readline().decode('utf-8', errors='replace').replace("\r\n", "")
        if line != "" and log:
            print(line)
        if "CPU halted" in line and not "golioth_coap_client" in line:
            raise RuntimeError(line)
        if time() - start_time > timeout_s:
            raise RuntimeError('Timeout')
        if str in line:
            return line

def set_credentials(ser):
    with open('credentials.yml', 'r') as f:
        credentials = yaml.safe_load(f)

    print('===== Setting credentials via CLI (logging disabled) ====')
    ser.write('\r\n'.encode())
    wait_for_str_in_line(ser, 'esp32>', log=False)

    settings = credentials['settings']
    for key, value in settings.items():
        ser.write('settings set {} {}\r\n'.format(key, value).encode())
        wait_for_str_in_line(ser, 'saved', log=False)
        wait_for_str_in_line(ser, 'esp32>', log=False)

def reset(ser):
    ser.write('\r\n'.encode())
    wait_for_str_in_line(ser, 'esp32>')
    ser.write('reset\r\n'.encode())
    # Wait for string that prints on next boot
    wait_for_str_in_line(ser, 'heap_init')
    wait_for_str_in_line(ser, 'esp32>')

def green_print(s):
    green = '\033[92m'
    resetcolor = '\033[0m'
    print(green + s + resetcolor)

def red_print(s):
    red = '\033[31m'
    resetcolor = '\033[0m'
    print(red + s + resetcolor)

def run_built_in_tests(ser):
    unity_test_end_re = '\.c:\d+:test_.*:(PASS|FAIL)'
    unity_done_re = '^\d+ Tests (\d+) Failures \d+ Ignored'
    num_test_failures = 0
    test_results = []

    ser.write('\r\n'.encode())
    wait_for_str_in_line(ser, 'esp32>')
    ser.write('built_in_test\r\n'.encode())

    start_time = time()
    timeout_s = 120
    while True:
        line = ser.readline().decode('utf-8', errors='replace').replace("\r\n", "")
        if line != "":
            print(line)
        if "CPU halted" in line:
            raise RuntimeError(line)
        if time() - start_time > timeout_s:
            raise RuntimeError('Timeout')

        unity_done = re.search(unity_done_re, line)
        if unity_done:
            num_test_failures = int(unity_done.groups()[0])
            print('================ END OF DEVICE OUTPUT ================')
            for tr in test_results:
                print(tr)
            return num_test_failures

        if re.search(unity_test_end_re, line):
            test_results.append(line)

def run_ota_test(ser):
    ser.write('\r\n'.encode())
    wait_for_str_in_line(ser, 'esp32>')
    ser.write('start_ota\r\n'.encode())
    wait_for_str_in_line(ser, 'State = Downloading')
    wait_for_str_in_line(ser, 'Erasing flash')
    wait_for_str_in_line(ser, 'Received response')

    # At this point the download is proceeding, so wait up to 8 minutes for it to complete.
    wait_for_str_in_line(ser, 'Total bytes written', 480)

    wait_for_str_in_line(ser, 'State = Downloaded')
    wait_for_str_in_line(ser, 'State = Updating')
    wait_for_str_in_line(ser, 'Rebooting into new image')
    wait_for_str_in_line(ser, 'heap_init') # on new boot
    wait_for_str_in_line(ser, 'esp32>')
    ser.write('\r\n'.encode())
    wait_for_str_in_line(ser, 'esp32>')

    # Issue the start_ota command one last time to finalize the OTA process
    ser.write('start_ota\r\n'.encode())
    wait_for_str_in_line(ser, 'Firmware updated successfully!')

    # A final reset, to make sure OTA partitions are stable, no accidental rollbacks
    reset(ser)
    ser.write('start_ota\r\n'.encode())
    wait_for_str_in_line(ser, 'Manifest does not contain different firmware version')

def main():
    if len(sys.argv) != 2:
        print('usage: {} <port>'.format(sys.argv[0]))
        sys.exit(-1)
    port = sys.argv[1]

    # Connect to the device over serial and use the shell CLI to interact and run tests
    ser = serial.Serial(port, 115200, timeout=1)

    # Set WiFi and Golioth credentials over device shell CLI
    set_credentials(ser)
    reset(ser)

    # Run built in tests on the device and check output
    for _ in range(1):
        num_test_failures = run_built_in_tests(ser)
        if num_test_failures != 0:
            break
        reset(ser)

    if num_test_failures == 0:
        run_ota_test(ser)

    if num_test_failures == 0:
        green_print('---------------------------')
        print('')
        green_print('  ✓ All Tests Passed 🎉')
        print('')
        green_print('---------------------------')
    else:
        red_print('---------------------------')
        print('')
        red_print('  ✗ Failed {} Tests'.format(num_test_failures))
        print('')
        red_print('---------------------------')

    sys.exit(num_test_failures)

if __name__ == "__main__":
    main()
