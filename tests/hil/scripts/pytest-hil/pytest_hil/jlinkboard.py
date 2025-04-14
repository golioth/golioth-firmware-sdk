import pylink

from pytest_hil.board import Board

class JLinkBoard(Board):
    def __init__(self, chip_name, full_erase=True):
        self.chip_name = chip_name
        self.full_erase = full_erase

    def program(self, fw_image):
        jlink = pylink.JLink()
        jlink.open(int(self.serial_number))
        jlink.set_tif(pylink.JLinkInterfaces.SWD)
        jlink.connect(self.chip_name)
        jlink.reset()
        if self.full_erase:
            jlink.erase()
        jlink.flash_file(fw_image, 0)
        jlink.reset(halt=False)
