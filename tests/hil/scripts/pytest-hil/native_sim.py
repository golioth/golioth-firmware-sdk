from contextlib import asynccontextmanager
from functools import partial
import subprocess

import trio

from linuxboard import LinuxBoard
from zephyrboard import ZephyrBoard

class NativeSimBoard(ZephyrBoard, LinuxBoard):
    def __init__(self, fw_image: str):
        LinuxBoard.__init__(self, fw_image)

    @asynccontextmanager
    async def started(self):
        async with trio.open_nursery() as nursery:
            self.nursery = nursery

            self.process = await self.nursery.start(partial(trio.run_process,
                                                            [self.binary],
                                                            stdin=subprocess.PIPE,
                                                            stdout=subprocess.PIPE))

            yield self

            self.process.terminate()

    async def reset(self):
        raise RuntimeError('Not available')
