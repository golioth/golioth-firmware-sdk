from espboard import ESPBoard
from zephyrboard import ZephyrBoard

class ESP32DevKitCWROVER(ZephyrBoard, ESPBoard):
    def __init__(self, *args, **kwargs):
        ESPBoard.__init__(self, 0x0)
        ZephyrBoard.__init__(self, *args, **kwargs)

    @property
    def USES_WIFI(self):
        return True
