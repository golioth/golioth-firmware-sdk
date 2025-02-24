from pytest_hil.zephyrboard import ZephyrBoard

class NCSBoard(ZephyrBoard):
    async def reset(self):
        await self.send_cmd('kernel reboot cold', wait_str='Booting nRF Connect SDK')
