from pytest_hil.zephyrboard import ZephyrBoard
from pytest_hil.bmpboard import BMPBoard
from time import sleep

class RAK5010(ZephyrBoard, BMPBoard):
    def program(self, *args, **kwargs):
        BMPBoard.program(self, *args, **kwargs)

        # Arbitrary sleep while the software-based USB serial becomes available
        sleep(3)

    @property
    def USES_WIFI(self):
        return False
