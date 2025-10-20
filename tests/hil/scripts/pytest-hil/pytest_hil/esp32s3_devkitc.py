from pytest_hil.espboard import ESPBoard
from pytest_hil.zephyrboard import ZephyrBoard

class ESP32S3DevKitC(ZephyrBoard, ESPBoard):
    def __init__(self, *args, **kwargs):
        ESPBoard.__init__(self, 0x0)
        ZephyrBoard.__init__(self, *args, **kwargs)

    @property
    def USES_WIFI(self):
        return True
