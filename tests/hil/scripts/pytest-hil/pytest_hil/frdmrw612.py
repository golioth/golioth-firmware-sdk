from pytest_hil.zephyrboard import ZephyrBoard
from pytest_hil.jlinkboard import JLinkBoard

class FRDMRW612(ZephyrBoard, JLinkBoard):
    def __init__(self, *args, **kwargs):
        JLinkBoard.__init__(self, 'RW612', full_erase=False)
        ZephyrBoard.__init__(self, *args, **kwargs)

    @property
    def USES_WIFI(self):
        return False
