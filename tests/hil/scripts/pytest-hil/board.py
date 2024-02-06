from abc import ABC, abstractmethod
import serial
from time import time, sleep
import re
import yaml

class Board(ABC):
    def __init__(self, port, baud, credentials_file, fw_image, serial_number):
        if serial_number:
            self.serial_number = serial_number
        self.port = port

        if fw_image:
            self.program(fw_image)

            # Wait for reboot
            sleep(3)

        self.serial_device = serial.Serial(port, baud, timeout=1)

        # Read credentials
        with open(credentials_file, 'r') as f:
            credentials = yaml.safe_load(f)
            settings = credentials['settings']

        # Set PSK credentials
        self.set_golioth_psk_credentials(settings['golioth/psk-id'], settings['golioth/psk'])

        # Set WiFi credentials
        if self.USES_WIFI:
            self.set_wifi_credentials(settings['wifi/ssid'], settings['wifi/psk'])

    def wait_for_regex_in_line(self, regex, timeout_s=20, log=True):
        start_time = time()
        while True:
            self.serial_device.timeout=timeout_s
            line = self.serial_device.read_until().decode('utf-8', errors='replace').replace("\r\n", "")
            if line != "" and log:
                print(line)
            if time() - start_time > timeout_s:
                raise RuntimeError('Timeout')
            regex_search = re.search(regex, line)
            if regex_search:
                return regex_search

    def send_cmd(self, cmd, wait_str=None):
        if wait_str == None:
            wait_str = self.PROMPT
        self.serial_device.write('\r\n'.encode())
        self.wait_for_regex_in_line(self.PROMPT)
        self.serial_device.write('{}\r\n'.format(cmd).encode())
        self.wait_for_regex_in_line(wait_str)

    @property
    @abstractmethod
    def PROMPT(self):
        pass

    @property
    @abstractmethod
    def USES_WIFI(self):
        pass

    @abstractmethod
    def reset(self):
        pass

    @abstractmethod
    def program(self, fw_image):
        pass

    @abstractmethod
    def set_wifi_credentials(self, ssid, psk):
        pass

    @abstractmethod
    def set_golioth_psk_credentials(self, psk_id, psk):
        pass
