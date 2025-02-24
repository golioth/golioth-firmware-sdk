from pytest_hil.ncsboard import NCSBoard
from pytest_hil.nordicboard import NordicBoard

class nRF9160DK(NCSBoard, NordicBoard):
    @property
    def USES_WIFI(self):
        return False
