import pylink

from pytest_hil.board import Board

class JLinkBoard(Board):
    def __init__(self, chip_name):
        self.chip_name = chip_name

    def program(self, fw_image):
        jlink = pylink.JLink()
        jlink.open(int(self.serial_number))
        jlink.set_tif(pylink.JLinkInterfaces.SWD)
        jlink.connect(self.chip_name)
        jlink.reset()
        jlink.erase()
        jlink.flash_file(fw_image, 0)
        jlink.reset(halt=False)
