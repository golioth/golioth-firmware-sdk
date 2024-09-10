from contextlib import asynccontextmanager
from functools import partial
import subprocess

import trio

from board import Board


class LinuxBoard(Board):
    def __init__(self, fw_image):
        self.binary = fw_image
        self.received = b''
        self.nursery: trio.Nursery | None = None
        self.process: trio.Process | None = None

    @asynccontextmanager
    async def started(self):
        async with trio.open_nursery() as nursery:
            self.nursery = nursery

            yield self

            if self.process:
                self.process.terminate()

    async def receive_some(self) -> bytes:
        assert self.process is not None
        assert self.process.stdout is not None

        return await self.process.stdout.receive_some()

    async def send_all(self, data: bytes):
        assert self.process is not None
        assert self.process.stdin is not None

        await self.process.stdin.send_all(data)

    @property
    def PROMPT(self):
        return ''

    @property
    def USES_WIFI(self):
        return False

    def set_setting(self, key, value):
        pass

    def reset(self):
        pass

    def program(self):
        pass

    async def set_wifi_credentials(self, ssid, psk):
        pass

    async def set_golioth_psk_credentials(self, psk_id, psk):
        self.golioth_psk_id = psk_id
        self.golioth_psk = psk

        env = {
            "GOLIOTH_PSK_ID" : self.golioth_psk_id,
            "GOLIOTH_PSK" : self.golioth_psk
        }

        self.process = await self.nursery.start(partial(trio.run_process,
                                                        ["stdbuf", "-o0", self.binary],
                                                        stdin=subprocess.PIPE,
                                                        stdout=subprocess.PIPE,
                                                        env=env))
