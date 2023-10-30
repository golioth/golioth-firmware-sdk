from zephyrboard import ZephyrBoard

class nRF52840DK(ZephyrBoard):
    @property
    def USES_WIFI(self):
        return True
