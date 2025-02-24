from pytest_hil.zephyrboard import ZephyrBoard
from pytest_hil.nordicboard import NordicBoard

class nRF52840DK(ZephyrBoard, NordicBoard):
    @property
    def USES_WIFI(self):
        return True
