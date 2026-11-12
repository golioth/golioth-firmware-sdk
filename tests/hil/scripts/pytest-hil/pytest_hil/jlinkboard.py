from contextlib import asynccontextmanager

import pylink
from trio_serial import SerialStream

from pytest_hil.board import Board

class JLinkBoard(Board):
    def __init__(self, chip_name, full_erase=True):
        self.chip_name = chip_name
        self.full_erase = full_erase

    @asynccontextmanager
    async def started(self):
        # The console must already be in raw mode when the new firmware starts printing,
        # otherwise the tty echoes the first bytes back into the device shell.
        async with SerialStream(port=self.port,
                                baudrate=self.baudrate) as serial:
            self.serial = serial

            if self.fw_image:
                self.program(self.fw_image)

            await self.wait_for_prompt()

            if self.USES_WIFI:
                await self.set_wifi_credentials(self.wifi_ssid, self.wifi_psk)

            yield self

    def program(self, fw_image):
        jlink = pylink.JLink()
        jlink.open(int(self.serial_number))
        jlink.set_tif(pylink.JLinkInterfaces.SWD)
        jlink.connect(self.chip_name)
        jlink.reset()
        if self.full_erase:
            jlink.erase()
        jlink.flash_file(fw_image, 0)
        jlink.reset(halt=False)
