from abc import ABC, abstractmethod
from contextlib import asynccontextmanager
import serial
from time import time, sleep
import re

import trio
from trio_serial import SerialStream

class Board(ABC):
    def __init__(self, port, baudrate, wifi_ssid, wifi_psk, fw_image, serial_number):
        if serial_number:
            self.serial_number = serial_number
        self.port = port
        self.baudrate = baudrate
        self.fw_image = fw_image
        self.wifi_ssid = wifi_ssid
        self.wifi_psk = wifi_psk

        self.received: bytes = b''

        self.serial: SerialStream | None = None

    @asynccontextmanager
    async def started(self):
        async with SerialStream(port=self.port,
                                baudrate=self.baudrate) as serial:
            self.serial = serial

            if self.fw_image:
                self.program(self.fw_image)

            # Wait for reboot
            await trio.sleep(6)

            # Set WiFi credentials
            if self.USES_WIFI:
                await self.set_wifi_credentials(self.wifi_ssid,
                                                    self.wifi_psk)

            yield self

    async def receive_some(self) -> bytes:
        assert self.serial is not None

        return await self.serial.receive_some()

    async def wait_for_regex_in_line(self, regex, timeout_s=20, log=True):
        with trio.fail_after(timeout_s):
            while True:
                # Check in already received data
                lines = self.received.splitlines(keepends=True)

                for idx, line in enumerate(lines):
                    regex_search = re.search(regex, line.replace(b'\r', b'').replace(b'\n', b'').decode('utf-8', errors='ignore'))
                    if regex_search:
                        # Drop this and any previous lines
                        self.received = b''.join(lines[idx+1:])

                        return regex_search

                # Drop all but last line (in case it is not complete)
                if len(lines) > 1:
                    self.received = lines[-1]

                # Receive more data
                chunk = await self.receive_some()
                if chunk == b'':
                    return

                if log:
                    print(chunk.replace(b'\r', b'').decode('utf-8', errors='ignore'), end='')

                self.received = self.received + chunk

    async def send_cmd(self, cmd, wait_str=None):
        if wait_str == None:
            wait_str = self.PROMPT
        await self.serial.send_all('\r\n\r\n'.encode())
        await self.wait_for_regex_in_line(self.PROMPT)
        await self.serial.send_all('{}\r\n'.format(cmd).encode())
        await self.wait_for_regex_in_line(wait_str)

    @property
    @abstractmethod
    def PROMPT(self):
        pass

    @property
    @abstractmethod
    def USES_WIFI(self):
        pass

    @abstractmethod
    async def reset(self):
        pass

    @abstractmethod
    def program(self, fw_image):
        pass

    @abstractmethod
    async def set_wifi_credentials(self, ssid, psk):
        pass

    @abstractmethod
    async def set_golioth_psk_credentials(self, psk_id, psk):
        pass
