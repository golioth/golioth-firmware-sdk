from zephyrboard import ZephyrBoard
from nordicboard import NordicBoard

class nRF52840DK(ZephyrBoard, NordicBoard):
    @property
    def USES_WIFI(self):
        return True
