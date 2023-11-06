from zephyrboard import ZephyrBoard
from jlinkboard import JLinkBoard

class MIMXRT1024EVK(ZephyrBoard, JLinkBoard):
    def __init__(self, *args, **kwargs):
        JLinkBoard.__init__(self, 'MIMXRT1024xxx5A')
        ZephyrBoard.__init__(self, *args, **kwargs)

    @property
    def USES_WIFI(self):
        return False
