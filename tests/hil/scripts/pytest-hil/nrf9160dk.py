from ncsboard import NCSBoard
from nordicboard import NordicBoard

class nRF9160DK(NCSBoard, NordicBoard):
    @property
    def USES_WIFI(self):
        return False
